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
	int shaderId;

	if (pass == 0)
	{
		shaderId = gl4GetProgramID(Type == ListType_Punch_Through ? 1 : 0,
				clipping + 1,
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
		CurrentShader = gl4.getShader(shaderId);
		if (CurrentShader->program == -1) {
			CurrentShader->cp_AlphaTest = Type == ListType_Punch_Through ? 1 : 0;
			CurrentShader->pp_ClipTestMode = clipping;
			CurrentShader->pp_Texture = Type == ListType_Punch_Through ? gp->pcw.Texture : 0;
			CurrentShader->pp_UseAlpha = 1;
			CurrentShader->pp_IgnoreTexA = gp->tsp.IgnoreTexA;
			CurrentShader->pp_ShadInstr = 0;
			CurrentShader->pp_Offset = 0;
			CurrentShader->pp_FogCtrl = 2;
			CurrentShader->pp_TwoVolumes = false;
			CurrentShader->pp_DepthFunc = 0;
			CurrentShader->pp_Gouraud = false;
			CurrentShader->pp_BumpMap = false;
			CurrentShader->fog_clamping = false;
			CurrentShader->pass = pass;
			gl4CompilePipelineShader(CurrentShader);
		}
	}
	else
	{
		// Two volumes mode only supported for OP and PT
		bool two_volumes_mode = (gp->tsp1.full != -1) && Type != ListType_Translucent;
		bool color_clamp = gp->tsp.ColorClamp && (pvrrc.fog_clamp_min != 0 || pvrrc.fog_clamp_max != 0xffffffff);

		int depth_func = 0;
		if (Type == ListType_Translucent)
		{
			if (SortingEnabled)
				depth_func = 6;		// GEQUAL
			else
				depth_func = gp->isp.DepthMode;
		}

		shaderId = gl4GetProgramID(Type == ListType_Punch_Through ? 1 : 0,
											 	  clipping + 1,
												  gp->pcw.Texture,
												  gp->tsp.UseAlpha,
												  gp->tsp.IgnoreTexA,
												  gp->tsp.ShadInstr,
												  gp->pcw.Offset,
												  gp->tsp.FogCtrl,
												  two_volumes_mode,
												  depth_func,
												  gp->pcw.Gouraud,
												  gp->tcw.PixelFmt == PixelBumpMap,
												  color_clamp,
												  pass);
		CurrentShader = gl4.getShader(shaderId);
		if (CurrentShader->program == -1) {
			CurrentShader->cp_AlphaTest = Type == ListType_Punch_Through ? 1 : 0;
			CurrentShader->pp_ClipTestMode = clipping;
			CurrentShader->pp_Texture = gp->pcw.Texture;
			CurrentShader->pp_UseAlpha = gp->tsp.UseAlpha;
			CurrentShader->pp_IgnoreTexA = gp->tsp.IgnoreTexA;
			CurrentShader->pp_ShadInstr = gp->tsp.ShadInstr;
			CurrentShader->pp_Offset = gp->pcw.Offset;
			CurrentShader->pp_FogCtrl = gp->tsp.FogCtrl;
			CurrentShader->pp_TwoVolumes = two_volumes_mode;
			CurrentShader->pp_DepthFunc = depth_func;
			CurrentShader->pp_Gouraud = gp->pcw.Gouraud;
			CurrentShader->pp_BumpMap = gp->tcw.PixelFmt == 4;
			CurrentShader->fog_clamping = color_clamp;
			CurrentShader->pass = pass;
			gl4CompilePipelineShader(CurrentShader);
		}
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
			glDrawElements(GL_TRIANGLE_STRIP, params->count, GL_UNSIGNED_SHORT, (GLvoid*)(2*params->first)); glCheck();
		}

		params++;
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
static void SetMVS_Mode(ModifierVolumeMode mv_mode, ISP_Modvol ispc)
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


static void SetupMainVBO()
{
	glBindVertexArray(gl4.vbo.vao);

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
	glBindVertexArray(gl4.vbo.vao);

	glBindBuffer(GL_ARRAY_BUFFER, gl4.vbo.modvols); glCheck();

	//setup vertex buffers attrib pointers
	glEnableVertexAttribArray(VERTEX_POS_ARRAY); glCheck();
	glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(float)*3, (void*)0); glCheck();

	glDisableVertexAttribArray(VERTEX_UV_ARRAY);
	glDisableVertexAttribArray(VERTEX_COL_OFFS_ARRAY);
	glDisableVertexAttribArray(VERTEX_COL_BASE_ARRAY);
	glDisableVertexAttribArray(VERTEX_UV1_ARRAY);
	glDisableVertexAttribArray(VERTEX_COL_OFFS1_ARRAY);
	glDisableVertexAttribArray(VERTEX_COL_BASE1_ARRAY);
}

static void DrawModVols(int first, int count)
{
	if (count == 0 || pvrrc.modtrig.used() == 0)
		return;

	gl4SetupModvolVBO();

	glcache.UseProgram(gl4.modvol_shader.program);

	glcache.DepthMask(GL_FALSE);
	glcache.DepthFunc(Zfunction[4]);

	if(0)
	{
		//simply draw the volumes -- for debugging
		SetCull(0);
		glDrawArrays(GL_TRIANGLES, first, count * 3);
		SetupMainVBO();
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

		SetupMainVBO();
	}

	//restore states
	glcache.Enable(GL_DEPTH_TEST);
	glcache.DepthMask(GL_TRUE);
}

void renderABuffer(bool sortFragments);
void DrawTranslucentModVols(int first, int count);
void checkOverflowAndReset();

static GLuint CreateColorFBOTexture()
{
	GLuint texId = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, texId);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_width, screen_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
	glCheck();

	return texId;
}

static void CreateTextures()
{
	stencilTexId = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, stencilTexId); glCheck();
	glcache.TexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);		// OpenGL >= 4.3
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// Using glTexStorage2D instead of glTexImage2D to satisfy requirement GL_TEXTURE_IMMUTABLE_FORMAT=true, needed for glTextureView below
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH32F_STENCIL8, screen_width, screen_height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, stencilTexId, 0); glCheck();
	glCheck();

	opaqueTexId = CreateColorFBOTexture();

	depthTexId = glcache.GenTexture();
	glTextureView(depthTexId, GL_TEXTURE_2D, stencilTexId, GL_DEPTH32F_STENCIL8, 0, 1, 0, 1);
	glCheck();
	glcache.BindTexture(GL_TEXTURE_2D, depthTexId);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glCheck();
}

void gl4DrawStrips(GLuint output_fbo)
{
	checkOverflowAndReset();

	if (geom_fbo == 0)
	{
		glGenFramebuffers(1, &geom_fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, geom_fbo);

		CreateTextures();

		GLuint uStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		verify(uStatus == GL_FRAMEBUFFER_COMPLETE);
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, geom_fbo);
		if (stencilTexId == 0)
			CreateTextures();
	}
	if (texSamplers[0] == 0)
		glGenSamplers(2, texSamplers);

	glcache.ClearColor(0, 0, 0, 0);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glcache.DepthMask(GL_TRUE);
	glStencilMask(0xFF);
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); glCheck();

	SetupMainVBO();
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
					glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, screen_width, screen_height, 0, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, NULL); glCheck();
					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthSaveTexId, 0); glCheck();
				}
				GLuint uStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				verify(uStatus == GL_FRAMEBUFFER_COMPLETE);

				glBindFramebuffer(GL_READ_FRAMEBUFFER, geom_fbo);
				glBlitFramebuffer(0, 0, screen_width, screen_height, 0, 0, screen_width, screen_height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
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
				glBlitFramebuffer(0, 0, screen_width, screen_height, 0, 0, screen_width, screen_height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
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
				GLuint texId = CreateColorFBOTexture();

				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

				glActiveTexture(GL_TEXTURE0);
				glBindSampler(0, 0);
				glBindTexture(GL_TEXTURE_2D, opaqueTexId);

				renderABuffer(current_pass.autosort);
				SetupMainVBO();

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
	SetupMainVBO();
}

void gl4DrawFramebuffer(float w, float h)
{
	struct Vertex vertices[] = {
		{ 0, h, 1, { 255, 255, 255, 255 }, { 0, 0, 0, 0 }, 0, 1 },
		{ 0, 0, 1, { 255, 255, 255, 255 }, { 0, 0, 0, 0 }, 0, 0 },
		{ w, h, 1, { 255, 255, 255, 255 }, { 0, 0, 0, 0 }, 1, 1 },
		{ w, 0, 1, { 255, 255, 255, 255 }, { 0, 0, 0, 0 }, 1, 0 },
	};
	GLushort indices[] = { 0, 1, 2, 1, 3 };

	glcache.Disable(GL_SCISSOR_TEST);
	glcache.Disable(GL_DEPTH_TEST);
	glcache.Disable(GL_STENCIL_TEST);
	glcache.Disable(GL_CULL_FACE);
	glcache.Disable(GL_BLEND);

	gl4ShaderUniforms.trilinear_alpha = 1.0;

	int shaderId = gl4GetProgramID(0,
				1,
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
	gl4PipelineShader *shader = gl4.getShader(shaderId);
	if (shader->program == -1)
	{
		shader->cp_AlphaTest = 0;
		shader->pp_ClipTestMode = 0;
		shader->pp_Texture = 1;
		shader->pp_UseAlpha = 0;
		shader->pp_IgnoreTexA = 1;
		shader->pp_ShadInstr = 0;
		shader->pp_Offset = 0;
		shader->pp_FogCtrl = 2;
		shader->pp_TwoVolumes = false;
		shader->pp_DepthFunc = 0;
		shader->pp_Gouraud = false;
		shader->pp_BumpMap = false;
		shader->fog_clamping = false;
		shader->pass = 1;
		gl4CompilePipelineShader(shader);
	}
	glcache.UseProgram(shader->program);
	gl4ShaderUniforms.Set(shader);

	glActiveTexture(GL_TEXTURE0);
	glcache.BindTexture(GL_TEXTURE_2D, fbTextureId);

	SetupMainVBO();
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STREAM_DRAW);

	glDrawElements(GL_TRIANGLE_STRIP, 5, GL_UNSIGNED_SHORT, (void *)0);

	glcache.DeleteTextures(1, &fbTextureId);
	fbTextureId = 0;

	glBufferData(GL_ARRAY_BUFFER, pvrrc.verts.bytes(), pvrrc.verts.head(), GL_STREAM_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, pvrrc.idx.bytes(), pvrrc.idx.head(), GL_STREAM_DRAW);
}
