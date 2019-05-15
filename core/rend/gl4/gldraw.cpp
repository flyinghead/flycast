#include <math.h>
#include "gl4.h"
#include "rend/gles/glcache.h"
#include "rend/rend.h"

/*

Drawing and related state management
Takes vertex, textures and renders to the currently set up target




*/

#define INVERT_DEPTH_FUNC
const static u32 Zfunction[]=
{
	GL_NEVER,      //GL_NEVER,              //0 Never
#ifndef INVERT_DEPTH_FUNC
	GL_LESS,        //GL_LESS/*EQUAL*/,     //1 Less
	GL_EQUAL,       //GL_EQUAL,             //2 Equal
	GL_LEQUAL,      //GL_LEQUAL,            //3 Less Or Equal
	GL_GREATER,     //GL_GREATER/*EQUAL*/,  //4 Greater
	GL_NOTEQUAL,    //GL_NOTEQUAL,          //5 Not Equal
	GL_GEQUAL,      //GL_GEQUAL,            //6 Greater Or Equal
#else
	GL_GREATER,                             //1 Less
	GL_EQUAL,                               //2 Equal
	GL_GEQUAL,                              //3 Less Or Equal
	GL_LESS,                                //4 Greater
	GL_NOTEQUAL,                            //5 Not Equal
	GL_LEQUAL,                              //6 Greater Or Equal
#endif
	GL_ALWAYS,      //GL_ALWAYS,            //7 Always
};

static gl4PipelineShader* CurrentShader;
extern u32 gcflip;
static GLuint geom_fbo;
GLuint stencilTexId;
GLuint opaqueTexId;
GLuint depthTexId;
static GLuint texSamplers[2];
static GLuint depth_fbo;
GLuint depthSaveTexId;

static gl4PipelineShader *gl4GetProgram(u32 cp_AlphaTest, u32 pp_ClipTestMode,
							u32 pp_Texture, u32 pp_UseAlpha, u32 pp_IgnoreTexA, u32 pp_ShadInstr, u32 pp_Offset,
							u32 pp_FogCtrl, bool pp_TwoVolumes, u32 pp_DepthFunc, bool pp_Gouraud, bool pp_BumpMap, bool fog_clamping, int pass)
{
	if (settings.rend.Rotate90 != gl4.rotate90)
	{
		gl4_delete_shaders();
		gl4.rotate90 = settings.rend.Rotate90;
	}
	u32 rv=0;

	rv|=pp_ClipTestMode;
	rv<<=1; rv|=cp_AlphaTest;
	rv<<=1; rv|=pp_Texture;
	rv<<=1; rv|=pp_UseAlpha;
	rv<<=1; rv|=pp_IgnoreTexA;
	rv<<=2; rv|=pp_ShadInstr;
	rv<<=1; rv|=pp_Offset;
	rv<<=2; rv|=pp_FogCtrl;
	rv <<= 1; rv |= (int)pp_TwoVolumes;
	rv <<= 3; rv |= pp_DepthFunc;
	rv <<= 1; rv |= (int)pp_Gouraud;
	rv <<= 1; rv |= pp_BumpMap;
	rv <<= 1; rv |= fog_clamping;
	rv <<= 2; rv |= pass;

	gl4PipelineShader *shader = &gl4.shaders[rv];
	if (shader->program == 0)
	{
		shader->cp_AlphaTest = cp_AlphaTest;
		shader->pp_ClipTestMode = pp_ClipTestMode;
		shader->pp_Texture = pp_Texture;
		shader->pp_UseAlpha = pp_UseAlpha;
		shader->pp_IgnoreTexA = pp_IgnoreTexA;
		shader->pp_ShadInstr = pp_ShadInstr;
		shader->pp_Offset = pp_Offset;
		shader->pp_FogCtrl = pp_FogCtrl;
		shader->pp_TwoVolumes = pp_TwoVolumes;
		shader->pp_DepthFunc = pp_DepthFunc;
		shader->pp_Gouraud = pp_Gouraud;
		shader->pp_BumpMap = pp_BumpMap;
		shader->fog_clamping = fog_clamping;
		shader->pass = pass;
		gl4CompilePipelineShader(shader, settings.rend.Rotate90);
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

template <u32 Type, bool SortingEnabled>
	static void SetGPState(const PolyParam* gp, int pass, u32 cflip=0)
{
	if (gp->pcw.Texture && gp->tsp.FilterMode > 1)
	{
		gl4ShaderUniforms.trilinear_alpha = 0.25 * (gp->tsp.MipMapD & 0x3);
		if (gp->tsp.FilterMode == 2)
			// Trilinear pass A
			gl4ShaderUniforms.trilinear_alpha = 1.0 - gl4ShaderUniforms.trilinear_alpha;
	}
	else
		gl4ShaderUniforms.trilinear_alpha = 1.0;

	s32 clipping = SetTileClip(gp->tileclip, -1);

	if (pass == 0)
	{
		CurrentShader = gl4GetProgram(Type == ListType_Punch_Through ? 1 : 0,
				clipping,
				Type == ListType_Punch_Through ? gp->pcw.Texture : 0,
				1,
				gp->tsp.IgnoreTexA,
				0,
				0,
				2,
				false,	// TODO Can PT have two different textures for area 0 and 1 ??
				0,
				false,
				false,
				false,
				pass);
	}
	else
	{
		// Two volumes mode only supported for OP and PT
		bool two_volumes_mode = (gp->tsp1.full != -1) && Type != ListType_Translucent;
		bool color_clamp = gp->tsp.ColorClamp && (pvrrc.fog_clamp_min != 0 || pvrrc.fog_clamp_max != 0xffffffff);

		int fog_ctrl = settings.rend.Fog ? gp->tsp.FogCtrl : 2;

		int depth_func = 0;
		if (Type == ListType_Translucent)
		{
			if (SortingEnabled)
				depth_func = 6;		// GEQUAL
			else
				depth_func = gp->isp.DepthMode;
		}

		CurrentShader = gl4GetProgram(Type == ListType_Punch_Through ? 1 : 0,
				clipping,
				gp->pcw.Texture,
				gp->tsp.UseAlpha,
				gp->tsp.IgnoreTexA,
				gp->tsp.ShadInstr,
				gp->pcw.Offset,
				fog_ctrl,
				two_volumes_mode,
				depth_func,
				gp->pcw.Gouraud,
				gp->tcw.PixelFmt == PixelBumpMap,
				color_clamp,
				pass);
	}

	glcache.UseProgram(CurrentShader->program);

	gl4ShaderUniforms.tsp0 = gp->tsp;
	gl4ShaderUniforms.tsp1 = gp->tsp1;
	gl4ShaderUniforms.tcw0 = gp->tcw;
	gl4ShaderUniforms.tcw1 = gp->tcw1;

	if (Type == ListType_Opaque || Type == ListType_Punch_Through)	// TODO Can PT have a >0 and <1 alpha?
	{
		gl4ShaderUniforms.tsp0.SrcInstr = 1;
		gl4ShaderUniforms.tsp0.DstInstr = 0;
		gl4ShaderUniforms.tsp1.SrcInstr = 1;
		gl4ShaderUniforms.tsp1.DstInstr = 0;
	}
	gl4ShaderUniforms.Set(CurrentShader);

	SetTileClip(gp->tileclip, CurrentShader->pp_ClipTest);

	//This bit control which pixels are affected
	//by modvols
	const u32 stencil=(gp->pcw.Shadow!=0)?0x80:0x0;

	glcache.StencilFunc(GL_ALWAYS,stencil,stencil);

	if (CurrentShader->pp_Texture)
	{
		for (int i = 0; i < 2; i++)
		{
			glActiveTexture(GL_TEXTURE0 + i);
			GLuint texid = i == 0 ? gp->texid : gp->texid1;

			glBindTexture(GL_TEXTURE_2D, texid == -1 ? 0 : texid);

			if (texid != -1)
			{
				TSP tsp = i == 0 ? gp->tsp : gp->tsp1;
				TCW tcw = i == 0 ? gp->tcw : gp->tcw1;

				glBindSampler(i, texSamplers[i]);
				SetTextureRepeatMode(i, GL_TEXTURE_WRAP_S, tsp.ClampU, tsp.FlipU);
				SetTextureRepeatMode(i, GL_TEXTURE_WRAP_T, tsp.ClampV, tsp.FlipV);

				//set texture filter mode
				if (tsp.FilterMode == 0)
				{
					//disable filtering, mipmaps
					glSamplerParameteri(texSamplers[i], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glSamplerParameteri(texSamplers[i], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				}
				else
				{
					//bilinear filtering
					//PowerVR supports also trilinear via two passes, but we ignore that for now
					glSamplerParameteri(texSamplers[i], GL_TEXTURE_MIN_FILTER, (tcw.MipMapped && settings.rend.UseMipmaps) ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
					glSamplerParameteri(texSamplers[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				}
			}
		}
		glActiveTexture(GL_TEXTURE0);
	}

	//set cull mode !
	//cflip is required when exploding triangles for triangle sorting
	//gcflip is global clip flip, needed for when rendering to texture due to mirrored Y direction
	SetCull(gp->isp.CullMode^cflip^gcflip);

	//set Z mode, only if required
	if (Type == ListType_Punch_Through || (Type == ListType_Translucent && SortingEnabled))
	{
		glcache.DepthFunc(Zfunction[6]);	// Greater or equal
	}
	else
	{
		glcache.DepthFunc(Zfunction[gp->isp.DepthMode]);
	}

	// Depth buffer is updated in pass 0 (and also in pass 1 for OP PT)
	if (pass < 2)
		glcache.DepthMask(!gp->isp.ZWriteDis);
	else
		glcache.DepthMask(GL_FALSE);
}

template <u32 Type, bool SortingEnabled>
static void DrawList(const List<PolyParam>& gply, int first, int count, int pass)
{
	PolyParam* params = &gply.head()[first];


	if (count==0)
		return;
	//we want at least 1 PParam

	while(count-->0)
	{
		if (params->count>2) //this actually happens for some games. No idea why ..
		{
			if (pass != 0)
			{
				// No need to draw this one
				if (Type == ListType_Translucent && params->tsp.SrcInstr == 0 && params->tsp.DstInstr == 1)
				{
					params++;
					continue;
				}
			}
			gl4ShaderUniforms.poly_number = params - gply.head();
			SetGPState<Type,SortingEnabled>(params, pass);
			glDrawElements(GL_TRIANGLE_STRIP, params->count, GL_UNSIGNED_INT, (GLvoid*)(sizeof(u32) * params->first)); glCheck();
		}

		params++;
	}
}

void gl4SetupMainVBO()
{
	glBindVertexArray(gl4.vbo.main_vao);

	glBindBuffer(GL_ARRAY_BUFFER, gl4.vbo.geometry); glCheck();
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl4.vbo.idxs); glCheck();

	//setup vertex buffers attrib pointers
	glEnableVertexAttribArray(VERTEX_POS_ARRAY); glCheck();
	glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,x)); glCheck();

	glEnableVertexAttribArray(VERTEX_COL_BASE_ARRAY); glCheck();
	glVertexAttribPointer(VERTEX_COL_BASE_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex,col)); glCheck();

	glEnableVertexAttribArray(VERTEX_COL_OFFS_ARRAY); glCheck();
	glVertexAttribPointer(VERTEX_COL_OFFS_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex,spc)); glCheck();

	glEnableVertexAttribArray(VERTEX_UV_ARRAY); glCheck();
	glVertexAttribPointer(VERTEX_UV_ARRAY, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,u)); glCheck();

	glEnableVertexAttribArray(VERTEX_COL_BASE1_ARRAY); glCheck();
	glVertexAttribPointer(VERTEX_COL_BASE1_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, col1)); glCheck();

	glEnableVertexAttribArray(VERTEX_COL_OFFS1_ARRAY); glCheck();
	glVertexAttribPointer(VERTEX_COL_OFFS1_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, spc1)); glCheck();

	glEnableVertexAttribArray(VERTEX_UV1_ARRAY); glCheck();
	glVertexAttribPointer(VERTEX_UV1_ARRAY, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u1)); glCheck();
}

void gl4SetupModvolVBO()
{
	glBindVertexArray(gl4.vbo.modvol_vao);

	glBindBuffer(GL_ARRAY_BUFFER, gl4.vbo.modvols); glCheck();

	//setup vertex buffers attrib pointers
	glEnableVertexAttribArray(VERTEX_POS_ARRAY); glCheck();
	glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(float)*3, (void*)0); glCheck();
}

static void DrawModVols(int first, int count)
{
	if (count == 0 || pvrrc.modtrig.used() == 0)
		return;

	glBindVertexArray(gl4.vbo.modvol_vao);

	glcache.UseProgram(gl4.modvol_shader.program);

	glcache.DepthMask(GL_FALSE);
	glcache.DepthFunc(Zfunction[4]);

	if(0)
	{
		//simply draw the volumes -- for debugging
		SetCull(0);
		glDrawArrays(GL_TRIANGLES, first, count * 3);
	}
	else
	{
		//Full emulation

		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

		ModifierVolumeParam* params = &pvrrc.global_param_mvo.head()[first];

		int mod_base = -1;

		for (u32 cmv = 0; cmv < count; cmv++)
		{
			ModifierVolumeParam& param = params[cmv];

			if (param.count == 0)
				continue;

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
	}

	//restore states
	glBindVertexArray(gl4.vbo.main_vao);
	glcache.Enable(GL_DEPTH_TEST);
	glcache.DepthMask(GL_TRUE);
}

void renderABuffer(bool sortFragments);
void DrawTranslucentModVols(int first, int count);
void checkOverflowAndReset();

static GLuint CreateColorFBOTexture(int width, int height)
{
	GLuint texId = glcache.GenTexture();
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
	glCheck();

	return texId;
}

static void CreateTextures(int width, int height)
{
	stencilTexId = glcache.GenTexture();
	glBindTexture(GL_TEXTURE_2D, stencilTexId); glCheck();
	glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);		// OpenGL >= 4.3
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// Using glTexStorage2D instead of glTexImage2D to satisfy requirement GL_TEXTURE_IMMUTABLE_FORMAT=true, needed for glTextureView below
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH32F_STENCIL8, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, stencilTexId, 0); glCheck();
	glCheck();

	opaqueTexId = CreateColorFBOTexture(width, height);

	depthTexId = glcache.GenTexture();
	glTextureView(depthTexId, GL_TEXTURE_2D, stencilTexId, GL_DEPTH32F_STENCIL8, 0, 1, 0, 1);
	glCheck();
	glBindTexture(GL_TEXTURE_2D, depthTexId);
	glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glCheck();
}

void gl4DrawStrips(GLuint output_fbo)
{
	checkOverflowAndReset();
	int scaled_width = (int)roundf(screen_width * settings.rend.ScreenScaling / 100.f);
	int scaled_height = (int)roundf(screen_height * settings.rend.ScreenScaling / 100.f);

	if (geom_fbo == 0)
	{
		glGenFramebuffers(1, &geom_fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, geom_fbo);

		CreateTextures(scaled_width, scaled_height);

		GLuint uStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		verify(uStatus == GL_FRAMEBUFFER_COMPLETE);
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, geom_fbo);
		if (stencilTexId == 0)
			CreateTextures(scaled_width, scaled_height);
	}
	if (texSamplers[0] == 0)
		glGenSamplers(2, texSamplers);

	glcache.DepthMask(GL_TRUE);
	glStencilMask(0xFF);
	glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); glCheck();

	//Draw the strips !

	//We use sampler 0
	glActiveTexture(GL_TEXTURE0);
	glcache.Disable(GL_BLEND);
	glProvokingVertex(GL_LAST_VERTEX_CONVENTION);

	RenderPass previous_pass = {0};
	int render_pass_count = pvrrc.render_passes.used();

	for (int render_pass = 0; render_pass < render_pass_count; render_pass++)
    {
        const RenderPass& current_pass = pvrrc.render_passes.head()[render_pass];

        // Check if we can skip this pass, in part or completely, in case nothing is drawn (Cosmic Smash)
		bool skip_op_pt = true;
		bool skip_tr = true;
		for (int j = previous_pass.op_count; skip_op_pt && j < current_pass.op_count; j++)
		{
			if (pvrrc.global_param_op.head()[j].count > 2)
				skip_op_pt = false;
		}
		for (int j = previous_pass.pt_count; skip_op_pt && j < current_pass.pt_count; j++)
		{
			if (pvrrc.global_param_pt.head()[j].count > 2)
				skip_op_pt = false;
		}
		for (int j = previous_pass.tr_count; skip_tr && j < current_pass.tr_count; j++)
		{
			if (pvrrc.global_param_tr.head()[j].count > 2)
				skip_tr = false;
		}
		if (skip_op_pt && skip_tr)
		{
			previous_pass = current_pass;
			continue;
		}
		glBindVertexArray(gl4.vbo.main_vao);

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
					glBindTexture(GL_TEXTURE_2D, depthSaveTexId);
					glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, scaled_width, scaled_height, 0, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, NULL); glCheck();
					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthSaveTexId, 0); glCheck();
				}
				GLuint uStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				verify(uStatus == GL_FRAMEBUFFER_COMPLETE);

				glBindFramebuffer(GL_READ_FRAMEBUFFER, geom_fbo);
				glBlitFramebuffer(0, 0, scaled_width, scaled_height, 0, 0, scaled_width, scaled_height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
				glCheck();

				glBindFramebuffer(GL_FRAMEBUFFER, geom_fbo);
			}
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glcache.Enable(GL_DEPTH_TEST);
			glcache.DepthMask(GL_TRUE);
			glcache.Enable(GL_STENCIL_TEST);
			glcache.StencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

			DrawList<ListType_Opaque, false>(pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count - previous_pass.op_count, 0);
			DrawList<ListType_Punch_Through, false>(pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count - previous_pass.pt_count, 0);

			// Modifier volumes
			if (settings.rend.ModifierVolumes)
				DrawModVols(previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count);

			//
			// PASS 2: Render OP and PT to fbo
			//
			if (render_pass == 0)
			{
				glcache.DepthMask(GL_TRUE);
				glClear(GL_DEPTH_BUFFER_BIT);
			}
			else
			{
				// Restore the depth buffer from the last render pass
				// FIXME This is pretty slow apparently (CS)
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, geom_fbo);
				glBindFramebuffer(GL_READ_FRAMEBUFFER, depth_fbo);
				glBlitFramebuffer(0, 0, scaled_width, scaled_height, 0, 0, scaled_width, scaled_height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
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
			DrawList<ListType_Opaque, false>(pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count - previous_pass.op_count, 1);

			//Alpha tested
			DrawList<ListType_Punch_Through, false>(pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count - previous_pass.pt_count, 1);

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
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glcache.Disable(GL_DEPTH_TEST);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, depthTexId);
			glActiveTexture(GL_TEXTURE0);

			//Alpha blended
			if (current_pass.autosort)
				DrawList<ListType_Translucent, true>(pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count, 3); // 3 because pass 2 is no more
			else
				DrawList<ListType_Translucent, false>(pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count, 3); // 3 because pass 2 is no more
			glCheck();

			// Translucent modifier volumes
			if (settings.rend.ModifierVolumes)
				DrawTranslucentModVols(previous_pass.mvo_tr_count, current_pass.mvo_tr_count - previous_pass.mvo_tr_count);

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
				if (current_pass.autosort)
					DrawList<ListType_Translucent, true>(pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count, 0);
				else
					DrawList<ListType_Translucent, false>(pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count, 0);

				//
				// PASS 3c: Render a-buffer to temporary texture
				//
				GLuint texId = CreateColorFBOTexture(scaled_width, scaled_height);

				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

				glActiveTexture(GL_TEXTURE0);
				glBindSampler(0, 0);
				glBindTexture(GL_TEXTURE_2D, opaqueTexId);

				renderABuffer(current_pass.autosort);

				glcache.DeleteTextures(1, &opaqueTexId);
				opaqueTexId = texId;

				glCheck();
			}
		}

		if (!skip_op_pt && render_pass < render_pass_count - 1)
		{
			// Clear the stencil from this pass
			glStencilMask(0xFF);
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
	glBindTexture(GL_TEXTURE_2D, opaqueTexId);
	renderABuffer(previous_pass.autosort);
}

static void gl4_draw_quad_texture(GLuint texture, bool upsideDown, float x = 0.f, float y = 0.f, float w = 0.f, float h = 0.f)
{
	glcache.Disable(GL_SCISSOR_TEST);
	glcache.Disable(GL_DEPTH_TEST);
	glcache.Disable(GL_STENCIL_TEST);
	glcache.Disable(GL_CULL_FACE);
	glcache.Disable(GL_BLEND);

	ShaderUniforms.trilinear_alpha = 1.0;

	CurrentShader = gl4GetProgram(0,
				0,
				1,
				0,
				1,
				0,
				0,
				2,
				false,
				0,
				false,
				false,
				false,
				1);
	glcache.UseProgram(CurrentShader->program);
	gl4ShaderUniforms.Set(CurrentShader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	abufferDrawQuad(upsideDown, x, y, w, h);
}

void gl4DrawFramebuffer(float w, float h)
{
	gl4_draw_quad_texture(fbTextureId, false, 0, 0, w, h);
	glcache.DeleteTextures(1, &fbTextureId);
	fbTextureId = 0;
}

bool gl4_render_output_framebuffer()
{
	glViewport(0, 0, screen_width, screen_height);
	glcache.Disable(GL_SCISSOR_TEST);
	if (gl.ofbo.fbo == 0)
		return false;
	glBindFramebuffer(GL_READ_FRAMEBUFFER, gl.ofbo.fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, gl.ofbo.width, gl.ofbo.height,
			0, 0, screen_width, screen_height,
			GL_COLOR_BUFFER_BIT, GL_LINEAR);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return true;
}
