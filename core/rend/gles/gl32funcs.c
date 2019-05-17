#include <stddef.h>
#include <GLES32/gl32.h>
#include <EGL/egl.h>
#include "gl32funcs.h"
#include "build.h"

void load_gles_symbols()
{
#ifdef GLES3
	for (int i = 0; rglgen_symbol_map[i].sym != NULL; i++)
		*(void **)rglgen_symbol_map[i].ptr = eglGetProcAddress(rglgen_symbol_map[i].sym);
#endif
}

#define SYM(x) { "gl" #x, &(gl##x) }
const struct rglgen_sym_map rglgen_symbol_map[] = {
    SYM(ActiveTexture),
    SYM(AttachShader),
    SYM(BindAttribLocation),
    SYM(BindBuffer),
    SYM(BindFramebuffer),
    SYM(BindRenderbuffer),
    SYM(BindTexture),
    SYM(BlendColor),
    SYM(BlendEquation),
    SYM(BlendEquationSeparate),
    SYM(BlendFunc),
    SYM(BlendFuncSeparate),
    SYM(BufferData),
    SYM(BufferSubData),
    SYM(CheckFramebufferStatus),
    SYM(Clear),
    SYM(ClearColor),
    SYM(ClearDepthf),
    SYM(ClearStencil),
    SYM(ColorMask),
    SYM(CompileShader),
    SYM(CompressedTexImage2D),
    SYM(CompressedTexSubImage2D),
    SYM(CopyTexImage2D),
    SYM(CopyTexSubImage2D),
    SYM(CreateProgram),
    SYM(CreateShader),
    SYM(CullFace),
    SYM(DeleteBuffers),
    SYM(DeleteFramebuffers),
    SYM(DeleteProgram),
    SYM(DeleteRenderbuffers),
    SYM(DeleteShader),
    SYM(DeleteTextures),
    SYM(DepthFunc),
    SYM(DepthMask),
    SYM(DepthRangef),
    SYM(DetachShader),
    SYM(Disable),
    SYM(DisableVertexAttribArray),
    SYM(DrawArrays),
    SYM(DrawElements),
    SYM(Enable),
    SYM(EnableVertexAttribArray),
    SYM(Finish),
    SYM(Flush),
    SYM(FramebufferRenderbuffer),
    SYM(FramebufferTexture2D),
    SYM(FrontFace),
    SYM(GenBuffers),
    SYM(GenerateMipmap),
    SYM(GenFramebuffers),
    SYM(GenRenderbuffers),
    SYM(GenTextures),
    SYM(GetActiveAttrib),
    SYM(GetActiveUniform),
    SYM(GetAttachedShaders),
    SYM(GetAttribLocation),
    SYM(GetBooleanv),
    SYM(GetBufferParameteriv),
    SYM(GetError),
    SYM(GetFloatv),
    SYM(GetFramebufferAttachmentParameteriv),
    SYM(GetIntegerv),
    SYM(GetProgramiv),
    SYM(GetProgramInfoLog),
    SYM(GetRenderbufferParameteriv),
    SYM(GetShaderiv),
    SYM(GetShaderInfoLog),
    SYM(GetShaderPrecisionFormat),
    SYM(GetShaderSource),
    SYM(GetString),
    SYM(GetTexParameterfv),
    SYM(GetTexParameteriv),
    SYM(GetUniformfv),
    SYM(GetUniformiv),
    SYM(GetUniformLocation),
    SYM(GetVertexAttribfv),
    SYM(GetVertexAttribiv),
    SYM(GetVertexAttribPointerv),
    SYM(Hint),
    SYM(IsBuffer),
    SYM(IsEnabled),
    SYM(IsFramebuffer),
    SYM(IsProgram),
    SYM(IsRenderbuffer),
    SYM(IsShader),
    SYM(IsTexture),
    SYM(LineWidth),
    SYM(LinkProgram),
    SYM(PixelStorei),
    SYM(PolygonOffset),
    SYM(ReadPixels),
    SYM(ReleaseShaderCompiler),
    SYM(RenderbufferStorage),
    SYM(SampleCoverage),
    SYM(Scissor),
    SYM(ShaderBinary),
    SYM(ShaderSource),
    SYM(StencilFunc),
    SYM(StencilFuncSeparate),
    SYM(StencilMask),
    SYM(StencilMaskSeparate),
    SYM(StencilOp),
    SYM(StencilOpSeparate),
    SYM(TexImage2D),
    SYM(TexParameterf),
    SYM(TexParameterfv),
    SYM(TexParameteri),
    SYM(TexParameteriv),
    SYM(TexSubImage2D),
    SYM(Uniform1f),
    SYM(Uniform1fv),
    SYM(Uniform1i),
    SYM(Uniform1iv),
    SYM(Uniform2f),
    SYM(Uniform2fv),
    SYM(Uniform2i),
    SYM(Uniform2iv),
    SYM(Uniform3f),
    SYM(Uniform3fv),
    SYM(Uniform3i),
    SYM(Uniform3iv),
    SYM(Uniform4f),
    SYM(Uniform4fv),
    SYM(Uniform4i),
    SYM(Uniform4iv),
    SYM(UniformMatrix2fv),
    SYM(UniformMatrix3fv),
    SYM(UniformMatrix4fv),
    SYM(UseProgram),
    SYM(ValidateProgram),
    SYM(VertexAttrib1f),
    SYM(VertexAttrib1fv),
    SYM(VertexAttrib2f),
    SYM(VertexAttrib2fv),
    SYM(VertexAttrib3f),
    SYM(VertexAttrib3fv),
    SYM(VertexAttrib4f),
    SYM(VertexAttrib4fv),
    SYM(VertexAttribPointer),
    SYM(Viewport),
    SYM(ReadBuffer),
    SYM(DrawRangeElements),
    SYM(TexImage3D),
    SYM(TexSubImage3D),
    SYM(CopyTexSubImage3D),
    SYM(CompressedTexImage3D),
    SYM(CompressedTexSubImage3D),
    SYM(GenQueries),
    SYM(DeleteQueries),
    SYM(IsQuery),
    SYM(BeginQuery),
    SYM(EndQuery),
    SYM(GetQueryiv),
    SYM(GetQueryObjectuiv),
    SYM(UnmapBuffer),
    SYM(GetBufferPointerv),
    SYM(DrawBuffers),
    SYM(UniformMatrix2x3fv),
    SYM(UniformMatrix3x2fv),
    SYM(UniformMatrix2x4fv),
    SYM(UniformMatrix4x2fv),
    SYM(UniformMatrix3x4fv),
    SYM(UniformMatrix4x3fv),
    SYM(BlitFramebuffer),
    SYM(RenderbufferStorageMultisample),
    SYM(FramebufferTextureLayer),
    SYM(MapBufferRange),
    SYM(FlushMappedBufferRange),
    SYM(BindVertexArray),
    SYM(DeleteVertexArrays),
    SYM(GenVertexArrays),
    SYM(IsVertexArray),
    SYM(GetIntegeri_v),
    SYM(BeginTransformFeedback),
    SYM(EndTransformFeedback),
    SYM(BindBufferRange),
    SYM(BindBufferBase),
    SYM(TransformFeedbackVaryings),
    SYM(GetTransformFeedbackVarying),
    SYM(VertexAttribIPointer),
    SYM(GetVertexAttribIiv),
    SYM(GetVertexAttribIuiv),
    SYM(VertexAttribI4i),
    SYM(VertexAttribI4ui),
    SYM(VertexAttribI4iv),
    SYM(VertexAttribI4uiv),
    SYM(GetUniformuiv),
    SYM(GetFragDataLocation),
    SYM(Uniform1ui),
    SYM(Uniform2ui),
    SYM(Uniform3ui),
    SYM(Uniform4ui),
    SYM(Uniform1uiv),
    SYM(Uniform2uiv),
    SYM(Uniform3uiv),
    SYM(Uniform4uiv),
    SYM(ClearBufferiv),
    SYM(ClearBufferuiv),
    SYM(ClearBufferfv),
    SYM(ClearBufferfi),
    SYM(GetStringi),
    SYM(CopyBufferSubData),
    SYM(GetUniformIndices),
    SYM(GetActiveUniformsiv),
    SYM(GetUniformBlockIndex),
    SYM(GetActiveUniformBlockiv),
    SYM(GetActiveUniformBlockName),
    SYM(UniformBlockBinding),
    SYM(DrawArraysInstanced),
    SYM(DrawElementsInstanced),
    SYM(FenceSync),
    SYM(IsSync),
    SYM(DeleteSync),
    SYM(ClientWaitSync),
    SYM(WaitSync),
    SYM(GetInteger64v),
    SYM(GetSynciv),
    SYM(GetInteger64i_v),
    SYM(GetBufferParameteri64v),
    SYM(GenSamplers),
    SYM(DeleteSamplers),
    SYM(IsSampler),
    SYM(BindSampler),
    SYM(SamplerParameteri),
    SYM(SamplerParameteriv),
    SYM(SamplerParameterf),
    SYM(SamplerParameterfv),
    SYM(GetSamplerParameteriv),
    SYM(GetSamplerParameterfv),
    SYM(VertexAttribDivisor),
    SYM(BindTransformFeedback),
    SYM(DeleteTransformFeedbacks),
    SYM(GenTransformFeedbacks),
    SYM(IsTransformFeedback),
    SYM(PauseTransformFeedback),
    SYM(ResumeTransformFeedback),
    SYM(GetProgramBinary),
    SYM(ProgramBinary),
    SYM(ProgramParameteri),
    SYM(InvalidateFramebuffer),
    SYM(InvalidateSubFramebuffer),
    SYM(TexStorage2D),
    SYM(TexStorage3D),
    SYM(GetInternalformativ),
    SYM(DispatchCompute),
    SYM(DispatchComputeIndirect),
    SYM(DrawArraysIndirect),
    SYM(DrawElementsIndirect),
    SYM(FramebufferParameteri),
    SYM(GetFramebufferParameteriv),
    SYM(GetProgramInterfaceiv),
    SYM(GetProgramResourceIndex),
    SYM(GetProgramResourceName),
    SYM(GetProgramResourceiv),
    SYM(GetProgramResourceLocation),
    SYM(UseProgramStages),
    SYM(ActiveShaderProgram),
    SYM(CreateShaderProgramv),
    SYM(BindProgramPipeline),
    SYM(DeleteProgramPipelines),
    SYM(GenProgramPipelines),
    SYM(IsProgramPipeline),
    SYM(GetProgramPipelineiv),
    SYM(ProgramUniform1i),
    SYM(ProgramUniform2i),
    SYM(ProgramUniform3i),
    SYM(ProgramUniform4i),
    SYM(ProgramUniform1ui),
    SYM(ProgramUniform2ui),
    SYM(ProgramUniform3ui),
    SYM(ProgramUniform4ui),
    SYM(ProgramUniform1f),
    SYM(ProgramUniform2f),
    SYM(ProgramUniform3f),
    SYM(ProgramUniform4f),
    SYM(ProgramUniform1iv),
    SYM(ProgramUniform2iv),
    SYM(ProgramUniform3iv),
    SYM(ProgramUniform4iv),
    SYM(ProgramUniform1uiv),
    SYM(ProgramUniform2uiv),
    SYM(ProgramUniform3uiv),
    SYM(ProgramUniform4uiv),
    SYM(ProgramUniform1fv),
    SYM(ProgramUniform2fv),
    SYM(ProgramUniform3fv),
    SYM(ProgramUniform4fv),
    SYM(ProgramUniformMatrix2fv),
    SYM(ProgramUniformMatrix3fv),
    SYM(ProgramUniformMatrix4fv),
    SYM(ProgramUniformMatrix2x3fv),
    SYM(ProgramUniformMatrix3x2fv),
    SYM(ProgramUniformMatrix2x4fv),
    SYM(ProgramUniformMatrix4x2fv),
    SYM(ProgramUniformMatrix3x4fv),
    SYM(ProgramUniformMatrix4x3fv),
    SYM(ValidateProgramPipeline),
    SYM(GetProgramPipelineInfoLog),
    SYM(BindImageTexture),
    SYM(GetBooleani_v),
    SYM(MemoryBarrier),
    SYM(MemoryBarrierByRegion),
    SYM(TexStorage2DMultisample),
    SYM(GetMultisamplefv),
    SYM(SampleMaski),
    SYM(GetTexLevelParameteriv),
    SYM(GetTexLevelParameterfv),
    SYM(BindVertexBuffer),
    SYM(VertexAttribFormat),
    SYM(VertexAttribIFormat),
    SYM(VertexAttribBinding),
    SYM(VertexBindingDivisor),
    SYM(BlendBarrier),
    SYM(CopyImageSubData),
    SYM(DebugMessageControl),
    SYM(DebugMessageInsert),
    SYM(DebugMessageCallback),
    SYM(GetDebugMessageLog),
    SYM(PushDebugGroup),
    SYM(PopDebugGroup),
    SYM(ObjectLabel),
    SYM(GetObjectLabel),
    SYM(ObjectPtrLabel),
    SYM(GetObjectPtrLabel),
    SYM(GetPointerv),
    SYM(Enablei),
    SYM(Disablei),
    SYM(BlendEquationi),
    SYM(BlendEquationSeparatei),
    SYM(BlendFunci),
    SYM(BlendFuncSeparatei),
    SYM(ColorMaski),
    SYM(IsEnabledi),
    SYM(DrawElementsBaseVertex),
    SYM(DrawRangeElementsBaseVertex),
    SYM(DrawElementsInstancedBaseVertex),
    SYM(FramebufferTexture),
    SYM(PrimitiveBoundingBox),
    SYM(GetGraphicsResetStatus),
    SYM(ReadnPixels),
    SYM(GetnUniformfv),
    SYM(GetnUniformiv),
    SYM(GetnUniformuiv),
    SYM(MinSampleShading),
    SYM(PatchParameteri),
    SYM(TexParameterIiv),
    SYM(TexParameterIuiv),
    SYM(GetTexParameterIiv),
    SYM(GetTexParameterIuiv),
    SYM(SamplerParameterIiv),
    SYM(SamplerParameterIuiv),
    SYM(GetSamplerParameterIiv),
    SYM(GetSamplerParameterIuiv),
    SYM(TexBuffer),
    SYM(TexBufferRange),
    SYM(TexStorage3DMultisample),

    { NULL, NULL },
};
RGLSYMGLACTIVETEXTUREPROC __rglgen_glActiveTexture;
RGLSYMGLATTACHSHADERPROC __rglgen_glAttachShader;
RGLSYMGLBINDATTRIBLOCATIONPROC __rglgen_glBindAttribLocation;
RGLSYMGLBINDBUFFERPROC __rglgen_glBindBuffer;
RGLSYMGLBINDFRAMEBUFFERPROC __rglgen_glBindFramebuffer;
RGLSYMGLBINDRENDERBUFFERPROC __rglgen_glBindRenderbuffer;
RGLSYMGLBINDTEXTUREPROC __rglgen_glBindTexture;
RGLSYMGLBLENDCOLORPROC __rglgen_glBlendColor;
RGLSYMGLBLENDEQUATIONPROC __rglgen_glBlendEquation;
RGLSYMGLBLENDEQUATIONSEPARATEPROC __rglgen_glBlendEquationSeparate;
RGLSYMGLBLENDFUNCPROC __rglgen_glBlendFunc;
RGLSYMGLBLENDFUNCSEPARATEPROC __rglgen_glBlendFuncSeparate;
RGLSYMGLBUFFERDATAPROC __rglgen_glBufferData;
RGLSYMGLBUFFERSUBDATAPROC __rglgen_glBufferSubData;
RGLSYMGLCHECKFRAMEBUFFERSTATUSPROC __rglgen_glCheckFramebufferStatus;
RGLSYMGLCLEARPROC __rglgen_glClear;
RGLSYMGLCLEARCOLORPROC __rglgen_glClearColor;
RGLSYMGLCLEARDEPTHFPROC __rglgen_glClearDepthf;
RGLSYMGLCLEARSTENCILPROC __rglgen_glClearStencil;
RGLSYMGLCOLORMASKPROC __rglgen_glColorMask;
RGLSYMGLCOMPILESHADERPROC __rglgen_glCompileShader;
RGLSYMGLCOMPRESSEDTEXIMAGE2DPROC __rglgen_glCompressedTexImage2D;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE2DPROC __rglgen_glCompressedTexSubImage2D;
RGLSYMGLCOPYTEXIMAGE2DPROC __rglgen_glCopyTexImage2D;
RGLSYMGLCOPYTEXSUBIMAGE2DPROC __rglgen_glCopyTexSubImage2D;
RGLSYMGLCREATEPROGRAMPROC __rglgen_glCreateProgram;
RGLSYMGLCREATESHADERPROC __rglgen_glCreateShader;
RGLSYMGLCULLFACEPROC __rglgen_glCullFace;
RGLSYMGLDELETEBUFFERSPROC __rglgen_glDeleteBuffers;
RGLSYMGLDELETEFRAMEBUFFERSPROC __rglgen_glDeleteFramebuffers;
RGLSYMGLDELETEPROGRAMPROC __rglgen_glDeleteProgram;
RGLSYMGLDELETERENDERBUFFERSPROC __rglgen_glDeleteRenderbuffers;
RGLSYMGLDELETESHADERPROC __rglgen_glDeleteShader;
RGLSYMGLDELETETEXTURESPROC __rglgen_glDeleteTextures;
RGLSYMGLDEPTHFUNCPROC __rglgen_glDepthFunc;
RGLSYMGLDEPTHMASKPROC __rglgen_glDepthMask;
RGLSYMGLDEPTHRANGEFPROC __rglgen_glDepthRangef;
RGLSYMGLDETACHSHADERPROC __rglgen_glDetachShader;
RGLSYMGLDISABLEPROC __rglgen_glDisable;
RGLSYMGLDISABLEVERTEXATTRIBARRAYPROC __rglgen_glDisableVertexAttribArray;
RGLSYMGLDRAWARRAYSPROC __rglgen_glDrawArrays;
RGLSYMGLDRAWELEMENTSPROC __rglgen_glDrawElements;
RGLSYMGLENABLEPROC __rglgen_glEnable;
RGLSYMGLENABLEVERTEXATTRIBARRAYPROC __rglgen_glEnableVertexAttribArray;
RGLSYMGLFINISHPROC __rglgen_glFinish;
RGLSYMGLFLUSHPROC __rglgen_glFlush;
RGLSYMGLFRAMEBUFFERRENDERBUFFERPROC __rglgen_glFramebufferRenderbuffer;
RGLSYMGLFRAMEBUFFERTEXTURE2DPROC __rglgen_glFramebufferTexture2D;
RGLSYMGLFRONTFACEPROC __rglgen_glFrontFace;
RGLSYMGLGENBUFFERSPROC __rglgen_glGenBuffers;
RGLSYMGLGENERATEMIPMAPPROC __rglgen_glGenerateMipmap;
RGLSYMGLGENFRAMEBUFFERSPROC __rglgen_glGenFramebuffers;
RGLSYMGLGENRENDERBUFFERSPROC __rglgen_glGenRenderbuffers;
RGLSYMGLGENTEXTURESPROC __rglgen_glGenTextures;
RGLSYMGLGETACTIVEATTRIBPROC __rglgen_glGetActiveAttrib;
RGLSYMGLGETACTIVEUNIFORMPROC __rglgen_glGetActiveUniform;
RGLSYMGLGETATTACHEDSHADERSPROC __rglgen_glGetAttachedShaders;
RGLSYMGLGETATTRIBLOCATIONPROC __rglgen_glGetAttribLocation;
RGLSYMGLGETBOOLEANVPROC __rglgen_glGetBooleanv;
RGLSYMGLGETBUFFERPARAMETERIVPROC __rglgen_glGetBufferParameteriv;
RGLSYMGLGETERRORPROC __rglgen_glGetError;
RGLSYMGLGETFLOATVPROC __rglgen_glGetFloatv;
RGLSYMGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC __rglgen_glGetFramebufferAttachmentParameteriv;
RGLSYMGLGETINTEGERVPROC __rglgen_glGetIntegerv;
RGLSYMGLGETPROGRAMIVPROC __rglgen_glGetProgramiv;
RGLSYMGLGETPROGRAMINFOLOGPROC __rglgen_glGetProgramInfoLog;
RGLSYMGLGETRENDERBUFFERPARAMETERIVPROC __rglgen_glGetRenderbufferParameteriv;
RGLSYMGLGETSHADERIVPROC __rglgen_glGetShaderiv;
RGLSYMGLGETSHADERINFOLOGPROC __rglgen_glGetShaderInfoLog;
RGLSYMGLGETSHADERPRECISIONFORMATPROC __rglgen_glGetShaderPrecisionFormat;
RGLSYMGLGETSHADERSOURCEPROC __rglgen_glGetShaderSource;
RGLSYMGLGETSTRINGPROC __rglgen_glGetString;
RGLSYMGLGETTEXPARAMETERFVPROC __rglgen_glGetTexParameterfv;
RGLSYMGLGETTEXPARAMETERIVPROC __rglgen_glGetTexParameteriv;
RGLSYMGLGETUNIFORMFVPROC __rglgen_glGetUniformfv;
RGLSYMGLGETUNIFORMIVPROC __rglgen_glGetUniformiv;
RGLSYMGLGETUNIFORMLOCATIONPROC __rglgen_glGetUniformLocation;
RGLSYMGLGETVERTEXATTRIBFVPROC __rglgen_glGetVertexAttribfv;
RGLSYMGLGETVERTEXATTRIBIVPROC __rglgen_glGetVertexAttribiv;
RGLSYMGLGETVERTEXATTRIBPOINTERVPROC __rglgen_glGetVertexAttribPointerv;
RGLSYMGLHINTPROC __rglgen_glHint;
RGLSYMGLISBUFFERPROC __rglgen_glIsBuffer;
RGLSYMGLISENABLEDPROC __rglgen_glIsEnabled;
RGLSYMGLISFRAMEBUFFERPROC __rglgen_glIsFramebuffer;
RGLSYMGLISPROGRAMPROC __rglgen_glIsProgram;
RGLSYMGLISRENDERBUFFERPROC __rglgen_glIsRenderbuffer;
RGLSYMGLISSHADERPROC __rglgen_glIsShader;
RGLSYMGLISTEXTUREPROC __rglgen_glIsTexture;
RGLSYMGLLINEWIDTHPROC __rglgen_glLineWidth;
RGLSYMGLLINKPROGRAMPROC __rglgen_glLinkProgram;
RGLSYMGLPIXELSTOREIPROC __rglgen_glPixelStorei;
RGLSYMGLPOLYGONOFFSETPROC __rglgen_glPolygonOffset;
RGLSYMGLREADPIXELSPROC __rglgen_glReadPixels;
RGLSYMGLRELEASESHADERCOMPILERPROC __rglgen_glReleaseShaderCompiler;
RGLSYMGLRENDERBUFFERSTORAGEPROC __rglgen_glRenderbufferStorage;
RGLSYMGLSAMPLECOVERAGEPROC __rglgen_glSampleCoverage;
RGLSYMGLSCISSORPROC __rglgen_glScissor;
RGLSYMGLSHADERBINARYPROC __rglgen_glShaderBinary;
RGLSYMGLSHADERSOURCEPROC __rglgen_glShaderSource;
RGLSYMGLSTENCILFUNCPROC __rglgen_glStencilFunc;
RGLSYMGLSTENCILFUNCSEPARATEPROC __rglgen_glStencilFuncSeparate;
RGLSYMGLSTENCILMASKPROC __rglgen_glStencilMask;
RGLSYMGLSTENCILMASKSEPARATEPROC __rglgen_glStencilMaskSeparate;
RGLSYMGLSTENCILOPPROC __rglgen_glStencilOp;
RGLSYMGLSTENCILOPSEPARATEPROC __rglgen_glStencilOpSeparate;
RGLSYMGLTEXIMAGE2DPROC __rglgen_glTexImage2D;
RGLSYMGLTEXPARAMETERFPROC __rglgen_glTexParameterf;
RGLSYMGLTEXPARAMETERFVPROC __rglgen_glTexParameterfv;
RGLSYMGLTEXPARAMETERIPROC __rglgen_glTexParameteri;
RGLSYMGLTEXPARAMETERIVPROC __rglgen_glTexParameteriv;
RGLSYMGLTEXSUBIMAGE2DPROC __rglgen_glTexSubImage2D;
RGLSYMGLUNIFORM1FPROC __rglgen_glUniform1f;
RGLSYMGLUNIFORM1FVPROC __rglgen_glUniform1fv;
RGLSYMGLUNIFORM1IPROC __rglgen_glUniform1i;
RGLSYMGLUNIFORM1IVPROC __rglgen_glUniform1iv;
RGLSYMGLUNIFORM2FPROC __rglgen_glUniform2f;
RGLSYMGLUNIFORM2FVPROC __rglgen_glUniform2fv;
RGLSYMGLUNIFORM2IPROC __rglgen_glUniform2i;
RGLSYMGLUNIFORM2IVPROC __rglgen_glUniform2iv;
RGLSYMGLUNIFORM3FPROC __rglgen_glUniform3f;
RGLSYMGLUNIFORM3FVPROC __rglgen_glUniform3fv;
RGLSYMGLUNIFORM3IPROC __rglgen_glUniform3i;
RGLSYMGLUNIFORM3IVPROC __rglgen_glUniform3iv;
RGLSYMGLUNIFORM4FPROC __rglgen_glUniform4f;
RGLSYMGLUNIFORM4FVPROC __rglgen_glUniform4fv;
RGLSYMGLUNIFORM4IPROC __rglgen_glUniform4i;
RGLSYMGLUNIFORM4IVPROC __rglgen_glUniform4iv;
RGLSYMGLUNIFORMMATRIX2FVPROC __rglgen_glUniformMatrix2fv;
RGLSYMGLUNIFORMMATRIX3FVPROC __rglgen_glUniformMatrix3fv;
RGLSYMGLUNIFORMMATRIX4FVPROC __rglgen_glUniformMatrix4fv;
RGLSYMGLUSEPROGRAMPROC __rglgen_glUseProgram;
RGLSYMGLVALIDATEPROGRAMPROC __rglgen_glValidateProgram;
RGLSYMGLVERTEXATTRIB1FPROC __rglgen_glVertexAttrib1f;
RGLSYMGLVERTEXATTRIB1FVPROC __rglgen_glVertexAttrib1fv;
RGLSYMGLVERTEXATTRIB2FPROC __rglgen_glVertexAttrib2f;
RGLSYMGLVERTEXATTRIB2FVPROC __rglgen_glVertexAttrib2fv;
RGLSYMGLVERTEXATTRIB3FPROC __rglgen_glVertexAttrib3f;
RGLSYMGLVERTEXATTRIB3FVPROC __rglgen_glVertexAttrib3fv;
RGLSYMGLVERTEXATTRIB4FPROC __rglgen_glVertexAttrib4f;
RGLSYMGLVERTEXATTRIB4FVPROC __rglgen_glVertexAttrib4fv;
RGLSYMGLVERTEXATTRIBPOINTERPROC __rglgen_glVertexAttribPointer;
RGLSYMGLVIEWPORTPROC __rglgen_glViewport;
RGLSYMGLREADBUFFERPROC __rglgen_glReadBuffer;
RGLSYMGLDRAWRANGEELEMENTSPROC __rglgen_glDrawRangeElements;
RGLSYMGLTEXIMAGE3DPROC __rglgen_glTexImage3D;
RGLSYMGLTEXSUBIMAGE3DPROC __rglgen_glTexSubImage3D;
RGLSYMGLCOPYTEXSUBIMAGE3DPROC __rglgen_glCopyTexSubImage3D;
RGLSYMGLCOMPRESSEDTEXIMAGE3DPROC __rglgen_glCompressedTexImage3D;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DPROC __rglgen_glCompressedTexSubImage3D;
RGLSYMGLGENQUERIESPROC __rglgen_glGenQueries;
RGLSYMGLDELETEQUERIESPROC __rglgen_glDeleteQueries;
RGLSYMGLISQUERYPROC __rglgen_glIsQuery;
RGLSYMGLBEGINQUERYPROC __rglgen_glBeginQuery;
RGLSYMGLENDQUERYPROC __rglgen_glEndQuery;
RGLSYMGLGETQUERYIVPROC __rglgen_glGetQueryiv;
RGLSYMGLGETQUERYOBJECTUIVPROC __rglgen_glGetQueryObjectuiv;
RGLSYMGLUNMAPBUFFERPROC __rglgen_glUnmapBuffer;
RGLSYMGLGETBUFFERPOINTERVPROC __rglgen_glGetBufferPointerv;
RGLSYMGLDRAWBUFFERSPROC __rglgen_glDrawBuffers;
RGLSYMGLUNIFORMMATRIX2X3FVPROC __rglgen_glUniformMatrix2x3fv;
RGLSYMGLUNIFORMMATRIX3X2FVPROC __rglgen_glUniformMatrix3x2fv;
RGLSYMGLUNIFORMMATRIX2X4FVPROC __rglgen_glUniformMatrix2x4fv;
RGLSYMGLUNIFORMMATRIX4X2FVPROC __rglgen_glUniformMatrix4x2fv;
RGLSYMGLUNIFORMMATRIX3X4FVPROC __rglgen_glUniformMatrix3x4fv;
RGLSYMGLUNIFORMMATRIX4X3FVPROC __rglgen_glUniformMatrix4x3fv;
RGLSYMGLBLITFRAMEBUFFERPROC __rglgen_glBlitFramebuffer;
RGLSYMGLRENDERBUFFERSTORAGEMULTISAMPLEPROC __rglgen_glRenderbufferStorageMultisample;
RGLSYMGLFRAMEBUFFERTEXTURELAYERPROC __rglgen_glFramebufferTextureLayer;
RGLSYMGLMAPBUFFERRANGEPROC __rglgen_glMapBufferRange;
RGLSYMGLFLUSHMAPPEDBUFFERRANGEPROC __rglgen_glFlushMappedBufferRange;
RGLSYMGLBINDVERTEXARRAYPROC __rglgen_glBindVertexArray;
RGLSYMGLDELETEVERTEXARRAYSPROC __rglgen_glDeleteVertexArrays;
RGLSYMGLGENVERTEXARRAYSPROC __rglgen_glGenVertexArrays;
RGLSYMGLISVERTEXARRAYPROC __rglgen_glIsVertexArray;
RGLSYMGLGETINTEGERI_VPROC __rglgen_glGetIntegeri_v;
RGLSYMGLBEGINTRANSFORMFEEDBACKPROC __rglgen_glBeginTransformFeedback;
RGLSYMGLENDTRANSFORMFEEDBACKPROC __rglgen_glEndTransformFeedback;
RGLSYMGLBINDBUFFERRANGEPROC __rglgen_glBindBufferRange;
RGLSYMGLBINDBUFFERBASEPROC __rglgen_glBindBufferBase;
RGLSYMGLTRANSFORMFEEDBACKVARYINGSPROC __rglgen_glTransformFeedbackVaryings;
RGLSYMGLGETTRANSFORMFEEDBACKVARYINGPROC __rglgen_glGetTransformFeedbackVarying;
RGLSYMGLVERTEXATTRIBIPOINTERPROC __rglgen_glVertexAttribIPointer;
RGLSYMGLGETVERTEXATTRIBIIVPROC __rglgen_glGetVertexAttribIiv;
RGLSYMGLGETVERTEXATTRIBIUIVPROC __rglgen_glGetVertexAttribIuiv;
RGLSYMGLVERTEXATTRIBI4IPROC __rglgen_glVertexAttribI4i;
RGLSYMGLVERTEXATTRIBI4UIPROC __rglgen_glVertexAttribI4ui;
RGLSYMGLVERTEXATTRIBI4IVPROC __rglgen_glVertexAttribI4iv;
RGLSYMGLVERTEXATTRIBI4UIVPROC __rglgen_glVertexAttribI4uiv;
RGLSYMGLGETUNIFORMUIVPROC __rglgen_glGetUniformuiv;
RGLSYMGLGETFRAGDATALOCATIONPROC __rglgen_glGetFragDataLocation;
RGLSYMGLUNIFORM1UIPROC __rglgen_glUniform1ui;
RGLSYMGLUNIFORM2UIPROC __rglgen_glUniform2ui;
RGLSYMGLUNIFORM3UIPROC __rglgen_glUniform3ui;
RGLSYMGLUNIFORM4UIPROC __rglgen_glUniform4ui;
RGLSYMGLUNIFORM1UIVPROC __rglgen_glUniform1uiv;
RGLSYMGLUNIFORM2UIVPROC __rglgen_glUniform2uiv;
RGLSYMGLUNIFORM3UIVPROC __rglgen_glUniform3uiv;
RGLSYMGLUNIFORM4UIVPROC __rglgen_glUniform4uiv;
RGLSYMGLCLEARBUFFERIVPROC __rglgen_glClearBufferiv;
RGLSYMGLCLEARBUFFERUIVPROC __rglgen_glClearBufferuiv;
RGLSYMGLCLEARBUFFERFVPROC __rglgen_glClearBufferfv;
RGLSYMGLCLEARBUFFERFIPROC __rglgen_glClearBufferfi;
RGLSYMGLGETSTRINGIPROC __rglgen_glGetStringi;
RGLSYMGLCOPYBUFFERSUBDATAPROC __rglgen_glCopyBufferSubData;
RGLSYMGLGETUNIFORMINDICESPROC __rglgen_glGetUniformIndices;
RGLSYMGLGETACTIVEUNIFORMSIVPROC __rglgen_glGetActiveUniformsiv;
RGLSYMGLGETUNIFORMBLOCKINDEXPROC __rglgen_glGetUniformBlockIndex;
RGLSYMGLGETACTIVEUNIFORMBLOCKIVPROC __rglgen_glGetActiveUniformBlockiv;
RGLSYMGLGETACTIVEUNIFORMBLOCKNAMEPROC __rglgen_glGetActiveUniformBlockName;
RGLSYMGLUNIFORMBLOCKBINDINGPROC __rglgen_glUniformBlockBinding;
RGLSYMGLDRAWARRAYSINSTANCEDPROC __rglgen_glDrawArraysInstanced;
RGLSYMGLDRAWELEMENTSINSTANCEDPROC __rglgen_glDrawElementsInstanced;
RGLSYMGLFENCESYNCPROC __rglgen_glFenceSync;
RGLSYMGLISSYNCPROC __rglgen_glIsSync;
RGLSYMGLDELETESYNCPROC __rglgen_glDeleteSync;
RGLSYMGLCLIENTWAITSYNCPROC __rglgen_glClientWaitSync;
RGLSYMGLWAITSYNCPROC __rglgen_glWaitSync;
RGLSYMGLGETINTEGER64VPROC __rglgen_glGetInteger64v;
RGLSYMGLGETSYNCIVPROC __rglgen_glGetSynciv;
RGLSYMGLGETINTEGER64I_VPROC __rglgen_glGetInteger64i_v;
RGLSYMGLGETBUFFERPARAMETERI64VPROC __rglgen_glGetBufferParameteri64v;
RGLSYMGLGENSAMPLERSPROC __rglgen_glGenSamplers;
RGLSYMGLDELETESAMPLERSPROC __rglgen_glDeleteSamplers;
RGLSYMGLISSAMPLERPROC __rglgen_glIsSampler;
RGLSYMGLBINDSAMPLERPROC __rglgen_glBindSampler;
RGLSYMGLSAMPLERPARAMETERIPROC __rglgen_glSamplerParameteri;
RGLSYMGLSAMPLERPARAMETERIVPROC __rglgen_glSamplerParameteriv;
RGLSYMGLSAMPLERPARAMETERFPROC __rglgen_glSamplerParameterf;
RGLSYMGLSAMPLERPARAMETERFVPROC __rglgen_glSamplerParameterfv;
RGLSYMGLGETSAMPLERPARAMETERIVPROC __rglgen_glGetSamplerParameteriv;
RGLSYMGLGETSAMPLERPARAMETERFVPROC __rglgen_glGetSamplerParameterfv;
RGLSYMGLVERTEXATTRIBDIVISORPROC __rglgen_glVertexAttribDivisor;
RGLSYMGLBINDTRANSFORMFEEDBACKPROC __rglgen_glBindTransformFeedback;
RGLSYMGLDELETETRANSFORMFEEDBACKSPROC __rglgen_glDeleteTransformFeedbacks;
RGLSYMGLGENTRANSFORMFEEDBACKSPROC __rglgen_glGenTransformFeedbacks;
RGLSYMGLISTRANSFORMFEEDBACKPROC __rglgen_glIsTransformFeedback;
RGLSYMGLPAUSETRANSFORMFEEDBACKPROC __rglgen_glPauseTransformFeedback;
RGLSYMGLRESUMETRANSFORMFEEDBACKPROC __rglgen_glResumeTransformFeedback;
RGLSYMGLGETPROGRAMBINARYPROC __rglgen_glGetProgramBinary;
RGLSYMGLPROGRAMBINARYPROC __rglgen_glProgramBinary;
RGLSYMGLPROGRAMPARAMETERIPROC __rglgen_glProgramParameteri;
RGLSYMGLINVALIDATEFRAMEBUFFERPROC __rglgen_glInvalidateFramebuffer;
RGLSYMGLINVALIDATESUBFRAMEBUFFERPROC __rglgen_glInvalidateSubFramebuffer;
RGLSYMGLTEXSTORAGE2DPROC __rglgen_glTexStorage2D;
RGLSYMGLTEXSTORAGE3DPROC __rglgen_glTexStorage3D;
RGLSYMGLGETINTERNALFORMATIVPROC __rglgen_glGetInternalformativ;
RGLSYMGLDISPATCHCOMPUTEPROC __rglgen_glDispatchCompute;
RGLSYMGLDISPATCHCOMPUTEINDIRECTPROC __rglgen_glDispatchComputeIndirect;
RGLSYMGLDRAWARRAYSINDIRECTPROC __rglgen_glDrawArraysIndirect;
RGLSYMGLDRAWELEMENTSINDIRECTPROC __rglgen_glDrawElementsIndirect;
RGLSYMGLFRAMEBUFFERPARAMETERIPROC __rglgen_glFramebufferParameteri;
RGLSYMGLGETFRAMEBUFFERPARAMETERIVPROC __rglgen_glGetFramebufferParameteriv;
RGLSYMGLGETPROGRAMINTERFACEIVPROC __rglgen_glGetProgramInterfaceiv;
RGLSYMGLGETPROGRAMRESOURCEINDEXPROC __rglgen_glGetProgramResourceIndex;
RGLSYMGLGETPROGRAMRESOURCENAMEPROC __rglgen_glGetProgramResourceName;
RGLSYMGLGETPROGRAMRESOURCEIVPROC __rglgen_glGetProgramResourceiv;
RGLSYMGLGETPROGRAMRESOURCELOCATIONPROC __rglgen_glGetProgramResourceLocation;
RGLSYMGLUSEPROGRAMSTAGESPROC __rglgen_glUseProgramStages;
RGLSYMGLACTIVESHADERPROGRAMPROC __rglgen_glActiveShaderProgram;
RGLSYMGLCREATESHADERPROGRAMVPROC __rglgen_glCreateShaderProgramv;
RGLSYMGLBINDPROGRAMPIPELINEPROC __rglgen_glBindProgramPipeline;
RGLSYMGLDELETEPROGRAMPIPELINESPROC __rglgen_glDeleteProgramPipelines;
RGLSYMGLGENPROGRAMPIPELINESPROC __rglgen_glGenProgramPipelines;
RGLSYMGLISPROGRAMPIPELINEPROC __rglgen_glIsProgramPipeline;
RGLSYMGLGETPROGRAMPIPELINEIVPROC __rglgen_glGetProgramPipelineiv;
RGLSYMGLPROGRAMUNIFORM1IPROC __rglgen_glProgramUniform1i;
RGLSYMGLPROGRAMUNIFORM2IPROC __rglgen_glProgramUniform2i;
RGLSYMGLPROGRAMUNIFORM3IPROC __rglgen_glProgramUniform3i;
RGLSYMGLPROGRAMUNIFORM4IPROC __rglgen_glProgramUniform4i;
RGLSYMGLPROGRAMUNIFORM1UIPROC __rglgen_glProgramUniform1ui;
RGLSYMGLPROGRAMUNIFORM2UIPROC __rglgen_glProgramUniform2ui;
RGLSYMGLPROGRAMUNIFORM3UIPROC __rglgen_glProgramUniform3ui;
RGLSYMGLPROGRAMUNIFORM4UIPROC __rglgen_glProgramUniform4ui;
RGLSYMGLPROGRAMUNIFORM1FPROC __rglgen_glProgramUniform1f;
RGLSYMGLPROGRAMUNIFORM2FPROC __rglgen_glProgramUniform2f;
RGLSYMGLPROGRAMUNIFORM3FPROC __rglgen_glProgramUniform3f;
RGLSYMGLPROGRAMUNIFORM4FPROC __rglgen_glProgramUniform4f;
RGLSYMGLPROGRAMUNIFORM1IVPROC __rglgen_glProgramUniform1iv;
RGLSYMGLPROGRAMUNIFORM2IVPROC __rglgen_glProgramUniform2iv;
RGLSYMGLPROGRAMUNIFORM3IVPROC __rglgen_glProgramUniform3iv;
RGLSYMGLPROGRAMUNIFORM4IVPROC __rglgen_glProgramUniform4iv;
RGLSYMGLPROGRAMUNIFORM1UIVPROC __rglgen_glProgramUniform1uiv;
RGLSYMGLPROGRAMUNIFORM2UIVPROC __rglgen_glProgramUniform2uiv;
RGLSYMGLPROGRAMUNIFORM3UIVPROC __rglgen_glProgramUniform3uiv;
RGLSYMGLPROGRAMUNIFORM4UIVPROC __rglgen_glProgramUniform4uiv;
RGLSYMGLPROGRAMUNIFORM1FVPROC __rglgen_glProgramUniform1fv;
RGLSYMGLPROGRAMUNIFORM2FVPROC __rglgen_glProgramUniform2fv;
RGLSYMGLPROGRAMUNIFORM3FVPROC __rglgen_glProgramUniform3fv;
RGLSYMGLPROGRAMUNIFORM4FVPROC __rglgen_glProgramUniform4fv;
RGLSYMGLPROGRAMUNIFORMMATRIX2FVPROC __rglgen_glProgramUniformMatrix2fv;
RGLSYMGLPROGRAMUNIFORMMATRIX3FVPROC __rglgen_glProgramUniformMatrix3fv;
RGLSYMGLPROGRAMUNIFORMMATRIX4FVPROC __rglgen_glProgramUniformMatrix4fv;
RGLSYMGLPROGRAMUNIFORMMATRIX2X3FVPROC __rglgen_glProgramUniformMatrix2x3fv;
RGLSYMGLPROGRAMUNIFORMMATRIX3X2FVPROC __rglgen_glProgramUniformMatrix3x2fv;
RGLSYMGLPROGRAMUNIFORMMATRIX2X4FVPROC __rglgen_glProgramUniformMatrix2x4fv;
RGLSYMGLPROGRAMUNIFORMMATRIX4X2FVPROC __rglgen_glProgramUniformMatrix4x2fv;
RGLSYMGLPROGRAMUNIFORMMATRIX3X4FVPROC __rglgen_glProgramUniformMatrix3x4fv;
RGLSYMGLPROGRAMUNIFORMMATRIX4X3FVPROC __rglgen_glProgramUniformMatrix4x3fv;
RGLSYMGLVALIDATEPROGRAMPIPELINEPROC __rglgen_glValidateProgramPipeline;
RGLSYMGLGETPROGRAMPIPELINEINFOLOGPROC __rglgen_glGetProgramPipelineInfoLog;
RGLSYMGLBINDIMAGETEXTUREPROC __rglgen_glBindImageTexture;
RGLSYMGLGETBOOLEANI_VPROC __rglgen_glGetBooleani_v;
RGLSYMGLMEMORYBARRIERPROC __rglgen_glMemoryBarrier;
RGLSYMGLMEMORYBARRIERBYREGIONPROC __rglgen_glMemoryBarrierByRegion;
RGLSYMGLTEXSTORAGE2DMULTISAMPLEPROC __rglgen_glTexStorage2DMultisample;
RGLSYMGLGETMULTISAMPLEFVPROC __rglgen_glGetMultisamplefv;
RGLSYMGLSAMPLEMASKIPROC __rglgen_glSampleMaski;
RGLSYMGLGETTEXLEVELPARAMETERIVPROC __rglgen_glGetTexLevelParameteriv;
RGLSYMGLGETTEXLEVELPARAMETERFVPROC __rglgen_glGetTexLevelParameterfv;
RGLSYMGLBINDVERTEXBUFFERPROC __rglgen_glBindVertexBuffer;
RGLSYMGLVERTEXATTRIBFORMATPROC __rglgen_glVertexAttribFormat;
RGLSYMGLVERTEXATTRIBIFORMATPROC __rglgen_glVertexAttribIFormat;
RGLSYMGLVERTEXATTRIBBINDINGPROC __rglgen_glVertexAttribBinding;
RGLSYMGLVERTEXBINDINGDIVISORPROC __rglgen_glVertexBindingDivisor;
RGLSYMGLBLENDBARRIERPROC __rglgen_glBlendBarrier;
RGLSYMGLCOPYIMAGESUBDATAPROC __rglgen_glCopyImageSubData;
RGLSYMGLDEBUGMESSAGECONTROLPROC __rglgen_glDebugMessageControl;
RGLSYMGLDEBUGMESSAGEINSERTPROC __rglgen_glDebugMessageInsert;
RGLSYMGLDEBUGMESSAGECALLBACKPROC __rglgen_glDebugMessageCallback;
RGLSYMGLGETDEBUGMESSAGELOGPROC __rglgen_glGetDebugMessageLog;
RGLSYMGLPUSHDEBUGGROUPPROC __rglgen_glPushDebugGroup;
RGLSYMGLPOPDEBUGGROUPPROC __rglgen_glPopDebugGroup;
RGLSYMGLOBJECTLABELPROC __rglgen_glObjectLabel;
RGLSYMGLGETOBJECTLABELPROC __rglgen_glGetObjectLabel;
RGLSYMGLOBJECTPTRLABELPROC __rglgen_glObjectPtrLabel;
RGLSYMGLGETOBJECTPTRLABELPROC __rglgen_glGetObjectPtrLabel;
RGLSYMGLGETPOINTERVPROC __rglgen_glGetPointerv;
RGLSYMGLENABLEIPROC __rglgen_glEnablei;
RGLSYMGLDISABLEIPROC __rglgen_glDisablei;
RGLSYMGLBLENDEQUATIONIPROC __rglgen_glBlendEquationi;
RGLSYMGLBLENDEQUATIONSEPARATEIPROC __rglgen_glBlendEquationSeparatei;
RGLSYMGLBLENDFUNCIPROC __rglgen_glBlendFunci;
RGLSYMGLBLENDFUNCSEPARATEIPROC __rglgen_glBlendFuncSeparatei;
RGLSYMGLCOLORMASKIPROC __rglgen_glColorMaski;
RGLSYMGLISENABLEDIPROC __rglgen_glIsEnabledi;
RGLSYMGLDRAWELEMENTSBASEVERTEXPROC __rglgen_glDrawElementsBaseVertex;
RGLSYMGLDRAWRANGEELEMENTSBASEVERTEXPROC __rglgen_glDrawRangeElementsBaseVertex;
RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC __rglgen_glDrawElementsInstancedBaseVertex;
RGLSYMGLFRAMEBUFFERTEXTUREPROC __rglgen_glFramebufferTexture;
RGLSYMGLPRIMITIVEBOUNDINGBOXPROC __rglgen_glPrimitiveBoundingBox;
RGLSYMGLGETGRAPHICSRESETSTATUSPROC __rglgen_glGetGraphicsResetStatus;
RGLSYMGLREADNPIXELSPROC __rglgen_glReadnPixels;
RGLSYMGLGETNUNIFORMFVPROC __rglgen_glGetnUniformfv;
RGLSYMGLGETNUNIFORMIVPROC __rglgen_glGetnUniformiv;
RGLSYMGLGETNUNIFORMUIVPROC __rglgen_glGetnUniformuiv;
RGLSYMGLMINSAMPLESHADINGPROC __rglgen_glMinSampleShading;
RGLSYMGLPATCHPARAMETERIPROC __rglgen_glPatchParameteri;
RGLSYMGLTEXPARAMETERIIVPROC __rglgen_glTexParameterIiv;
RGLSYMGLTEXPARAMETERIUIVPROC __rglgen_glTexParameterIuiv;
RGLSYMGLGETTEXPARAMETERIIVPROC __rglgen_glGetTexParameterIiv;
RGLSYMGLGETTEXPARAMETERIUIVPROC __rglgen_glGetTexParameterIuiv;
RGLSYMGLSAMPLERPARAMETERIIVPROC __rglgen_glSamplerParameterIiv;
RGLSYMGLSAMPLERPARAMETERIUIVPROC __rglgen_glSamplerParameterIuiv;
RGLSYMGLGETSAMPLERPARAMETERIIVPROC __rglgen_glGetSamplerParameterIiv;
RGLSYMGLGETSAMPLERPARAMETERIUIVPROC __rglgen_glGetSamplerParameterIuiv;
RGLSYMGLTEXBUFFERPROC __rglgen_glTexBuffer;
RGLSYMGLTEXBUFFERRANGEPROC __rglgen_glTexBufferRange;
RGLSYMGLTEXSTORAGE3DMULTISAMPLEPROC __rglgen_glTexStorage3DMultisample;

