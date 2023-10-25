/*
	Copyright 2018 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "gl4.h"
#include "rend/gles/glcache.h"
#include "rend/gles/naomi2.h"
#include "rend/tileclip.h"
#include "rend/osd.h"

#include <memory>

static gl4PipelineShader* CurrentShader;
GLuint geom_fbo;
GLuint stencilTexId;
GLuint opaqueTexId;
GLuint depthTexId;
GLuint texSamplers[2];
GLuint depth_fbo;
GLuint depthSaveTexId;

static gl4PipelineShader *gl4GetProgram(bool cp_AlphaTest, bool pp_InsideClipping,
							bool pp_Texture, bool pp_UseAlpha, bool pp_IgnoreTexA, u32 pp_ShadInstr, bool pp_Offset,
							u32 pp_FogCtrl, bool pp_TwoVolumes, bool pp_Gouraud, bool pp_BumpMap, bool fog_clamping,
							bool palette, bool naomi2, Pass pass)
{
	u32 rv=0;

	rv |= (int)pp_InsideClipping;
	rv <<= 1; rv |= (int)cp_AlphaTest;
	rv <<= 1; rv |= (int)pp_Texture;
	rv <<= 1; rv |= (int)pp_UseAlpha;
	rv <<= 1; rv |= (int)pp_IgnoreTexA;
	rv <<= 2; rv |= pp_ShadInstr;
	rv <<= 1; rv |= (int)pp_Offset;
	rv <<= 2; rv |= pp_FogCtrl;
	rv <<= 1; rv |= (int)pp_TwoVolumes;
	rv <<= 1; rv |= (int)pp_Gouraud;
	rv <<= 1; rv |= (int)pp_BumpMap;
	rv <<= 1; rv |= (int)fog_clamping;
	rv <<= 1; rv |= (int)palette;
	rv <<= 1; rv |= (int)naomi2;
	rv <<= 2; rv |= (int)pass;
	rv <<= 1; rv |= (int)(!settings.platform.isNaomi2() && config::NativeDepthInterpolation);

	gl4PipelineShader *shader = &gl4.shaders[rv];
	if (shader->program == 0)
	{
		shader->cp_AlphaTest = cp_AlphaTest;
		shader->pp_InsideClipping = pp_InsideClipping;
		shader->pp_Texture = pp_Texture;
		shader->pp_UseAlpha = pp_UseAlpha;
		shader->pp_IgnoreTexA = pp_IgnoreTexA;
		shader->pp_ShadInstr = pp_ShadInstr;
		shader->pp_Offset = pp_Offset;
		shader->pp_FogCtrl = pp_FogCtrl;
		shader->pp_TwoVolumes = pp_TwoVolumes;
		shader->pp_Gouraud = pp_Gouraud;
		shader->pp_BumpMap = pp_BumpMap;
		shader->fog_clamping = fog_clamping;
		shader->palette = palette;
		shader->naomi2 = naomi2;
		shader->pass = pass;
		shader->divPosZ = !settings.platform.isNaomi2() && config::NativeDepthInterpolation;
		gl4CompilePipelineShader(shader);
	}

	return shader;
}

static void SetTextureRepeatMode(int index, GLuint dir, u32 clamp, u32 mirror)
{
	if (clamp)
		glSamplerParameteri(texSamplers[index], dir, GL_CLAMP_TO_EDGE);
	else
		glSamplerParameteri(texSamplers[index], dir, mirror ? GL_MIRRORED_REPEAT : GL_REPEAT);
}

static void SetBaseClipping()
{
	if (gl4ShaderUniforms.base_clipping.enabled)
	{
		glcache.Enable(GL_SCISSOR_TEST);
		glcache.Scissor(gl4ShaderUniforms.base_clipping.x, gl4ShaderUniforms.base_clipping.y, gl4ShaderUniforms.base_clipping.width, gl4ShaderUniforms.base_clipping.height);
	}
	else
		glcache.Disable(GL_SCISSOR_TEST);
}

template <u32 Type, bool SortingEnabled, Pass pass>
static void SetGPState(const PolyParam* gp)
{
	// Trilinear filtering. Ignore if texture isn't mipmapped (shenmue snowflakes)
	if (gp->pcw.Texture && gp->tsp.FilterMode > 1 && Type != ListType_Punch_Through && gp->tcw.MipMapped == 1)
	{
		gl4ShaderUniforms.trilinear_alpha = 0.25f * (gp->tsp.MipMapD & 0x3);
		if (gp->tsp.FilterMode == 2)
			// Trilinear pass A
			gl4ShaderUniforms.trilinear_alpha = 1.0f - gl4ShaderUniforms.trilinear_alpha;
	}
	else
		gl4ShaderUniforms.trilinear_alpha = 1.0;

	int clip_rect[4] = {};
	TileClipping clipmode = GetTileClip(gp->tileclip, ViewportMatrix, clip_rect);
	bool gpuPalette;

	if (pass == Pass::Depth)
	{
		gpuPalette = gp->texture != nullptr && Type == ListType_Punch_Through ? gp->texture->gpuPalette : false;
		CurrentShader = gl4GetProgram(Type == ListType_Punch_Through ? true : false,
				clipmode == TileClipping::Inside,
				Type == ListType_Punch_Through ? gp->pcw.Texture : false,
				true,
				gp->tsp.IgnoreTexA,
				0,
				false,
				2,
				false,	// TODO Can PT have two different textures for area 0 and 1 ??
				false,
				false,
				false,
				gpuPalette,
				gp->isNaomi2(),
				pass);
	}
	else
	{
		// Two volumes mode only supported for OP and PT
		bool two_volumes_mode = (gp->tsp1.full != (u32)-1) && Type != ListType_Translucent;
		bool color_clamp = gp->tsp.ColorClamp && (pvrrc.fog_clamp_min.full != 0 || pvrrc.fog_clamp_max.full != 0xffffffff);

		int fog_ctrl = config::Fog ? gp->tsp.FogCtrl : 2;
		gpuPalette = gp->texture != nullptr ? gp->texture->gpuPalette : false;

		CurrentShader = gl4GetProgram(Type == ListType_Punch_Through ? true : false,
				clipmode == TileClipping::Inside,
				gp->pcw.Texture,
				gp->tsp.UseAlpha,
				gp->tsp.IgnoreTexA,
				gp->tsp.ShadInstr,
				gp->pcw.Offset,
				fog_ctrl,
				two_volumes_mode,
				gp->pcw.Gouraud,
				gp->tcw.PixelFmt == PixelBumpMap,
				color_clamp,
				gpuPalette,
				gp->isNaomi2(),
				pass);
	}
	glcache.UseProgram(CurrentShader->program);

	if (gpuPalette)
	{
		if (gp->tcw.PixelFmt == PixelPal4)
			gl4ShaderUniforms.palette_index = gp->tcw.PalSelect << 4;
		else
			gl4ShaderUniforms.palette_index = (gp->tcw.PalSelect >> 4) << 8;
	}

	gl4ShaderUniforms.tsp0 = gp->tsp;
	gl4ShaderUniforms.tsp1 = gp->tsp1;
	gl4ShaderUniforms.tcw0 = gp->tcw;
	gl4ShaderUniforms.tcw1 = gp->tcw1;
	gl4ShaderUniforms.Set(CurrentShader);

	if (pass == Pass::Color && (Type == ListType_Translucent || Type == ListType_Punch_Through))
	{
		glcache.Enable(GL_BLEND);
		glcache.BlendFunc(SrcBlendGL[gp->tsp.SrcInstr], DstBlendGL[gp->tsp.DstInstr]);
	}
	else
		glcache.Disable(GL_BLEND);

	if (clipmode == TileClipping::Inside)
		glUniform4f(CurrentShader->pp_ClipTest, (float)clip_rect[0], (float)clip_rect[1],
				(float)(clip_rect[0] + clip_rect[2]), (float)(clip_rect[1] + clip_rect[3]));
	if (clipmode == TileClipping::Outside)
	{
		glcache.Enable(GL_SCISSOR_TEST);
		glcache.Scissor(clip_rect[0], clip_rect[1], clip_rect[2], clip_rect[3]);
	}
	else
		SetBaseClipping();

	// This bit controls which pixels are affected by modvols
	const u32 stencil = gp->pcw.Shadow != 0 ? 0x80 : 0x0;

	glcache.StencilFunc(GL_ALWAYS, stencil, stencil);

	if (CurrentShader->pp_Texture)
	{
		for (int i = 0; i < 2; i++)
		{
			glActiveTexture(GL_TEXTURE0 + i);
			TextureCacheData *texture = (TextureCacheData *)(i == 0 ? gp->texture : gp->texture1);

			if (i == 0)
				glcache.BindTexture(GL_TEXTURE_2D, texture == nullptr ? 0 : texture->texID);
			else
				glBindTexture(GL_TEXTURE_2D, texture == nullptr ? 0 : texture->texID);

			if (texture != nullptr)
			{
				TSP tsp = i == 0 ? gp->tsp : gp->tsp1;

				glBindSampler(i, texSamplers[i]);
				SetTextureRepeatMode(i, GL_TEXTURE_WRAP_S, tsp.ClampU, tsp.FlipU);
				SetTextureRepeatMode(i, GL_TEXTURE_WRAP_T, tsp.ClampV, tsp.FlipV);

				bool nearest_filter;
				if (config::TextureFiltering == 0) {
					nearest_filter = tsp.FilterMode == 0;
				} else if (config::TextureFiltering == 1) {
					nearest_filter = true;
				} else {
					nearest_filter = false;
				}

				bool mipmapped = gp->tcw.MipMapped != 0 && gp->tcw.ScanOrder == 0 && config::UseMipmaps;

				//set texture filter mode
				if (nearest_filter)
				{
					//nearest-neighbor filtering
					glSamplerParameteri(texSamplers[i], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glSamplerParameteri(texSamplers[i], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				}
				else
				{
					//bilinear filtering
					//PowerVR supports also trilinear via two passes, but we ignore that for now
					glSamplerParameteri(texSamplers[i], GL_TEXTURE_MIN_FILTER, mipmapped && Type != ListType_Punch_Through ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
					glSamplerParameteri(texSamplers[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				}

				if (mipmapped)
					glSamplerParameterf(texSamplers[i], GL_TEXTURE_LOD_BIAS, D_Adjust_LoD_Bias[tsp.MipMapD]);

				if (gl.max_anisotropy > 1.f)
				{
					if (config::AnisotropicFiltering > 1 && !nearest_filter)
						glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY,
								std::min<float>(config::AnisotropicFiltering, gl.max_anisotropy));
					else
						glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 1.f);
				}
			}
		}
		glActiveTexture(GL_TEXTURE0);
	}

	SetCull(gp->isp.CullMode ^ 1);

	//set Z mode, only if required
	if (Type == ListType_Punch_Through || (pass == Pass::Depth && SortingEnabled))
	{
		glcache.DepthFunc(GL_GEQUAL);
	}
	else if (Type == ListType_Opaque || (Type == ListType_Translucent && !SortingEnabled))
	{
		glcache.DepthFunc(Zfunction[gp->isp.DepthMode]);
	}

	if (pass == Pass::Depth || pass == Pass::Color)
	{
		// Z Write Disable seems to be ignored for punch-through polys
		// Fixes Worms World Party, Bust-a-Move 4 and Re-Volt
		if (Type == ListType_Punch_Through)
			glcache.DepthMask(GL_TRUE);
		else
			glcache.DepthMask(!gp->isp.ZWriteDis);
	}
	else
		glcache.DepthMask(GL_FALSE);
	if (gp->isNaomi2())
		setN2Uniforms(gp, CurrentShader, pvrrc);
}

template <u32 Type, bool SortingEnabled, Pass pass>
static void DrawList(const std::vector<PolyParam>& gply, int first, int count)
{
	if (count == 0)
		return;
	const PolyParam* params = &gply[first];

	u32 firstVertexIdx = Type == ListType_Translucent ? pvrrc.idx[gply[0].first] : 0;
	while (count-- > 0)
	{
		if (params->count > 2)
		{
			if ((Type == ListType_Opaque || (Type == ListType_Translucent && !SortingEnabled)) && params->isp.DepthMode == 0)
			{
				// depthFunc = never
				params++;
				continue;
			}
			if (pass == Pass::OIT && Type == ListType_Translucent && params->tsp.SrcInstr == 0 && params->tsp.DstInstr == 1)
			{
				// dst = dst
				params++;
				continue;
			}
			gl4ShaderUniforms.poly_number = ((params - &gply[0]) << 17) - firstVertexIdx;
			SetGPState<Type, SortingEnabled, pass>(params);
			glDrawElements(GL_TRIANGLE_STRIP, params->count, GL_UNSIGNED_INT, (GLvoid*)(sizeof(u32) * params->first)); glCheck();
		}

		params++;
	}
}

void gl4SetupMainVBO()
{
	gl4.vbo.getMainVAO().bind(gl4.vbo.getVertexBuffer(), gl4.vbo.getIndexBuffer());
}

void Gl4MainVertexArray::defineVtxAttribs()
{
	//setup vertex buffers attrib pointers
	glEnableVertexAttribArray(VERTEX_POS_ARRAY);
	glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,x));

	glEnableVertexAttribArray(VERTEX_COL_BASE_ARRAY);
	glVertexAttribPointer(VERTEX_COL_BASE_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex,col));

	glEnableVertexAttribArray(VERTEX_COL_OFFS_ARRAY);
	glVertexAttribPointer(VERTEX_COL_OFFS_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex,spc));

	glEnableVertexAttribArray(VERTEX_UV_ARRAY);
	glVertexAttribPointer(VERTEX_UV_ARRAY, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,u));

	glEnableVertexAttribArray(VERTEX_COL_BASE1_ARRAY);
	glVertexAttribPointer(VERTEX_COL_BASE1_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, col1));

	glEnableVertexAttribArray(VERTEX_COL_OFFS1_ARRAY);
	glVertexAttribPointer(VERTEX_COL_OFFS1_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, spc1));

	glEnableVertexAttribArray(VERTEX_UV1_ARRAY);
	glVertexAttribPointer(VERTEX_UV1_ARRAY, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u1));

	glEnableVertexAttribArray(VERTEX_NORM_ARRAY);
	glVertexAttribPointer(VERTEX_NORM_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, nx));
}

void gl4SetupModvolVBO()
{
	gl4.vbo.getModVolVAO().bind(gl4.vbo.getModVolBuffer());
}

void Gl4ModvolVertexArray::defineVtxAttribs()
{
	glEnableVertexAttribArray(VERTEX_POS_ARRAY); glCheck();
	glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(float)*3, (void*)0); glCheck();
}

static void DrawModVols(int first, int count)
{
	if (count == 0 || pvrrc.modtrig.empty())
		return;

	gl4SetupModvolVBO();

	glcache.Disable(GL_BLEND);
	SetBaseClipping();

	glcache.Enable(GL_DEPTH_TEST);
	glcache.DepthMask(GL_FALSE);
	glcache.DepthFunc(GL_GREATER);

	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	ModifierVolumeParam* params = &pvrrc.global_param_mvo[first];

	int mod_base = -1;

	for (int cmv = 0; cmv < count; cmv++)
	{
		ModifierVolumeParam& param = params[cmv];

		if (param.count == 0)
			continue;
		if (param.isNaomi2())
		{
			glcache.UseProgram(gl4.n2ModVolShader.program);
			glUniformMatrix4fv(gl4.n2ModVolShader.mvMat, 1, GL_FALSE, pvrrc.matrices[param.mvMatrix].mat);
			glUniformMatrix4fv(gl4.n2ModVolShader.projMat, 1, GL_FALSE, pvrrc.matrices[param.projMatrix].mat);
		}
		else
			glcache.UseProgram(gl4.modvol_shader.program);

		u32 mv_mode = param.isp.DepthMode;

		if (mod_base == -1)
			mod_base = param.first;

		if (!param.isp.VolumeLast && mv_mode > 0)
			SetMVS_Mode(Or, param.isp);		// OR'ing (open volume or quad)
		else
			SetMVS_Mode(Xor, param.isp);	// XOR'ing (closed volume)

		glDrawArrays(GL_TRIANGLES, param.first * 3, param.count * 3);

		if (mv_mode == 1 || mv_mode == 2)
		{
			// Sum the area
			SetMVS_Mode(mv_mode == 1 ? Inclusion : Exclusion, param.isp);
			glDrawArrays(GL_TRIANGLES, mod_base * 3, (param.first + param.count - mod_base) * 3);
			mod_base = -1;
		}
	}

	//restore states
	gl4SetupMainVBO();
	glcache.Enable(GL_DEPTH_TEST);
	glcache.DepthMask(GL_TRUE);
}

static GLuint CreateColorFBOTexture(int width, int height)
{
	GLuint texId = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, texId);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
	glCheck();

	return texId;
}

void gl4CreateTextures(int width, int height)
{
	if (geom_fbo == 0)
	{
		glGenFramebuffers(1, &geom_fbo);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, geom_fbo);

	stencilTexId = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, stencilTexId); glCheck();
	glcache.TexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);		// OpenGL >= 4.3
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// Using glTexStorage2D instead of glTexImage2D to satisfy requirement GL_TEXTURE_IMMUTABLE_FORMAT=true, needed for glTextureView below
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH32F_STENCIL8, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, stencilTexId, 0); glCheck();
	glCheck();

	opaqueTexId = CreateColorFBOTexture(width, height);

	depthTexId = glcache.GenTexture();
	glTextureView(depthTexId, GL_TEXTURE_2D, stencilTexId, GL_DEPTH32F_STENCIL8, 0, 1, 0, 1);
	glCheck();
	glcache.BindTexture(GL_TEXTURE_2D, depthTexId);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glCheck();

	GLuint uStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	verify(uStatus == GL_FRAMEBUFFER_COMPLETE);
}

void gl4DrawStrips(GLuint output_fbo, int width, int height)
{
	checkOverflowAndReset();
	glBindFramebuffer(GL_FRAMEBUFFER, geom_fbo);
	if (!pvrrc.isRTT)
	{
		glcache.Disable(GL_SCISSOR_TEST);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		if (pvrrc.clearFramebuffer)
		{
			// Clear framebuffer
			glcache.ClearColor(VO_BORDER_COL.red(), VO_BORDER_COL.green(), VO_BORDER_COL.blue(), 1.f);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		else
		{
			// Copy previous framebuffer content (in case of partial render)
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, geom_fbo);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, output_fbo);
			glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glCheck();
			glBindFramebuffer(GL_FRAMEBUFFER, geom_fbo);
		}
		if (gl4ShaderUniforms.base_clipping.enabled)
			glcache.Enable(GL_SCISSOR_TEST);
	}
	if (texSamplers[0] == 0)
		glGenSamplers(2, texSamplers);

	glcache.DepthMask(GL_TRUE);
	glClearDepth(0.0);
	glcache.StencilMask(0xFF);
	glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); glCheck();

	//Draw the strips !

	//We use sampler 0
	glActiveTexture(GL_TEXTURE0);
	glProvokingVertex(GL_LAST_VERTEX_CONVENTION);

	RenderPass previous_pass = {};
	int render_pass_count = pvrrc.render_passes.size();

	for (int render_pass = 0; render_pass < render_pass_count; render_pass++)
    {
        const RenderPass& current_pass = pvrrc.render_passes[render_pass];

        // Check if we can skip this pass, in part or completely, in case nothing is drawn (Cosmic Smash)
		bool skip_op_pt = true;
		bool skip_tr = true;
		for (u32 j = previous_pass.op_count; skip_op_pt && j < current_pass.op_count; j++)
		{
			if (pvrrc.global_param_op[j].count > 2)
				skip_op_pt = false;
		}
		for (u32 j = previous_pass.pt_count; skip_op_pt && j < current_pass.pt_count; j++)
		{
			if (pvrrc.global_param_pt[j].count > 2)
				skip_op_pt = false;
		}
		for (u32 j = previous_pass.tr_count; skip_tr && j < current_pass.tr_count; j++)
		{
			if (pvrrc.global_param_tr[j].count > 2)
				skip_tr = false;
		}
		if (skip_op_pt && skip_tr)
		{
			previous_pass = current_pass;
			continue;
		}
        DEBUG_LOG(RENDERER, "Render pass %d/%d OP %d PT %d TR %d MV %d TMV %d autosort %d", render_pass + 1, render_pass_count,
        		current_pass.op_count - previous_pass.op_count,
				current_pass.pt_count - previous_pass.pt_count,
				current_pass.tr_count - previous_pass.tr_count,
				current_pass.mvo_count - previous_pass.mvo_count,
				current_pass.mv_op_tr_shared ? current_pass.mvo_count - previous_pass.mvo_count : current_pass.mvo_tr_count - previous_pass.mvo_tr_count,
				current_pass.autosort);

        gl4SetupMainVBO();

		if (!skip_op_pt)
		{
			//
			// PASS 1: Geometry pass to update depth and stencil
			//
			if (render_pass > 0)
			{
				// Make a copy of the depth buffer that will be reused in pass 2
				if (depth_fbo == 0)
					glGenFramebuffers(1, &depth_fbo);
				glBindFramebuffer(GL_FRAMEBUFFER, depth_fbo);
				if (depthSaveTexId == 0)
				{
					depthSaveTexId = glcache.GenTexture();
					glcache.BindTexture(GL_TEXTURE_2D, depthSaveTexId);
					glcache.TexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
					glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, max_image_width, max_image_height, 0, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, NULL); glCheck();
					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthSaveTexId, 0); glCheck();
				}
				GLuint uStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				verify(uStatus == GL_FRAMEBUFFER_COMPLETE);

				glBindFramebuffer(GL_READ_FRAMEBUFFER, geom_fbo);
				glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
				glCheck();

				glBindFramebuffer(GL_FRAMEBUFFER, geom_fbo);
			}
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glcache.Enable(GL_DEPTH_TEST);
			glcache.DepthMask(GL_TRUE);
			glcache.Enable(GL_STENCIL_TEST);
			glcache.StencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

			DrawList<ListType_Opaque, false, Pass::Depth>(pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count - previous_pass.op_count);
			DrawList<ListType_Punch_Through, false, Pass::Depth>(pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count - previous_pass.pt_count);

			// Modifier volumes
			if (config::ModifierVolumes)
				DrawModVols(previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count);

			//
			// PASS 2: Render OP and PT to fbo
			//
			if (render_pass == 0)
			{
				glcache.DepthMask(GL_TRUE);
				glClearDepth(0.0);
				glClear(GL_DEPTH_BUFFER_BIT);
			}
			else
			{
				// Restore the depth buffer from the last render pass
				// FIXME This is pretty slow apparently (CS)
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, geom_fbo);
				glBindFramebuffer(GL_READ_FRAMEBUFFER, depth_fbo);
				glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
				glCheck();
				glBindFramebuffer(GL_FRAMEBUFFER, geom_fbo);
			}

			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glcache.Disable(GL_STENCIL_TEST);

			// Bind stencil buffer for the fragment shader (shadowing)
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, stencilTexId);
			glActiveTexture(GL_TEXTURE0);
			glCheck();

			//Opaque
			DrawList<ListType_Opaque, false, Pass::Color>(pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count - previous_pass.op_count);

			//Alpha tested
			DrawList<ListType_Punch_Through, false, Pass::Color>(pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count - previous_pass.pt_count);

			// Unbind stencil
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE0);
		}

		if (!skip_tr)
		{
			//
			// PASS 3: Render TR to a-buffers
			//
			if (current_pass.autosort)
			{
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
				glcache.Disable(GL_DEPTH_TEST);

				// Although the depth test is disabled and thus writes to the depth buffer are also disabled,
				// AMD cards have serious issues when the depth/stencil texture is still bound to the framebuffer
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, depthTexId);
				glActiveTexture(GL_TEXTURE0);
				DrawList<ListType_Translucent, true, Pass::OIT>(pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count);

				// Translucent modifier volumes
				if (config::ModifierVolumes)
				{
					SetBaseClipping();
					if (current_pass.mv_op_tr_shared)
						DrawTranslucentModVols(previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count, true);
					else
						DrawTranslucentModVols(previous_pass.mvo_tr_count, current_pass.mvo_tr_count - previous_pass.mvo_tr_count, false);
				}

				// Rebind the depth/stencil texture to the framebuffer
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, stencilTexId, 0);

				if (render_pass < render_pass_count - 1)
				{
					//
					// PASS 3b: Geometry pass with TR to update the depth for the next TA render pass
					//
					// Unbind depth texture
					glActiveTexture(GL_TEXTURE2);
					glBindTexture(GL_TEXTURE_2D, 0);
					glActiveTexture(GL_TEXTURE0);

					glcache.Enable(GL_DEPTH_TEST);
					DrawList<ListType_Translucent, true, Pass::Depth>(pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count);
				}
			}
			else
			{
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				glcache.Enable(GL_DEPTH_TEST);
				DrawList<ListType_Translucent, false, Pass::Color>(pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count);
				glcache.Disable(GL_BLEND);
			}
			glCheck();

			if (render_pass < render_pass_count - 1)
			{
				//
				// PASS 3c: Render a-buffer to temporary texture
				//
				GLuint texId = CreateColorFBOTexture(max_image_width, max_image_height);

				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

				glActiveTexture(GL_TEXTURE0);
				glBindSampler(0, 0);
				glcache.BindTexture(GL_TEXTURE_2D, opaqueTexId);

				renderABuffer(false);

				glcache.DeleteTextures(1, &opaqueTexId);
				opaqueTexId = texId;

				glCheck();
			}
		}

		if (!skip_op_pt && render_pass < render_pass_count - 1)
		{
			// Clear the stencil from this pass
			glcache.StencilMask(0xFF);
			glClear(GL_STENCIL_BUFFER_BIT);
		}

		previous_pass = current_pass;
	}

	//
	// PASS 4: Render a-buffers to screen
	//
	glBindFramebuffer(GL_FRAMEBUFFER, output_fbo); glCheck();
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	glActiveTexture(GL_TEXTURE0);
	glBindSampler(0, 0);
	glcache.BindTexture(GL_TEXTURE_2D, opaqueTexId);
	renderABuffer(true);
}
