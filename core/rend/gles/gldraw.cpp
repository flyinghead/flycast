#include "glcache.h"
#include "gles.h"
#include "rend/tileclip.h"
#include "rend/osd.h"
#include "naomi2.h"
#include "rend/transform_matrix.h"
#include "postprocess.h"

#include <memory>

/*

Drawing and related state management
Takes vertex, textures and renders to the currently set up target

*/

const static u32 CullModes[] =
{
	GL_NONE, //0    No culling          No culling
	GL_NONE, //1    Cull if Small       Cull if ( |det| < fpu_cull_val )

	GL_FRONT, //2   Cull if Negative    Cull if ( |det| < 0 ) or ( |det| < fpu_cull_val )
	GL_BACK,  //3   Cull if Positive    Cull if ( |det| > 0 ) or ( |det| < fpu_cull_val )
};
const u32 Zfunction[] =
{
	GL_NEVER,       //0 Never
	GL_LESS,        //1 Less
	GL_EQUAL,       //2 Equal
	GL_LEQUAL,      //3 Less Or Equal
	GL_GREATER,     //4 Greater
	GL_NOTEQUAL,    //5 Not Equal
	GL_GEQUAL,      //6 Greater Or Equal
	GL_ALWAYS,      //7 Always
};

/*
0   Zero                  (0, 0, 0, 0)
1   One                   (1, 1, 1, 1)
2   Other Color           (OR, OG, OB, OA)
3   Inverse Other Color   (1-OR, 1-OG, 1-OB, 1-OA)
4   SRC Alpha             (SA, SA, SA, SA)
5   Inverse SRC Alpha     (1-SA, 1-SA, 1-SA, 1-SA)
6   DST Alpha             (DA, DA, DA, DA)
7   Inverse DST Alpha     (1-DA, 1-DA, 1-DA, 1-DA)
*/

const u32 DstBlendGL[] =
{
	GL_ZERO,
	GL_ONE,
	GL_SRC_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA
};

const u32 SrcBlendGL[] =
{
	GL_ZERO,
	GL_ONE,
	GL_DST_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA
};

PipelineShader* CurrentShader;
u32 gcflip;

void SetCull(u32 CullMode)
{
	if (CullModes[CullMode] == GL_NONE)
		glcache.Disable(GL_CULL_FACE);
	else
	{
		glcache.Enable(GL_CULL_FACE);
		glcache.CullFace(CullModes[CullMode]); //GL_FRONT/GL_BACK, ...
	}
}

static void SetTextureRepeatMode(GLuint dir, u32 clamp, u32 mirror)
{
	if (clamp)
		glcache.TexParameteri(GL_TEXTURE_2D, dir, GL_CLAMP_TO_EDGE);
	else
		glcache.TexParameteri(GL_TEXTURE_2D, dir, mirror ? GL_MIRRORED_REPEAT : GL_REPEAT);
}

static void SetBaseClipping()
{
	if (ShaderUniforms.base_clipping.enabled)
	{
		glcache.Enable(GL_SCISSOR_TEST);
		glcache.Scissor(ShaderUniforms.base_clipping.x, ShaderUniforms.base_clipping.y, ShaderUniforms.base_clipping.width, ShaderUniforms.base_clipping.height);
	}
	else
		glcache.Disable(GL_SCISSOR_TEST);
}

template <u32 Type, bool SortingEnabled>
void SetGPState(const PolyParam* gp,u32 cflip=0)
{
	if (gp->pcw.Texture && gp->tsp.FilterMode > 1 && Type != ListType_Punch_Through && gp->tcw.MipMapped == 1)
	{
		ShaderUniforms.trilinear_alpha = 0.25f * (gp->tsp.MipMapD & 0x3);
		if (gp->tsp.FilterMode == 2)
			// Trilinear pass A
			ShaderUniforms.trilinear_alpha = 1.f - ShaderUniforms.trilinear_alpha;
	}
	else
		ShaderUniforms.trilinear_alpha = 1.f;

	bool color_clamp = gp->tsp.ColorClamp && (pvrrc.fog_clamp_min.full != 0 || pvrrc.fog_clamp_max.full != 0xffffffff);
	int fog_ctrl = config::Fog ? gp->tsp.FogCtrl : 2;

	int clip_rect[4] = {};
	TileClipping clipmode = GetTileClip(gp->tileclip, ViewportMatrix, clip_rect);
	TextureCacheData *texture = (TextureCacheData *)gp->texture;
	bool gpuPalette = texture != nullptr ? texture->gpuPalette : false;

	CurrentShader = GetProgram(Type == ListType_Punch_Through ? true : false,
								  clipmode == TileClipping::Inside,
								  gp->pcw.Texture,
								  gp->tsp.UseAlpha,
								  gp->tsp.IgnoreTexA,
								  gp->tsp.ShadInstr,
								  gp->pcw.Offset,
								  fog_ctrl,
								  gp->pcw.Gouraud,
								  gp->tcw.PixelFmt == PixelBumpMap,
								  color_clamp,
								  ShaderUniforms.trilinear_alpha != 1.f,
								  gpuPalette,
								  gp->isNaomi2());
	
	glcache.UseProgram(CurrentShader->program);
	if (CurrentShader->trilinear_alpha != -1)
		glUniform1f(CurrentShader->trilinear_alpha, ShaderUniforms.trilinear_alpha);
	if (gpuPalette)
	{
		if (gp->tcw.PixelFmt == PixelPal4)
			ShaderUniforms.palette_index = gp->tcw.PalSelect << 4;
		else
			ShaderUniforms.palette_index = (gp->tcw.PalSelect >> 4) << 8;
		glUniform1i(CurrentShader->palette_index, ShaderUniforms.palette_index);
	}

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

	if (config::ModifierVolumes)
	{
		//This bit control which pixels are affected
		//by modvols
		const u32 stencil = gp->pcw.Shadow != 0 ? 0x80 : 0;
		glcache.StencilFunc(GL_ALWAYS, stencil, stencil);
	}

	if (texture != nullptr)
	{
		glcache.BindTexture(GL_TEXTURE_2D, texture->texID);

		SetTextureRepeatMode(GL_TEXTURE_WRAP_S, gp->tsp.ClampU, gp->tsp.FlipU);
		SetTextureRepeatMode(GL_TEXTURE_WRAP_T, gp->tsp.ClampV, gp->tsp.FlipV);

		bool nearest_filter;
		if (config::TextureFiltering == 0) {
			nearest_filter = gp->tsp.FilterMode == 0 || gpuPalette;
		} else if (config::TextureFiltering == 1) {
			nearest_filter = true;
		} else {
			nearest_filter = false;
		}

		bool mipmapped = texture->IsMipmapped();

		//set texture filter mode
		if (nearest_filter)
		{
			//nearest-neighbor filtering
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
		else
		{
			//bilinear filtering
			//PowerVR supports also trilinear via two passes, but we ignore that for now
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmapped ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}

#ifdef GL_TEXTURE_LOD_BIAS
		if (!gl.is_gles && gl.gl_major >= 3 && mipmapped)
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, D_Adjust_LoD_Bias[gp->tsp.MipMapD]);
#endif

		if (gl.max_anisotropy > 1.f)
		{
			if (config::AnisotropicFiltering > 1 && !nearest_filter)
			{
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY,
						std::min<float>(config::AnisotropicFiltering, gl.max_anisotropy));
				// Set the recommended minification filter for best results
				if (mipmapped)
					glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			}
			else
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 1.f);
		}
	}

	// Apparently punch-through polys support blending, or at least some combinations
	if (Type == ListType_Translucent || Type == ListType_Punch_Through)
	{
		glcache.Enable(GL_BLEND);
		glcache.BlendFunc(SrcBlendGL[gp->tsp.SrcInstr],DstBlendGL[gp->tsp.DstInstr]);
	}
	else
		glcache.Disable(GL_BLEND);

	//set cull mode !
	//cflip is required when exploding triangles for triangle sorting
	//gcflip is global clip flip, needed for when rendering to texture due to mirrored Y direction
	SetCull(gp->isp.CullMode^cflip^gcflip);

	//set Z mode, only if required
	if (Type == ListType_Punch_Through || (Type == ListType_Translucent && SortingEnabled))
	{
		glcache.DepthFunc(Zfunction[6]); // >=
	}
	else
	{
		glcache.DepthFunc(Zfunction[gp->isp.DepthMode]);
	}

	if (SortingEnabled /* && !config::PerStripSorting */) // Looks glitchy too but less missing graphics (but wrong depth order...)
		glcache.DepthMask(GL_FALSE);
	else
	{
		// Z Write Disable seems to be ignored for punch-through.
		// Fixes Worms World Party, Bust-a-Move 4 and Re-Volt
		if (Type == ListType_Punch_Through)
			glcache.DepthMask(GL_TRUE);
		else
			glcache.DepthMask(!gp->isp.ZWriteDis);
	}
	if (CurrentShader->naomi2)
		setN2Uniforms(gp, CurrentShader, pvrrc);
}

template <u32 Type, bool SortingEnabled>
void DrawList(const std::vector<PolyParam>& gply, int first, int count)
{
	if (count == 0)
		return;
	const PolyParam* params = &gply[first];

	glcache.Enable(GL_STENCIL_TEST);
	glcache.StencilFunc(GL_ALWAYS,0,0);
	glcache.StencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);

	for (; count > 0; count--, params++)
	{
		if (params->count < 3)
			continue;
		if ((Type == ListType_Opaque || (Type == ListType_Translucent && !SortingEnabled))
				&& params->isp.DepthMode == 0)
			// depthFunc = never
			continue;
		SetGPState<Type,SortingEnabled>(params);
		glDrawElements(GL_TRIANGLE_STRIP, params->count, gl.index_type,
				(GLvoid*)(gl.get_index_size() * params->first)); glCheck();
	}
}

static void drawSorted(int first, int count, bool multipass)
{
	if (count == 0)
		return;
	glcache.Enable(GL_STENCIL_TEST);
	glcache.StencilFunc(GL_ALWAYS,0,0);
	glcache.StencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);

	int end = first + count;
	for (int p = first; p < end; p++)
	{
		const PolyParam* params = &pvrrc.global_param_tr[pvrrc.sortedTriangles[p].polyIndex];
		SetGPState<ListType_Translucent,true>(params);
		glDrawElements(GL_TRIANGLES, pvrrc.sortedTriangles[p].count, gl.index_type,
				(GLvoid*)(gl.get_index_size() * pvrrc.sortedTriangles[p].first));
	}

	if (multipass && config::TranslucentPolygonDepthMask)
	{
		// Write to the depth buffer now. The next render pass might need it. (Cosmic Smash)
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glcache.Disable(GL_BLEND);

		glcache.StencilMask(0);

		// We use the modifier volumes shader because it's fast. We don't need textures, etc.
		glcache.UseProgram(gl.modvol_shader.program);
		glUniform1f(gl.modvol_shader.sp_ShaderColor, 1.f);

		glcache.DepthFunc(GL_GEQUAL);
		glcache.DepthMask(GL_TRUE);

		for (int p = first; p < end; p++)
		{
			const PolyParam* params = &pvrrc.global_param_tr[pvrrc.sortedTriangles[p].polyIndex];
			if (!params->isp.ZWriteDis)
			{
				// FIXME no clipping in modvol shader
				//SetTileClip(gp->tileclip,true);

				SetCull(params->isp.CullMode ^ gcflip);

				glDrawElements(GL_TRIANGLES, pvrrc.sortedTriangles[p].count, gl.index_type,
						(GLvoid*)(gl.get_index_size() * pvrrc.sortedTriangles[p].first));
			}
		}
		glcache.StencilMask(0xFF);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
}

//All pixels are in area 0 by default.
//If inside an 'in' volume, they are in area 1
//if inside an 'out' volume, they are in area 0
/*
	Stencil bits:
		bit 7: mv affected (must be preserved)
		bit 1: current volume state
		but 0: summary result (starts off as 0)

	Lower 2 bits:

	IN volume (logical OR):
	00 -> 00
	01 -> 01
	10 -> 01
	11 -> 01

	Out volume (logical AND):
	00 -> 00
	01 -> 00
	10 -> 00
	11 -> 01
*/
void SetMVS_Mode(ModifierVolumeMode mv_mode, ISP_Modvol ispc)
{
	if (mv_mode == Xor)
	{
		// set states
		glcache.Enable(GL_DEPTH_TEST);
		// write only bit 1
		glcache.StencilMask(2);
		// no stencil testing
		glcache.StencilFunc(GL_ALWAYS, 0, 2);
		// count the number of pixels in front of the Z buffer (xor zpass)
		glcache.StencilOp(GL_KEEP, GL_KEEP, GL_INVERT);

		// Cull mode needs to be set
		SetCull(ispc.CullMode);
	}
	else if (mv_mode == Or)
	{
		// set states
		glcache.Enable(GL_DEPTH_TEST);
		// write only bit 1
		glcache.StencilMask(2);
		// no stencil testing
		glcache.StencilFunc(GL_ALWAYS, 2, 2);
		// Or'ing of all triangles
		glcache.StencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

		// Cull mode needs to be set
		SetCull(ispc.CullMode);
	}
	else
	{
		// Inclusion or Exclusion volume

		// no depth test
		glcache.Disable(GL_DEPTH_TEST);
		// write bits 1:0
		glcache.StencilMask(3);

		if (mv_mode == Inclusion)
		{
			// Inclusion volume
			//res : old : final
			//0   : 0      : 00
			//0   : 1      : 01
			//1   : 0      : 01
			//1   : 1      : 01

			// if (1<=st) st=1; else st=0;
			glcache.StencilFunc(GL_LEQUAL,1,3);
			glcache.StencilOp(GL_ZERO, GL_ZERO, GL_REPLACE);
		}
		else
		{
			// Exclusion volume
			/*
				I've only seen a single game use it, so i guess it doesn't matter ? (Zombie revenge)
				(actually, i think there was also another, racing game)
			*/
			// The initial value for exclusion volumes is 1 so we need to invert the result before and'ing.
			//res : old : final
			//0   : 0   : 00
			//0   : 1   : 01
			//1   : 0   : 00
			//1   : 1   : 00

			// if (1 == st) st = 1; else st = 0;
			glcache.StencilFunc(GL_EQUAL, 1, 3);
			glcache.StencilOp(GL_ZERO, GL_ZERO, GL_KEEP);
		}
	}
}

void MainVertexArray::defineVtxAttribs()
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

	glEnableVertexAttribArray(VERTEX_NORM_ARRAY);
	glVertexAttribPointer(VERTEX_NORM_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, nx));
}

void SetupMainVBO()
{
	gl.vbo.mainVAO.bind(gl.vbo.geometry.get(), gl.vbo.idxs.get());
	glCheck();
}

void ModvolVertexArray::defineVtxAttribs()
{
	glEnableVertexAttribArray(VERTEX_POS_ARRAY);
	glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(float)*3, (void*)0);

	glDisableVertexAttribArray(VERTEX_UV_ARRAY);
	glDisableVertexAttribArray(VERTEX_COL_OFFS_ARRAY);
	glDisableVertexAttribArray(VERTEX_COL_BASE_ARRAY);
}

static void SetupModvolVBO()
{
	gl.vbo.modvolVAO.bind(gl.vbo.modvols.get());
}

void DrawModVols(int first, int count)
{
	if (count == 0 || pvrrc.modtrig.empty())
		return;

	SetupModvolVBO();

	glcache.Disable(GL_BLEND);
	SetBaseClipping();

	glcache.Enable(GL_DEPTH_TEST);
	glcache.DepthMask(GL_FALSE);
	glcache.DepthFunc(GL_GREATER);

	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	ModifierVolumeParam* params = &pvrrc.global_param_mvo[first];

	int mod_base = -1;
	int curMVMat = -1;
	int curProjMat = -1;

	for (int cmv = 0; cmv < count; cmv++)
	{
		ModifierVolumeParam& param = params[cmv];

		if (param.count == 0)
			continue;
		if (param.isNaomi2())
		{
			glcache.UseProgram(gl.n2ModVolShader.program);
			if (param.mvMatrix != curMVMat)
			{
				curMVMat = param.mvMatrix;
				glUniformMatrix4fv(gl.n2ModVolShader.mvMat, 1, GL_FALSE, pvrrc.matrices[curMVMat].mat);
			}
			if (param.projMatrix != curProjMat)
			{
				curProjMat = param.projMatrix;
				glUniformMatrix4fv(gl.n2ModVolShader.projMat, 1, GL_FALSE, pvrrc.matrices[curProjMat].mat);
			}
		}
		else
		{
			glcache.UseProgram(gl.modvol_shader.program);
		}

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
	//disable culling
	SetCull(0);
	//enable color writes
	glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);

	//black out any stencil with '1'
	glcache.Enable(GL_BLEND);
	glcache.BlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	glcache.Enable(GL_STENCIL_TEST);
	glcache.StencilFunc(GL_EQUAL,0x81,0x81); //only pixels that are Modvol enabled, and in area 1

	//clear the stencil result bit
	glcache.StencilMask(0x3);    //write to lsb
	glcache.StencilOp(GL_ZERO,GL_ZERO,GL_ZERO);

	//don't do depth testing
	glcache.Disable(GL_DEPTH_TEST);

	SetupMainVBO();
	glDrawArrays(GL_TRIANGLE_STRIP,0,4);

	//restore states
	glcache.Enable(GL_DEPTH_TEST);
}

void DrawStrips()
{
	SetupMainVBO();
	//Draw the strips !

	//We use sampler 0
	glActiveTexture(GL_TEXTURE0);

	RenderPass previous_pass = {};
    for (int render_pass = 0; render_pass < (int)pvrrc.render_passes.size(); render_pass++)
    {
        const RenderPass& current_pass = pvrrc.render_passes[render_pass];

        DEBUG_LOG(RENDERER, "Render pass %d OP %d PT %d TR %d MV %d", render_pass + 1,
        		current_pass.op_count - previous_pass.op_count,
				current_pass.pt_count - previous_pass.pt_count,
				current_pass.tr_count - previous_pass.tr_count,
				current_pass.mvo_count - previous_pass.mvo_count);

		//initial state
		glcache.Enable(GL_DEPTH_TEST);
		glcache.DepthMask(GL_TRUE);

		//Opaque
		DrawList<ListType_Opaque,false>(pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count - previous_pass.op_count);

		//Alpha tested
		DrawList<ListType_Punch_Through,false>(pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count - previous_pass.pt_count);

		// Modifier volumes
		if (config::ModifierVolumes)
			DrawModVols(previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count);

		//Alpha blended
		{
			if (current_pass.autosort)
            {
				if (!config::PerStripSorting)
					drawSorted(previous_pass.sorted_tr_count, current_pass.sorted_tr_count - previous_pass.sorted_tr_count, render_pass < (int)pvrrc.render_passes.size() - 1);
				else
					DrawList<ListType_Translucent,true>(pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count);
            }
			else
				DrawList<ListType_Translucent,false>(pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count);
		}
		previous_pass = current_pass;
	}
}

void OpenGLRenderer::RenderFramebuffer(const FramebufferInfo& info)
{
	glReadFramebuffer(info);
	saveCurrentFramebuffer();
	getVideoShift(gl.ofbo.shiftX, gl.ofbo.shiftY);
#ifdef LIBRETRO
	if (config::PowerVR2Filter || gl.ofbo.shiftX != 0 || gl.ofbo.shiftY != 0)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, postProcessor.getFramebuffer(gl.dcfb.width, gl.dcfb.height));
		glcache.BindTexture(GL_TEXTURE_2D, gl.dcfb.tex);
	}
#else
	if (gl.ofbo2.framebuffer != nullptr
			&& (gl.dcfb.width != gl.ofbo2.framebuffer->getWidth() || gl.dcfb.height != gl.ofbo2.framebuffer->getHeight()))
		gl.ofbo2.framebuffer.reset();

	if (gl.ofbo2.framebuffer == nullptr)
		gl.ofbo2.framebuffer = std::make_unique<GlFramebuffer>(gl.dcfb.width, gl.dcfb.height, false, true);
	else
		gl.ofbo2.framebuffer->bind();
	glCheck();
	gl.ofbo2.ready = true;
#endif
	this->width = gl.dcfb.width;
	this->height = gl.dcfb.height;
	gl.ofbo.aspectRatio = getDCFramebufferAspectRatio();

	glViewport(0, 0, gl.dcfb.width, gl.dcfb.height);
	glcache.Disable(GL_SCISSOR_TEST);

	if (info.fb_r_ctrl.fb_enable == 0 || info.vo_control.blank_video == 1)
	{
		// Video output disabled
		glcache.ClearColor(info.vo_border_col.red(), info.vo_border_col.green(), info.vo_border_col.blue(), 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	else
	{
		glcache.Disable(GL_BLEND);
		drawQuad(gl.dcfb.tex, false, true);
	}
#ifdef LIBRETRO
	if (config::PowerVR2Filter || gl.ofbo.shiftX != 0 || gl.ofbo.shiftY != 0)
		postProcessor.render(glsm_get_current_framebuffer());
#else
	renderLastFrame();
#endif

	DrawOSD(false);
	frameRendered = true;
	restoreCurrentFramebuffer();
}

void writeFramebufferToVRAM()
{
	u32 width = (pvrrc.ta_GLOB_TILE_CLIP.tile_x_num + 1) * 32;
	u32 height = (pvrrc.ta_GLOB_TILE_CLIP.tile_y_num + 1) * 32;

	float xscale = pvrrc.scaler_ctl.hscale == 1 ? 0.5f : 1.f;
	float yscale = 1024.f / pvrrc.scaler_ctl.vscalefactor;
	if (std::abs(yscale - 1.f) < 0.01)
		yscale = 1.f;
	FB_X_CLIP_type xClip = pvrrc.fb_X_CLIP;
	FB_Y_CLIP_type yClip = pvrrc.fb_Y_CLIP;

	if (xscale != 1.f || yscale != 1.f)
	{
		u32 scaledW = width * xscale;
		u32 scaledH = height * yscale;

		if (gl.fbscaling.framebuffer != nullptr
				&& (gl.fbscaling.framebuffer->getWidth() != (int)scaledW || gl.fbscaling.framebuffer->getHeight() != (int)scaledH))
			gl.fbscaling.framebuffer.reset();
		if (gl.fbscaling.framebuffer == nullptr)
			gl.fbscaling.framebuffer = std::make_unique<GlFramebuffer>(scaledW, scaledH);

		if (gl.gl_major < 3)
		{
			gl.fbscaling.framebuffer->bind();
			glViewport(0, 0, scaledW, scaledH);
			glcache.Disable(GL_SCISSOR_TEST);
			glcache.ClearColor(1.f, 0.f, 0.f, 1.f);
			glClear(GL_COLOR_BUFFER_BIT);
			glcache.BindTexture(GL_TEXTURE_2D, gl.ofbo.framebuffer->getTexture());
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glcache.Disable(GL_BLEND);
			drawQuad(gl.ofbo.framebuffer->getTexture(), false);
		}
		else
		{
#ifndef GLES2
			gl.ofbo.framebuffer->bind(GL_READ_FRAMEBUFFER);
			gl.fbscaling.framebuffer->bind(GL_DRAW_FRAMEBUFFER);
			glcache.Disable(GL_SCISSOR_TEST);
			glBlitFramebuffer(0, 0, width, height,
					0, 0, scaledW, scaledH,
					GL_COLOR_BUFFER_BIT, GL_LINEAR);
			gl.fbscaling.framebuffer->bind();
#endif
		}

		width = scaledW;
		height = scaledH;
		// FB_Y_CLIP is applied before vscalefactor if > 1, so it must be scaled here
		if (yscale > 1) {
			yClip.min = std::round(yClip.min * yscale);
			yClip.max = std::round(yClip.max * yscale);
		}
	}
	u32 tex_addr = pvrrc.fb_W_SOF1 & VRAM_MASK; // TODO SCALER_CTL.interlace, SCALER_CTL.fieldselect

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	u32 linestride = pvrrc.fb_W_LINESTRIDE * 8;

	PixelBuffer<u32> tmp_buf;
	tmp_buf.init(width, height);

	u8 *p = (u8 *)tmp_buf.data();
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, p);

	xClip.min = std::min(xClip.min, width - 1);
	xClip.max = std::min(xClip.max, width - 1);
	yClip.min = std::min(yClip.min, height - 1);
	yClip.max = std::min(yClip.max, height - 1);
	WriteFramebuffer(width, height, p, tex_addr, pvrrc.fb_W_CTRL, linestride, xClip, yClip);

	glBindFramebuffer(GL_FRAMEBUFFER, gl.ofbo.origFbo);
	glCheck();
}

bool OpenGLRenderer::renderLastFrame()
{
	GlFramebuffer *framebuffer = gl.ofbo2.ready ? gl.ofbo2.framebuffer.get() : gl.ofbo.framebuffer.get();
	if (framebuffer == nullptr)
		return false;

	glcache.Disable(GL_SCISSOR_TEST);
	float screenAR = (float)settings.display.width / settings.display.height;
	float renderAR = gl.ofbo.aspectRatio;

	int dx = 0;
	int dy = 0;
	if (renderAR > screenAR)
		dy = (int)roundf(settings.display.height * (1 - screenAR / renderAR) / 2.f);
	else
		dx = (int)roundf(settings.display.width * (1 - renderAR / screenAR) / 2.f);

	if (gl.gl_major < 3 || config::Rotate90)
	{
		glViewport(dx, dy, settings.display.width - dx * 2, settings.display.height - dy * 2);
		glBindFramebuffer(GL_FRAMEBUFFER, gl.ofbo.origFbo);
		glcache.ClearColor(VO_BORDER_COL.red(), VO_BORDER_COL.green(), VO_BORDER_COL.blue(), 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, config::TextureFiltering == 1 ? GL_NEAREST : GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, config::TextureFiltering == 1 ? GL_NEAREST : GL_LINEAR);
		float *vertices = nullptr;
		if (gl.ofbo.shiftX != 0 || gl.ofbo.shiftY != 0)
		{
			static float sverts[20] = {
				-1.f,  1.f, 1.f, 0.f, 1.f,
				-1.f, -1.f, 1.f, 0.f, 0.f,
				 1.f,  1.f, 1.f, 1.f, 1.f,
				 1.f, -1.f, 1.f, 1.f, 0.f,
			};
			sverts[0] = sverts[5] = -1.f + gl.ofbo.shiftX * 2.f / framebuffer->getWidth();
			sverts[10] = sverts[15] = sverts[0] + 2;
			sverts[1] = sverts[11] = 1.f - gl.ofbo.shiftY * 2.f / framebuffer->getHeight();
			sverts[6] = sverts[16] = sverts[1] - 2;
			vertices = sverts;
		}
		glcache.Disable(GL_BLEND);
		drawQuad(framebuffer->getTexture(), config::Rotate90, false, vertices);
	}
	else
	{
#ifndef GLES2
		framebuffer->bind(GL_READ_FRAMEBUFFER);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gl.ofbo.origFbo);
		glcache.ClearColor(VO_BORDER_COL.red(), VO_BORDER_COL.green(), VO_BORDER_COL.blue(), 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
		glBlitFramebuffer(-gl.ofbo.shiftX, gl.ofbo.shiftY, framebuffer->getWidth() - gl.ofbo.shiftX, framebuffer->getHeight() + gl.ofbo.shiftY,
				dx, dy, settings.display.width - dx, settings.display.height - dy,
				GL_COLOR_BUFFER_BIT, config::TextureFiltering == 1 ? GL_NEAREST : GL_LINEAR);
    	glBindFramebuffer(GL_FRAMEBUFFER, gl.ofbo.origFbo);
#endif
	}
	return true;
}

#ifdef LIBRETRO
#include "vmu_xhair.h"

static GLuint vmuTextureId[4] {};
static GLuint lightgunTextureId[4] {};

static void updateVmuTexture(int vmu_screen_number)
{
	if (vmuTextureId[vmu_screen_number] == 0)
	{
		vmuTextureId[vmu_screen_number] = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, vmuTextureId[vmu_screen_number]);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else
		glcache.BindTexture(GL_TEXTURE_2D, vmuTextureId[vmu_screen_number]);


	const u32 *data = vmu_lcd_data[vmu_screen_number * 2];
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VMU_SCREEN_WIDTH, VMU_SCREEN_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	vmu_lcd_changed[vmu_screen_number * 2] = false;
}

void DrawVmuTexture(u8 vmu_screen_number, int width, int height)
{
	constexpr float vmu_padding = 8.f;
	float x = vmu_padding;
	float y = vmu_padding;
	float w = (float)VMU_SCREEN_WIDTH * vmu_screen_params[vmu_screen_number].vmu_screen_size_mult * 4.f / 3.f / gl.ofbo.aspectRatio;
	float h = (float)VMU_SCREEN_HEIGHT * vmu_screen_params[vmu_screen_number].vmu_screen_size_mult;

	if (vmu_lcd_changed[vmu_screen_number * 2] || vmuTextureId[vmu_screen_number] == 0)
		updateVmuTexture(vmu_screen_number);

	switch (vmu_screen_params[vmu_screen_number].vmu_screen_position)
	{
		case UPPER_LEFT:
			break;
		case UPPER_RIGHT:
			x = width - x - w;
			break;
		case LOWER_LEFT:
			y = height - y - h;
			break;
		case LOWER_RIGHT:
			x = width - x - w;
			y = height - y - h;
			break;
	}
	float x1 = (x + w) * 2 / width - 1;
	float y1 = -(y + h) * 2 / height + 1;
	x = x * 2 / width - 1;
	y = -y * 2 / height + 1;
	float vertices[20] = {
		x,  y1, 1.f, 0.f, 0.f,
		x,  y,  1.f, 0.f, 1.f,
		x1, y1, 1.f, 1.f, 0.f,
		x1, y,  1.f, 1.f, 1.f,
	};
	glcache.Enable(GL_BLEND);
	glcache.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	drawQuad(vmuTextureId[vmu_screen_number], false, false, vertices);
}

static void updateLightGunTexture(int port)
{
	s32 x,y ;
	u8 temp_tex_buffer[LIGHTGUN_CROSSHAIR_SIZE*LIGHTGUN_CROSSHAIR_SIZE*4];
	u8 *dst = temp_tex_buffer;
	u8 *src = NULL ;

	if (lightgunTextureId[port] == 0)
	{
		lightgunTextureId[port] = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, lightgunTextureId[port]);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else
		glcache.BindTexture(GL_TEXTURE_2D, lightgunTextureId[port]);

	u8* colour = &( lightgun_palette[ lightgun_params[port].colour * 3 ] );

	for ( y = LIGHTGUN_CROSSHAIR_SIZE-1 ; y >= 0 ; y--)
	{
		src = lightgun_img_crosshair + (y*LIGHTGUN_CROSSHAIR_SIZE) ;

		for ( x = 0 ; x < LIGHTGUN_CROSSHAIR_SIZE ; x++)
		{
			if ( src[x] )
			{
				*dst++ = colour[0] ;
				*dst++ = colour[1] ;
				*dst++ = colour[2] ;
				*dst++ = 0xFF ;
			}
			else
			{
				*dst++ = 0 ;
				*dst++ = 0 ;
				*dst++ = 0 ;
				*dst++ = 0 ;
			}
		}
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LIGHTGUN_CROSSHAIR_SIZE, LIGHTGUN_CROSSHAIR_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, temp_tex_buffer);

	lightgun_params[port].dirty = false;
}

void DrawGunCrosshair(u8 port, int width, int height)
{
	if (lightgun_params[port].offscreen || lightgun_params[port].colour == 0)
		return;

	float w = (float)LIGHTGUN_CROSSHAIR_SIZE * 4.f / 3.f / gl.ofbo.aspectRatio;
	float h = (float)LIGHTGUN_CROSSHAIR_SIZE;
	auto [x, y] = getCrosshairPosition(port);
	x -= w / 2;
	y -= h / 2;

	if (lightgun_params[port].dirty || lightgunTextureId[port] == 0)
		updateLightGunTexture(port);

	float x1 = (x + w) * 2 / width - 1;
	float y1 = -(y + h) * 2 / height + 1;
	x = x * 2 / width - 1;
	y = -y * 2 / height + 1;
	float vertices[20] = {
		x,  y1, 1.f, 0.f, 0.f,
		x,  y,  1.f, 0.f, 1.f,
		x1, y1, 1.f, 1.f, 0.f,
		x1, y,  1.f, 1.f, 1.f,
	};
	glcache.Enable(GL_BLEND);
	glcache.BlendFunc(GL_SRC_ALPHA, GL_ONE);
	drawQuad(lightgunTextureId[port], false, false, vertices);

	glcache.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void termVmuLightgun()
{
	glcache.DeleteTextures(std::size(vmuTextureId), vmuTextureId);
	memset(vmuTextureId, 0, sizeof(vmuTextureId));
	glcache.DeleteTextures(std::size(lightgunTextureId), lightgunTextureId);
	memset(lightgunTextureId, 0, sizeof(lightgunTextureId));
}
#endif
