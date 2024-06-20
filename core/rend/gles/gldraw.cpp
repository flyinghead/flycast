#include "glcache.h"
#include "gles.h"
#include "rend/tileclip.h"
#include "rend/osd.h"
#include "naomi2.h"
#include "rend/transform_matrix.h"
#ifdef LIBRETRO
#include "postprocess.h"
#include "vmu_xhair.h"
#endif

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
	float trilinear_alpha;
	if (gp->pcw.Texture && gp->tsp.FilterMode > 1 && Type != ListType_Punch_Through && gp->tcw.MipMapped == 1)
	{
		trilinear_alpha = 0.25f * (gp->tsp.MipMapD & 0x3);
		if (gp->tsp.FilterMode == 2)
			// Trilinear pass A
			trilinear_alpha = 1.f - trilinear_alpha;
	}
	else
		trilinear_alpha = 1.f;

	bool color_clamp = gp->tsp.ColorClamp && (pvrrc.fog_clamp_min.full != 0 || pvrrc.fog_clamp_max.full != 0xffffffff);
	int fog_ctrl = config::Fog ? gp->tsp.FogCtrl : 2;

	int clip_rect[4] = {};
	TileClipping clipmode = GetTileClip(gp->tileclip, ViewportMatrix, clip_rect);
	TextureCacheData *texture = (TextureCacheData *)gp->texture;
	int gpuPalette = texture == nullptr || !texture->gpuPalette ? 0
			: gp->tsp.FilterMode + 1;
	if (gpuPalette != 0)
	{
		if (config::TextureFiltering == 1)
			gpuPalette = 1; // force nearest
		else if (config::TextureFiltering == 2)
			gpuPalette = 2; // force linear
	}

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
								  trilinear_alpha != 1.f,
								  gpuPalette,
								  gp->isNaomi2(),
								  ShaderUniforms.dithering);
	
	glcache.UseProgram(CurrentShader->program);
	if (CurrentShader->trilinear_alpha != -1)
		glUniform1f(CurrentShader->trilinear_alpha, trilinear_alpha);
	if (gpuPalette != 0)
	{
		int paletteIndex;
		if (gp->tcw.PixelFmt == PixelPal4)
			paletteIndex = gp->tcw.PalSelect << 4;
		else
			paletteIndex = (gp->tcw.PalSelect >> 4) << 8;
		glUniform1i(CurrentShader->palette_index, paletteIndex);
		if (gpuPalette == 2 && CurrentShader->texSize != -1)
		{
			float texSize[] { (float)texture->width, (float)texture->height };
			glUniform2fv(CurrentShader->texSize, 1, texSize);
		}
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
		if (gpuPalette != 0)
			nearest_filter = true;
		else if (config::TextureFiltering == 0)
			nearest_filter = gp->tsp.FilterMode == 0;
		else if (config::TextureFiltering == 1)
			nearest_filter = true;
		else
			nearest_filter = false;

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
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmapped  && Type != ListType_Punch_Through ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
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
				if (mipmapped && Type != ListType_Punch_Through)
					glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			}
			else
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 1.f);
		}
	}

	// Apparently punch-through polys support blending, or at least some combinations
	// Opaque polygons support blending in list continuations (wild guess)
	glcache.Enable(GL_BLEND);
	glcache.BlendFunc(SrcBlendGL[gp->tsp.SrcInstr],DstBlendGL[gp->tsp.DstInstr]);

	//set cull mode !
	//cflip is required when exploding triangles for triangle sorting
	SetCull(gp->isp.CullMode ^ cflip ^ 1);

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

				SetCull(params->isp.CullMode ^ 1);

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
	initVideoRoutingFrameBuffer();
	glReadFramebuffer(info);
	saveCurrentFramebuffer();
	getVideoShift(gl.ofbo.shiftX, gl.ofbo.shiftY);
#ifdef LIBRETRO
	glBindFramebuffer(GL_FRAMEBUFFER, postProcessor.getFramebuffer(gl.dcfb.width, gl.dcfb.height));
	glcache.BindTexture(GL_TEXTURE_2D, gl.dcfb.tex);
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
		drawQuad(gl.dcfb.tex, false, false);
	}
#ifdef LIBRETRO
	postProcessor.render(glsm_get_current_framebuffer());
#else
	renderLastFrame();
#endif

	DrawOSD(false);
	frameRendered = true;
	renderVideoRouting();
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
			glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
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
				-1.f, -1.f, 1.f, 0.f, 1.f,
				-1.f,  1.f, 1.f, 0.f, 0.f,
				 1.f, -1.f, 1.f, 1.f, 1.f,
				 1.f,  1.f, 1.f, 1.f, 0.f,
			};
			sverts[0] = sverts[5] = -1.f + gl.ofbo.shiftX * 2.f / framebuffer->getWidth();
			sverts[10] = sverts[15] = sverts[0] + 2;
			sverts[1] = sverts[11] = -1.f - gl.ofbo.shiftY * 2.f / framebuffer->getHeight();
			sverts[6] = sverts[16] = sverts[1] + 2;
			vertices = sverts;
		}
		glcache.Disable(GL_BLEND);
		drawQuad(framebuffer->getTexture(), config::Rotate90, true, vertices);
	}
	else
	{
#ifndef GLES2
		framebuffer->bind(GL_READ_FRAMEBUFFER);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gl.ofbo.origFbo);
		glcache.ClearColor(VO_BORDER_COL.red(), VO_BORDER_COL.green(), VO_BORDER_COL.blue(), 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
		glBlitFramebuffer(-gl.ofbo.shiftX, -gl.ofbo.shiftY, framebuffer->getWidth() - gl.ofbo.shiftX, framebuffer->getHeight() - gl.ofbo.shiftY,
				dx, settings.display.height - dy, settings.display.width - dx, dy,
				GL_COLOR_BUFFER_BIT, config::TextureFiltering == 1 ? GL_NEAREST : GL_LINEAR);
    	glBindFramebuffer(GL_FRAMEBUFFER, gl.ofbo.origFbo);
#endif
	}
	return true;
}

bool OpenGLRenderer::GetLastFrame(std::vector<u8>& data, int& width, int& height)
{
	GlFramebuffer *framebuffer = gl.ofbo2.ready ? gl.ofbo2.framebuffer.get() : gl.ofbo.framebuffer.get();
	if (framebuffer == nullptr)
		return false;
	if (width != 0) {
		height = width / gl.ofbo.aspectRatio;
	}
	else if (height != 0) {
		width = gl.ofbo.aspectRatio * height;
	}
	else
	{
		width = framebuffer->getWidth();
		height = framebuffer->getHeight();
		if (config::Rotate90)
			std::swap(width, height);
		// We need square pixels for PNG
		int w = gl.ofbo.aspectRatio * height;
		if (width > w)
			height = width / gl.ofbo.aspectRatio;
		else
			width = w;
	}

	GlFramebuffer dstFramebuffer(width, height, false, false);

	glViewport(0, 0, width, height);
	glcache.Disable(GL_BLEND);
	verify(framebuffer->getTexture() != 0);
	const float *vertices = nullptr;
	if (config::Rotate90)
	{
		static float rvertices[4][5] = {
			{ -1.f,  1.f, 1.f, 1.f, 0.f },
			{ -1.f, -1.f, 1.f, 1.f, 1.f },
			{  1.f,  1.f, 1.f, 0.f, 0.f },
			{  1.f, -1.f, 1.f, 0.f, 1.f },
		};
		vertices = &rvertices[0][0];
	}
	drawQuad(framebuffer->getTexture(), config::Rotate90, false, vertices);

	data.resize(width * height * 3);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	if (gl.is_gles)
	{
		// GL_RGB not supported
		std::vector<u8> tmp(width * height * 4);
		glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, tmp.data());
		u8 *dst = data.data();
		const u8 *src = tmp.data();
		while (src <= &tmp.back())
		{
			*dst++ = *src++;
			*dst++ = *src++;
			*dst++ = *src++;
			src++;
		}
	}
	else {
		glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data.data());
	}
	restoreCurrentFramebuffer();
	glCheck();

	return true;
}

static GLuint vmuTextureId[8] {};
static GLuint lightgunTextureId {};
static u64 vmuLastUpdated[8] {};

static void updateVmuTexture(int vmuIndex)
{
	if (vmuTextureId[vmuIndex] == 0)
	{
		vmuTextureId[vmuIndex] = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, vmuTextureId[vmuIndex]);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else
		glcache.BindTexture(GL_TEXTURE_2D, vmuTextureId[vmuIndex]);

	const u32 *data = vmu_lcd_data[vmuIndex];
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 48, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	vmuLastUpdated[vmuIndex] = vmuLastChanged[vmuIndex];
}

static void drawVmuTexture(u8 vmuIndex, int width, int height)
{
	const float *color = nullptr;
#ifndef LIBRETRO
	const float vmu_padding = 8.f * settings.display.uiScale;
	const float w = 96.f * settings.display.uiScale;
	const float h = 64.f * settings.display.uiScale;

	float x;
	float y;
	if (vmuIndex & 2)
		x = width - vmu_padding - w;
	else
		x = vmu_padding;
	if (vmuIndex & 4)
	{
		y = height - vmu_padding - h;
		if (vmuIndex & 1)
			y -= vmu_padding + h;
	}
	else
	{
		y = vmu_padding;
		if (vmuIndex & 1)
			y += vmu_padding + h;
	}
	const float blend_factor[4] = { 0.75f, 0.75f, 0.75f, 0.75f };
	color = blend_factor;
#else
	if (vmuIndex & 1)
		return;
	const float vmu_padding_x = 8.f * width / 640.f * 4.f / 3.f / gl.ofbo.aspectRatio;
	const float vmu_padding_y = 8.f * height / 480.f;
	const float w = (float)VMU_SCREEN_WIDTH * width / 640.f * 4.f / 3.f / gl.ofbo.aspectRatio
			* vmu_screen_params[vmuIndex / 2].vmu_screen_size_mult;
	const float h = (float)VMU_SCREEN_HEIGHT * height / 480.f
			* vmu_screen_params[vmuIndex / 2].vmu_screen_size_mult;

	float x;
	float y;

	switch (vmu_screen_params[vmuIndex / 2].vmu_screen_position)
	{
	case UPPER_LEFT:
	default:
		x = vmu_padding_x;
		y = vmu_padding_y;
		break;
	case UPPER_RIGHT:
		x = width - vmu_padding_x - w;
		y = vmu_padding_y;
		break;
	case LOWER_LEFT:
		x = vmu_padding_x;
		y = height - vmu_padding_y - h;
		break;
	case LOWER_RIGHT:
		x = width - vmu_padding_x - w;
		y = height - vmu_padding_y - h;
		break;
	}
#endif

	if (vmuLastChanged[vmuIndex] != vmuLastUpdated[vmuIndex]  || vmuTextureId[vmuIndex] == 0)
		updateVmuTexture(vmuIndex);

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
	drawQuad(vmuTextureId[vmuIndex], false, false, vertices, color);
}

static void updateLightGunTexture()
{
	if (lightgunTextureId == 0)
	{
		lightgunTextureId = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, lightgunTextureId);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, getCrosshairTextureData());
	}
}

static void drawGunCrosshair(u8 port, int width, int height)
{
	if (config::CrosshairColor[port] == 0)
		return;
	if (settings.platform.isConsole()
			&& config::MapleMainDevices[port] != MDT_LightGun)
		return;

	auto [x, y] = getCrosshairPosition(port);
#ifdef LIBRETRO
	float halfWidth = lightgun_crosshair_size / 2.f / config::ScreenStretching * 100.f * config::RenderResolution / 480.f;
	float halfHeight = lightgun_crosshair_size / 2.f * config::RenderResolution / 480.f;
	x /= config::ScreenStretching / 100.f;
#else
	float halfWidth = config::CrosshairSize * settings.display.uiScale / 2.f;
	float halfHeight = halfWidth;
#endif

	updateLightGunTexture();

	float x1 = (x + halfWidth) * 2 / width - 1;
	float y1 = -(y + halfHeight) * 2 / height + 1;
	x = (x - halfWidth) * 2 / width - 1;
	y = -(y - halfHeight) * 2 / height + 1;
	float vertices[20] = {
		x,  y1, 1.f, 0.f, 0.f,
		x,  y,  1.f, 0.f, 1.f,
		x1, y1, 1.f, 1.f, 0.f,
		x1, y,  1.f, 1.f, 1.f,
	};
	const float color[4] = {
			(config::CrosshairColor[port] & 0xff) / 255.f,
			((config::CrosshairColor[port] >> 8) & 0xff) / 255.f,
			((config::CrosshairColor[port] >> 16) & 0xff) / 255.f,
			((config::CrosshairColor[port] >> 24) & 0xff) / 255.f
	};
	glcache.Enable(GL_BLEND);
	glcache.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	drawQuad(lightgunTextureId, false, false, vertices, color);
}

void drawVmusAndCrosshairs(int width, int height)
{
#ifndef LIBRETRO
	width = settings.display.width;
	height = settings.display.height;
	glViewport(0, 0, width, height);
	glBindFramebuffer(GL_FRAMEBUFFER, gl.ofbo.origFbo);
	const bool showVmus = config::FloatVMUs;
#else
	const bool showVmus = true;
#endif

	if (settings.platform.isConsole() && showVmus)
	{
		for (int i = 0; i < 8 ; i++)
			if (vmu_lcd_status[i])
				drawVmuTexture(i, width, height);
	}

	if (crosshairsNeeded()) {
		for (int i = 0 ; i < 4 ; i++)
			drawGunCrosshair(i, width, height);
	}
	glCheck();
}

void termVmuLightgun()
{
	glcache.DeleteTextures(std::size(vmuTextureId), vmuTextureId);
	memset(vmuTextureId, 0, sizeof(vmuTextureId));
	glcache.DeleteTextures(1, &lightgunTextureId);
	lightgunTextureId = 0;
}
