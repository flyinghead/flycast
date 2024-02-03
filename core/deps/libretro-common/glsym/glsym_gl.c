/* Copyright (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this libretro SDK code part (glsym).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stddef.h>

#include <glsym/glsym.h>

#define SYM(x) { "gl" #x, (void*)&(gl##x) }

const struct rglgen_sym_map rglgen_symbol_map[] = {
    SYM(CullFace),
    SYM(FrontFace),
    SYM(Hint),
    SYM(LineWidth),
    SYM(PointSize),
    SYM(PolygonMode),
    SYM(Scissor),
    SYM(TexParameterf),
    SYM(TexParameterfv),
    SYM(TexParameteri),
    SYM(TexParameteriv),
    SYM(TexImage1D),
    SYM(TexImage2D),
    SYM(DrawBuffer),
    SYM(Clear),
    SYM(ClearColor),
    SYM(ClearStencil),
    SYM(ClearDepth),
    SYM(StencilMask),
    SYM(ColorMask),
    SYM(DepthMask),
    SYM(Disable),
    SYM(Enable),
    SYM(Finish),
    SYM(Flush),
    SYM(BlendFunc),
    SYM(LogicOp),
    SYM(StencilFunc),
    SYM(StencilOp),
    SYM(DepthFunc),
    SYM(PixelStoref),
    SYM(PixelStorei),
    SYM(ReadBuffer),
    SYM(ReadPixels),
    SYM(GetBooleanv),
    SYM(GetDoublev),
    SYM(GetError),
    SYM(GetFloatv),
    SYM(GetIntegerv),
    SYM(GetString),
    SYM(GetTexImage),
    SYM(GetTexParameterfv),
    SYM(GetTexParameteriv),
    SYM(GetTexLevelParameterfv),
    SYM(GetTexLevelParameteriv),
    SYM(IsEnabled),
    SYM(DepthRange),
    SYM(Viewport),
    SYM(DrawArrays),
    SYM(DrawElements),
    SYM(GetPointerv),
    SYM(PolygonOffset),
    SYM(CopyTexImage1D),
    SYM(CopyTexImage2D),
    SYM(CopyTexSubImage1D),
    SYM(CopyTexSubImage2D),
    SYM(TexSubImage1D),
    SYM(TexSubImage2D),
    SYM(BindTexture),
    SYM(DeleteTextures),
    SYM(GenTextures),
    SYM(IsTexture),
    SYM(DrawRangeElements),
    SYM(TexImage3D),
    SYM(TexSubImage3D),
    SYM(CopyTexSubImage3D),
    SYM(ActiveTexture),
    SYM(SampleCoverage),
    SYM(CompressedTexImage3D),
    SYM(CompressedTexImage2D),
    SYM(CompressedTexImage1D),
    SYM(CompressedTexSubImage3D),
    SYM(CompressedTexSubImage2D),
    SYM(CompressedTexSubImage1D),
    SYM(GetCompressedTexImage),
    SYM(BlendFuncSeparate),
    SYM(MultiDrawArrays),
    SYM(MultiDrawElements),
    SYM(PointParameterf),
    SYM(PointParameterfv),
    SYM(PointParameteri),
    SYM(PointParameteriv),
    SYM(BlendColor),
    SYM(BlendEquation),
    SYM(GenQueries),
    SYM(DeleteQueries),
    SYM(IsQuery),
    SYM(BeginQuery),
    SYM(EndQuery),
    SYM(GetQueryiv),
    SYM(GetQueryObjectiv),
    SYM(GetQueryObjectuiv),
    SYM(BindBuffer),
    SYM(DeleteBuffers),
    SYM(GenBuffers),
    SYM(IsBuffer),
    SYM(BufferData),
    SYM(BufferSubData),
    SYM(GetBufferSubData),
    SYM(MapBuffer),
    SYM(UnmapBuffer),
    SYM(GetBufferParameteriv),
    SYM(GetBufferPointerv),
    SYM(BlendEquationSeparate),
    SYM(DrawBuffers),
    SYM(StencilOpSeparate),
    SYM(StencilFuncSeparate),
    SYM(StencilMaskSeparate),
    SYM(AttachShader),
    SYM(BindAttribLocation),
    SYM(CompileShader),
    SYM(CreateProgram),
    SYM(CreateShader),
    SYM(DeleteProgram),
    SYM(DeleteShader),
    SYM(DetachShader),
    SYM(DisableVertexAttribArray),
    SYM(EnableVertexAttribArray),
    SYM(GetActiveAttrib),
    SYM(GetActiveUniform),
    SYM(GetAttachedShaders),
    SYM(GetAttribLocation),
    SYM(GetProgramiv),
    SYM(GetProgramInfoLog),
    SYM(GetShaderiv),
    SYM(GetShaderInfoLog),
    SYM(GetShaderSource),
    SYM(GetUniformLocation),
    SYM(GetUniformfv),
    SYM(GetUniformiv),
    SYM(GetVertexAttribdv),
    SYM(GetVertexAttribfv),
    SYM(GetVertexAttribiv),
    SYM(GetVertexAttribPointerv),
    SYM(IsProgram),
    SYM(IsShader),
    SYM(LinkProgram),
    SYM(ShaderSource),
    SYM(UseProgram),
    SYM(Uniform1f),
    SYM(Uniform2f),
    SYM(Uniform3f),
    SYM(Uniform4f),
    SYM(Uniform1i),
    SYM(Uniform2i),
    SYM(Uniform3i),
    SYM(Uniform4i),
    SYM(Uniform1fv),
    SYM(Uniform2fv),
    SYM(Uniform3fv),
    SYM(Uniform4fv),
    SYM(Uniform1iv),
    SYM(Uniform2iv),
    SYM(Uniform3iv),
    SYM(Uniform4iv),
    SYM(UniformMatrix2fv),
    SYM(UniformMatrix3fv),
    SYM(UniformMatrix4fv),
    SYM(ValidateProgram),
    SYM(VertexAttrib1d),
    SYM(VertexAttrib1dv),
    SYM(VertexAttrib1f),
    SYM(VertexAttrib1fv),
    SYM(VertexAttrib1s),
    SYM(VertexAttrib1sv),
    SYM(VertexAttrib2d),
    SYM(VertexAttrib2dv),
    SYM(VertexAttrib2f),
    SYM(VertexAttrib2fv),
    SYM(VertexAttrib2s),
    SYM(VertexAttrib2sv),
    SYM(VertexAttrib3d),
    SYM(VertexAttrib3dv),
    SYM(VertexAttrib3f),
    SYM(VertexAttrib3fv),
    SYM(VertexAttrib3s),
    SYM(VertexAttrib3sv),
    SYM(VertexAttrib4Nbv),
    SYM(VertexAttrib4Niv),
    SYM(VertexAttrib4Nsv),
    SYM(VertexAttrib4Nub),
    SYM(VertexAttrib4Nubv),
    SYM(VertexAttrib4Nuiv),
    SYM(VertexAttrib4Nusv),
    SYM(VertexAttrib4bv),
    SYM(VertexAttrib4d),
    SYM(VertexAttrib4dv),
    SYM(VertexAttrib4f),
    SYM(VertexAttrib4fv),
    SYM(VertexAttrib4iv),
    SYM(VertexAttrib4s),
    SYM(VertexAttrib4sv),
    SYM(VertexAttrib4ubv),
    SYM(VertexAttrib4uiv),
    SYM(VertexAttrib4usv),
    SYM(VertexAttribPointer),
    SYM(UniformMatrix2x3fv),
    SYM(UniformMatrix3x2fv),
    SYM(UniformMatrix2x4fv),
    SYM(UniformMatrix4x2fv),
    SYM(UniformMatrix3x4fv),
    SYM(UniformMatrix4x3fv),
    SYM(ColorMaski),
    SYM(GetBooleani_v),
    SYM(GetIntegeri_v),
    SYM(Enablei),
    SYM(Disablei),
    SYM(IsEnabledi),
    SYM(BeginTransformFeedback),
    SYM(EndTransformFeedback),
    SYM(BindBufferRange),
    SYM(BindBufferBase),
    SYM(TransformFeedbackVaryings),
    SYM(GetTransformFeedbackVarying),
    SYM(ClampColor),
    SYM(BeginConditionalRender),
    SYM(EndConditionalRender),
    SYM(VertexAttribIPointer),
    SYM(GetVertexAttribIiv),
    SYM(GetVertexAttribIuiv),
    SYM(VertexAttribI1i),
    SYM(VertexAttribI2i),
    SYM(VertexAttribI3i),
    SYM(VertexAttribI4i),
    SYM(VertexAttribI1ui),
    SYM(VertexAttribI2ui),
    SYM(VertexAttribI3ui),
    SYM(VertexAttribI4ui),
    SYM(VertexAttribI1iv),
    SYM(VertexAttribI2iv),
    SYM(VertexAttribI3iv),
    SYM(VertexAttribI4iv),
    SYM(VertexAttribI1uiv),
    SYM(VertexAttribI2uiv),
    SYM(VertexAttribI3uiv),
    SYM(VertexAttribI4uiv),
    SYM(VertexAttribI4bv),
    SYM(VertexAttribI4sv),
    SYM(VertexAttribI4ubv),
    SYM(VertexAttribI4usv),
    SYM(GetUniformuiv),
    SYM(BindFragDataLocation),
    SYM(GetFragDataLocation),
    SYM(Uniform1ui),
    SYM(Uniform2ui),
    SYM(Uniform3ui),
    SYM(Uniform4ui),
    SYM(Uniform1uiv),
    SYM(Uniform2uiv),
    SYM(Uniform3uiv),
    SYM(Uniform4uiv),
    SYM(TexParameterIiv),
    SYM(TexParameterIuiv),
    SYM(GetTexParameterIiv),
    SYM(GetTexParameterIuiv),
    SYM(ClearBufferiv),
    SYM(ClearBufferuiv),
    SYM(ClearBufferfv),
    SYM(ClearBufferfi),
    SYM(GetStringi),
    SYM(IsRenderbuffer),
    SYM(BindRenderbuffer),
    SYM(DeleteRenderbuffers),
    SYM(GenRenderbuffers),
    SYM(RenderbufferStorage),
    SYM(GetRenderbufferParameteriv),
    SYM(IsFramebuffer),
    SYM(BindFramebuffer),
    SYM(DeleteFramebuffers),
    SYM(GenFramebuffers),
    SYM(CheckFramebufferStatus),
    SYM(FramebufferTexture1D),
    SYM(FramebufferTexture2D),
    SYM(FramebufferTexture3D),
    SYM(FramebufferRenderbuffer),
    SYM(GetFramebufferAttachmentParameteriv),
    SYM(GenerateMipmap),
    SYM(BlitFramebuffer),
    SYM(RenderbufferStorageMultisample),
    SYM(FramebufferTextureLayer),
    SYM(MapBufferRange),
    SYM(FlushMappedBufferRange),
    SYM(BindVertexArray),
    SYM(DeleteVertexArrays),
    SYM(GenVertexArrays),
    SYM(IsVertexArray),
    SYM(DrawArraysInstanced),
    SYM(DrawElementsInstanced),
    SYM(TexBuffer),
    SYM(PrimitiveRestartIndex),
    SYM(CopyBufferSubData),
    SYM(GetUniformIndices),
    SYM(GetActiveUniformsiv),
    SYM(GetActiveUniformName),
    SYM(GetUniformBlockIndex),
    SYM(GetActiveUniformBlockiv),
    SYM(GetActiveUniformBlockName),
    SYM(UniformBlockBinding),
    SYM(DrawElementsBaseVertex),
    SYM(DrawRangeElementsBaseVertex),
    SYM(DrawElementsInstancedBaseVertex),
    SYM(MultiDrawElementsBaseVertex),
    SYM(ProvokingVertex),
    SYM(FenceSync),
    SYM(IsSync),
    SYM(DeleteSync),
    SYM(ClientWaitSync),
    SYM(WaitSync),
    SYM(GetInteger64v),
    SYM(GetSynciv),
    SYM(GetInteger64i_v),
    SYM(GetBufferParameteri64v),
    SYM(FramebufferTexture),
    SYM(TexImage2DMultisample),
    SYM(TexImage3DMultisample),
    SYM(GetMultisamplefv),
    SYM(SampleMaski),
    SYM(BindFragDataLocationIndexed),
    SYM(GetFragDataIndex),
    SYM(GenSamplers),
    SYM(DeleteSamplers),
    SYM(IsSampler),
    SYM(BindSampler),
    SYM(SamplerParameteri),
    SYM(SamplerParameteriv),
    SYM(SamplerParameterf),
    SYM(SamplerParameterfv),
    SYM(SamplerParameterIiv),
    SYM(SamplerParameterIuiv),
    SYM(GetSamplerParameteriv),
    SYM(GetSamplerParameterIiv),
    SYM(GetSamplerParameterfv),
    SYM(GetSamplerParameterIuiv),
    SYM(QueryCounter),
    SYM(GetQueryObjecti64v),
    SYM(GetQueryObjectui64v),
    SYM(VertexAttribDivisor),
    SYM(VertexAttribP1ui),
    SYM(VertexAttribP1uiv),
    SYM(VertexAttribP2ui),
    SYM(VertexAttribP2uiv),
    SYM(VertexAttribP3ui),
    SYM(VertexAttribP3uiv),
    SYM(VertexAttribP4ui),
    SYM(VertexAttribP4uiv),
    SYM(MinSampleShading),
    SYM(BlendEquationi),
    SYM(BlendEquationSeparatei),
    SYM(BlendFunci),
    SYM(BlendFuncSeparatei),
    SYM(DrawArraysIndirect),
    SYM(DrawElementsIndirect),
    SYM(Uniform1d),
    SYM(Uniform2d),
    SYM(Uniform3d),
    SYM(Uniform4d),
    SYM(Uniform1dv),
    SYM(Uniform2dv),
    SYM(Uniform3dv),
    SYM(Uniform4dv),
    SYM(UniformMatrix2dv),
    SYM(UniformMatrix3dv),
    SYM(UniformMatrix4dv),
    SYM(UniformMatrix2x3dv),
    SYM(UniformMatrix2x4dv),
    SYM(UniformMatrix3x2dv),
    SYM(UniformMatrix3x4dv),
    SYM(UniformMatrix4x2dv),
    SYM(UniformMatrix4x3dv),
    SYM(GetUniformdv),
    SYM(GetSubroutineUniformLocation),
    SYM(GetSubroutineIndex),
    SYM(GetActiveSubroutineUniformiv),
    SYM(GetActiveSubroutineUniformName),
    SYM(GetActiveSubroutineName),
    SYM(UniformSubroutinesuiv),
    SYM(GetUniformSubroutineuiv),
    SYM(GetProgramStageiv),
    SYM(PatchParameteri),
    SYM(PatchParameterfv),
    SYM(BindTransformFeedback),
    SYM(DeleteTransformFeedbacks),
    SYM(GenTransformFeedbacks),
    SYM(IsTransformFeedback),
    SYM(PauseTransformFeedback),
    SYM(ResumeTransformFeedback),
    SYM(DrawTransformFeedback),
    SYM(DrawTransformFeedbackStream),
    SYM(BeginQueryIndexed),
    SYM(EndQueryIndexed),
    SYM(GetQueryIndexediv),
    SYM(ReleaseShaderCompiler),
    SYM(ShaderBinary),
    SYM(GetShaderPrecisionFormat),
    SYM(DepthRangef),
    SYM(ClearDepthf),
    SYM(GetProgramBinary),
    SYM(ProgramBinary),
    SYM(ProgramParameteri),
    SYM(UseProgramStages),
    SYM(ActiveShaderProgram),
    SYM(CreateShaderProgramv),
    SYM(BindProgramPipeline),
    SYM(DeleteProgramPipelines),
    SYM(GenProgramPipelines),
    SYM(IsProgramPipeline),
    SYM(GetProgramPipelineiv),
    SYM(ProgramUniform1i),
    SYM(ProgramUniform1iv),
    SYM(ProgramUniform1f),
    SYM(ProgramUniform1fv),
    SYM(ProgramUniform1d),
    SYM(ProgramUniform1dv),
    SYM(ProgramUniform1ui),
    SYM(ProgramUniform1uiv),
    SYM(ProgramUniform2i),
    SYM(ProgramUniform2iv),
    SYM(ProgramUniform2f),
    SYM(ProgramUniform2fv),
    SYM(ProgramUniform2d),
    SYM(ProgramUniform2dv),
    SYM(ProgramUniform2ui),
    SYM(ProgramUniform2uiv),
    SYM(ProgramUniform3i),
    SYM(ProgramUniform3iv),
    SYM(ProgramUniform3f),
    SYM(ProgramUniform3fv),
    SYM(ProgramUniform3d),
    SYM(ProgramUniform3dv),
    SYM(ProgramUniform3ui),
    SYM(ProgramUniform3uiv),
    SYM(ProgramUniform4i),
    SYM(ProgramUniform4iv),
    SYM(ProgramUniform4f),
    SYM(ProgramUniform4fv),
    SYM(ProgramUniform4d),
    SYM(ProgramUniform4dv),
    SYM(ProgramUniform4ui),
    SYM(ProgramUniform4uiv),
    SYM(ProgramUniformMatrix2fv),
    SYM(ProgramUniformMatrix3fv),
    SYM(ProgramUniformMatrix4fv),
    SYM(ProgramUniformMatrix2dv),
    SYM(ProgramUniformMatrix3dv),
    SYM(ProgramUniformMatrix4dv),
    SYM(ProgramUniformMatrix2x3fv),
    SYM(ProgramUniformMatrix3x2fv),
    SYM(ProgramUniformMatrix2x4fv),
    SYM(ProgramUniformMatrix4x2fv),
    SYM(ProgramUniformMatrix3x4fv),
    SYM(ProgramUniformMatrix4x3fv),
    SYM(ProgramUniformMatrix2x3dv),
    SYM(ProgramUniformMatrix3x2dv),
    SYM(ProgramUniformMatrix2x4dv),
    SYM(ProgramUniformMatrix4x2dv),
    SYM(ProgramUniformMatrix3x4dv),
    SYM(ProgramUniformMatrix4x3dv),
    SYM(ValidateProgramPipeline),
    SYM(GetProgramPipelineInfoLog),
    SYM(VertexAttribL1d),
    SYM(VertexAttribL2d),
    SYM(VertexAttribL3d),
    SYM(VertexAttribL4d),
    SYM(VertexAttribL1dv),
    SYM(VertexAttribL2dv),
    SYM(VertexAttribL3dv),
    SYM(VertexAttribL4dv),
    SYM(VertexAttribLPointer),
    SYM(GetVertexAttribLdv),
    SYM(ViewportArrayv),
    SYM(ViewportIndexedf),
    SYM(ViewportIndexedfv),
    SYM(ScissorArrayv),
    SYM(ScissorIndexed),
    SYM(ScissorIndexedv),
    SYM(DepthRangeArrayv),
    SYM(DepthRangeIndexed),
    SYM(GetFloati_v),
    SYM(GetDoublei_v),
    SYM(DrawArraysInstancedBaseInstance),
    SYM(DrawElementsInstancedBaseInstance),
    SYM(DrawElementsInstancedBaseVertexBaseInstance),
    SYM(GetInternalformativ),
    SYM(GetActiveAtomicCounterBufferiv),
    SYM(BindImageTexture),
    SYM(MemoryBarrier),
    SYM(TexStorage1D),
    SYM(TexStorage2D),
    SYM(TexStorage3D),
    SYM(DrawTransformFeedbackInstanced),
    SYM(DrawTransformFeedbackStreamInstanced),
    SYM(ClearBufferData),
    SYM(ClearBufferSubData),
    SYM(DispatchCompute),
    SYM(DispatchComputeIndirect),
    SYM(CopyImageSubData),
    SYM(FramebufferParameteri),
    SYM(GetFramebufferParameteriv),
    SYM(GetInternalformati64v),
    SYM(InvalidateTexSubImage),
    SYM(InvalidateTexImage),
    SYM(InvalidateBufferSubData),
    SYM(InvalidateBufferData),
    SYM(InvalidateFramebuffer),
    SYM(InvalidateSubFramebuffer),
    SYM(MultiDrawArraysIndirect),
    SYM(MultiDrawElementsIndirect),
    SYM(GetProgramInterfaceiv),
    SYM(GetProgramResourceIndex),
    SYM(GetProgramResourceName),
    SYM(GetProgramResourceiv),
    SYM(GetProgramResourceLocation),
    SYM(GetProgramResourceLocationIndex),
    SYM(ShaderStorageBlockBinding),
    SYM(TexBufferRange),
    SYM(TexStorage2DMultisample),
    SYM(TexStorage3DMultisample),
    SYM(TextureView),
    SYM(BindVertexBuffer),
    SYM(VertexAttribFormat),
    SYM(VertexAttribIFormat),
    SYM(VertexAttribLFormat),
    SYM(VertexAttribBinding),
    SYM(VertexBindingDivisor),
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
    SYM(BufferStorage),
    SYM(ClearTexImage),
    SYM(ClearTexSubImage),
    SYM(BindBuffersBase),
    SYM(BindBuffersRange),
    SYM(BindTextures),
    SYM(BindSamplers),
    SYM(BindImageTextures),
    SYM(BindVertexBuffers),
    SYM(ClipControl),
    SYM(CreateTransformFeedbacks),
    SYM(TransformFeedbackBufferBase),
    SYM(TransformFeedbackBufferRange),
    SYM(GetTransformFeedbackiv),
    SYM(GetTransformFeedbacki_v),
    SYM(GetTransformFeedbacki64_v),
    SYM(CreateBuffers),
    SYM(NamedBufferStorage),
    SYM(NamedBufferData),
    SYM(NamedBufferSubData),
    SYM(CopyNamedBufferSubData),
    SYM(ClearNamedBufferData),
    SYM(ClearNamedBufferSubData),
    SYM(MapNamedBuffer),
    SYM(MapNamedBufferRange),
    SYM(UnmapNamedBuffer),
    SYM(FlushMappedNamedBufferRange),
    SYM(GetNamedBufferParameteriv),
    SYM(GetNamedBufferParameteri64v),
    SYM(GetNamedBufferPointerv),
    SYM(GetNamedBufferSubData),
    SYM(CreateFramebuffers),
    SYM(NamedFramebufferRenderbuffer),
    SYM(NamedFramebufferParameteri),
    SYM(NamedFramebufferTexture),
    SYM(NamedFramebufferTextureLayer),
    SYM(NamedFramebufferDrawBuffer),
    SYM(NamedFramebufferDrawBuffers),
    SYM(NamedFramebufferReadBuffer),
    SYM(InvalidateNamedFramebufferData),
    SYM(InvalidateNamedFramebufferSubData),
    SYM(ClearNamedFramebufferiv),
    SYM(ClearNamedFramebufferuiv),
    SYM(ClearNamedFramebufferfv),
    SYM(ClearNamedFramebufferfi),
    SYM(BlitNamedFramebuffer),
    SYM(CheckNamedFramebufferStatus),
    SYM(GetNamedFramebufferParameteriv),
    SYM(GetNamedFramebufferAttachmentParameteriv),
    SYM(CreateRenderbuffers),
    SYM(NamedRenderbufferStorage),
    SYM(NamedRenderbufferStorageMultisample),
    SYM(GetNamedRenderbufferParameteriv),
    SYM(CreateTextures),
    SYM(TextureBuffer),
    SYM(TextureBufferRange),
    SYM(TextureStorage1D),
    SYM(TextureStorage2D),
    SYM(TextureStorage3D),
    SYM(TextureStorage2DMultisample),
    SYM(TextureStorage3DMultisample),
    SYM(TextureSubImage1D),
    SYM(TextureSubImage2D),
    SYM(TextureSubImage3D),
    SYM(CompressedTextureSubImage1D),
    SYM(CompressedTextureSubImage2D),
    SYM(CompressedTextureSubImage3D),
    SYM(CopyTextureSubImage1D),
    SYM(CopyTextureSubImage2D),
    SYM(CopyTextureSubImage3D),
    SYM(TextureParameterf),
    SYM(TextureParameterfv),
    SYM(TextureParameteri),
    SYM(TextureParameterIiv),
    SYM(TextureParameterIuiv),
    SYM(TextureParameteriv),
    SYM(GenerateTextureMipmap),
    SYM(BindTextureUnit),
    SYM(GetTextureImage),
    SYM(GetCompressedTextureImage),
    SYM(GetTextureLevelParameterfv),
    SYM(GetTextureLevelParameteriv),
    SYM(GetTextureParameterfv),
    SYM(GetTextureParameterIiv),
    SYM(GetTextureParameterIuiv),
    SYM(GetTextureParameteriv),
    SYM(CreateVertexArrays),
    SYM(DisableVertexArrayAttrib),
    SYM(EnableVertexArrayAttrib),
    SYM(VertexArrayElementBuffer),
    SYM(VertexArrayVertexBuffer),
    SYM(VertexArrayVertexBuffers),
    SYM(VertexArrayAttribBinding),
    SYM(VertexArrayAttribFormat),
    SYM(VertexArrayAttribIFormat),
    SYM(VertexArrayAttribLFormat),
    SYM(VertexArrayBindingDivisor),
    SYM(GetVertexArrayiv),
    SYM(GetVertexArrayIndexediv),
    SYM(GetVertexArrayIndexed64iv),
    SYM(CreateSamplers),
    SYM(CreateProgramPipelines),
    SYM(CreateQueries),
    SYM(GetQueryBufferObjecti64v),
    SYM(GetQueryBufferObjectiv),
    SYM(GetQueryBufferObjectui64v),
    SYM(GetQueryBufferObjectuiv),
    SYM(MemoryBarrierByRegion),
    SYM(GetTextureSubImage),
    SYM(GetCompressedTextureSubImage),
    SYM(GetGraphicsResetStatus),
    SYM(GetnCompressedTexImage),
    SYM(GetnTexImage),
    SYM(GetnUniformdv),
    SYM(GetnUniformfv),
    SYM(GetnUniformiv),
    SYM(GetnUniformuiv),
    SYM(ReadnPixels),
    SYM(TextureBarrier),
    SYM(SpecializeShader),
    SYM(MultiDrawArraysIndirectCount),
    SYM(MultiDrawElementsIndirectCount),
    SYM(PolygonOffsetClamp),
    SYM(PrimitiveBoundingBoxARB),
    SYM(GetTextureHandleARB),
    SYM(GetTextureSamplerHandleARB),
    SYM(MakeTextureHandleResidentARB),
    SYM(MakeTextureHandleNonResidentARB),
    SYM(GetImageHandleARB),
    SYM(MakeImageHandleResidentARB),
    SYM(MakeImageHandleNonResidentARB),
    SYM(UniformHandleui64ARB),
    SYM(UniformHandleui64vARB),
    SYM(ProgramUniformHandleui64ARB),
    SYM(ProgramUniformHandleui64vARB),
    SYM(IsTextureHandleResidentARB),
    SYM(IsImageHandleResidentARB),
    SYM(VertexAttribL1ui64ARB),
    SYM(VertexAttribL1ui64vARB),
    SYM(GetVertexAttribLui64vARB),
    SYM(DispatchComputeGroupSizeARB),
    SYM(DebugMessageControlARB),
    SYM(DebugMessageInsertARB),
    SYM(DebugMessageCallbackARB),
    SYM(GetDebugMessageLogARB),
    SYM(BlendEquationiARB),
    SYM(BlendEquationSeparateiARB),
    SYM(BlendFunciARB),
    SYM(BlendFuncSeparateiARB),
    SYM(DrawArraysInstancedARB),
    SYM(DrawElementsInstancedARB),
    SYM(ProgramParameteriARB),
    SYM(FramebufferTextureARB),
    SYM(FramebufferTextureLayerARB),
    SYM(FramebufferTextureFaceARB),
    SYM(SpecializeShaderARB),
    SYM(Uniform1i64ARB),
    SYM(Uniform2i64ARB),
    SYM(Uniform3i64ARB),
    SYM(Uniform4i64ARB),
    SYM(Uniform1i64vARB),
    SYM(Uniform2i64vARB),
    SYM(Uniform3i64vARB),
    SYM(Uniform4i64vARB),
    SYM(Uniform1ui64ARB),
    SYM(Uniform2ui64ARB),
    SYM(Uniform3ui64ARB),
    SYM(Uniform4ui64ARB),
    SYM(Uniform1ui64vARB),
    SYM(Uniform2ui64vARB),
    SYM(Uniform3ui64vARB),
    SYM(Uniform4ui64vARB),
    SYM(GetUniformi64vARB),
    SYM(GetUniformui64vARB),
    SYM(GetnUniformi64vARB),
    SYM(GetnUniformui64vARB),
    SYM(ProgramUniform1i64ARB),
    SYM(ProgramUniform2i64ARB),
    SYM(ProgramUniform3i64ARB),
    SYM(ProgramUniform4i64ARB),
    SYM(ProgramUniform1i64vARB),
    SYM(ProgramUniform2i64vARB),
    SYM(ProgramUniform3i64vARB),
    SYM(ProgramUniform4i64vARB),
    SYM(ProgramUniform1ui64ARB),
    SYM(ProgramUniform2ui64ARB),
    SYM(ProgramUniform3ui64ARB),
    SYM(ProgramUniform4ui64ARB),
    SYM(ProgramUniform1ui64vARB),
    SYM(ProgramUniform2ui64vARB),
    SYM(ProgramUniform3ui64vARB),
    SYM(ProgramUniform4ui64vARB),
    SYM(MultiDrawArraysIndirectCountARB),
    SYM(MultiDrawElementsIndirectCountARB),
    SYM(VertexAttribDivisorARB),
    SYM(MaxShaderCompilerThreadsARB),
    SYM(GetGraphicsResetStatusARB),
    SYM(GetnTexImageARB),
    SYM(ReadnPixelsARB),
    SYM(GetnCompressedTexImageARB),
    SYM(GetnUniformfvARB),
    SYM(GetnUniformivARB),
    SYM(GetnUniformuivARB),
    SYM(GetnUniformdvARB),
    SYM(FramebufferSampleLocationsfvARB),
    SYM(NamedFramebufferSampleLocationsfvARB),
    SYM(EvaluateDepthValuesARB),
    SYM(MinSampleShadingARB),
    SYM(NamedStringARB),
    SYM(DeleteNamedStringARB),
    SYM(CompileShaderIncludeARB),
    SYM(IsNamedStringARB),
    SYM(GetNamedStringARB),
    SYM(GetNamedStringivARB),
    SYM(BufferPageCommitmentARB),
    SYM(NamedBufferPageCommitmentEXT),
    SYM(NamedBufferPageCommitmentARB),
    SYM(TexPageCommitmentARB),
    SYM(TexBufferARB),
    SYM(BlendBarrierKHR),
    SYM(MaxShaderCompilerThreadsKHR),
    SYM(EGLImageTargetTexStorageEXT),
    SYM(EGLImageTargetTextureStorageEXT),
    SYM(LabelObjectEXT),
    SYM(GetObjectLabelEXT),
    SYM(InsertEventMarkerEXT),
    SYM(PushGroupMarkerEXT),
    SYM(PopGroupMarkerEXT),
    SYM(MatrixLoadfEXT),
    SYM(MatrixLoaddEXT),
    SYM(MatrixMultfEXT),
    SYM(MatrixMultdEXT),
    SYM(MatrixLoadIdentityEXT),
    SYM(MatrixRotatefEXT),
    SYM(MatrixRotatedEXT),
    SYM(MatrixScalefEXT),
    SYM(MatrixScaledEXT),
    SYM(MatrixTranslatefEXT),
    SYM(MatrixTranslatedEXT),
    SYM(MatrixFrustumEXT),
    SYM(MatrixOrthoEXT),
    SYM(MatrixPopEXT),
    SYM(MatrixPushEXT),
    SYM(ClientAttribDefaultEXT),
    SYM(PushClientAttribDefaultEXT),
    SYM(TextureParameterfEXT),
    SYM(TextureParameterfvEXT),
    SYM(TextureParameteriEXT),
    SYM(TextureParameterivEXT),
    SYM(TextureImage1DEXT),
    SYM(TextureImage2DEXT),
    SYM(TextureSubImage1DEXT),
    SYM(TextureSubImage2DEXT),
    SYM(CopyTextureImage1DEXT),
    SYM(CopyTextureImage2DEXT),
    SYM(CopyTextureSubImage1DEXT),
    SYM(CopyTextureSubImage2DEXT),
    SYM(GetTextureImageEXT),
    SYM(GetTextureParameterfvEXT),
    SYM(GetTextureParameterivEXT),
    SYM(GetTextureLevelParameterfvEXT),
    SYM(GetTextureLevelParameterivEXT),
    SYM(TextureImage3DEXT),
    SYM(TextureSubImage3DEXT),
    SYM(CopyTextureSubImage3DEXT),
    SYM(BindMultiTextureEXT),
    SYM(MultiTexCoordPointerEXT),
    SYM(MultiTexEnvfEXT),
    SYM(MultiTexEnvfvEXT),
    SYM(MultiTexEnviEXT),
    SYM(MultiTexEnvivEXT),
    SYM(MultiTexGendEXT),
    SYM(MultiTexGendvEXT),
    SYM(MultiTexGenfEXT),
    SYM(MultiTexGenfvEXT),
    SYM(MultiTexGeniEXT),
    SYM(MultiTexGenivEXT),
    SYM(GetMultiTexEnvfvEXT),
    SYM(GetMultiTexEnvivEXT),
    SYM(GetMultiTexGendvEXT),
    SYM(GetMultiTexGenfvEXT),
    SYM(GetMultiTexGenivEXT),
    SYM(MultiTexParameteriEXT),
    SYM(MultiTexParameterivEXT),
    SYM(MultiTexParameterfEXT),
    SYM(MultiTexParameterfvEXT),
    SYM(MultiTexImage1DEXT),
    SYM(MultiTexImage2DEXT),
    SYM(MultiTexSubImage1DEXT),
    SYM(MultiTexSubImage2DEXT),
    SYM(CopyMultiTexImage1DEXT),
    SYM(CopyMultiTexImage2DEXT),
    SYM(CopyMultiTexSubImage1DEXT),
    SYM(CopyMultiTexSubImage2DEXT),
    SYM(GetMultiTexImageEXT),
    SYM(GetMultiTexParameterfvEXT),
    SYM(GetMultiTexParameterivEXT),
    SYM(GetMultiTexLevelParameterfvEXT),
    SYM(GetMultiTexLevelParameterivEXT),
    SYM(MultiTexImage3DEXT),
    SYM(MultiTexSubImage3DEXT),
    SYM(CopyMultiTexSubImage3DEXT),
    SYM(EnableClientStateIndexedEXT),
    SYM(DisableClientStateIndexedEXT),
    SYM(GetFloatIndexedvEXT),
    SYM(GetDoubleIndexedvEXT),
    SYM(GetPointerIndexedvEXT),
    SYM(EnableIndexedEXT),
    SYM(DisableIndexedEXT),
    SYM(IsEnabledIndexedEXT),
    SYM(GetIntegerIndexedvEXT),
    SYM(GetBooleanIndexedvEXT),
    SYM(CompressedTextureImage3DEXT),
    SYM(CompressedTextureImage2DEXT),
    SYM(CompressedTextureImage1DEXT),
    SYM(CompressedTextureSubImage3DEXT),
    SYM(CompressedTextureSubImage2DEXT),
    SYM(CompressedTextureSubImage1DEXT),
    SYM(GetCompressedTextureImageEXT),
    SYM(CompressedMultiTexImage3DEXT),
    SYM(CompressedMultiTexImage2DEXT),
    SYM(CompressedMultiTexImage1DEXT),
    SYM(CompressedMultiTexSubImage3DEXT),
    SYM(CompressedMultiTexSubImage2DEXT),
    SYM(CompressedMultiTexSubImage1DEXT),
    SYM(GetCompressedMultiTexImageEXT),
    SYM(MatrixLoadTransposefEXT),
    SYM(MatrixLoadTransposedEXT),
    SYM(MatrixMultTransposefEXT),
    SYM(MatrixMultTransposedEXT),
    SYM(NamedBufferDataEXT),
    SYM(NamedBufferSubDataEXT),
    SYM(MapNamedBufferEXT),
    SYM(UnmapNamedBufferEXT),
    SYM(GetNamedBufferParameterivEXT),
    SYM(GetNamedBufferPointervEXT),
    SYM(GetNamedBufferSubDataEXT),
    SYM(ProgramUniform1fEXT),
    SYM(ProgramUniform2fEXT),
    SYM(ProgramUniform3fEXT),
    SYM(ProgramUniform4fEXT),
    SYM(ProgramUniform1iEXT),
    SYM(ProgramUniform2iEXT),
    SYM(ProgramUniform3iEXT),
    SYM(ProgramUniform4iEXT),
    SYM(ProgramUniform1fvEXT),
    SYM(ProgramUniform2fvEXT),
    SYM(ProgramUniform3fvEXT),
    SYM(ProgramUniform4fvEXT),
    SYM(ProgramUniform1ivEXT),
    SYM(ProgramUniform2ivEXT),
    SYM(ProgramUniform3ivEXT),
    SYM(ProgramUniform4ivEXT),
    SYM(ProgramUniformMatrix2fvEXT),
    SYM(ProgramUniformMatrix3fvEXT),
    SYM(ProgramUniformMatrix4fvEXT),
    SYM(ProgramUniformMatrix2x3fvEXT),
    SYM(ProgramUniformMatrix3x2fvEXT),
    SYM(ProgramUniformMatrix2x4fvEXT),
    SYM(ProgramUniformMatrix4x2fvEXT),
    SYM(ProgramUniformMatrix3x4fvEXT),
    SYM(ProgramUniformMatrix4x3fvEXT),
    SYM(TextureBufferEXT),
    SYM(MultiTexBufferEXT),
    SYM(TextureParameterIivEXT),
    SYM(TextureParameterIuivEXT),
    SYM(GetTextureParameterIivEXT),
    SYM(GetTextureParameterIuivEXT),
    SYM(MultiTexParameterIivEXT),
    SYM(MultiTexParameterIuivEXT),
    SYM(GetMultiTexParameterIivEXT),
    SYM(GetMultiTexParameterIuivEXT),
    SYM(ProgramUniform1uiEXT),
    SYM(ProgramUniform2uiEXT),
    SYM(ProgramUniform3uiEXT),
    SYM(ProgramUniform4uiEXT),
    SYM(ProgramUniform1uivEXT),
    SYM(ProgramUniform2uivEXT),
    SYM(ProgramUniform3uivEXT),
    SYM(ProgramUniform4uivEXT),
    SYM(NamedProgramLocalParameters4fvEXT),
    SYM(NamedProgramLocalParameterI4iEXT),
    SYM(NamedProgramLocalParameterI4ivEXT),
    SYM(NamedProgramLocalParametersI4ivEXT),
    SYM(NamedProgramLocalParameterI4uiEXT),
    SYM(NamedProgramLocalParameterI4uivEXT),
    SYM(NamedProgramLocalParametersI4uivEXT),
    SYM(GetNamedProgramLocalParameterIivEXT),
    SYM(GetNamedProgramLocalParameterIuivEXT),
    SYM(EnableClientStateiEXT),
    SYM(DisableClientStateiEXT),
    SYM(GetFloati_vEXT),
    SYM(GetDoublei_vEXT),
    SYM(GetPointeri_vEXT),
    SYM(NamedProgramStringEXT),
    SYM(NamedProgramLocalParameter4dEXT),
    SYM(NamedProgramLocalParameter4dvEXT),
    SYM(NamedProgramLocalParameter4fEXT),
    SYM(NamedProgramLocalParameter4fvEXT),
    SYM(GetNamedProgramLocalParameterdvEXT),
    SYM(GetNamedProgramLocalParameterfvEXT),
    SYM(GetNamedProgramivEXT),
    SYM(GetNamedProgramStringEXT),
    SYM(NamedRenderbufferStorageEXT),
    SYM(GetNamedRenderbufferParameterivEXT),
    SYM(NamedRenderbufferStorageMultisampleEXT),
    SYM(NamedRenderbufferStorageMultisampleCoverageEXT),
    SYM(CheckNamedFramebufferStatusEXT),
    SYM(NamedFramebufferTexture1DEXT),
    SYM(NamedFramebufferTexture2DEXT),
    SYM(NamedFramebufferTexture3DEXT),
    SYM(NamedFramebufferRenderbufferEXT),
    SYM(GetNamedFramebufferAttachmentParameterivEXT),
    SYM(GenerateTextureMipmapEXT),
    SYM(GenerateMultiTexMipmapEXT),
    SYM(FramebufferDrawBufferEXT),
    SYM(FramebufferDrawBuffersEXT),
    SYM(FramebufferReadBufferEXT),
    SYM(GetFramebufferParameterivEXT),
    SYM(NamedCopyBufferSubDataEXT),
    SYM(NamedFramebufferTextureEXT),
    SYM(NamedFramebufferTextureLayerEXT),
    SYM(NamedFramebufferTextureFaceEXT),
    SYM(TextureRenderbufferEXT),
    SYM(MultiTexRenderbufferEXT),
    SYM(VertexArrayVertexOffsetEXT),
    SYM(VertexArrayColorOffsetEXT),
    SYM(VertexArrayEdgeFlagOffsetEXT),
    SYM(VertexArrayIndexOffsetEXT),
    SYM(VertexArrayNormalOffsetEXT),
    SYM(VertexArrayTexCoordOffsetEXT),
    SYM(VertexArrayMultiTexCoordOffsetEXT),
    SYM(VertexArrayFogCoordOffsetEXT),
    SYM(VertexArraySecondaryColorOffsetEXT),
    SYM(VertexArrayVertexAttribOffsetEXT),
    SYM(VertexArrayVertexAttribIOffsetEXT),
    SYM(EnableVertexArrayEXT),
    SYM(DisableVertexArrayEXT),
    SYM(EnableVertexArrayAttribEXT),
    SYM(DisableVertexArrayAttribEXT),
    SYM(GetVertexArrayIntegervEXT),
    SYM(GetVertexArrayPointervEXT),
    SYM(GetVertexArrayIntegeri_vEXT),
    SYM(GetVertexArrayPointeri_vEXT),
    SYM(MapNamedBufferRangeEXT),
    SYM(FlushMappedNamedBufferRangeEXT),
    SYM(NamedBufferStorageEXT),
    SYM(ClearNamedBufferDataEXT),
    SYM(ClearNamedBufferSubDataEXT),
    SYM(NamedFramebufferParameteriEXT),
    SYM(GetNamedFramebufferParameterivEXT),
    SYM(ProgramUniform1dEXT),
    SYM(ProgramUniform2dEXT),
    SYM(ProgramUniform3dEXT),
    SYM(ProgramUniform4dEXT),
    SYM(ProgramUniform1dvEXT),
    SYM(ProgramUniform2dvEXT),
    SYM(ProgramUniform3dvEXT),
    SYM(ProgramUniform4dvEXT),
    SYM(ProgramUniformMatrix2dvEXT),
    SYM(ProgramUniformMatrix3dvEXT),
    SYM(ProgramUniformMatrix4dvEXT),
    SYM(ProgramUniformMatrix2x3dvEXT),
    SYM(ProgramUniformMatrix2x4dvEXT),
    SYM(ProgramUniformMatrix3x2dvEXT),
    SYM(ProgramUniformMatrix3x4dvEXT),
    SYM(ProgramUniformMatrix4x2dvEXT),
    SYM(ProgramUniformMatrix4x3dvEXT),
    SYM(TextureBufferRangeEXT),
    SYM(TextureStorage1DEXT),
    SYM(TextureStorage2DEXT),
    SYM(TextureStorage3DEXT),
    SYM(TextureStorage2DMultisampleEXT),
    SYM(TextureStorage3DMultisampleEXT),
    SYM(VertexArrayBindVertexBufferEXT),
    SYM(VertexArrayVertexAttribFormatEXT),
    SYM(VertexArrayVertexAttribIFormatEXT),
    SYM(VertexArrayVertexAttribLFormatEXT),
    SYM(VertexArrayVertexAttribBindingEXT),
    SYM(VertexArrayVertexBindingDivisorEXT),
    SYM(VertexArrayVertexAttribLOffsetEXT),
    SYM(TexturePageCommitmentEXT),
    SYM(VertexArrayVertexAttribDivisorEXT),
    SYM(DrawArraysInstancedEXT),
    SYM(DrawElementsInstancedEXT),
    SYM(PolygonOffsetClampEXT),
    SYM(RasterSamplesEXT),
    SYM(UseShaderProgramEXT),
    SYM(ActiveProgramEXT),
    SYM(CreateShaderProgramEXT),
    SYM(FramebufferFetchBarrierEXT),
    SYM(WindowRectanglesEXT),
    SYM(MultiDrawArraysIndirectBindlessNV),
    SYM(MultiDrawElementsIndirectBindlessNV),
    SYM(MultiDrawArraysIndirectBindlessCountNV),
    SYM(MultiDrawElementsIndirectBindlessCountNV),
    SYM(GetTextureHandleNV),
    SYM(GetTextureSamplerHandleNV),
    SYM(MakeTextureHandleResidentNV),
    SYM(MakeTextureHandleNonResidentNV),
    SYM(GetImageHandleNV),
    SYM(MakeImageHandleResidentNV),
    SYM(MakeImageHandleNonResidentNV),
    SYM(UniformHandleui64NV),
    SYM(UniformHandleui64vNV),
    SYM(ProgramUniformHandleui64NV),
    SYM(ProgramUniformHandleui64vNV),
    SYM(IsTextureHandleResidentNV),
    SYM(IsImageHandleResidentNV),
    SYM(BlendParameteriNV),
    SYM(BlendBarrierNV),
    SYM(ViewportPositionWScaleNV),
    SYM(CreateStatesNV),
    SYM(DeleteStatesNV),
    SYM(IsStateNV),
    SYM(StateCaptureNV),
    SYM(GetCommandHeaderNV),
    SYM(GetStageIndexNV),
    SYM(DrawCommandsNV),
    SYM(DrawCommandsAddressNV),
    SYM(DrawCommandsStatesNV),
    SYM(DrawCommandsStatesAddressNV),
    SYM(CreateCommandListsNV),
    SYM(DeleteCommandListsNV),
    SYM(IsCommandListNV),
    SYM(ListDrawCommandsStatesClientNV),
    SYM(CommandListSegmentsNV),
    SYM(CompileCommandListNV),
    SYM(CallCommandListNV),
    SYM(BeginConditionalRenderNV),
    SYM(EndConditionalRenderNV),
    SYM(SubpixelPrecisionBiasNV),
    SYM(ConservativeRasterParameterfNV),
    SYM(ConservativeRasterParameteriNV),
    SYM(DrawVkImageNV),
    SYM(WaitVkSemaphoreNV),
    SYM(SignalVkSemaphoreNV),
    SYM(SignalVkFenceNV),
    SYM(FragmentCoverageColorNV),
    SYM(CoverageModulationTableNV),
    SYM(GetCoverageModulationTableNV),
    SYM(CoverageModulationNV),
    SYM(RenderbufferStorageMultisampleCoverageNV),
    SYM(Uniform1i64NV),
    SYM(Uniform2i64NV),
    SYM(Uniform3i64NV),
    SYM(Uniform4i64NV),
    SYM(Uniform1i64vNV),
    SYM(Uniform2i64vNV),
    SYM(Uniform3i64vNV),
    SYM(Uniform4i64vNV),
    SYM(Uniform1ui64NV),
    SYM(Uniform2ui64NV),
    SYM(Uniform3ui64NV),
    SYM(Uniform4ui64NV),
    SYM(Uniform1ui64vNV),
    SYM(Uniform2ui64vNV),
    SYM(Uniform3ui64vNV),
    SYM(Uniform4ui64vNV),
    SYM(GetUniformi64vNV),
    SYM(ProgramUniform1i64NV),
    SYM(ProgramUniform2i64NV),
    SYM(ProgramUniform3i64NV),
    SYM(ProgramUniform4i64NV),
    SYM(ProgramUniform1i64vNV),
    SYM(ProgramUniform2i64vNV),
    SYM(ProgramUniform3i64vNV),
    SYM(ProgramUniform4i64vNV),
    SYM(ProgramUniform1ui64NV),
    SYM(ProgramUniform2ui64NV),
    SYM(ProgramUniform3ui64NV),
    SYM(ProgramUniform4ui64NV),
    SYM(ProgramUniform1ui64vNV),
    SYM(ProgramUniform2ui64vNV),
    SYM(ProgramUniform3ui64vNV),
    SYM(ProgramUniform4ui64vNV),
    SYM(GetInternalformatSampleivNV),
    SYM(GetMemoryObjectDetachedResourcesuivNV),
    SYM(ResetMemoryObjectParameterNV),
    SYM(TexAttachMemoryNV),
    SYM(BufferAttachMemoryNV),
    SYM(TextureAttachMemoryNV),
    SYM(NamedBufferAttachMemoryNV),
    SYM(DrawMeshTasksNV),
    SYM(DrawMeshTasksIndirectNV),
    SYM(MultiDrawMeshTasksIndirectNV),
    SYM(MultiDrawMeshTasksIndirectCountNV),
    SYM(GenPathsNV),
    SYM(DeletePathsNV),
    SYM(IsPathNV),
    SYM(PathCommandsNV),
    SYM(PathCoordsNV),
    SYM(PathSubCommandsNV),
    SYM(PathSubCoordsNV),
    SYM(PathStringNV),
    SYM(PathGlyphsNV),
    SYM(PathGlyphRangeNV),
    SYM(WeightPathsNV),
    SYM(CopyPathNV),
    SYM(InterpolatePathsNV),
    SYM(TransformPathNV),
    SYM(PathParameterivNV),
    SYM(PathParameteriNV),
    SYM(PathParameterfvNV),
    SYM(PathParameterfNV),
    SYM(PathDashArrayNV),
    SYM(PathStencilFuncNV),
    SYM(PathStencilDepthOffsetNV),
    SYM(StencilFillPathNV),
    SYM(StencilStrokePathNV),
    SYM(StencilFillPathInstancedNV),
    SYM(StencilStrokePathInstancedNV),
    SYM(PathCoverDepthFuncNV),
    SYM(CoverFillPathNV),
    SYM(CoverStrokePathNV),
    SYM(CoverFillPathInstancedNV),
    SYM(CoverStrokePathInstancedNV),
    SYM(GetPathParameterivNV),
    SYM(GetPathParameterfvNV),
    SYM(GetPathCommandsNV),
    SYM(GetPathCoordsNV),
    SYM(GetPathDashArrayNV),
    SYM(GetPathMetricsNV),
    SYM(GetPathMetricRangeNV),
    SYM(GetPathSpacingNV),
    SYM(IsPointInFillPathNV),
    SYM(IsPointInStrokePathNV),
    SYM(GetPathLengthNV),
    SYM(PointAlongPathNV),
    SYM(MatrixLoad3x2fNV),
    SYM(MatrixLoad3x3fNV),
    SYM(MatrixLoadTranspose3x3fNV),
    SYM(MatrixMult3x2fNV),
    SYM(MatrixMult3x3fNV),
    SYM(MatrixMultTranspose3x3fNV),
    SYM(StencilThenCoverFillPathNV),
    SYM(StencilThenCoverStrokePathNV),
    SYM(StencilThenCoverFillPathInstancedNV),
    SYM(StencilThenCoverStrokePathInstancedNV),
    SYM(PathGlyphIndexRangeNV),
    SYM(PathGlyphIndexArrayNV),
    SYM(PathMemoryGlyphIndexArrayNV),
    SYM(ProgramPathFragmentInputGenNV),
    SYM(GetProgramResourcefvNV),
    SYM(FramebufferSampleLocationsfvNV),
    SYM(NamedFramebufferSampleLocationsfvNV),
    SYM(ResolveDepthValuesNV),
    SYM(ScissorExclusiveNV),
    SYM(ScissorExclusiveArrayvNV),
    SYM(MakeBufferResidentNV),
    SYM(MakeBufferNonResidentNV),
    SYM(IsBufferResidentNV),
    SYM(MakeNamedBufferResidentNV),
    SYM(MakeNamedBufferNonResidentNV),
    SYM(IsNamedBufferResidentNV),
    SYM(GetBufferParameterui64vNV),
    SYM(GetNamedBufferParameterui64vNV),
    SYM(GetIntegerui64vNV),
    SYM(Uniformui64NV),
    SYM(Uniformui64vNV),
    SYM(GetUniformui64vNV),
    SYM(ProgramUniformui64NV),
    SYM(ProgramUniformui64vNV),
    SYM(BindShadingRateImageNV),
    SYM(GetShadingRateImagePaletteNV),
    SYM(GetShadingRateSampleLocationivNV),
    SYM(ShadingRateImageBarrierNV),
    SYM(ShadingRateImagePaletteNV),
    SYM(ShadingRateSampleOrderNV),
    SYM(ShadingRateSampleOrderCustomNV),
    SYM(TextureBarrierNV),
    SYM(VertexAttribL1i64NV),
    SYM(VertexAttribL2i64NV),
    SYM(VertexAttribL3i64NV),
    SYM(VertexAttribL4i64NV),
    SYM(VertexAttribL1i64vNV),
    SYM(VertexAttribL2i64vNV),
    SYM(VertexAttribL3i64vNV),
    SYM(VertexAttribL4i64vNV),
    SYM(VertexAttribL1ui64NV),
    SYM(VertexAttribL2ui64NV),
    SYM(VertexAttribL3ui64NV),
    SYM(VertexAttribL4ui64NV),
    SYM(VertexAttribL1ui64vNV),
    SYM(VertexAttribL2ui64vNV),
    SYM(VertexAttribL3ui64vNV),
    SYM(VertexAttribL4ui64vNV),
    SYM(GetVertexAttribLi64vNV),
    SYM(GetVertexAttribLui64vNV),
    SYM(VertexAttribLFormatNV),
    SYM(BufferAddressRangeNV),
    SYM(VertexFormatNV),
    SYM(NormalFormatNV),
    SYM(ColorFormatNV),
    SYM(IndexFormatNV),
    SYM(TexCoordFormatNV),
    SYM(EdgeFlagFormatNV),
    SYM(SecondaryColorFormatNV),
    SYM(FogCoordFormatNV),
    SYM(VertexAttribFormatNV),
    SYM(VertexAttribIFormatNV),
    SYM(GetIntegerui64i_vNV),
    SYM(ViewportSwizzleNV),
    SYM(FramebufferTextureMultiviewOVR),
#ifndef __APPLE__
    SYM(CreateSyncFromCLeventARB),
    SYM(GetVkProcAddrNV),
#endif

    { NULL, NULL },
};
RGLSYMGLCULLFACEPROC __rglgen_glCullFace;
RGLSYMGLFRONTFACEPROC __rglgen_glFrontFace;
RGLSYMGLHINTPROC __rglgen_glHint;
RGLSYMGLLINEWIDTHPROC __rglgen_glLineWidth;
RGLSYMGLPOINTSIZEPROC __rglgen_glPointSize;
RGLSYMGLPOLYGONMODEPROC __rglgen_glPolygonMode;
RGLSYMGLSCISSORPROC __rglgen_glScissor;
RGLSYMGLTEXPARAMETERFPROC __rglgen_glTexParameterf;
RGLSYMGLTEXPARAMETERFVPROC __rglgen_glTexParameterfv;
RGLSYMGLTEXPARAMETERIPROC __rglgen_glTexParameteri;
RGLSYMGLTEXPARAMETERIVPROC __rglgen_glTexParameteriv;
RGLSYMGLTEXIMAGE1DPROC __rglgen_glTexImage1D;
RGLSYMGLTEXIMAGE2DPROC __rglgen_glTexImage2D;
RGLSYMGLDRAWBUFFERPROC __rglgen_glDrawBuffer;
RGLSYMGLCLEARPROC __rglgen_glClear;
RGLSYMGLCLEARCOLORPROC __rglgen_glClearColor;
RGLSYMGLCLEARSTENCILPROC __rglgen_glClearStencil;
RGLSYMGLCLEARDEPTHPROC __rglgen_glClearDepth;
RGLSYMGLSTENCILMASKPROC __rglgen_glStencilMask;
RGLSYMGLCOLORMASKPROC __rglgen_glColorMask;
RGLSYMGLDEPTHMASKPROC __rglgen_glDepthMask;
RGLSYMGLDISABLEPROC __rglgen_glDisable;
RGLSYMGLENABLEPROC __rglgen_glEnable;
RGLSYMGLFINISHPROC __rglgen_glFinish;
RGLSYMGLFLUSHPROC __rglgen_glFlush;
RGLSYMGLBLENDFUNCPROC __rglgen_glBlendFunc;
RGLSYMGLLOGICOPPROC __rglgen_glLogicOp;
RGLSYMGLSTENCILFUNCPROC __rglgen_glStencilFunc;
RGLSYMGLSTENCILOPPROC __rglgen_glStencilOp;
RGLSYMGLDEPTHFUNCPROC __rglgen_glDepthFunc;
RGLSYMGLPIXELSTOREFPROC __rglgen_glPixelStoref;
RGLSYMGLPIXELSTOREIPROC __rglgen_glPixelStorei;
RGLSYMGLREADBUFFERPROC __rglgen_glReadBuffer;
RGLSYMGLREADPIXELSPROC __rglgen_glReadPixels;
RGLSYMGLGETBOOLEANVPROC __rglgen_glGetBooleanv;
RGLSYMGLGETDOUBLEVPROC __rglgen_glGetDoublev;
RGLSYMGLGETERRORPROC __rglgen_glGetError;
RGLSYMGLGETFLOATVPROC __rglgen_glGetFloatv;
RGLSYMGLGETINTEGERVPROC __rglgen_glGetIntegerv;
RGLSYMGLGETSTRINGPROC __rglgen_glGetString;
RGLSYMGLGETTEXIMAGEPROC __rglgen_glGetTexImage;
RGLSYMGLGETTEXPARAMETERFVPROC __rglgen_glGetTexParameterfv;
RGLSYMGLGETTEXPARAMETERIVPROC __rglgen_glGetTexParameteriv;
RGLSYMGLGETTEXLEVELPARAMETERFVPROC __rglgen_glGetTexLevelParameterfv;
RGLSYMGLGETTEXLEVELPARAMETERIVPROC __rglgen_glGetTexLevelParameteriv;
RGLSYMGLISENABLEDPROC __rglgen_glIsEnabled;
RGLSYMGLDEPTHRANGEPROC __rglgen_glDepthRange;
RGLSYMGLVIEWPORTPROC __rglgen_glViewport;
RGLSYMGLDRAWARRAYSPROC __rglgen_glDrawArrays;
RGLSYMGLDRAWELEMENTSPROC __rglgen_glDrawElements;
RGLSYMGLGETPOINTERVPROC __rglgen_glGetPointerv;
RGLSYMGLPOLYGONOFFSETPROC __rglgen_glPolygonOffset;
RGLSYMGLCOPYTEXIMAGE1DPROC __rglgen_glCopyTexImage1D;
RGLSYMGLCOPYTEXIMAGE2DPROC __rglgen_glCopyTexImage2D;
RGLSYMGLCOPYTEXSUBIMAGE1DPROC __rglgen_glCopyTexSubImage1D;
RGLSYMGLCOPYTEXSUBIMAGE2DPROC __rglgen_glCopyTexSubImage2D;
RGLSYMGLTEXSUBIMAGE1DPROC __rglgen_glTexSubImage1D;
RGLSYMGLTEXSUBIMAGE2DPROC __rglgen_glTexSubImage2D;
RGLSYMGLBINDTEXTUREPROC __rglgen_glBindTexture;
RGLSYMGLDELETETEXTURESPROC __rglgen_glDeleteTextures;
RGLSYMGLGENTEXTURESPROC __rglgen_glGenTextures;
RGLSYMGLISTEXTUREPROC __rglgen_glIsTexture;
RGLSYMGLDRAWRANGEELEMENTSPROC __rglgen_glDrawRangeElements;
RGLSYMGLTEXIMAGE3DPROC __rglgen_glTexImage3D;
RGLSYMGLTEXSUBIMAGE3DPROC __rglgen_glTexSubImage3D;
RGLSYMGLCOPYTEXSUBIMAGE3DPROC __rglgen_glCopyTexSubImage3D;
RGLSYMGLACTIVETEXTUREPROC __rglgen_glActiveTexture;
RGLSYMGLSAMPLECOVERAGEPROC __rglgen_glSampleCoverage;
RGLSYMGLCOMPRESSEDTEXIMAGE3DPROC __rglgen_glCompressedTexImage3D;
RGLSYMGLCOMPRESSEDTEXIMAGE2DPROC __rglgen_glCompressedTexImage2D;
RGLSYMGLCOMPRESSEDTEXIMAGE1DPROC __rglgen_glCompressedTexImage1D;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DPROC __rglgen_glCompressedTexSubImage3D;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE2DPROC __rglgen_glCompressedTexSubImage2D;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE1DPROC __rglgen_glCompressedTexSubImage1D;
RGLSYMGLGETCOMPRESSEDTEXIMAGEPROC __rglgen_glGetCompressedTexImage;
RGLSYMGLBLENDFUNCSEPARATEPROC __rglgen_glBlendFuncSeparate;
RGLSYMGLMULTIDRAWARRAYSPROC __rglgen_glMultiDrawArrays;
RGLSYMGLMULTIDRAWELEMENTSPROC __rglgen_glMultiDrawElements;
RGLSYMGLPOINTPARAMETERFPROC __rglgen_glPointParameterf;
RGLSYMGLPOINTPARAMETERFVPROC __rglgen_glPointParameterfv;
RGLSYMGLPOINTPARAMETERIPROC __rglgen_glPointParameteri;
RGLSYMGLPOINTPARAMETERIVPROC __rglgen_glPointParameteriv;
RGLSYMGLBLENDCOLORPROC __rglgen_glBlendColor;
RGLSYMGLBLENDEQUATIONPROC __rglgen_glBlendEquation;
RGLSYMGLGENQUERIESPROC __rglgen_glGenQueries;
RGLSYMGLDELETEQUERIESPROC __rglgen_glDeleteQueries;
RGLSYMGLISQUERYPROC __rglgen_glIsQuery;
RGLSYMGLBEGINQUERYPROC __rglgen_glBeginQuery;
RGLSYMGLENDQUERYPROC __rglgen_glEndQuery;
RGLSYMGLGETQUERYIVPROC __rglgen_glGetQueryiv;
RGLSYMGLGETQUERYOBJECTIVPROC __rglgen_glGetQueryObjectiv;
RGLSYMGLGETQUERYOBJECTUIVPROC __rglgen_glGetQueryObjectuiv;
RGLSYMGLBINDBUFFERPROC __rglgen_glBindBuffer;
RGLSYMGLDELETEBUFFERSPROC __rglgen_glDeleteBuffers;
RGLSYMGLGENBUFFERSPROC __rglgen_glGenBuffers;
RGLSYMGLISBUFFERPROC __rglgen_glIsBuffer;
RGLSYMGLBUFFERDATAPROC __rglgen_glBufferData;
RGLSYMGLBUFFERSUBDATAPROC __rglgen_glBufferSubData;
RGLSYMGLGETBUFFERSUBDATAPROC __rglgen_glGetBufferSubData;
RGLSYMGLMAPBUFFERPROC __rglgen_glMapBuffer;
RGLSYMGLUNMAPBUFFERPROC __rglgen_glUnmapBuffer;
RGLSYMGLGETBUFFERPARAMETERIVPROC __rglgen_glGetBufferParameteriv;
RGLSYMGLGETBUFFERPOINTERVPROC __rglgen_glGetBufferPointerv;
RGLSYMGLBLENDEQUATIONSEPARATEPROC __rglgen_glBlendEquationSeparate;
RGLSYMGLDRAWBUFFERSPROC __rglgen_glDrawBuffers;
RGLSYMGLSTENCILOPSEPARATEPROC __rglgen_glStencilOpSeparate;
RGLSYMGLSTENCILFUNCSEPARATEPROC __rglgen_glStencilFuncSeparate;
RGLSYMGLSTENCILMASKSEPARATEPROC __rglgen_glStencilMaskSeparate;
RGLSYMGLATTACHSHADERPROC __rglgen_glAttachShader;
RGLSYMGLBINDATTRIBLOCATIONPROC __rglgen_glBindAttribLocation;
RGLSYMGLCOMPILESHADERPROC __rglgen_glCompileShader;
RGLSYMGLCREATEPROGRAMPROC __rglgen_glCreateProgram;
RGLSYMGLCREATESHADERPROC __rglgen_glCreateShader;
RGLSYMGLDELETEPROGRAMPROC __rglgen_glDeleteProgram;
RGLSYMGLDELETESHADERPROC __rglgen_glDeleteShader;
RGLSYMGLDETACHSHADERPROC __rglgen_glDetachShader;
RGLSYMGLDISABLEVERTEXATTRIBARRAYPROC __rglgen_glDisableVertexAttribArray;
RGLSYMGLENABLEVERTEXATTRIBARRAYPROC __rglgen_glEnableVertexAttribArray;
RGLSYMGLGETACTIVEATTRIBPROC __rglgen_glGetActiveAttrib;
RGLSYMGLGETACTIVEUNIFORMPROC __rglgen_glGetActiveUniform;
RGLSYMGLGETATTACHEDSHADERSPROC __rglgen_glGetAttachedShaders;
RGLSYMGLGETATTRIBLOCATIONPROC __rglgen_glGetAttribLocation;
RGLSYMGLGETPROGRAMIVPROC __rglgen_glGetProgramiv;
RGLSYMGLGETPROGRAMINFOLOGPROC __rglgen_glGetProgramInfoLog;
RGLSYMGLGETSHADERIVPROC __rglgen_glGetShaderiv;
RGLSYMGLGETSHADERINFOLOGPROC __rglgen_glGetShaderInfoLog;
RGLSYMGLGETSHADERSOURCEPROC __rglgen_glGetShaderSource;
RGLSYMGLGETUNIFORMLOCATIONPROC __rglgen_glGetUniformLocation;
RGLSYMGLGETUNIFORMFVPROC __rglgen_glGetUniformfv;
RGLSYMGLGETUNIFORMIVPROC __rglgen_glGetUniformiv;
RGLSYMGLGETVERTEXATTRIBDVPROC __rglgen_glGetVertexAttribdv;
RGLSYMGLGETVERTEXATTRIBFVPROC __rglgen_glGetVertexAttribfv;
RGLSYMGLGETVERTEXATTRIBIVPROC __rglgen_glGetVertexAttribiv;
RGLSYMGLGETVERTEXATTRIBPOINTERVPROC __rglgen_glGetVertexAttribPointerv;
RGLSYMGLISPROGRAMPROC __rglgen_glIsProgram;
RGLSYMGLISSHADERPROC __rglgen_glIsShader;
RGLSYMGLLINKPROGRAMPROC __rglgen_glLinkProgram;
RGLSYMGLSHADERSOURCEPROC __rglgen_glShaderSource;
RGLSYMGLUSEPROGRAMPROC __rglgen_glUseProgram;
RGLSYMGLUNIFORM1FPROC __rglgen_glUniform1f;
RGLSYMGLUNIFORM2FPROC __rglgen_glUniform2f;
RGLSYMGLUNIFORM3FPROC __rglgen_glUniform3f;
RGLSYMGLUNIFORM4FPROC __rglgen_glUniform4f;
RGLSYMGLUNIFORM1IPROC __rglgen_glUniform1i;
RGLSYMGLUNIFORM2IPROC __rglgen_glUniform2i;
RGLSYMGLUNIFORM3IPROC __rglgen_glUniform3i;
RGLSYMGLUNIFORM4IPROC __rglgen_glUniform4i;
RGLSYMGLUNIFORM1FVPROC __rglgen_glUniform1fv;
RGLSYMGLUNIFORM2FVPROC __rglgen_glUniform2fv;
RGLSYMGLUNIFORM3FVPROC __rglgen_glUniform3fv;
RGLSYMGLUNIFORM4FVPROC __rglgen_glUniform4fv;
RGLSYMGLUNIFORM1IVPROC __rglgen_glUniform1iv;
RGLSYMGLUNIFORM2IVPROC __rglgen_glUniform2iv;
RGLSYMGLUNIFORM3IVPROC __rglgen_glUniform3iv;
RGLSYMGLUNIFORM4IVPROC __rglgen_glUniform4iv;
RGLSYMGLUNIFORMMATRIX2FVPROC __rglgen_glUniformMatrix2fv;
RGLSYMGLUNIFORMMATRIX3FVPROC __rglgen_glUniformMatrix3fv;
RGLSYMGLUNIFORMMATRIX4FVPROC __rglgen_glUniformMatrix4fv;
RGLSYMGLVALIDATEPROGRAMPROC __rglgen_glValidateProgram;
RGLSYMGLVERTEXATTRIB1DPROC __rglgen_glVertexAttrib1d;
RGLSYMGLVERTEXATTRIB1DVPROC __rglgen_glVertexAttrib1dv;
RGLSYMGLVERTEXATTRIB1FPROC __rglgen_glVertexAttrib1f;
RGLSYMGLVERTEXATTRIB1FVPROC __rglgen_glVertexAttrib1fv;
RGLSYMGLVERTEXATTRIB1SPROC __rglgen_glVertexAttrib1s;
RGLSYMGLVERTEXATTRIB1SVPROC __rglgen_glVertexAttrib1sv;
RGLSYMGLVERTEXATTRIB2DPROC __rglgen_glVertexAttrib2d;
RGLSYMGLVERTEXATTRIB2DVPROC __rglgen_glVertexAttrib2dv;
RGLSYMGLVERTEXATTRIB2FPROC __rglgen_glVertexAttrib2f;
RGLSYMGLVERTEXATTRIB2FVPROC __rglgen_glVertexAttrib2fv;
RGLSYMGLVERTEXATTRIB2SPROC __rglgen_glVertexAttrib2s;
RGLSYMGLVERTEXATTRIB2SVPROC __rglgen_glVertexAttrib2sv;
RGLSYMGLVERTEXATTRIB3DPROC __rglgen_glVertexAttrib3d;
RGLSYMGLVERTEXATTRIB3DVPROC __rglgen_glVertexAttrib3dv;
RGLSYMGLVERTEXATTRIB3FPROC __rglgen_glVertexAttrib3f;
RGLSYMGLVERTEXATTRIB3FVPROC __rglgen_glVertexAttrib3fv;
RGLSYMGLVERTEXATTRIB3SPROC __rglgen_glVertexAttrib3s;
RGLSYMGLVERTEXATTRIB3SVPROC __rglgen_glVertexAttrib3sv;
RGLSYMGLVERTEXATTRIB4NBVPROC __rglgen_glVertexAttrib4Nbv;
RGLSYMGLVERTEXATTRIB4NIVPROC __rglgen_glVertexAttrib4Niv;
RGLSYMGLVERTEXATTRIB4NSVPROC __rglgen_glVertexAttrib4Nsv;
RGLSYMGLVERTEXATTRIB4NUBPROC __rglgen_glVertexAttrib4Nub;
RGLSYMGLVERTEXATTRIB4NUBVPROC __rglgen_glVertexAttrib4Nubv;
RGLSYMGLVERTEXATTRIB4NUIVPROC __rglgen_glVertexAttrib4Nuiv;
RGLSYMGLVERTEXATTRIB4NUSVPROC __rglgen_glVertexAttrib4Nusv;
RGLSYMGLVERTEXATTRIB4BVPROC __rglgen_glVertexAttrib4bv;
RGLSYMGLVERTEXATTRIB4DPROC __rglgen_glVertexAttrib4d;
RGLSYMGLVERTEXATTRIB4DVPROC __rglgen_glVertexAttrib4dv;
RGLSYMGLVERTEXATTRIB4FPROC __rglgen_glVertexAttrib4f;
RGLSYMGLVERTEXATTRIB4FVPROC __rglgen_glVertexAttrib4fv;
RGLSYMGLVERTEXATTRIB4IVPROC __rglgen_glVertexAttrib4iv;
RGLSYMGLVERTEXATTRIB4SPROC __rglgen_glVertexAttrib4s;
RGLSYMGLVERTEXATTRIB4SVPROC __rglgen_glVertexAttrib4sv;
RGLSYMGLVERTEXATTRIB4UBVPROC __rglgen_glVertexAttrib4ubv;
RGLSYMGLVERTEXATTRIB4UIVPROC __rglgen_glVertexAttrib4uiv;
RGLSYMGLVERTEXATTRIB4USVPROC __rglgen_glVertexAttrib4usv;
RGLSYMGLVERTEXATTRIBPOINTERPROC __rglgen_glVertexAttribPointer;
RGLSYMGLUNIFORMMATRIX2X3FVPROC __rglgen_glUniformMatrix2x3fv;
RGLSYMGLUNIFORMMATRIX3X2FVPROC __rglgen_glUniformMatrix3x2fv;
RGLSYMGLUNIFORMMATRIX2X4FVPROC __rglgen_glUniformMatrix2x4fv;
RGLSYMGLUNIFORMMATRIX4X2FVPROC __rglgen_glUniformMatrix4x2fv;
RGLSYMGLUNIFORMMATRIX3X4FVPROC __rglgen_glUniformMatrix3x4fv;
RGLSYMGLUNIFORMMATRIX4X3FVPROC __rglgen_glUniformMatrix4x3fv;
RGLSYMGLCOLORMASKIPROC __rglgen_glColorMaski;
RGLSYMGLGETBOOLEANI_VPROC __rglgen_glGetBooleani_v;
RGLSYMGLGETINTEGERI_VPROC __rglgen_glGetIntegeri_v;
RGLSYMGLENABLEIPROC __rglgen_glEnablei;
RGLSYMGLDISABLEIPROC __rglgen_glDisablei;
RGLSYMGLISENABLEDIPROC __rglgen_glIsEnabledi;
RGLSYMGLBEGINTRANSFORMFEEDBACKPROC __rglgen_glBeginTransformFeedback;
RGLSYMGLENDTRANSFORMFEEDBACKPROC __rglgen_glEndTransformFeedback;
RGLSYMGLBINDBUFFERRANGEPROC __rglgen_glBindBufferRange;
RGLSYMGLBINDBUFFERBASEPROC __rglgen_glBindBufferBase;
RGLSYMGLTRANSFORMFEEDBACKVARYINGSPROC __rglgen_glTransformFeedbackVaryings;
RGLSYMGLGETTRANSFORMFEEDBACKVARYINGPROC __rglgen_glGetTransformFeedbackVarying;
RGLSYMGLCLAMPCOLORPROC __rglgen_glClampColor;
RGLSYMGLBEGINCONDITIONALRENDERPROC __rglgen_glBeginConditionalRender;
RGLSYMGLENDCONDITIONALRENDERPROC __rglgen_glEndConditionalRender;
RGLSYMGLVERTEXATTRIBIPOINTERPROC __rglgen_glVertexAttribIPointer;
RGLSYMGLGETVERTEXATTRIBIIVPROC __rglgen_glGetVertexAttribIiv;
RGLSYMGLGETVERTEXATTRIBIUIVPROC __rglgen_glGetVertexAttribIuiv;
RGLSYMGLVERTEXATTRIBI1IPROC __rglgen_glVertexAttribI1i;
RGLSYMGLVERTEXATTRIBI2IPROC __rglgen_glVertexAttribI2i;
RGLSYMGLVERTEXATTRIBI3IPROC __rglgen_glVertexAttribI3i;
RGLSYMGLVERTEXATTRIBI4IPROC __rglgen_glVertexAttribI4i;
RGLSYMGLVERTEXATTRIBI1UIPROC __rglgen_glVertexAttribI1ui;
RGLSYMGLVERTEXATTRIBI2UIPROC __rglgen_glVertexAttribI2ui;
RGLSYMGLVERTEXATTRIBI3UIPROC __rglgen_glVertexAttribI3ui;
RGLSYMGLVERTEXATTRIBI4UIPROC __rglgen_glVertexAttribI4ui;
RGLSYMGLVERTEXATTRIBI1IVPROC __rglgen_glVertexAttribI1iv;
RGLSYMGLVERTEXATTRIBI2IVPROC __rglgen_glVertexAttribI2iv;
RGLSYMGLVERTEXATTRIBI3IVPROC __rglgen_glVertexAttribI3iv;
RGLSYMGLVERTEXATTRIBI4IVPROC __rglgen_glVertexAttribI4iv;
RGLSYMGLVERTEXATTRIBI1UIVPROC __rglgen_glVertexAttribI1uiv;
RGLSYMGLVERTEXATTRIBI2UIVPROC __rglgen_glVertexAttribI2uiv;
RGLSYMGLVERTEXATTRIBI3UIVPROC __rglgen_glVertexAttribI3uiv;
RGLSYMGLVERTEXATTRIBI4UIVPROC __rglgen_glVertexAttribI4uiv;
RGLSYMGLVERTEXATTRIBI4BVPROC __rglgen_glVertexAttribI4bv;
RGLSYMGLVERTEXATTRIBI4SVPROC __rglgen_glVertexAttribI4sv;
RGLSYMGLVERTEXATTRIBI4UBVPROC __rglgen_glVertexAttribI4ubv;
RGLSYMGLVERTEXATTRIBI4USVPROC __rglgen_glVertexAttribI4usv;
RGLSYMGLGETUNIFORMUIVPROC __rglgen_glGetUniformuiv;
RGLSYMGLBINDFRAGDATALOCATIONPROC __rglgen_glBindFragDataLocation;
RGLSYMGLGETFRAGDATALOCATIONPROC __rglgen_glGetFragDataLocation;
RGLSYMGLUNIFORM1UIPROC __rglgen_glUniform1ui;
RGLSYMGLUNIFORM2UIPROC __rglgen_glUniform2ui;
RGLSYMGLUNIFORM3UIPROC __rglgen_glUniform3ui;
RGLSYMGLUNIFORM4UIPROC __rglgen_glUniform4ui;
RGLSYMGLUNIFORM1UIVPROC __rglgen_glUniform1uiv;
RGLSYMGLUNIFORM2UIVPROC __rglgen_glUniform2uiv;
RGLSYMGLUNIFORM3UIVPROC __rglgen_glUniform3uiv;
RGLSYMGLUNIFORM4UIVPROC __rglgen_glUniform4uiv;
RGLSYMGLTEXPARAMETERIIVPROC __rglgen_glTexParameterIiv;
RGLSYMGLTEXPARAMETERIUIVPROC __rglgen_glTexParameterIuiv;
RGLSYMGLGETTEXPARAMETERIIVPROC __rglgen_glGetTexParameterIiv;
RGLSYMGLGETTEXPARAMETERIUIVPROC __rglgen_glGetTexParameterIuiv;
RGLSYMGLCLEARBUFFERIVPROC __rglgen_glClearBufferiv;
RGLSYMGLCLEARBUFFERUIVPROC __rglgen_glClearBufferuiv;
RGLSYMGLCLEARBUFFERFVPROC __rglgen_glClearBufferfv;
RGLSYMGLCLEARBUFFERFIPROC __rglgen_glClearBufferfi;
RGLSYMGLGETSTRINGIPROC __rglgen_glGetStringi;
RGLSYMGLISRENDERBUFFERPROC __rglgen_glIsRenderbuffer;
RGLSYMGLBINDRENDERBUFFERPROC __rglgen_glBindRenderbuffer;
RGLSYMGLDELETERENDERBUFFERSPROC __rglgen_glDeleteRenderbuffers;
RGLSYMGLGENRENDERBUFFERSPROC __rglgen_glGenRenderbuffers;
RGLSYMGLRENDERBUFFERSTORAGEPROC __rglgen_glRenderbufferStorage;
RGLSYMGLGETRENDERBUFFERPARAMETERIVPROC __rglgen_glGetRenderbufferParameteriv;
RGLSYMGLISFRAMEBUFFERPROC __rglgen_glIsFramebuffer;
RGLSYMGLBINDFRAMEBUFFERPROC __rglgen_glBindFramebuffer;
RGLSYMGLDELETEFRAMEBUFFERSPROC __rglgen_glDeleteFramebuffers;
RGLSYMGLGENFRAMEBUFFERSPROC __rglgen_glGenFramebuffers;
RGLSYMGLCHECKFRAMEBUFFERSTATUSPROC __rglgen_glCheckFramebufferStatus;
RGLSYMGLFRAMEBUFFERTEXTURE1DPROC __rglgen_glFramebufferTexture1D;
RGLSYMGLFRAMEBUFFERTEXTURE2DPROC __rglgen_glFramebufferTexture2D;
RGLSYMGLFRAMEBUFFERTEXTURE3DPROC __rglgen_glFramebufferTexture3D;
RGLSYMGLFRAMEBUFFERRENDERBUFFERPROC __rglgen_glFramebufferRenderbuffer;
RGLSYMGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC __rglgen_glGetFramebufferAttachmentParameteriv;
RGLSYMGLGENERATEMIPMAPPROC __rglgen_glGenerateMipmap;
RGLSYMGLBLITFRAMEBUFFERPROC __rglgen_glBlitFramebuffer;
RGLSYMGLRENDERBUFFERSTORAGEMULTISAMPLEPROC __rglgen_glRenderbufferStorageMultisample;
RGLSYMGLFRAMEBUFFERTEXTURELAYERPROC __rglgen_glFramebufferTextureLayer;
RGLSYMGLMAPBUFFERRANGEPROC __rglgen_glMapBufferRange;
RGLSYMGLFLUSHMAPPEDBUFFERRANGEPROC __rglgen_glFlushMappedBufferRange;
RGLSYMGLBINDVERTEXARRAYPROC __rglgen_glBindVertexArray;
RGLSYMGLDELETEVERTEXARRAYSPROC __rglgen_glDeleteVertexArrays;
RGLSYMGLGENVERTEXARRAYSPROC __rglgen_glGenVertexArrays;
RGLSYMGLISVERTEXARRAYPROC __rglgen_glIsVertexArray;
RGLSYMGLDRAWARRAYSINSTANCEDPROC __rglgen_glDrawArraysInstanced;
RGLSYMGLDRAWELEMENTSINSTANCEDPROC __rglgen_glDrawElementsInstanced;
RGLSYMGLTEXBUFFERPROC __rglgen_glTexBuffer;
RGLSYMGLPRIMITIVERESTARTINDEXPROC __rglgen_glPrimitiveRestartIndex;
RGLSYMGLCOPYBUFFERSUBDATAPROC __rglgen_glCopyBufferSubData;
RGLSYMGLGETUNIFORMINDICESPROC __rglgen_glGetUniformIndices;
RGLSYMGLGETACTIVEUNIFORMSIVPROC __rglgen_glGetActiveUniformsiv;
RGLSYMGLGETACTIVEUNIFORMNAMEPROC __rglgen_glGetActiveUniformName;
RGLSYMGLGETUNIFORMBLOCKINDEXPROC __rglgen_glGetUniformBlockIndex;
RGLSYMGLGETACTIVEUNIFORMBLOCKIVPROC __rglgen_glGetActiveUniformBlockiv;
RGLSYMGLGETACTIVEUNIFORMBLOCKNAMEPROC __rglgen_glGetActiveUniformBlockName;
RGLSYMGLUNIFORMBLOCKBINDINGPROC __rglgen_glUniformBlockBinding;
RGLSYMGLDRAWELEMENTSBASEVERTEXPROC __rglgen_glDrawElementsBaseVertex;
RGLSYMGLDRAWRANGEELEMENTSBASEVERTEXPROC __rglgen_glDrawRangeElementsBaseVertex;
RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC __rglgen_glDrawElementsInstancedBaseVertex;
RGLSYMGLMULTIDRAWELEMENTSBASEVERTEXPROC __rglgen_glMultiDrawElementsBaseVertex;
RGLSYMGLPROVOKINGVERTEXPROC __rglgen_glProvokingVertex;
RGLSYMGLFENCESYNCPROC __rglgen_glFenceSync;
RGLSYMGLISSYNCPROC __rglgen_glIsSync;
RGLSYMGLDELETESYNCPROC __rglgen_glDeleteSync;
RGLSYMGLCLIENTWAITSYNCPROC __rglgen_glClientWaitSync;
RGLSYMGLWAITSYNCPROC __rglgen_glWaitSync;
RGLSYMGLGETINTEGER64VPROC __rglgen_glGetInteger64v;
RGLSYMGLGETSYNCIVPROC __rglgen_glGetSynciv;
RGLSYMGLGETINTEGER64I_VPROC __rglgen_glGetInteger64i_v;
RGLSYMGLGETBUFFERPARAMETERI64VPROC __rglgen_glGetBufferParameteri64v;
RGLSYMGLFRAMEBUFFERTEXTUREPROC __rglgen_glFramebufferTexture;
RGLSYMGLTEXIMAGE2DMULTISAMPLEPROC __rglgen_glTexImage2DMultisample;
RGLSYMGLTEXIMAGE3DMULTISAMPLEPROC __rglgen_glTexImage3DMultisample;
RGLSYMGLGETMULTISAMPLEFVPROC __rglgen_glGetMultisamplefv;
RGLSYMGLSAMPLEMASKIPROC __rglgen_glSampleMaski;
RGLSYMGLBINDFRAGDATALOCATIONINDEXEDPROC __rglgen_glBindFragDataLocationIndexed;
RGLSYMGLGETFRAGDATAINDEXPROC __rglgen_glGetFragDataIndex;
RGLSYMGLGENSAMPLERSPROC __rglgen_glGenSamplers;
RGLSYMGLDELETESAMPLERSPROC __rglgen_glDeleteSamplers;
RGLSYMGLISSAMPLERPROC __rglgen_glIsSampler;
RGLSYMGLBINDSAMPLERPROC __rglgen_glBindSampler;
RGLSYMGLSAMPLERPARAMETERIPROC __rglgen_glSamplerParameteri;
RGLSYMGLSAMPLERPARAMETERIVPROC __rglgen_glSamplerParameteriv;
RGLSYMGLSAMPLERPARAMETERFPROC __rglgen_glSamplerParameterf;
RGLSYMGLSAMPLERPARAMETERFVPROC __rglgen_glSamplerParameterfv;
RGLSYMGLSAMPLERPARAMETERIIVPROC __rglgen_glSamplerParameterIiv;
RGLSYMGLSAMPLERPARAMETERIUIVPROC __rglgen_glSamplerParameterIuiv;
RGLSYMGLGETSAMPLERPARAMETERIVPROC __rglgen_glGetSamplerParameteriv;
RGLSYMGLGETSAMPLERPARAMETERIIVPROC __rglgen_glGetSamplerParameterIiv;
RGLSYMGLGETSAMPLERPARAMETERFVPROC __rglgen_glGetSamplerParameterfv;
RGLSYMGLGETSAMPLERPARAMETERIUIVPROC __rglgen_glGetSamplerParameterIuiv;
RGLSYMGLQUERYCOUNTERPROC __rglgen_glQueryCounter;
RGLSYMGLGETQUERYOBJECTI64VPROC __rglgen_glGetQueryObjecti64v;
RGLSYMGLGETQUERYOBJECTUI64VPROC __rglgen_glGetQueryObjectui64v;
RGLSYMGLVERTEXATTRIBDIVISORPROC __rglgen_glVertexAttribDivisor;
RGLSYMGLVERTEXATTRIBP1UIPROC __rglgen_glVertexAttribP1ui;
RGLSYMGLVERTEXATTRIBP1UIVPROC __rglgen_glVertexAttribP1uiv;
RGLSYMGLVERTEXATTRIBP2UIPROC __rglgen_glVertexAttribP2ui;
RGLSYMGLVERTEXATTRIBP2UIVPROC __rglgen_glVertexAttribP2uiv;
RGLSYMGLVERTEXATTRIBP3UIPROC __rglgen_glVertexAttribP3ui;
RGLSYMGLVERTEXATTRIBP3UIVPROC __rglgen_glVertexAttribP3uiv;
RGLSYMGLVERTEXATTRIBP4UIPROC __rglgen_glVertexAttribP4ui;
RGLSYMGLVERTEXATTRIBP4UIVPROC __rglgen_glVertexAttribP4uiv;
RGLSYMGLMINSAMPLESHADINGPROC __rglgen_glMinSampleShading;
RGLSYMGLBLENDEQUATIONIPROC __rglgen_glBlendEquationi;
RGLSYMGLBLENDEQUATIONSEPARATEIPROC __rglgen_glBlendEquationSeparatei;
RGLSYMGLBLENDFUNCIPROC __rglgen_glBlendFunci;
RGLSYMGLBLENDFUNCSEPARATEIPROC __rglgen_glBlendFuncSeparatei;
RGLSYMGLDRAWARRAYSINDIRECTPROC __rglgen_glDrawArraysIndirect;
RGLSYMGLDRAWELEMENTSINDIRECTPROC __rglgen_glDrawElementsIndirect;
RGLSYMGLUNIFORM1DPROC __rglgen_glUniform1d;
RGLSYMGLUNIFORM2DPROC __rglgen_glUniform2d;
RGLSYMGLUNIFORM3DPROC __rglgen_glUniform3d;
RGLSYMGLUNIFORM4DPROC __rglgen_glUniform4d;
RGLSYMGLUNIFORM1DVPROC __rglgen_glUniform1dv;
RGLSYMGLUNIFORM2DVPROC __rglgen_glUniform2dv;
RGLSYMGLUNIFORM3DVPROC __rglgen_glUniform3dv;
RGLSYMGLUNIFORM4DVPROC __rglgen_glUniform4dv;
RGLSYMGLUNIFORMMATRIX2DVPROC __rglgen_glUniformMatrix2dv;
RGLSYMGLUNIFORMMATRIX3DVPROC __rglgen_glUniformMatrix3dv;
RGLSYMGLUNIFORMMATRIX4DVPROC __rglgen_glUniformMatrix4dv;
RGLSYMGLUNIFORMMATRIX2X3DVPROC __rglgen_glUniformMatrix2x3dv;
RGLSYMGLUNIFORMMATRIX2X4DVPROC __rglgen_glUniformMatrix2x4dv;
RGLSYMGLUNIFORMMATRIX3X2DVPROC __rglgen_glUniformMatrix3x2dv;
RGLSYMGLUNIFORMMATRIX3X4DVPROC __rglgen_glUniformMatrix3x4dv;
RGLSYMGLUNIFORMMATRIX4X2DVPROC __rglgen_glUniformMatrix4x2dv;
RGLSYMGLUNIFORMMATRIX4X3DVPROC __rglgen_glUniformMatrix4x3dv;
RGLSYMGLGETUNIFORMDVPROC __rglgen_glGetUniformdv;
RGLSYMGLGETSUBROUTINEUNIFORMLOCATIONPROC __rglgen_glGetSubroutineUniformLocation;
RGLSYMGLGETSUBROUTINEINDEXPROC __rglgen_glGetSubroutineIndex;
RGLSYMGLGETACTIVESUBROUTINEUNIFORMIVPROC __rglgen_glGetActiveSubroutineUniformiv;
RGLSYMGLGETACTIVESUBROUTINEUNIFORMNAMEPROC __rglgen_glGetActiveSubroutineUniformName;
RGLSYMGLGETACTIVESUBROUTINENAMEPROC __rglgen_glGetActiveSubroutineName;
RGLSYMGLUNIFORMSUBROUTINESUIVPROC __rglgen_glUniformSubroutinesuiv;
RGLSYMGLGETUNIFORMSUBROUTINEUIVPROC __rglgen_glGetUniformSubroutineuiv;
RGLSYMGLGETPROGRAMSTAGEIVPROC __rglgen_glGetProgramStageiv;
RGLSYMGLPATCHPARAMETERIPROC __rglgen_glPatchParameteri;
RGLSYMGLPATCHPARAMETERFVPROC __rglgen_glPatchParameterfv;
RGLSYMGLBINDTRANSFORMFEEDBACKPROC __rglgen_glBindTransformFeedback;
RGLSYMGLDELETETRANSFORMFEEDBACKSPROC __rglgen_glDeleteTransformFeedbacks;
RGLSYMGLGENTRANSFORMFEEDBACKSPROC __rglgen_glGenTransformFeedbacks;
RGLSYMGLISTRANSFORMFEEDBACKPROC __rglgen_glIsTransformFeedback;
RGLSYMGLPAUSETRANSFORMFEEDBACKPROC __rglgen_glPauseTransformFeedback;
RGLSYMGLRESUMETRANSFORMFEEDBACKPROC __rglgen_glResumeTransformFeedback;
RGLSYMGLDRAWTRANSFORMFEEDBACKPROC __rglgen_glDrawTransformFeedback;
RGLSYMGLDRAWTRANSFORMFEEDBACKSTREAMPROC __rglgen_glDrawTransformFeedbackStream;
RGLSYMGLBEGINQUERYINDEXEDPROC __rglgen_glBeginQueryIndexed;
RGLSYMGLENDQUERYINDEXEDPROC __rglgen_glEndQueryIndexed;
RGLSYMGLGETQUERYINDEXEDIVPROC __rglgen_glGetQueryIndexediv;
RGLSYMGLRELEASESHADERCOMPILERPROC __rglgen_glReleaseShaderCompiler;
RGLSYMGLSHADERBINARYPROC __rglgen_glShaderBinary;
RGLSYMGLGETSHADERPRECISIONFORMATPROC __rglgen_glGetShaderPrecisionFormat;
RGLSYMGLDEPTHRANGEFPROC __rglgen_glDepthRangef;
RGLSYMGLCLEARDEPTHFPROC __rglgen_glClearDepthf;
RGLSYMGLGETPROGRAMBINARYPROC __rglgen_glGetProgramBinary;
RGLSYMGLPROGRAMBINARYPROC __rglgen_glProgramBinary;
RGLSYMGLPROGRAMPARAMETERIPROC __rglgen_glProgramParameteri;
RGLSYMGLUSEPROGRAMSTAGESPROC __rglgen_glUseProgramStages;
RGLSYMGLACTIVESHADERPROGRAMPROC __rglgen_glActiveShaderProgram;
RGLSYMGLCREATESHADERPROGRAMVPROC __rglgen_glCreateShaderProgramv;
RGLSYMGLBINDPROGRAMPIPELINEPROC __rglgen_glBindProgramPipeline;
RGLSYMGLDELETEPROGRAMPIPELINESPROC __rglgen_glDeleteProgramPipelines;
RGLSYMGLGENPROGRAMPIPELINESPROC __rglgen_glGenProgramPipelines;
RGLSYMGLISPROGRAMPIPELINEPROC __rglgen_glIsProgramPipeline;
RGLSYMGLGETPROGRAMPIPELINEIVPROC __rglgen_glGetProgramPipelineiv;
RGLSYMGLPROGRAMUNIFORM1IPROC __rglgen_glProgramUniform1i;
RGLSYMGLPROGRAMUNIFORM1IVPROC __rglgen_glProgramUniform1iv;
RGLSYMGLPROGRAMUNIFORM1FPROC __rglgen_glProgramUniform1f;
RGLSYMGLPROGRAMUNIFORM1FVPROC __rglgen_glProgramUniform1fv;
RGLSYMGLPROGRAMUNIFORM1DPROC __rglgen_glProgramUniform1d;
RGLSYMGLPROGRAMUNIFORM1DVPROC __rglgen_glProgramUniform1dv;
RGLSYMGLPROGRAMUNIFORM1UIPROC __rglgen_glProgramUniform1ui;
RGLSYMGLPROGRAMUNIFORM1UIVPROC __rglgen_glProgramUniform1uiv;
RGLSYMGLPROGRAMUNIFORM2IPROC __rglgen_glProgramUniform2i;
RGLSYMGLPROGRAMUNIFORM2IVPROC __rglgen_glProgramUniform2iv;
RGLSYMGLPROGRAMUNIFORM2FPROC __rglgen_glProgramUniform2f;
RGLSYMGLPROGRAMUNIFORM2FVPROC __rglgen_glProgramUniform2fv;
RGLSYMGLPROGRAMUNIFORM2DPROC __rglgen_glProgramUniform2d;
RGLSYMGLPROGRAMUNIFORM2DVPROC __rglgen_glProgramUniform2dv;
RGLSYMGLPROGRAMUNIFORM2UIPROC __rglgen_glProgramUniform2ui;
RGLSYMGLPROGRAMUNIFORM2UIVPROC __rglgen_glProgramUniform2uiv;
RGLSYMGLPROGRAMUNIFORM3IPROC __rglgen_glProgramUniform3i;
RGLSYMGLPROGRAMUNIFORM3IVPROC __rglgen_glProgramUniform3iv;
RGLSYMGLPROGRAMUNIFORM3FPROC __rglgen_glProgramUniform3f;
RGLSYMGLPROGRAMUNIFORM3FVPROC __rglgen_glProgramUniform3fv;
RGLSYMGLPROGRAMUNIFORM3DPROC __rglgen_glProgramUniform3d;
RGLSYMGLPROGRAMUNIFORM3DVPROC __rglgen_glProgramUniform3dv;
RGLSYMGLPROGRAMUNIFORM3UIPROC __rglgen_glProgramUniform3ui;
RGLSYMGLPROGRAMUNIFORM3UIVPROC __rglgen_glProgramUniform3uiv;
RGLSYMGLPROGRAMUNIFORM4IPROC __rglgen_glProgramUniform4i;
RGLSYMGLPROGRAMUNIFORM4IVPROC __rglgen_glProgramUniform4iv;
RGLSYMGLPROGRAMUNIFORM4FPROC __rglgen_glProgramUniform4f;
RGLSYMGLPROGRAMUNIFORM4FVPROC __rglgen_glProgramUniform4fv;
RGLSYMGLPROGRAMUNIFORM4DPROC __rglgen_glProgramUniform4d;
RGLSYMGLPROGRAMUNIFORM4DVPROC __rglgen_glProgramUniform4dv;
RGLSYMGLPROGRAMUNIFORM4UIPROC __rglgen_glProgramUniform4ui;
RGLSYMGLPROGRAMUNIFORM4UIVPROC __rglgen_glProgramUniform4uiv;
RGLSYMGLPROGRAMUNIFORMMATRIX2FVPROC __rglgen_glProgramUniformMatrix2fv;
RGLSYMGLPROGRAMUNIFORMMATRIX3FVPROC __rglgen_glProgramUniformMatrix3fv;
RGLSYMGLPROGRAMUNIFORMMATRIX4FVPROC __rglgen_glProgramUniformMatrix4fv;
RGLSYMGLPROGRAMUNIFORMMATRIX2DVPROC __rglgen_glProgramUniformMatrix2dv;
RGLSYMGLPROGRAMUNIFORMMATRIX3DVPROC __rglgen_glProgramUniformMatrix3dv;
RGLSYMGLPROGRAMUNIFORMMATRIX4DVPROC __rglgen_glProgramUniformMatrix4dv;
RGLSYMGLPROGRAMUNIFORMMATRIX2X3FVPROC __rglgen_glProgramUniformMatrix2x3fv;
RGLSYMGLPROGRAMUNIFORMMATRIX3X2FVPROC __rglgen_glProgramUniformMatrix3x2fv;
RGLSYMGLPROGRAMUNIFORMMATRIX2X4FVPROC __rglgen_glProgramUniformMatrix2x4fv;
RGLSYMGLPROGRAMUNIFORMMATRIX4X2FVPROC __rglgen_glProgramUniformMatrix4x2fv;
RGLSYMGLPROGRAMUNIFORMMATRIX3X4FVPROC __rglgen_glProgramUniformMatrix3x4fv;
RGLSYMGLPROGRAMUNIFORMMATRIX4X3FVPROC __rglgen_glProgramUniformMatrix4x3fv;
RGLSYMGLPROGRAMUNIFORMMATRIX2X3DVPROC __rglgen_glProgramUniformMatrix2x3dv;
RGLSYMGLPROGRAMUNIFORMMATRIX3X2DVPROC __rglgen_glProgramUniformMatrix3x2dv;
RGLSYMGLPROGRAMUNIFORMMATRIX2X4DVPROC __rglgen_glProgramUniformMatrix2x4dv;
RGLSYMGLPROGRAMUNIFORMMATRIX4X2DVPROC __rglgen_glProgramUniformMatrix4x2dv;
RGLSYMGLPROGRAMUNIFORMMATRIX3X4DVPROC __rglgen_glProgramUniformMatrix3x4dv;
RGLSYMGLPROGRAMUNIFORMMATRIX4X3DVPROC __rglgen_glProgramUniformMatrix4x3dv;
RGLSYMGLVALIDATEPROGRAMPIPELINEPROC __rglgen_glValidateProgramPipeline;
RGLSYMGLGETPROGRAMPIPELINEINFOLOGPROC __rglgen_glGetProgramPipelineInfoLog;
RGLSYMGLVERTEXATTRIBL1DPROC __rglgen_glVertexAttribL1d;
RGLSYMGLVERTEXATTRIBL2DPROC __rglgen_glVertexAttribL2d;
RGLSYMGLVERTEXATTRIBL3DPROC __rglgen_glVertexAttribL3d;
RGLSYMGLVERTEXATTRIBL4DPROC __rglgen_glVertexAttribL4d;
RGLSYMGLVERTEXATTRIBL1DVPROC __rglgen_glVertexAttribL1dv;
RGLSYMGLVERTEXATTRIBL2DVPROC __rglgen_glVertexAttribL2dv;
RGLSYMGLVERTEXATTRIBL3DVPROC __rglgen_glVertexAttribL3dv;
RGLSYMGLVERTEXATTRIBL4DVPROC __rglgen_glVertexAttribL4dv;
RGLSYMGLVERTEXATTRIBLPOINTERPROC __rglgen_glVertexAttribLPointer;
RGLSYMGLGETVERTEXATTRIBLDVPROC __rglgen_glGetVertexAttribLdv;
RGLSYMGLVIEWPORTARRAYVPROC __rglgen_glViewportArrayv;
RGLSYMGLVIEWPORTINDEXEDFPROC __rglgen_glViewportIndexedf;
RGLSYMGLVIEWPORTINDEXEDFVPROC __rglgen_glViewportIndexedfv;
RGLSYMGLSCISSORARRAYVPROC __rglgen_glScissorArrayv;
RGLSYMGLSCISSORINDEXEDPROC __rglgen_glScissorIndexed;
RGLSYMGLSCISSORINDEXEDVPROC __rglgen_glScissorIndexedv;
RGLSYMGLDEPTHRANGEARRAYVPROC __rglgen_glDepthRangeArrayv;
RGLSYMGLDEPTHRANGEINDEXEDPROC __rglgen_glDepthRangeIndexed;
RGLSYMGLGETFLOATI_VPROC __rglgen_glGetFloati_v;
RGLSYMGLGETDOUBLEI_VPROC __rglgen_glGetDoublei_v;
RGLSYMGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC __rglgen_glDrawArraysInstancedBaseInstance;
RGLSYMGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC __rglgen_glDrawElementsInstancedBaseInstance;
RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC __rglgen_glDrawElementsInstancedBaseVertexBaseInstance;
RGLSYMGLGETINTERNALFORMATIVPROC __rglgen_glGetInternalformativ;
RGLSYMGLGETACTIVEATOMICCOUNTERBUFFERIVPROC __rglgen_glGetActiveAtomicCounterBufferiv;
RGLSYMGLBINDIMAGETEXTUREPROC __rglgen_glBindImageTexture;
RGLSYMGLMEMORYBARRIERPROC __rglgen_glMemoryBarrier;
RGLSYMGLTEXSTORAGE1DPROC __rglgen_glTexStorage1D;
RGLSYMGLTEXSTORAGE2DPROC __rglgen_glTexStorage2D;
RGLSYMGLTEXSTORAGE3DPROC __rglgen_glTexStorage3D;
RGLSYMGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC __rglgen_glDrawTransformFeedbackInstanced;
RGLSYMGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC __rglgen_glDrawTransformFeedbackStreamInstanced;
RGLSYMGLCLEARBUFFERDATAPROC __rglgen_glClearBufferData;
RGLSYMGLCLEARBUFFERSUBDATAPROC __rglgen_glClearBufferSubData;
RGLSYMGLDISPATCHCOMPUTEPROC __rglgen_glDispatchCompute;
RGLSYMGLDISPATCHCOMPUTEINDIRECTPROC __rglgen_glDispatchComputeIndirect;
RGLSYMGLCOPYIMAGESUBDATAPROC __rglgen_glCopyImageSubData;
RGLSYMGLFRAMEBUFFERPARAMETERIPROC __rglgen_glFramebufferParameteri;
RGLSYMGLGETFRAMEBUFFERPARAMETERIVPROC __rglgen_glGetFramebufferParameteriv;
RGLSYMGLGETINTERNALFORMATI64VPROC __rglgen_glGetInternalformati64v;
RGLSYMGLINVALIDATETEXSUBIMAGEPROC __rglgen_glInvalidateTexSubImage;
RGLSYMGLINVALIDATETEXIMAGEPROC __rglgen_glInvalidateTexImage;
RGLSYMGLINVALIDATEBUFFERSUBDATAPROC __rglgen_glInvalidateBufferSubData;
RGLSYMGLINVALIDATEBUFFERDATAPROC __rglgen_glInvalidateBufferData;
RGLSYMGLINVALIDATEFRAMEBUFFERPROC __rglgen_glInvalidateFramebuffer;
RGLSYMGLINVALIDATESUBFRAMEBUFFERPROC __rglgen_glInvalidateSubFramebuffer;
RGLSYMGLMULTIDRAWARRAYSINDIRECTPROC __rglgen_glMultiDrawArraysIndirect;
RGLSYMGLMULTIDRAWELEMENTSINDIRECTPROC __rglgen_glMultiDrawElementsIndirect;
RGLSYMGLGETPROGRAMINTERFACEIVPROC __rglgen_glGetProgramInterfaceiv;
RGLSYMGLGETPROGRAMRESOURCEINDEXPROC __rglgen_glGetProgramResourceIndex;
RGLSYMGLGETPROGRAMRESOURCENAMEPROC __rglgen_glGetProgramResourceName;
RGLSYMGLGETPROGRAMRESOURCEIVPROC __rglgen_glGetProgramResourceiv;
RGLSYMGLGETPROGRAMRESOURCELOCATIONPROC __rglgen_glGetProgramResourceLocation;
RGLSYMGLGETPROGRAMRESOURCELOCATIONINDEXPROC __rglgen_glGetProgramResourceLocationIndex;
RGLSYMGLSHADERSTORAGEBLOCKBINDINGPROC __rglgen_glShaderStorageBlockBinding;
RGLSYMGLTEXBUFFERRANGEPROC __rglgen_glTexBufferRange;
RGLSYMGLTEXSTORAGE2DMULTISAMPLEPROC __rglgen_glTexStorage2DMultisample;
RGLSYMGLTEXSTORAGE3DMULTISAMPLEPROC __rglgen_glTexStorage3DMultisample;
RGLSYMGLTEXTUREVIEWPROC __rglgen_glTextureView;
RGLSYMGLBINDVERTEXBUFFERPROC __rglgen_glBindVertexBuffer;
RGLSYMGLVERTEXATTRIBFORMATPROC __rglgen_glVertexAttribFormat;
RGLSYMGLVERTEXATTRIBIFORMATPROC __rglgen_glVertexAttribIFormat;
RGLSYMGLVERTEXATTRIBLFORMATPROC __rglgen_glVertexAttribLFormat;
RGLSYMGLVERTEXATTRIBBINDINGPROC __rglgen_glVertexAttribBinding;
RGLSYMGLVERTEXBINDINGDIVISORPROC __rglgen_glVertexBindingDivisor;
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
RGLSYMGLBUFFERSTORAGEPROC __rglgen_glBufferStorage;
RGLSYMGLCLEARTEXIMAGEPROC __rglgen_glClearTexImage;
RGLSYMGLCLEARTEXSUBIMAGEPROC __rglgen_glClearTexSubImage;
RGLSYMGLBINDBUFFERSBASEPROC __rglgen_glBindBuffersBase;
RGLSYMGLBINDBUFFERSRANGEPROC __rglgen_glBindBuffersRange;
RGLSYMGLBINDTEXTURESPROC __rglgen_glBindTextures;
RGLSYMGLBINDSAMPLERSPROC __rglgen_glBindSamplers;
RGLSYMGLBINDIMAGETEXTURESPROC __rglgen_glBindImageTextures;
RGLSYMGLBINDVERTEXBUFFERSPROC __rglgen_glBindVertexBuffers;
RGLSYMGLCLIPCONTROLPROC __rglgen_glClipControl;
RGLSYMGLCREATETRANSFORMFEEDBACKSPROC __rglgen_glCreateTransformFeedbacks;
RGLSYMGLTRANSFORMFEEDBACKBUFFERBASEPROC __rglgen_glTransformFeedbackBufferBase;
RGLSYMGLTRANSFORMFEEDBACKBUFFERRANGEPROC __rglgen_glTransformFeedbackBufferRange;
RGLSYMGLGETTRANSFORMFEEDBACKIVPROC __rglgen_glGetTransformFeedbackiv;
RGLSYMGLGETTRANSFORMFEEDBACKI_VPROC __rglgen_glGetTransformFeedbacki_v;
RGLSYMGLGETTRANSFORMFEEDBACKI64_VPROC __rglgen_glGetTransformFeedbacki64_v;
RGLSYMGLCREATEBUFFERSPROC __rglgen_glCreateBuffers;
RGLSYMGLNAMEDBUFFERSTORAGEPROC __rglgen_glNamedBufferStorage;
RGLSYMGLNAMEDBUFFERDATAPROC __rglgen_glNamedBufferData;
RGLSYMGLNAMEDBUFFERSUBDATAPROC __rglgen_glNamedBufferSubData;
RGLSYMGLCOPYNAMEDBUFFERSUBDATAPROC __rglgen_glCopyNamedBufferSubData;
RGLSYMGLCLEARNAMEDBUFFERDATAPROC __rglgen_glClearNamedBufferData;
RGLSYMGLCLEARNAMEDBUFFERSUBDATAPROC __rglgen_glClearNamedBufferSubData;
RGLSYMGLMAPNAMEDBUFFERPROC __rglgen_glMapNamedBuffer;
RGLSYMGLMAPNAMEDBUFFERRANGEPROC __rglgen_glMapNamedBufferRange;
RGLSYMGLUNMAPNAMEDBUFFERPROC __rglgen_glUnmapNamedBuffer;
RGLSYMGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC __rglgen_glFlushMappedNamedBufferRange;
RGLSYMGLGETNAMEDBUFFERPARAMETERIVPROC __rglgen_glGetNamedBufferParameteriv;
RGLSYMGLGETNAMEDBUFFERPARAMETERI64VPROC __rglgen_glGetNamedBufferParameteri64v;
RGLSYMGLGETNAMEDBUFFERPOINTERVPROC __rglgen_glGetNamedBufferPointerv;
RGLSYMGLGETNAMEDBUFFERSUBDATAPROC __rglgen_glGetNamedBufferSubData;
RGLSYMGLCREATEFRAMEBUFFERSPROC __rglgen_glCreateFramebuffers;
RGLSYMGLNAMEDFRAMEBUFFERRENDERBUFFERPROC __rglgen_glNamedFramebufferRenderbuffer;
RGLSYMGLNAMEDFRAMEBUFFERPARAMETERIPROC __rglgen_glNamedFramebufferParameteri;
RGLSYMGLNAMEDFRAMEBUFFERTEXTUREPROC __rglgen_glNamedFramebufferTexture;
RGLSYMGLNAMEDFRAMEBUFFERTEXTURELAYERPROC __rglgen_glNamedFramebufferTextureLayer;
RGLSYMGLNAMEDFRAMEBUFFERDRAWBUFFERPROC __rglgen_glNamedFramebufferDrawBuffer;
RGLSYMGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC __rglgen_glNamedFramebufferDrawBuffers;
RGLSYMGLNAMEDFRAMEBUFFERREADBUFFERPROC __rglgen_glNamedFramebufferReadBuffer;
RGLSYMGLINVALIDATENAMEDFRAMEBUFFERDATAPROC __rglgen_glInvalidateNamedFramebufferData;
RGLSYMGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC __rglgen_glInvalidateNamedFramebufferSubData;
RGLSYMGLCLEARNAMEDFRAMEBUFFERIVPROC __rglgen_glClearNamedFramebufferiv;
RGLSYMGLCLEARNAMEDFRAMEBUFFERUIVPROC __rglgen_glClearNamedFramebufferuiv;
RGLSYMGLCLEARNAMEDFRAMEBUFFERFVPROC __rglgen_glClearNamedFramebufferfv;
RGLSYMGLCLEARNAMEDFRAMEBUFFERFIPROC __rglgen_glClearNamedFramebufferfi;
RGLSYMGLBLITNAMEDFRAMEBUFFERPROC __rglgen_glBlitNamedFramebuffer;
RGLSYMGLCHECKNAMEDFRAMEBUFFERSTATUSPROC __rglgen_glCheckNamedFramebufferStatus;
RGLSYMGLGETNAMEDFRAMEBUFFERPARAMETERIVPROC __rglgen_glGetNamedFramebufferParameteriv;
RGLSYMGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC __rglgen_glGetNamedFramebufferAttachmentParameteriv;
RGLSYMGLCREATERENDERBUFFERSPROC __rglgen_glCreateRenderbuffers;
RGLSYMGLNAMEDRENDERBUFFERSTORAGEPROC __rglgen_glNamedRenderbufferStorage;
RGLSYMGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC __rglgen_glNamedRenderbufferStorageMultisample;
RGLSYMGLGETNAMEDRENDERBUFFERPARAMETERIVPROC __rglgen_glGetNamedRenderbufferParameteriv;
RGLSYMGLCREATETEXTURESPROC __rglgen_glCreateTextures;
RGLSYMGLTEXTUREBUFFERPROC __rglgen_glTextureBuffer;
RGLSYMGLTEXTUREBUFFERRANGEPROC __rglgen_glTextureBufferRange;
RGLSYMGLTEXTURESTORAGE1DPROC __rglgen_glTextureStorage1D;
RGLSYMGLTEXTURESTORAGE2DPROC __rglgen_glTextureStorage2D;
RGLSYMGLTEXTURESTORAGE3DPROC __rglgen_glTextureStorage3D;
RGLSYMGLTEXTURESTORAGE2DMULTISAMPLEPROC __rglgen_glTextureStorage2DMultisample;
RGLSYMGLTEXTURESTORAGE3DMULTISAMPLEPROC __rglgen_glTextureStorage3DMultisample;
RGLSYMGLTEXTURESUBIMAGE1DPROC __rglgen_glTextureSubImage1D;
RGLSYMGLTEXTURESUBIMAGE2DPROC __rglgen_glTextureSubImage2D;
RGLSYMGLTEXTURESUBIMAGE3DPROC __rglgen_glTextureSubImage3D;
RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE1DPROC __rglgen_glCompressedTextureSubImage1D;
RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE2DPROC __rglgen_glCompressedTextureSubImage2D;
RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE3DPROC __rglgen_glCompressedTextureSubImage3D;
RGLSYMGLCOPYTEXTURESUBIMAGE1DPROC __rglgen_glCopyTextureSubImage1D;
RGLSYMGLCOPYTEXTURESUBIMAGE2DPROC __rglgen_glCopyTextureSubImage2D;
RGLSYMGLCOPYTEXTURESUBIMAGE3DPROC __rglgen_glCopyTextureSubImage3D;
RGLSYMGLTEXTUREPARAMETERFPROC __rglgen_glTextureParameterf;
RGLSYMGLTEXTUREPARAMETERFVPROC __rglgen_glTextureParameterfv;
RGLSYMGLTEXTUREPARAMETERIPROC __rglgen_glTextureParameteri;
RGLSYMGLTEXTUREPARAMETERIIVPROC __rglgen_glTextureParameterIiv;
RGLSYMGLTEXTUREPARAMETERIUIVPROC __rglgen_glTextureParameterIuiv;
RGLSYMGLTEXTUREPARAMETERIVPROC __rglgen_glTextureParameteriv;
RGLSYMGLGENERATETEXTUREMIPMAPPROC __rglgen_glGenerateTextureMipmap;
RGLSYMGLBINDTEXTUREUNITPROC __rglgen_glBindTextureUnit;
RGLSYMGLGETTEXTUREIMAGEPROC __rglgen_glGetTextureImage;
RGLSYMGLGETCOMPRESSEDTEXTUREIMAGEPROC __rglgen_glGetCompressedTextureImage;
RGLSYMGLGETTEXTURELEVELPARAMETERFVPROC __rglgen_glGetTextureLevelParameterfv;
RGLSYMGLGETTEXTURELEVELPARAMETERIVPROC __rglgen_glGetTextureLevelParameteriv;
RGLSYMGLGETTEXTUREPARAMETERFVPROC __rglgen_glGetTextureParameterfv;
RGLSYMGLGETTEXTUREPARAMETERIIVPROC __rglgen_glGetTextureParameterIiv;
RGLSYMGLGETTEXTUREPARAMETERIUIVPROC __rglgen_glGetTextureParameterIuiv;
RGLSYMGLGETTEXTUREPARAMETERIVPROC __rglgen_glGetTextureParameteriv;
RGLSYMGLCREATEVERTEXARRAYSPROC __rglgen_glCreateVertexArrays;
RGLSYMGLDISABLEVERTEXARRAYATTRIBPROC __rglgen_glDisableVertexArrayAttrib;
RGLSYMGLENABLEVERTEXARRAYATTRIBPROC __rglgen_glEnableVertexArrayAttrib;
RGLSYMGLVERTEXARRAYELEMENTBUFFERPROC __rglgen_glVertexArrayElementBuffer;
RGLSYMGLVERTEXARRAYVERTEXBUFFERPROC __rglgen_glVertexArrayVertexBuffer;
RGLSYMGLVERTEXARRAYVERTEXBUFFERSPROC __rglgen_glVertexArrayVertexBuffers;
RGLSYMGLVERTEXARRAYATTRIBBINDINGPROC __rglgen_glVertexArrayAttribBinding;
RGLSYMGLVERTEXARRAYATTRIBFORMATPROC __rglgen_glVertexArrayAttribFormat;
RGLSYMGLVERTEXARRAYATTRIBIFORMATPROC __rglgen_glVertexArrayAttribIFormat;
RGLSYMGLVERTEXARRAYATTRIBLFORMATPROC __rglgen_glVertexArrayAttribLFormat;
RGLSYMGLVERTEXARRAYBINDINGDIVISORPROC __rglgen_glVertexArrayBindingDivisor;
RGLSYMGLGETVERTEXARRAYIVPROC __rglgen_glGetVertexArrayiv;
RGLSYMGLGETVERTEXARRAYINDEXEDIVPROC __rglgen_glGetVertexArrayIndexediv;
RGLSYMGLGETVERTEXARRAYINDEXED64IVPROC __rglgen_glGetVertexArrayIndexed64iv;
RGLSYMGLCREATESAMPLERSPROC __rglgen_glCreateSamplers;
RGLSYMGLCREATEPROGRAMPIPELINESPROC __rglgen_glCreateProgramPipelines;
RGLSYMGLCREATEQUERIESPROC __rglgen_glCreateQueries;
RGLSYMGLGETQUERYBUFFEROBJECTI64VPROC __rglgen_glGetQueryBufferObjecti64v;
RGLSYMGLGETQUERYBUFFEROBJECTIVPROC __rglgen_glGetQueryBufferObjectiv;
RGLSYMGLGETQUERYBUFFEROBJECTUI64VPROC __rglgen_glGetQueryBufferObjectui64v;
RGLSYMGLGETQUERYBUFFEROBJECTUIVPROC __rglgen_glGetQueryBufferObjectuiv;
RGLSYMGLMEMORYBARRIERBYREGIONPROC __rglgen_glMemoryBarrierByRegion;
RGLSYMGLGETTEXTURESUBIMAGEPROC __rglgen_glGetTextureSubImage;
RGLSYMGLGETCOMPRESSEDTEXTURESUBIMAGEPROC __rglgen_glGetCompressedTextureSubImage;
RGLSYMGLGETGRAPHICSRESETSTATUSPROC __rglgen_glGetGraphicsResetStatus;
RGLSYMGLGETNCOMPRESSEDTEXIMAGEPROC __rglgen_glGetnCompressedTexImage;
RGLSYMGLGETNTEXIMAGEPROC __rglgen_glGetnTexImage;
RGLSYMGLGETNUNIFORMDVPROC __rglgen_glGetnUniformdv;
RGLSYMGLGETNUNIFORMFVPROC __rglgen_glGetnUniformfv;
RGLSYMGLGETNUNIFORMIVPROC __rglgen_glGetnUniformiv;
RGLSYMGLGETNUNIFORMUIVPROC __rglgen_glGetnUniformuiv;
RGLSYMGLREADNPIXELSPROC __rglgen_glReadnPixels;
RGLSYMGLTEXTUREBARRIERPROC __rglgen_glTextureBarrier;
RGLSYMGLSPECIALIZESHADERPROC __rglgen_glSpecializeShader;
RGLSYMGLMULTIDRAWARRAYSINDIRECTCOUNTPROC __rglgen_glMultiDrawArraysIndirectCount;
RGLSYMGLMULTIDRAWELEMENTSINDIRECTCOUNTPROC __rglgen_glMultiDrawElementsIndirectCount;
RGLSYMGLPOLYGONOFFSETCLAMPPROC __rglgen_glPolygonOffsetClamp;
RGLSYMGLPRIMITIVEBOUNDINGBOXARBPROC __rglgen_glPrimitiveBoundingBoxARB;
RGLSYMGLGETTEXTUREHANDLEARBPROC __rglgen_glGetTextureHandleARB;
RGLSYMGLGETTEXTURESAMPLERHANDLEARBPROC __rglgen_glGetTextureSamplerHandleARB;
RGLSYMGLMAKETEXTUREHANDLERESIDENTARBPROC __rglgen_glMakeTextureHandleResidentARB;
RGLSYMGLMAKETEXTUREHANDLENONRESIDENTARBPROC __rglgen_glMakeTextureHandleNonResidentARB;
RGLSYMGLGETIMAGEHANDLEARBPROC __rglgen_glGetImageHandleARB;
RGLSYMGLMAKEIMAGEHANDLERESIDENTARBPROC __rglgen_glMakeImageHandleResidentARB;
RGLSYMGLMAKEIMAGEHANDLENONRESIDENTARBPROC __rglgen_glMakeImageHandleNonResidentARB;
RGLSYMGLUNIFORMHANDLEUI64ARBPROC __rglgen_glUniformHandleui64ARB;
RGLSYMGLUNIFORMHANDLEUI64VARBPROC __rglgen_glUniformHandleui64vARB;
RGLSYMGLPROGRAMUNIFORMHANDLEUI64ARBPROC __rglgen_glProgramUniformHandleui64ARB;
RGLSYMGLPROGRAMUNIFORMHANDLEUI64VARBPROC __rglgen_glProgramUniformHandleui64vARB;
RGLSYMGLISTEXTUREHANDLERESIDENTARBPROC __rglgen_glIsTextureHandleResidentARB;
RGLSYMGLISIMAGEHANDLERESIDENTARBPROC __rglgen_glIsImageHandleResidentARB;
RGLSYMGLVERTEXATTRIBL1UI64ARBPROC __rglgen_glVertexAttribL1ui64ARB;
RGLSYMGLVERTEXATTRIBL1UI64VARBPROC __rglgen_glVertexAttribL1ui64vARB;
RGLSYMGLGETVERTEXATTRIBLUI64VARBPROC __rglgen_glGetVertexAttribLui64vARB;
RGLSYMGLDISPATCHCOMPUTEGROUPSIZEARBPROC __rglgen_glDispatchComputeGroupSizeARB;
RGLSYMGLDEBUGMESSAGECONTROLARBPROC __rglgen_glDebugMessageControlARB;
RGLSYMGLDEBUGMESSAGEINSERTARBPROC __rglgen_glDebugMessageInsertARB;
RGLSYMGLDEBUGMESSAGECALLBACKARBPROC __rglgen_glDebugMessageCallbackARB;
RGLSYMGLGETDEBUGMESSAGELOGARBPROC __rglgen_glGetDebugMessageLogARB;
RGLSYMGLBLENDEQUATIONIARBPROC __rglgen_glBlendEquationiARB;
RGLSYMGLBLENDEQUATIONSEPARATEIARBPROC __rglgen_glBlendEquationSeparateiARB;
RGLSYMGLBLENDFUNCIARBPROC __rglgen_glBlendFunciARB;
RGLSYMGLBLENDFUNCSEPARATEIARBPROC __rglgen_glBlendFuncSeparateiARB;
RGLSYMGLDRAWARRAYSINSTANCEDARBPROC __rglgen_glDrawArraysInstancedARB;
RGLSYMGLDRAWELEMENTSINSTANCEDARBPROC __rglgen_glDrawElementsInstancedARB;
RGLSYMGLPROGRAMPARAMETERIARBPROC __rglgen_glProgramParameteriARB;
RGLSYMGLFRAMEBUFFERTEXTUREARBPROC __rglgen_glFramebufferTextureARB;
RGLSYMGLFRAMEBUFFERTEXTURELAYERARBPROC __rglgen_glFramebufferTextureLayerARB;
RGLSYMGLFRAMEBUFFERTEXTUREFACEARBPROC __rglgen_glFramebufferTextureFaceARB;
RGLSYMGLSPECIALIZESHADERARBPROC __rglgen_glSpecializeShaderARB;
RGLSYMGLUNIFORM1I64ARBPROC __rglgen_glUniform1i64ARB;
RGLSYMGLUNIFORM2I64ARBPROC __rglgen_glUniform2i64ARB;
RGLSYMGLUNIFORM3I64ARBPROC __rglgen_glUniform3i64ARB;
RGLSYMGLUNIFORM4I64ARBPROC __rglgen_glUniform4i64ARB;
RGLSYMGLUNIFORM1I64VARBPROC __rglgen_glUniform1i64vARB;
RGLSYMGLUNIFORM2I64VARBPROC __rglgen_glUniform2i64vARB;
RGLSYMGLUNIFORM3I64VARBPROC __rglgen_glUniform3i64vARB;
RGLSYMGLUNIFORM4I64VARBPROC __rglgen_glUniform4i64vARB;
RGLSYMGLUNIFORM1UI64ARBPROC __rglgen_glUniform1ui64ARB;
RGLSYMGLUNIFORM2UI64ARBPROC __rglgen_glUniform2ui64ARB;
RGLSYMGLUNIFORM3UI64ARBPROC __rglgen_glUniform3ui64ARB;
RGLSYMGLUNIFORM4UI64ARBPROC __rglgen_glUniform4ui64ARB;
RGLSYMGLUNIFORM1UI64VARBPROC __rglgen_glUniform1ui64vARB;
RGLSYMGLUNIFORM2UI64VARBPROC __rglgen_glUniform2ui64vARB;
RGLSYMGLUNIFORM3UI64VARBPROC __rglgen_glUniform3ui64vARB;
RGLSYMGLUNIFORM4UI64VARBPROC __rglgen_glUniform4ui64vARB;
RGLSYMGLGETUNIFORMI64VARBPROC __rglgen_glGetUniformi64vARB;
RGLSYMGLGETUNIFORMUI64VARBPROC __rglgen_glGetUniformui64vARB;
RGLSYMGLGETNUNIFORMI64VARBPROC __rglgen_glGetnUniformi64vARB;
RGLSYMGLGETNUNIFORMUI64VARBPROC __rglgen_glGetnUniformui64vARB;
RGLSYMGLPROGRAMUNIFORM1I64ARBPROC __rglgen_glProgramUniform1i64ARB;
RGLSYMGLPROGRAMUNIFORM2I64ARBPROC __rglgen_glProgramUniform2i64ARB;
RGLSYMGLPROGRAMUNIFORM3I64ARBPROC __rglgen_glProgramUniform3i64ARB;
RGLSYMGLPROGRAMUNIFORM4I64ARBPROC __rglgen_glProgramUniform4i64ARB;
RGLSYMGLPROGRAMUNIFORM1I64VARBPROC __rglgen_glProgramUniform1i64vARB;
RGLSYMGLPROGRAMUNIFORM2I64VARBPROC __rglgen_glProgramUniform2i64vARB;
RGLSYMGLPROGRAMUNIFORM3I64VARBPROC __rglgen_glProgramUniform3i64vARB;
RGLSYMGLPROGRAMUNIFORM4I64VARBPROC __rglgen_glProgramUniform4i64vARB;
RGLSYMGLPROGRAMUNIFORM1UI64ARBPROC __rglgen_glProgramUniform1ui64ARB;
RGLSYMGLPROGRAMUNIFORM2UI64ARBPROC __rglgen_glProgramUniform2ui64ARB;
RGLSYMGLPROGRAMUNIFORM3UI64ARBPROC __rglgen_glProgramUniform3ui64ARB;
RGLSYMGLPROGRAMUNIFORM4UI64ARBPROC __rglgen_glProgramUniform4ui64ARB;
RGLSYMGLPROGRAMUNIFORM1UI64VARBPROC __rglgen_glProgramUniform1ui64vARB;
RGLSYMGLPROGRAMUNIFORM2UI64VARBPROC __rglgen_glProgramUniform2ui64vARB;
RGLSYMGLPROGRAMUNIFORM3UI64VARBPROC __rglgen_glProgramUniform3ui64vARB;
RGLSYMGLPROGRAMUNIFORM4UI64VARBPROC __rglgen_glProgramUniform4ui64vARB;
RGLSYMGLMULTIDRAWARRAYSINDIRECTCOUNTARBPROC __rglgen_glMultiDrawArraysIndirectCountARB;
RGLSYMGLMULTIDRAWELEMENTSINDIRECTCOUNTARBPROC __rglgen_glMultiDrawElementsIndirectCountARB;
RGLSYMGLVERTEXATTRIBDIVISORARBPROC __rglgen_glVertexAttribDivisorARB;
RGLSYMGLMAXSHADERCOMPILERTHREADSARBPROC __rglgen_glMaxShaderCompilerThreadsARB;
RGLSYMGLGETGRAPHICSRESETSTATUSARBPROC __rglgen_glGetGraphicsResetStatusARB;
RGLSYMGLGETNTEXIMAGEARBPROC __rglgen_glGetnTexImageARB;
RGLSYMGLREADNPIXELSARBPROC __rglgen_glReadnPixelsARB;
RGLSYMGLGETNCOMPRESSEDTEXIMAGEARBPROC __rglgen_glGetnCompressedTexImageARB;
RGLSYMGLGETNUNIFORMFVARBPROC __rglgen_glGetnUniformfvARB;
RGLSYMGLGETNUNIFORMIVARBPROC __rglgen_glGetnUniformivARB;
RGLSYMGLGETNUNIFORMUIVARBPROC __rglgen_glGetnUniformuivARB;
RGLSYMGLGETNUNIFORMDVARBPROC __rglgen_glGetnUniformdvARB;
RGLSYMGLFRAMEBUFFERSAMPLELOCATIONSFVARBPROC __rglgen_glFramebufferSampleLocationsfvARB;
RGLSYMGLNAMEDFRAMEBUFFERSAMPLELOCATIONSFVARBPROC __rglgen_glNamedFramebufferSampleLocationsfvARB;
RGLSYMGLEVALUATEDEPTHVALUESARBPROC __rglgen_glEvaluateDepthValuesARB;
RGLSYMGLMINSAMPLESHADINGARBPROC __rglgen_glMinSampleShadingARB;
RGLSYMGLNAMEDSTRINGARBPROC __rglgen_glNamedStringARB;
RGLSYMGLDELETENAMEDSTRINGARBPROC __rglgen_glDeleteNamedStringARB;
RGLSYMGLCOMPILESHADERINCLUDEARBPROC __rglgen_glCompileShaderIncludeARB;
RGLSYMGLISNAMEDSTRINGARBPROC __rglgen_glIsNamedStringARB;
RGLSYMGLGETNAMEDSTRINGARBPROC __rglgen_glGetNamedStringARB;
RGLSYMGLGETNAMEDSTRINGIVARBPROC __rglgen_glGetNamedStringivARB;
RGLSYMGLBUFFERPAGECOMMITMENTARBPROC __rglgen_glBufferPageCommitmentARB;
RGLSYMGLNAMEDBUFFERPAGECOMMITMENTEXTPROC __rglgen_glNamedBufferPageCommitmentEXT;
RGLSYMGLNAMEDBUFFERPAGECOMMITMENTARBPROC __rglgen_glNamedBufferPageCommitmentARB;
RGLSYMGLTEXPAGECOMMITMENTARBPROC __rglgen_glTexPageCommitmentARB;
RGLSYMGLTEXBUFFERARBPROC __rglgen_glTexBufferARB;
RGLSYMGLBLENDBARRIERKHRPROC __rglgen_glBlendBarrierKHR;
RGLSYMGLMAXSHADERCOMPILERTHREADSKHRPROC __rglgen_glMaxShaderCompilerThreadsKHR;
RGLSYMGLEGLIMAGETARGETTEXSTORAGEEXTPROC __rglgen_glEGLImageTargetTexStorageEXT;
RGLSYMGLEGLIMAGETARGETTEXTURESTORAGEEXTPROC __rglgen_glEGLImageTargetTextureStorageEXT;
RGLSYMGLLABELOBJECTEXTPROC __rglgen_glLabelObjectEXT;
RGLSYMGLGETOBJECTLABELEXTPROC __rglgen_glGetObjectLabelEXT;
RGLSYMGLINSERTEVENTMARKEREXTPROC __rglgen_glInsertEventMarkerEXT;
RGLSYMGLPUSHGROUPMARKEREXTPROC __rglgen_glPushGroupMarkerEXT;
RGLSYMGLPOPGROUPMARKEREXTPROC __rglgen_glPopGroupMarkerEXT;
RGLSYMGLMATRIXLOADFEXTPROC __rglgen_glMatrixLoadfEXT;
RGLSYMGLMATRIXLOADDEXTPROC __rglgen_glMatrixLoaddEXT;
RGLSYMGLMATRIXMULTFEXTPROC __rglgen_glMatrixMultfEXT;
RGLSYMGLMATRIXMULTDEXTPROC __rglgen_glMatrixMultdEXT;
RGLSYMGLMATRIXLOADIDENTITYEXTPROC __rglgen_glMatrixLoadIdentityEXT;
RGLSYMGLMATRIXROTATEFEXTPROC __rglgen_glMatrixRotatefEXT;
RGLSYMGLMATRIXROTATEDEXTPROC __rglgen_glMatrixRotatedEXT;
RGLSYMGLMATRIXSCALEFEXTPROC __rglgen_glMatrixScalefEXT;
RGLSYMGLMATRIXSCALEDEXTPROC __rglgen_glMatrixScaledEXT;
RGLSYMGLMATRIXTRANSLATEFEXTPROC __rglgen_glMatrixTranslatefEXT;
RGLSYMGLMATRIXTRANSLATEDEXTPROC __rglgen_glMatrixTranslatedEXT;
RGLSYMGLMATRIXFRUSTUMEXTPROC __rglgen_glMatrixFrustumEXT;
RGLSYMGLMATRIXORTHOEXTPROC __rglgen_glMatrixOrthoEXT;
RGLSYMGLMATRIXPOPEXTPROC __rglgen_glMatrixPopEXT;
RGLSYMGLMATRIXPUSHEXTPROC __rglgen_glMatrixPushEXT;
RGLSYMGLCLIENTATTRIBDEFAULTEXTPROC __rglgen_glClientAttribDefaultEXT;
RGLSYMGLPUSHCLIENTATTRIBDEFAULTEXTPROC __rglgen_glPushClientAttribDefaultEXT;
RGLSYMGLTEXTUREPARAMETERFEXTPROC __rglgen_glTextureParameterfEXT;
RGLSYMGLTEXTUREPARAMETERFVEXTPROC __rglgen_glTextureParameterfvEXT;
RGLSYMGLTEXTUREPARAMETERIEXTPROC __rglgen_glTextureParameteriEXT;
RGLSYMGLTEXTUREPARAMETERIVEXTPROC __rglgen_glTextureParameterivEXT;
RGLSYMGLTEXTUREIMAGE1DEXTPROC __rglgen_glTextureImage1DEXT;
RGLSYMGLTEXTUREIMAGE2DEXTPROC __rglgen_glTextureImage2DEXT;
RGLSYMGLTEXTURESUBIMAGE1DEXTPROC __rglgen_glTextureSubImage1DEXT;
RGLSYMGLTEXTURESUBIMAGE2DEXTPROC __rglgen_glTextureSubImage2DEXT;
RGLSYMGLCOPYTEXTUREIMAGE1DEXTPROC __rglgen_glCopyTextureImage1DEXT;
RGLSYMGLCOPYTEXTUREIMAGE2DEXTPROC __rglgen_glCopyTextureImage2DEXT;
RGLSYMGLCOPYTEXTURESUBIMAGE1DEXTPROC __rglgen_glCopyTextureSubImage1DEXT;
RGLSYMGLCOPYTEXTURESUBIMAGE2DEXTPROC __rglgen_glCopyTextureSubImage2DEXT;
RGLSYMGLGETTEXTUREIMAGEEXTPROC __rglgen_glGetTextureImageEXT;
RGLSYMGLGETTEXTUREPARAMETERFVEXTPROC __rglgen_glGetTextureParameterfvEXT;
RGLSYMGLGETTEXTUREPARAMETERIVEXTPROC __rglgen_glGetTextureParameterivEXT;
RGLSYMGLGETTEXTURELEVELPARAMETERFVEXTPROC __rglgen_glGetTextureLevelParameterfvEXT;
RGLSYMGLGETTEXTURELEVELPARAMETERIVEXTPROC __rglgen_glGetTextureLevelParameterivEXT;
RGLSYMGLTEXTUREIMAGE3DEXTPROC __rglgen_glTextureImage3DEXT;
RGLSYMGLTEXTURESUBIMAGE3DEXTPROC __rglgen_glTextureSubImage3DEXT;
RGLSYMGLCOPYTEXTURESUBIMAGE3DEXTPROC __rglgen_glCopyTextureSubImage3DEXT;
RGLSYMGLBINDMULTITEXTUREEXTPROC __rglgen_glBindMultiTextureEXT;
RGLSYMGLMULTITEXCOORDPOINTEREXTPROC __rglgen_glMultiTexCoordPointerEXT;
RGLSYMGLMULTITEXENVFEXTPROC __rglgen_glMultiTexEnvfEXT;
RGLSYMGLMULTITEXENVFVEXTPROC __rglgen_glMultiTexEnvfvEXT;
RGLSYMGLMULTITEXENVIEXTPROC __rglgen_glMultiTexEnviEXT;
RGLSYMGLMULTITEXENVIVEXTPROC __rglgen_glMultiTexEnvivEXT;
RGLSYMGLMULTITEXGENDEXTPROC __rglgen_glMultiTexGendEXT;
RGLSYMGLMULTITEXGENDVEXTPROC __rglgen_glMultiTexGendvEXT;
RGLSYMGLMULTITEXGENFEXTPROC __rglgen_glMultiTexGenfEXT;
RGLSYMGLMULTITEXGENFVEXTPROC __rglgen_glMultiTexGenfvEXT;
RGLSYMGLMULTITEXGENIEXTPROC __rglgen_glMultiTexGeniEXT;
RGLSYMGLMULTITEXGENIVEXTPROC __rglgen_glMultiTexGenivEXT;
RGLSYMGLGETMULTITEXENVFVEXTPROC __rglgen_glGetMultiTexEnvfvEXT;
RGLSYMGLGETMULTITEXENVIVEXTPROC __rglgen_glGetMultiTexEnvivEXT;
RGLSYMGLGETMULTITEXGENDVEXTPROC __rglgen_glGetMultiTexGendvEXT;
RGLSYMGLGETMULTITEXGENFVEXTPROC __rglgen_glGetMultiTexGenfvEXT;
RGLSYMGLGETMULTITEXGENIVEXTPROC __rglgen_glGetMultiTexGenivEXT;
RGLSYMGLMULTITEXPARAMETERIEXTPROC __rglgen_glMultiTexParameteriEXT;
RGLSYMGLMULTITEXPARAMETERIVEXTPROC __rglgen_glMultiTexParameterivEXT;
RGLSYMGLMULTITEXPARAMETERFEXTPROC __rglgen_glMultiTexParameterfEXT;
RGLSYMGLMULTITEXPARAMETERFVEXTPROC __rglgen_glMultiTexParameterfvEXT;
RGLSYMGLMULTITEXIMAGE1DEXTPROC __rglgen_glMultiTexImage1DEXT;
RGLSYMGLMULTITEXIMAGE2DEXTPROC __rglgen_glMultiTexImage2DEXT;
RGLSYMGLMULTITEXSUBIMAGE1DEXTPROC __rglgen_glMultiTexSubImage1DEXT;
RGLSYMGLMULTITEXSUBIMAGE2DEXTPROC __rglgen_glMultiTexSubImage2DEXT;
RGLSYMGLCOPYMULTITEXIMAGE1DEXTPROC __rglgen_glCopyMultiTexImage1DEXT;
RGLSYMGLCOPYMULTITEXIMAGE2DEXTPROC __rglgen_glCopyMultiTexImage2DEXT;
RGLSYMGLCOPYMULTITEXSUBIMAGE1DEXTPROC __rglgen_glCopyMultiTexSubImage1DEXT;
RGLSYMGLCOPYMULTITEXSUBIMAGE2DEXTPROC __rglgen_glCopyMultiTexSubImage2DEXT;
RGLSYMGLGETMULTITEXIMAGEEXTPROC __rglgen_glGetMultiTexImageEXT;
RGLSYMGLGETMULTITEXPARAMETERFVEXTPROC __rglgen_glGetMultiTexParameterfvEXT;
RGLSYMGLGETMULTITEXPARAMETERIVEXTPROC __rglgen_glGetMultiTexParameterivEXT;
RGLSYMGLGETMULTITEXLEVELPARAMETERFVEXTPROC __rglgen_glGetMultiTexLevelParameterfvEXT;
RGLSYMGLGETMULTITEXLEVELPARAMETERIVEXTPROC __rglgen_glGetMultiTexLevelParameterivEXT;
RGLSYMGLMULTITEXIMAGE3DEXTPROC __rglgen_glMultiTexImage3DEXT;
RGLSYMGLMULTITEXSUBIMAGE3DEXTPROC __rglgen_glMultiTexSubImage3DEXT;
RGLSYMGLCOPYMULTITEXSUBIMAGE3DEXTPROC __rglgen_glCopyMultiTexSubImage3DEXT;
RGLSYMGLENABLECLIENTSTATEINDEXEDEXTPROC __rglgen_glEnableClientStateIndexedEXT;
RGLSYMGLDISABLECLIENTSTATEINDEXEDEXTPROC __rglgen_glDisableClientStateIndexedEXT;
RGLSYMGLGETFLOATINDEXEDVEXTPROC __rglgen_glGetFloatIndexedvEXT;
RGLSYMGLGETDOUBLEINDEXEDVEXTPROC __rglgen_glGetDoubleIndexedvEXT;
RGLSYMGLGETPOINTERINDEXEDVEXTPROC __rglgen_glGetPointerIndexedvEXT;
RGLSYMGLENABLEINDEXEDEXTPROC __rglgen_glEnableIndexedEXT;
RGLSYMGLDISABLEINDEXEDEXTPROC __rglgen_glDisableIndexedEXT;
RGLSYMGLISENABLEDINDEXEDEXTPROC __rglgen_glIsEnabledIndexedEXT;
RGLSYMGLGETINTEGERINDEXEDVEXTPROC __rglgen_glGetIntegerIndexedvEXT;
RGLSYMGLGETBOOLEANINDEXEDVEXTPROC __rglgen_glGetBooleanIndexedvEXT;
RGLSYMGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC __rglgen_glCompressedTextureImage3DEXT;
RGLSYMGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC __rglgen_glCompressedTextureImage2DEXT;
RGLSYMGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC __rglgen_glCompressedTextureImage1DEXT;
RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC __rglgen_glCompressedTextureSubImage3DEXT;
RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC __rglgen_glCompressedTextureSubImage2DEXT;
RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC __rglgen_glCompressedTextureSubImage1DEXT;
RGLSYMGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC __rglgen_glGetCompressedTextureImageEXT;
RGLSYMGLCOMPRESSEDMULTITEXIMAGE3DEXTPROC __rglgen_glCompressedMultiTexImage3DEXT;
RGLSYMGLCOMPRESSEDMULTITEXIMAGE2DEXTPROC __rglgen_glCompressedMultiTexImage2DEXT;
RGLSYMGLCOMPRESSEDMULTITEXIMAGE1DEXTPROC __rglgen_glCompressedMultiTexImage1DEXT;
RGLSYMGLCOMPRESSEDMULTITEXSUBIMAGE3DEXTPROC __rglgen_glCompressedMultiTexSubImage3DEXT;
RGLSYMGLCOMPRESSEDMULTITEXSUBIMAGE2DEXTPROC __rglgen_glCompressedMultiTexSubImage2DEXT;
RGLSYMGLCOMPRESSEDMULTITEXSUBIMAGE1DEXTPROC __rglgen_glCompressedMultiTexSubImage1DEXT;
RGLSYMGLGETCOMPRESSEDMULTITEXIMAGEEXTPROC __rglgen_glGetCompressedMultiTexImageEXT;
RGLSYMGLMATRIXLOADTRANSPOSEFEXTPROC __rglgen_glMatrixLoadTransposefEXT;
RGLSYMGLMATRIXLOADTRANSPOSEDEXTPROC __rglgen_glMatrixLoadTransposedEXT;
RGLSYMGLMATRIXMULTTRANSPOSEFEXTPROC __rglgen_glMatrixMultTransposefEXT;
RGLSYMGLMATRIXMULTTRANSPOSEDEXTPROC __rglgen_glMatrixMultTransposedEXT;
RGLSYMGLNAMEDBUFFERDATAEXTPROC __rglgen_glNamedBufferDataEXT;
RGLSYMGLNAMEDBUFFERSUBDATAEXTPROC __rglgen_glNamedBufferSubDataEXT;
RGLSYMGLMAPNAMEDBUFFEREXTPROC __rglgen_glMapNamedBufferEXT;
RGLSYMGLUNMAPNAMEDBUFFEREXTPROC __rglgen_glUnmapNamedBufferEXT;
RGLSYMGLGETNAMEDBUFFERPARAMETERIVEXTPROC __rglgen_glGetNamedBufferParameterivEXT;
RGLSYMGLGETNAMEDBUFFERPOINTERVEXTPROC __rglgen_glGetNamedBufferPointervEXT;
RGLSYMGLGETNAMEDBUFFERSUBDATAEXTPROC __rglgen_glGetNamedBufferSubDataEXT;
RGLSYMGLPROGRAMUNIFORM1FEXTPROC __rglgen_glProgramUniform1fEXT;
RGLSYMGLPROGRAMUNIFORM2FEXTPROC __rglgen_glProgramUniform2fEXT;
RGLSYMGLPROGRAMUNIFORM3FEXTPROC __rglgen_glProgramUniform3fEXT;
RGLSYMGLPROGRAMUNIFORM4FEXTPROC __rglgen_glProgramUniform4fEXT;
RGLSYMGLPROGRAMUNIFORM1IEXTPROC __rglgen_glProgramUniform1iEXT;
RGLSYMGLPROGRAMUNIFORM2IEXTPROC __rglgen_glProgramUniform2iEXT;
RGLSYMGLPROGRAMUNIFORM3IEXTPROC __rglgen_glProgramUniform3iEXT;
RGLSYMGLPROGRAMUNIFORM4IEXTPROC __rglgen_glProgramUniform4iEXT;
RGLSYMGLPROGRAMUNIFORM1FVEXTPROC __rglgen_glProgramUniform1fvEXT;
RGLSYMGLPROGRAMUNIFORM2FVEXTPROC __rglgen_glProgramUniform2fvEXT;
RGLSYMGLPROGRAMUNIFORM3FVEXTPROC __rglgen_glProgramUniform3fvEXT;
RGLSYMGLPROGRAMUNIFORM4FVEXTPROC __rglgen_glProgramUniform4fvEXT;
RGLSYMGLPROGRAMUNIFORM1IVEXTPROC __rglgen_glProgramUniform1ivEXT;
RGLSYMGLPROGRAMUNIFORM2IVEXTPROC __rglgen_glProgramUniform2ivEXT;
RGLSYMGLPROGRAMUNIFORM3IVEXTPROC __rglgen_glProgramUniform3ivEXT;
RGLSYMGLPROGRAMUNIFORM4IVEXTPROC __rglgen_glProgramUniform4ivEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX2FVEXTPROC __rglgen_glProgramUniformMatrix2fvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX3FVEXTPROC __rglgen_glProgramUniformMatrix3fvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX4FVEXTPROC __rglgen_glProgramUniformMatrix4fvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC __rglgen_glProgramUniformMatrix2x3fvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC __rglgen_glProgramUniformMatrix3x2fvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC __rglgen_glProgramUniformMatrix2x4fvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC __rglgen_glProgramUniformMatrix4x2fvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC __rglgen_glProgramUniformMatrix3x4fvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC __rglgen_glProgramUniformMatrix4x3fvEXT;
RGLSYMGLTEXTUREBUFFEREXTPROC __rglgen_glTextureBufferEXT;
RGLSYMGLMULTITEXBUFFEREXTPROC __rglgen_glMultiTexBufferEXT;
RGLSYMGLTEXTUREPARAMETERIIVEXTPROC __rglgen_glTextureParameterIivEXT;
RGLSYMGLTEXTUREPARAMETERIUIVEXTPROC __rglgen_glTextureParameterIuivEXT;
RGLSYMGLGETTEXTUREPARAMETERIIVEXTPROC __rglgen_glGetTextureParameterIivEXT;
RGLSYMGLGETTEXTUREPARAMETERIUIVEXTPROC __rglgen_glGetTextureParameterIuivEXT;
RGLSYMGLMULTITEXPARAMETERIIVEXTPROC __rglgen_glMultiTexParameterIivEXT;
RGLSYMGLMULTITEXPARAMETERIUIVEXTPROC __rglgen_glMultiTexParameterIuivEXT;
RGLSYMGLGETMULTITEXPARAMETERIIVEXTPROC __rglgen_glGetMultiTexParameterIivEXT;
RGLSYMGLGETMULTITEXPARAMETERIUIVEXTPROC __rglgen_glGetMultiTexParameterIuivEXT;
RGLSYMGLPROGRAMUNIFORM1UIEXTPROC __rglgen_glProgramUniform1uiEXT;
RGLSYMGLPROGRAMUNIFORM2UIEXTPROC __rglgen_glProgramUniform2uiEXT;
RGLSYMGLPROGRAMUNIFORM3UIEXTPROC __rglgen_glProgramUniform3uiEXT;
RGLSYMGLPROGRAMUNIFORM4UIEXTPROC __rglgen_glProgramUniform4uiEXT;
RGLSYMGLPROGRAMUNIFORM1UIVEXTPROC __rglgen_glProgramUniform1uivEXT;
RGLSYMGLPROGRAMUNIFORM2UIVEXTPROC __rglgen_glProgramUniform2uivEXT;
RGLSYMGLPROGRAMUNIFORM3UIVEXTPROC __rglgen_glProgramUniform3uivEXT;
RGLSYMGLPROGRAMUNIFORM4UIVEXTPROC __rglgen_glProgramUniform4uivEXT;
RGLSYMGLNAMEDPROGRAMLOCALPARAMETERS4FVEXTPROC __rglgen_glNamedProgramLocalParameters4fvEXT;
RGLSYMGLNAMEDPROGRAMLOCALPARAMETERI4IEXTPROC __rglgen_glNamedProgramLocalParameterI4iEXT;
RGLSYMGLNAMEDPROGRAMLOCALPARAMETERI4IVEXTPROC __rglgen_glNamedProgramLocalParameterI4ivEXT;
RGLSYMGLNAMEDPROGRAMLOCALPARAMETERSI4IVEXTPROC __rglgen_glNamedProgramLocalParametersI4ivEXT;
RGLSYMGLNAMEDPROGRAMLOCALPARAMETERI4UIEXTPROC __rglgen_glNamedProgramLocalParameterI4uiEXT;
RGLSYMGLNAMEDPROGRAMLOCALPARAMETERI4UIVEXTPROC __rglgen_glNamedProgramLocalParameterI4uivEXT;
RGLSYMGLNAMEDPROGRAMLOCALPARAMETERSI4UIVEXTPROC __rglgen_glNamedProgramLocalParametersI4uivEXT;
RGLSYMGLGETNAMEDPROGRAMLOCALPARAMETERIIVEXTPROC __rglgen_glGetNamedProgramLocalParameterIivEXT;
RGLSYMGLGETNAMEDPROGRAMLOCALPARAMETERIUIVEXTPROC __rglgen_glGetNamedProgramLocalParameterIuivEXT;
RGLSYMGLENABLECLIENTSTATEIEXTPROC __rglgen_glEnableClientStateiEXT;
RGLSYMGLDISABLECLIENTSTATEIEXTPROC __rglgen_glDisableClientStateiEXT;
RGLSYMGLGETFLOATI_VEXTPROC __rglgen_glGetFloati_vEXT;
RGLSYMGLGETDOUBLEI_VEXTPROC __rglgen_glGetDoublei_vEXT;
RGLSYMGLGETPOINTERI_VEXTPROC __rglgen_glGetPointeri_vEXT;
RGLSYMGLNAMEDPROGRAMSTRINGEXTPROC __rglgen_glNamedProgramStringEXT;
RGLSYMGLNAMEDPROGRAMLOCALPARAMETER4DEXTPROC __rglgen_glNamedProgramLocalParameter4dEXT;
RGLSYMGLNAMEDPROGRAMLOCALPARAMETER4DVEXTPROC __rglgen_glNamedProgramLocalParameter4dvEXT;
RGLSYMGLNAMEDPROGRAMLOCALPARAMETER4FEXTPROC __rglgen_glNamedProgramLocalParameter4fEXT;
RGLSYMGLNAMEDPROGRAMLOCALPARAMETER4FVEXTPROC __rglgen_glNamedProgramLocalParameter4fvEXT;
RGLSYMGLGETNAMEDPROGRAMLOCALPARAMETERDVEXTPROC __rglgen_glGetNamedProgramLocalParameterdvEXT;
RGLSYMGLGETNAMEDPROGRAMLOCALPARAMETERFVEXTPROC __rglgen_glGetNamedProgramLocalParameterfvEXT;
RGLSYMGLGETNAMEDPROGRAMIVEXTPROC __rglgen_glGetNamedProgramivEXT;
RGLSYMGLGETNAMEDPROGRAMSTRINGEXTPROC __rglgen_glGetNamedProgramStringEXT;
RGLSYMGLNAMEDRENDERBUFFERSTORAGEEXTPROC __rglgen_glNamedRenderbufferStorageEXT;
RGLSYMGLGETNAMEDRENDERBUFFERPARAMETERIVEXTPROC __rglgen_glGetNamedRenderbufferParameterivEXT;
RGLSYMGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC __rglgen_glNamedRenderbufferStorageMultisampleEXT;
RGLSYMGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLECOVERAGEEXTPROC __rglgen_glNamedRenderbufferStorageMultisampleCoverageEXT;
RGLSYMGLCHECKNAMEDFRAMEBUFFERSTATUSEXTPROC __rglgen_glCheckNamedFramebufferStatusEXT;
RGLSYMGLNAMEDFRAMEBUFFERTEXTURE1DEXTPROC __rglgen_glNamedFramebufferTexture1DEXT;
RGLSYMGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC __rglgen_glNamedFramebufferTexture2DEXT;
RGLSYMGLNAMEDFRAMEBUFFERTEXTURE3DEXTPROC __rglgen_glNamedFramebufferTexture3DEXT;
RGLSYMGLNAMEDFRAMEBUFFERRENDERBUFFEREXTPROC __rglgen_glNamedFramebufferRenderbufferEXT;
RGLSYMGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC __rglgen_glGetNamedFramebufferAttachmentParameterivEXT;
RGLSYMGLGENERATETEXTUREMIPMAPEXTPROC __rglgen_glGenerateTextureMipmapEXT;
RGLSYMGLGENERATEMULTITEXMIPMAPEXTPROC __rglgen_glGenerateMultiTexMipmapEXT;
RGLSYMGLFRAMEBUFFERDRAWBUFFEREXTPROC __rglgen_glFramebufferDrawBufferEXT;
RGLSYMGLFRAMEBUFFERDRAWBUFFERSEXTPROC __rglgen_glFramebufferDrawBuffersEXT;
RGLSYMGLFRAMEBUFFERREADBUFFEREXTPROC __rglgen_glFramebufferReadBufferEXT;
RGLSYMGLGETFRAMEBUFFERPARAMETERIVEXTPROC __rglgen_glGetFramebufferParameterivEXT;
RGLSYMGLNAMEDCOPYBUFFERSUBDATAEXTPROC __rglgen_glNamedCopyBufferSubDataEXT;
RGLSYMGLNAMEDFRAMEBUFFERTEXTUREEXTPROC __rglgen_glNamedFramebufferTextureEXT;
RGLSYMGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC __rglgen_glNamedFramebufferTextureLayerEXT;
RGLSYMGLNAMEDFRAMEBUFFERTEXTUREFACEEXTPROC __rglgen_glNamedFramebufferTextureFaceEXT;
RGLSYMGLTEXTURERENDERBUFFEREXTPROC __rglgen_glTextureRenderbufferEXT;
RGLSYMGLMULTITEXRENDERBUFFEREXTPROC __rglgen_glMultiTexRenderbufferEXT;
RGLSYMGLVERTEXARRAYVERTEXOFFSETEXTPROC __rglgen_glVertexArrayVertexOffsetEXT;
RGLSYMGLVERTEXARRAYCOLOROFFSETEXTPROC __rglgen_glVertexArrayColorOffsetEXT;
RGLSYMGLVERTEXARRAYEDGEFLAGOFFSETEXTPROC __rglgen_glVertexArrayEdgeFlagOffsetEXT;
RGLSYMGLVERTEXARRAYINDEXOFFSETEXTPROC __rglgen_glVertexArrayIndexOffsetEXT;
RGLSYMGLVERTEXARRAYNORMALOFFSETEXTPROC __rglgen_glVertexArrayNormalOffsetEXT;
RGLSYMGLVERTEXARRAYTEXCOORDOFFSETEXTPROC __rglgen_glVertexArrayTexCoordOffsetEXT;
RGLSYMGLVERTEXARRAYMULTITEXCOORDOFFSETEXTPROC __rglgen_glVertexArrayMultiTexCoordOffsetEXT;
RGLSYMGLVERTEXARRAYFOGCOORDOFFSETEXTPROC __rglgen_glVertexArrayFogCoordOffsetEXT;
RGLSYMGLVERTEXARRAYSECONDARYCOLOROFFSETEXTPROC __rglgen_glVertexArraySecondaryColorOffsetEXT;
RGLSYMGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC __rglgen_glVertexArrayVertexAttribOffsetEXT;
RGLSYMGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC __rglgen_glVertexArrayVertexAttribIOffsetEXT;
RGLSYMGLENABLEVERTEXARRAYEXTPROC __rglgen_glEnableVertexArrayEXT;
RGLSYMGLDISABLEVERTEXARRAYEXTPROC __rglgen_glDisableVertexArrayEXT;
RGLSYMGLENABLEVERTEXARRAYATTRIBEXTPROC __rglgen_glEnableVertexArrayAttribEXT;
RGLSYMGLDISABLEVERTEXARRAYATTRIBEXTPROC __rglgen_glDisableVertexArrayAttribEXT;
RGLSYMGLGETVERTEXARRAYINTEGERVEXTPROC __rglgen_glGetVertexArrayIntegervEXT;
RGLSYMGLGETVERTEXARRAYPOINTERVEXTPROC __rglgen_glGetVertexArrayPointervEXT;
RGLSYMGLGETVERTEXARRAYINTEGERI_VEXTPROC __rglgen_glGetVertexArrayIntegeri_vEXT;
RGLSYMGLGETVERTEXARRAYPOINTERI_VEXTPROC __rglgen_glGetVertexArrayPointeri_vEXT;
RGLSYMGLMAPNAMEDBUFFERRANGEEXTPROC __rglgen_glMapNamedBufferRangeEXT;
RGLSYMGLFLUSHMAPPEDNAMEDBUFFERRANGEEXTPROC __rglgen_glFlushMappedNamedBufferRangeEXT;
RGLSYMGLNAMEDBUFFERSTORAGEEXTPROC __rglgen_glNamedBufferStorageEXT;
RGLSYMGLCLEARNAMEDBUFFERDATAEXTPROC __rglgen_glClearNamedBufferDataEXT;
RGLSYMGLCLEARNAMEDBUFFERSUBDATAEXTPROC __rglgen_glClearNamedBufferSubDataEXT;
RGLSYMGLNAMEDFRAMEBUFFERPARAMETERIEXTPROC __rglgen_glNamedFramebufferParameteriEXT;
RGLSYMGLGETNAMEDFRAMEBUFFERPARAMETERIVEXTPROC __rglgen_glGetNamedFramebufferParameterivEXT;
RGLSYMGLPROGRAMUNIFORM1DEXTPROC __rglgen_glProgramUniform1dEXT;
RGLSYMGLPROGRAMUNIFORM2DEXTPROC __rglgen_glProgramUniform2dEXT;
RGLSYMGLPROGRAMUNIFORM3DEXTPROC __rglgen_glProgramUniform3dEXT;
RGLSYMGLPROGRAMUNIFORM4DEXTPROC __rglgen_glProgramUniform4dEXT;
RGLSYMGLPROGRAMUNIFORM1DVEXTPROC __rglgen_glProgramUniform1dvEXT;
RGLSYMGLPROGRAMUNIFORM2DVEXTPROC __rglgen_glProgramUniform2dvEXT;
RGLSYMGLPROGRAMUNIFORM3DVEXTPROC __rglgen_glProgramUniform3dvEXT;
RGLSYMGLPROGRAMUNIFORM4DVEXTPROC __rglgen_glProgramUniform4dvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX2DVEXTPROC __rglgen_glProgramUniformMatrix2dvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX3DVEXTPROC __rglgen_glProgramUniformMatrix3dvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX4DVEXTPROC __rglgen_glProgramUniformMatrix4dvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX2X3DVEXTPROC __rglgen_glProgramUniformMatrix2x3dvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX2X4DVEXTPROC __rglgen_glProgramUniformMatrix2x4dvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX3X2DVEXTPROC __rglgen_glProgramUniformMatrix3x2dvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX3X4DVEXTPROC __rglgen_glProgramUniformMatrix3x4dvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX4X2DVEXTPROC __rglgen_glProgramUniformMatrix4x2dvEXT;
RGLSYMGLPROGRAMUNIFORMMATRIX4X3DVEXTPROC __rglgen_glProgramUniformMatrix4x3dvEXT;
RGLSYMGLTEXTUREBUFFERRANGEEXTPROC __rglgen_glTextureBufferRangeEXT;
RGLSYMGLTEXTURESTORAGE1DEXTPROC __rglgen_glTextureStorage1DEXT;
RGLSYMGLTEXTURESTORAGE2DEXTPROC __rglgen_glTextureStorage2DEXT;
RGLSYMGLTEXTURESTORAGE3DEXTPROC __rglgen_glTextureStorage3DEXT;
RGLSYMGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC __rglgen_glTextureStorage2DMultisampleEXT;
RGLSYMGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC __rglgen_glTextureStorage3DMultisampleEXT;
RGLSYMGLVERTEXARRAYBINDVERTEXBUFFEREXTPROC __rglgen_glVertexArrayBindVertexBufferEXT;
RGLSYMGLVERTEXARRAYVERTEXATTRIBFORMATEXTPROC __rglgen_glVertexArrayVertexAttribFormatEXT;
RGLSYMGLVERTEXARRAYVERTEXATTRIBIFORMATEXTPROC __rglgen_glVertexArrayVertexAttribIFormatEXT;
RGLSYMGLVERTEXARRAYVERTEXATTRIBLFORMATEXTPROC __rglgen_glVertexArrayVertexAttribLFormatEXT;
RGLSYMGLVERTEXARRAYVERTEXATTRIBBINDINGEXTPROC __rglgen_glVertexArrayVertexAttribBindingEXT;
RGLSYMGLVERTEXARRAYVERTEXBINDINGDIVISOREXTPROC __rglgen_glVertexArrayVertexBindingDivisorEXT;
RGLSYMGLVERTEXARRAYVERTEXATTRIBLOFFSETEXTPROC __rglgen_glVertexArrayVertexAttribLOffsetEXT;
RGLSYMGLTEXTUREPAGECOMMITMENTEXTPROC __rglgen_glTexturePageCommitmentEXT;
RGLSYMGLVERTEXARRAYVERTEXATTRIBDIVISOREXTPROC __rglgen_glVertexArrayVertexAttribDivisorEXT;
RGLSYMGLDRAWARRAYSINSTANCEDEXTPROC __rglgen_glDrawArraysInstancedEXT;
RGLSYMGLDRAWELEMENTSINSTANCEDEXTPROC __rglgen_glDrawElementsInstancedEXT;
RGLSYMGLPOLYGONOFFSETCLAMPEXTPROC __rglgen_glPolygonOffsetClampEXT;
RGLSYMGLRASTERSAMPLESEXTPROC __rglgen_glRasterSamplesEXT;
RGLSYMGLUSESHADERPROGRAMEXTPROC __rglgen_glUseShaderProgramEXT;
RGLSYMGLACTIVEPROGRAMEXTPROC __rglgen_glActiveProgramEXT;
RGLSYMGLCREATESHADERPROGRAMEXTPROC __rglgen_glCreateShaderProgramEXT;
RGLSYMGLFRAMEBUFFERFETCHBARRIEREXTPROC __rglgen_glFramebufferFetchBarrierEXT;
RGLSYMGLWINDOWRECTANGLESEXTPROC __rglgen_glWindowRectanglesEXT;
RGLSYMGLMULTIDRAWARRAYSINDIRECTBINDLESSNVPROC __rglgen_glMultiDrawArraysIndirectBindlessNV;
RGLSYMGLMULTIDRAWELEMENTSINDIRECTBINDLESSNVPROC __rglgen_glMultiDrawElementsIndirectBindlessNV;
RGLSYMGLMULTIDRAWARRAYSINDIRECTBINDLESSCOUNTNVPROC __rglgen_glMultiDrawArraysIndirectBindlessCountNV;
RGLSYMGLMULTIDRAWELEMENTSINDIRECTBINDLESSCOUNTNVPROC __rglgen_glMultiDrawElementsIndirectBindlessCountNV;
RGLSYMGLGETTEXTUREHANDLENVPROC __rglgen_glGetTextureHandleNV;
RGLSYMGLGETTEXTURESAMPLERHANDLENVPROC __rglgen_glGetTextureSamplerHandleNV;
RGLSYMGLMAKETEXTUREHANDLERESIDENTNVPROC __rglgen_glMakeTextureHandleResidentNV;
RGLSYMGLMAKETEXTUREHANDLENONRESIDENTNVPROC __rglgen_glMakeTextureHandleNonResidentNV;
RGLSYMGLGETIMAGEHANDLENVPROC __rglgen_glGetImageHandleNV;
RGLSYMGLMAKEIMAGEHANDLERESIDENTNVPROC __rglgen_glMakeImageHandleResidentNV;
RGLSYMGLMAKEIMAGEHANDLENONRESIDENTNVPROC __rglgen_glMakeImageHandleNonResidentNV;
RGLSYMGLUNIFORMHANDLEUI64NVPROC __rglgen_glUniformHandleui64NV;
RGLSYMGLUNIFORMHANDLEUI64VNVPROC __rglgen_glUniformHandleui64vNV;
RGLSYMGLPROGRAMUNIFORMHANDLEUI64NVPROC __rglgen_glProgramUniformHandleui64NV;
RGLSYMGLPROGRAMUNIFORMHANDLEUI64VNVPROC __rglgen_glProgramUniformHandleui64vNV;
RGLSYMGLISTEXTUREHANDLERESIDENTNVPROC __rglgen_glIsTextureHandleResidentNV;
RGLSYMGLISIMAGEHANDLERESIDENTNVPROC __rglgen_glIsImageHandleResidentNV;
RGLSYMGLBLENDPARAMETERINVPROC __rglgen_glBlendParameteriNV;
RGLSYMGLBLENDBARRIERNVPROC __rglgen_glBlendBarrierNV;
RGLSYMGLVIEWPORTPOSITIONWSCALENVPROC __rglgen_glViewportPositionWScaleNV;
RGLSYMGLCREATESTATESNVPROC __rglgen_glCreateStatesNV;
RGLSYMGLDELETESTATESNVPROC __rglgen_glDeleteStatesNV;
RGLSYMGLISSTATENVPROC __rglgen_glIsStateNV;
RGLSYMGLSTATECAPTURENVPROC __rglgen_glStateCaptureNV;
RGLSYMGLGETCOMMANDHEADERNVPROC __rglgen_glGetCommandHeaderNV;
RGLSYMGLGETSTAGEINDEXNVPROC __rglgen_glGetStageIndexNV;
RGLSYMGLDRAWCOMMANDSNVPROC __rglgen_glDrawCommandsNV;
RGLSYMGLDRAWCOMMANDSADDRESSNVPROC __rglgen_glDrawCommandsAddressNV;
RGLSYMGLDRAWCOMMANDSSTATESNVPROC __rglgen_glDrawCommandsStatesNV;
RGLSYMGLDRAWCOMMANDSSTATESADDRESSNVPROC __rglgen_glDrawCommandsStatesAddressNV;
RGLSYMGLCREATECOMMANDLISTSNVPROC __rglgen_glCreateCommandListsNV;
RGLSYMGLDELETECOMMANDLISTSNVPROC __rglgen_glDeleteCommandListsNV;
RGLSYMGLISCOMMANDLISTNVPROC __rglgen_glIsCommandListNV;
RGLSYMGLLISTDRAWCOMMANDSSTATESCLIENTNVPROC __rglgen_glListDrawCommandsStatesClientNV;
RGLSYMGLCOMMANDLISTSEGMENTSNVPROC __rglgen_glCommandListSegmentsNV;
RGLSYMGLCOMPILECOMMANDLISTNVPROC __rglgen_glCompileCommandListNV;
RGLSYMGLCALLCOMMANDLISTNVPROC __rglgen_glCallCommandListNV;
RGLSYMGLBEGINCONDITIONALRENDERNVPROC __rglgen_glBeginConditionalRenderNV;
RGLSYMGLENDCONDITIONALRENDERNVPROC __rglgen_glEndConditionalRenderNV;
RGLSYMGLSUBPIXELPRECISIONBIASNVPROC __rglgen_glSubpixelPrecisionBiasNV;
RGLSYMGLCONSERVATIVERASTERPARAMETERFNVPROC __rglgen_glConservativeRasterParameterfNV;
RGLSYMGLCONSERVATIVERASTERPARAMETERINVPROC __rglgen_glConservativeRasterParameteriNV;
RGLSYMGLDRAWVKIMAGENVPROC __rglgen_glDrawVkImageNV;
RGLSYMGLWAITVKSEMAPHORENVPROC __rglgen_glWaitVkSemaphoreNV;
RGLSYMGLSIGNALVKSEMAPHORENVPROC __rglgen_glSignalVkSemaphoreNV;
RGLSYMGLSIGNALVKFENCENVPROC __rglgen_glSignalVkFenceNV;
RGLSYMGLFRAGMENTCOVERAGECOLORNVPROC __rglgen_glFragmentCoverageColorNV;
RGLSYMGLCOVERAGEMODULATIONTABLENVPROC __rglgen_glCoverageModulationTableNV;
RGLSYMGLGETCOVERAGEMODULATIONTABLENVPROC __rglgen_glGetCoverageModulationTableNV;
RGLSYMGLCOVERAGEMODULATIONNVPROC __rglgen_glCoverageModulationNV;
RGLSYMGLRENDERBUFFERSTORAGEMULTISAMPLECOVERAGENVPROC __rglgen_glRenderbufferStorageMultisampleCoverageNV;
RGLSYMGLUNIFORM1I64NVPROC __rglgen_glUniform1i64NV;
RGLSYMGLUNIFORM2I64NVPROC __rglgen_glUniform2i64NV;
RGLSYMGLUNIFORM3I64NVPROC __rglgen_glUniform3i64NV;
RGLSYMGLUNIFORM4I64NVPROC __rglgen_glUniform4i64NV;
RGLSYMGLUNIFORM1I64VNVPROC __rglgen_glUniform1i64vNV;
RGLSYMGLUNIFORM2I64VNVPROC __rglgen_glUniform2i64vNV;
RGLSYMGLUNIFORM3I64VNVPROC __rglgen_glUniform3i64vNV;
RGLSYMGLUNIFORM4I64VNVPROC __rglgen_glUniform4i64vNV;
RGLSYMGLUNIFORM1UI64NVPROC __rglgen_glUniform1ui64NV;
RGLSYMGLUNIFORM2UI64NVPROC __rglgen_glUniform2ui64NV;
RGLSYMGLUNIFORM3UI64NVPROC __rglgen_glUniform3ui64NV;
RGLSYMGLUNIFORM4UI64NVPROC __rglgen_glUniform4ui64NV;
RGLSYMGLUNIFORM1UI64VNVPROC __rglgen_glUniform1ui64vNV;
RGLSYMGLUNIFORM2UI64VNVPROC __rglgen_glUniform2ui64vNV;
RGLSYMGLUNIFORM3UI64VNVPROC __rglgen_glUniform3ui64vNV;
RGLSYMGLUNIFORM4UI64VNVPROC __rglgen_glUniform4ui64vNV;
RGLSYMGLGETUNIFORMI64VNVPROC __rglgen_glGetUniformi64vNV;
RGLSYMGLPROGRAMUNIFORM1I64NVPROC __rglgen_glProgramUniform1i64NV;
RGLSYMGLPROGRAMUNIFORM2I64NVPROC __rglgen_glProgramUniform2i64NV;
RGLSYMGLPROGRAMUNIFORM3I64NVPROC __rglgen_glProgramUniform3i64NV;
RGLSYMGLPROGRAMUNIFORM4I64NVPROC __rglgen_glProgramUniform4i64NV;
RGLSYMGLPROGRAMUNIFORM1I64VNVPROC __rglgen_glProgramUniform1i64vNV;
RGLSYMGLPROGRAMUNIFORM2I64VNVPROC __rglgen_glProgramUniform2i64vNV;
RGLSYMGLPROGRAMUNIFORM3I64VNVPROC __rglgen_glProgramUniform3i64vNV;
RGLSYMGLPROGRAMUNIFORM4I64VNVPROC __rglgen_glProgramUniform4i64vNV;
RGLSYMGLPROGRAMUNIFORM1UI64NVPROC __rglgen_glProgramUniform1ui64NV;
RGLSYMGLPROGRAMUNIFORM2UI64NVPROC __rglgen_glProgramUniform2ui64NV;
RGLSYMGLPROGRAMUNIFORM3UI64NVPROC __rglgen_glProgramUniform3ui64NV;
RGLSYMGLPROGRAMUNIFORM4UI64NVPROC __rglgen_glProgramUniform4ui64NV;
RGLSYMGLPROGRAMUNIFORM1UI64VNVPROC __rglgen_glProgramUniform1ui64vNV;
RGLSYMGLPROGRAMUNIFORM2UI64VNVPROC __rglgen_glProgramUniform2ui64vNV;
RGLSYMGLPROGRAMUNIFORM3UI64VNVPROC __rglgen_glProgramUniform3ui64vNV;
RGLSYMGLPROGRAMUNIFORM4UI64VNVPROC __rglgen_glProgramUniform4ui64vNV;
RGLSYMGLGETINTERNALFORMATSAMPLEIVNVPROC __rglgen_glGetInternalformatSampleivNV;
RGLSYMGLGETMEMORYOBJECTDETACHEDRESOURCESUIVNVPROC __rglgen_glGetMemoryObjectDetachedResourcesuivNV;
RGLSYMGLRESETMEMORYOBJECTPARAMETERNVPROC __rglgen_glResetMemoryObjectParameterNV;
RGLSYMGLTEXATTACHMEMORYNVPROC __rglgen_glTexAttachMemoryNV;
RGLSYMGLBUFFERATTACHMEMORYNVPROC __rglgen_glBufferAttachMemoryNV;
RGLSYMGLTEXTUREATTACHMEMORYNVPROC __rglgen_glTextureAttachMemoryNV;
RGLSYMGLNAMEDBUFFERATTACHMEMORYNVPROC __rglgen_glNamedBufferAttachMemoryNV;
RGLSYMGLDRAWMESHTASKSNVPROC __rglgen_glDrawMeshTasksNV;
RGLSYMGLDRAWMESHTASKSINDIRECTNVPROC __rglgen_glDrawMeshTasksIndirectNV;
RGLSYMGLMULTIDRAWMESHTASKSINDIRECTNVPROC __rglgen_glMultiDrawMeshTasksIndirectNV;
RGLSYMGLMULTIDRAWMESHTASKSINDIRECTCOUNTNVPROC __rglgen_glMultiDrawMeshTasksIndirectCountNV;
RGLSYMGLGENPATHSNVPROC __rglgen_glGenPathsNV;
RGLSYMGLDELETEPATHSNVPROC __rglgen_glDeletePathsNV;
RGLSYMGLISPATHNVPROC __rglgen_glIsPathNV;
RGLSYMGLPATHCOMMANDSNVPROC __rglgen_glPathCommandsNV;
RGLSYMGLPATHCOORDSNVPROC __rglgen_glPathCoordsNV;
RGLSYMGLPATHSUBCOMMANDSNVPROC __rglgen_glPathSubCommandsNV;
RGLSYMGLPATHSUBCOORDSNVPROC __rglgen_glPathSubCoordsNV;
RGLSYMGLPATHSTRINGNVPROC __rglgen_glPathStringNV;
RGLSYMGLPATHGLYPHSNVPROC __rglgen_glPathGlyphsNV;
RGLSYMGLPATHGLYPHRANGENVPROC __rglgen_glPathGlyphRangeNV;
RGLSYMGLWEIGHTPATHSNVPROC __rglgen_glWeightPathsNV;
RGLSYMGLCOPYPATHNVPROC __rglgen_glCopyPathNV;
RGLSYMGLINTERPOLATEPATHSNVPROC __rglgen_glInterpolatePathsNV;
RGLSYMGLTRANSFORMPATHNVPROC __rglgen_glTransformPathNV;
RGLSYMGLPATHPARAMETERIVNVPROC __rglgen_glPathParameterivNV;
RGLSYMGLPATHPARAMETERINVPROC __rglgen_glPathParameteriNV;
RGLSYMGLPATHPARAMETERFVNVPROC __rglgen_glPathParameterfvNV;
RGLSYMGLPATHPARAMETERFNVPROC __rglgen_glPathParameterfNV;
RGLSYMGLPATHDASHARRAYNVPROC __rglgen_glPathDashArrayNV;
RGLSYMGLPATHSTENCILFUNCNVPROC __rglgen_glPathStencilFuncNV;
RGLSYMGLPATHSTENCILDEPTHOFFSETNVPROC __rglgen_glPathStencilDepthOffsetNV;
RGLSYMGLSTENCILFILLPATHNVPROC __rglgen_glStencilFillPathNV;
RGLSYMGLSTENCILSTROKEPATHNVPROC __rglgen_glStencilStrokePathNV;
RGLSYMGLSTENCILFILLPATHINSTANCEDNVPROC __rglgen_glStencilFillPathInstancedNV;
RGLSYMGLSTENCILSTROKEPATHINSTANCEDNVPROC __rglgen_glStencilStrokePathInstancedNV;
RGLSYMGLPATHCOVERDEPTHFUNCNVPROC __rglgen_glPathCoverDepthFuncNV;
RGLSYMGLCOVERFILLPATHNVPROC __rglgen_glCoverFillPathNV;
RGLSYMGLCOVERSTROKEPATHNVPROC __rglgen_glCoverStrokePathNV;
RGLSYMGLCOVERFILLPATHINSTANCEDNVPROC __rglgen_glCoverFillPathInstancedNV;
RGLSYMGLCOVERSTROKEPATHINSTANCEDNVPROC __rglgen_glCoverStrokePathInstancedNV;
RGLSYMGLGETPATHPARAMETERIVNVPROC __rglgen_glGetPathParameterivNV;
RGLSYMGLGETPATHPARAMETERFVNVPROC __rglgen_glGetPathParameterfvNV;
RGLSYMGLGETPATHCOMMANDSNVPROC __rglgen_glGetPathCommandsNV;
RGLSYMGLGETPATHCOORDSNVPROC __rglgen_glGetPathCoordsNV;
RGLSYMGLGETPATHDASHARRAYNVPROC __rglgen_glGetPathDashArrayNV;
RGLSYMGLGETPATHMETRICSNVPROC __rglgen_glGetPathMetricsNV;
RGLSYMGLGETPATHMETRICRANGENVPROC __rglgen_glGetPathMetricRangeNV;
RGLSYMGLGETPATHSPACINGNVPROC __rglgen_glGetPathSpacingNV;
RGLSYMGLISPOINTINFILLPATHNVPROC __rglgen_glIsPointInFillPathNV;
RGLSYMGLISPOINTINSTROKEPATHNVPROC __rglgen_glIsPointInStrokePathNV;
RGLSYMGLGETPATHLENGTHNVPROC __rglgen_glGetPathLengthNV;
RGLSYMGLPOINTALONGPATHNVPROC __rglgen_glPointAlongPathNV;
RGLSYMGLMATRIXLOAD3X2FNVPROC __rglgen_glMatrixLoad3x2fNV;
RGLSYMGLMATRIXLOAD3X3FNVPROC __rglgen_glMatrixLoad3x3fNV;
RGLSYMGLMATRIXLOADTRANSPOSE3X3FNVPROC __rglgen_glMatrixLoadTranspose3x3fNV;
RGLSYMGLMATRIXMULT3X2FNVPROC __rglgen_glMatrixMult3x2fNV;
RGLSYMGLMATRIXMULT3X3FNVPROC __rglgen_glMatrixMult3x3fNV;
RGLSYMGLMATRIXMULTTRANSPOSE3X3FNVPROC __rglgen_glMatrixMultTranspose3x3fNV;
RGLSYMGLSTENCILTHENCOVERFILLPATHNVPROC __rglgen_glStencilThenCoverFillPathNV;
RGLSYMGLSTENCILTHENCOVERSTROKEPATHNVPROC __rglgen_glStencilThenCoverStrokePathNV;
RGLSYMGLSTENCILTHENCOVERFILLPATHINSTANCEDNVPROC __rglgen_glStencilThenCoverFillPathInstancedNV;
RGLSYMGLSTENCILTHENCOVERSTROKEPATHINSTANCEDNVPROC __rglgen_glStencilThenCoverStrokePathInstancedNV;
RGLSYMGLPATHGLYPHINDEXRANGENVPROC __rglgen_glPathGlyphIndexRangeNV;
RGLSYMGLPATHGLYPHINDEXARRAYNVPROC __rglgen_glPathGlyphIndexArrayNV;
RGLSYMGLPATHMEMORYGLYPHINDEXARRAYNVPROC __rglgen_glPathMemoryGlyphIndexArrayNV;
RGLSYMGLPROGRAMPATHFRAGMENTINPUTGENNVPROC __rglgen_glProgramPathFragmentInputGenNV;
RGLSYMGLGETPROGRAMRESOURCEFVNVPROC __rglgen_glGetProgramResourcefvNV;
RGLSYMGLFRAMEBUFFERSAMPLELOCATIONSFVNVPROC __rglgen_glFramebufferSampleLocationsfvNV;
RGLSYMGLNAMEDFRAMEBUFFERSAMPLELOCATIONSFVNVPROC __rglgen_glNamedFramebufferSampleLocationsfvNV;
RGLSYMGLRESOLVEDEPTHVALUESNVPROC __rglgen_glResolveDepthValuesNV;
RGLSYMGLSCISSOREXCLUSIVENVPROC __rglgen_glScissorExclusiveNV;
RGLSYMGLSCISSOREXCLUSIVEARRAYVNVPROC __rglgen_glScissorExclusiveArrayvNV;
RGLSYMGLMAKEBUFFERRESIDENTNVPROC __rglgen_glMakeBufferResidentNV;
RGLSYMGLMAKEBUFFERNONRESIDENTNVPROC __rglgen_glMakeBufferNonResidentNV;
RGLSYMGLISBUFFERRESIDENTNVPROC __rglgen_glIsBufferResidentNV;
RGLSYMGLMAKENAMEDBUFFERRESIDENTNVPROC __rglgen_glMakeNamedBufferResidentNV;
RGLSYMGLMAKENAMEDBUFFERNONRESIDENTNVPROC __rglgen_glMakeNamedBufferNonResidentNV;
RGLSYMGLISNAMEDBUFFERRESIDENTNVPROC __rglgen_glIsNamedBufferResidentNV;
RGLSYMGLGETBUFFERPARAMETERUI64VNVPROC __rglgen_glGetBufferParameterui64vNV;
RGLSYMGLGETNAMEDBUFFERPARAMETERUI64VNVPROC __rglgen_glGetNamedBufferParameterui64vNV;
RGLSYMGLGETINTEGERUI64VNVPROC __rglgen_glGetIntegerui64vNV;
RGLSYMGLUNIFORMUI64NVPROC __rglgen_glUniformui64NV;
RGLSYMGLUNIFORMUI64VNVPROC __rglgen_glUniformui64vNV;
RGLSYMGLGETUNIFORMUI64VNVPROC __rglgen_glGetUniformui64vNV;
RGLSYMGLPROGRAMUNIFORMUI64NVPROC __rglgen_glProgramUniformui64NV;
RGLSYMGLPROGRAMUNIFORMUI64VNVPROC __rglgen_glProgramUniformui64vNV;
RGLSYMGLBINDSHADINGRATEIMAGENVPROC __rglgen_glBindShadingRateImageNV;
RGLSYMGLGETSHADINGRATEIMAGEPALETTENVPROC __rglgen_glGetShadingRateImagePaletteNV;
RGLSYMGLGETSHADINGRATESAMPLELOCATIONIVNVPROC __rglgen_glGetShadingRateSampleLocationivNV;
RGLSYMGLSHADINGRATEIMAGEBARRIERNVPROC __rglgen_glShadingRateImageBarrierNV;
RGLSYMGLSHADINGRATEIMAGEPALETTENVPROC __rglgen_glShadingRateImagePaletteNV;
RGLSYMGLSHADINGRATESAMPLEORDERNVPROC __rglgen_glShadingRateSampleOrderNV;
RGLSYMGLSHADINGRATESAMPLEORDERCUSTOMNVPROC __rglgen_glShadingRateSampleOrderCustomNV;
RGLSYMGLTEXTUREBARRIERNVPROC __rglgen_glTextureBarrierNV;
RGLSYMGLVERTEXATTRIBL1I64NVPROC __rglgen_glVertexAttribL1i64NV;
RGLSYMGLVERTEXATTRIBL2I64NVPROC __rglgen_glVertexAttribL2i64NV;
RGLSYMGLVERTEXATTRIBL3I64NVPROC __rglgen_glVertexAttribL3i64NV;
RGLSYMGLVERTEXATTRIBL4I64NVPROC __rglgen_glVertexAttribL4i64NV;
RGLSYMGLVERTEXATTRIBL1I64VNVPROC __rglgen_glVertexAttribL1i64vNV;
RGLSYMGLVERTEXATTRIBL2I64VNVPROC __rglgen_glVertexAttribL2i64vNV;
RGLSYMGLVERTEXATTRIBL3I64VNVPROC __rglgen_glVertexAttribL3i64vNV;
RGLSYMGLVERTEXATTRIBL4I64VNVPROC __rglgen_glVertexAttribL4i64vNV;
RGLSYMGLVERTEXATTRIBL1UI64NVPROC __rglgen_glVertexAttribL1ui64NV;
RGLSYMGLVERTEXATTRIBL2UI64NVPROC __rglgen_glVertexAttribL2ui64NV;
RGLSYMGLVERTEXATTRIBL3UI64NVPROC __rglgen_glVertexAttribL3ui64NV;
RGLSYMGLVERTEXATTRIBL4UI64NVPROC __rglgen_glVertexAttribL4ui64NV;
RGLSYMGLVERTEXATTRIBL1UI64VNVPROC __rglgen_glVertexAttribL1ui64vNV;
RGLSYMGLVERTEXATTRIBL2UI64VNVPROC __rglgen_glVertexAttribL2ui64vNV;
RGLSYMGLVERTEXATTRIBL3UI64VNVPROC __rglgen_glVertexAttribL3ui64vNV;
RGLSYMGLVERTEXATTRIBL4UI64VNVPROC __rglgen_glVertexAttribL4ui64vNV;
RGLSYMGLGETVERTEXATTRIBLI64VNVPROC __rglgen_glGetVertexAttribLi64vNV;
RGLSYMGLGETVERTEXATTRIBLUI64VNVPROC __rglgen_glGetVertexAttribLui64vNV;
RGLSYMGLVERTEXATTRIBLFORMATNVPROC __rglgen_glVertexAttribLFormatNV;
RGLSYMGLBUFFERADDRESSRANGENVPROC __rglgen_glBufferAddressRangeNV;
RGLSYMGLVERTEXFORMATNVPROC __rglgen_glVertexFormatNV;
RGLSYMGLNORMALFORMATNVPROC __rglgen_glNormalFormatNV;
RGLSYMGLCOLORFORMATNVPROC __rglgen_glColorFormatNV;
RGLSYMGLINDEXFORMATNVPROC __rglgen_glIndexFormatNV;
RGLSYMGLTEXCOORDFORMATNVPROC __rglgen_glTexCoordFormatNV;
RGLSYMGLEDGEFLAGFORMATNVPROC __rglgen_glEdgeFlagFormatNV;
RGLSYMGLSECONDARYCOLORFORMATNVPROC __rglgen_glSecondaryColorFormatNV;
RGLSYMGLFOGCOORDFORMATNVPROC __rglgen_glFogCoordFormatNV;
RGLSYMGLVERTEXATTRIBFORMATNVPROC __rglgen_glVertexAttribFormatNV;
RGLSYMGLVERTEXATTRIBIFORMATNVPROC __rglgen_glVertexAttribIFormatNV;
RGLSYMGLGETINTEGERUI64I_VNVPROC __rglgen_glGetIntegerui64i_vNV;
RGLSYMGLVIEWPORTSWIZZLENVPROC __rglgen_glViewportSwizzleNV;
RGLSYMGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC __rglgen_glFramebufferTextureMultiviewOVR;

#ifndef __APPLE__
RGLSYMGLCREATESYNCFROMCLEVENTARBPROC __rglgen_glCreateSyncFromCLeventARB;
RGLSYMGLGETVKPROCADDRNVPROC __rglgen_glGetVkProcAddrNV;
#endif
