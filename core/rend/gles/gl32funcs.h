#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (GL_APIENTRY *RGLGENGLDEBUGPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
typedef void (GL_APIENTRY *RGLGENGLDEBUGPROCKHR)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);

typedef void (GL_APIENTRYP RGLSYMGLACTIVETEXTUREPROC) (GLenum texture);
typedef void (GL_APIENTRYP RGLSYMGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (GL_APIENTRYP RGLSYMGLBINDATTRIBLOCATIONPROC) (GLuint program, GLuint index, const GLchar *name);
typedef void (GL_APIENTRYP RGLSYMGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
typedef void (GL_APIENTRYP RGLSYMGLBINDFRAMEBUFFERPROC) (GLenum target, GLuint framebuffer);
typedef void (GL_APIENTRYP RGLSYMGLBINDRENDERBUFFERPROC) (GLenum target, GLuint renderbuffer);
typedef void (GL_APIENTRYP RGLSYMGLBINDTEXTUREPROC) (GLenum target, GLuint texture);
typedef void (GL_APIENTRYP RGLSYMGLBLENDCOLORPROC) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (GL_APIENTRYP RGLSYMGLBLENDEQUATIONPROC) (GLenum mode);
typedef void (GL_APIENTRYP RGLSYMGLBLENDEQUATIONSEPARATEPROC) (GLenum modeRGB, GLenum modeAlpha);
typedef void (GL_APIENTRYP RGLSYMGLBLENDFUNCPROC) (GLenum sfactor, GLenum dfactor);
typedef void (GL_APIENTRYP RGLSYMGLBLENDFUNCSEPARATEPROC) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
typedef void (GL_APIENTRYP RGLSYMGLBUFFERDATAPROC) (GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (GL_APIENTRYP RGLSYMGLBUFFERSUBDATAPROC) (GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
typedef GLenum (GL_APIENTRYP RGLSYMGLCHECKFRAMEBUFFERSTATUSPROC) (GLenum target);
typedef void (GL_APIENTRYP RGLSYMGLCLEARPROC) (GLbitfield mask);
typedef void (GL_APIENTRYP RGLSYMGLCLEARCOLORPROC) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (GL_APIENTRYP RGLSYMGLCLEARDEPTHFPROC) (GLfloat d);
typedef void (GL_APIENTRYP RGLSYMGLCLEARSTENCILPROC) (GLint s);
typedef void (GL_APIENTRYP RGLSYMGLCOLORMASKPROC) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
typedef void (GL_APIENTRYP RGLSYMGLCOMPILESHADERPROC) (GLuint shader);
typedef void (GL_APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE2DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data);
typedef void (GL_APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
typedef void (GL_APIENTRYP RGLSYMGLCOPYTEXIMAGE2DPROC) (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (GL_APIENTRYP RGLSYMGLCOPYTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef GLuint (GL_APIENTRYP RGLSYMGLCREATEPROGRAMPROC) (void);
typedef GLuint (GL_APIENTRYP RGLSYMGLCREATESHADERPROC) (GLenum type);
typedef void (GL_APIENTRYP RGLSYMGLCULLFACEPROC) (GLenum mode);
typedef void (GL_APIENTRYP RGLSYMGLDELETEBUFFERSPROC) (GLsizei n, const GLuint *buffers);
typedef void (GL_APIENTRYP RGLSYMGLDELETEFRAMEBUFFERSPROC) (GLsizei n, const GLuint *framebuffers);
typedef void (GL_APIENTRYP RGLSYMGLDELETEPROGRAMPROC) (GLuint program);
typedef void (GL_APIENTRYP RGLSYMGLDELETERENDERBUFFERSPROC) (GLsizei n, const GLuint *renderbuffers);
typedef void (GL_APIENTRYP RGLSYMGLDELETESHADERPROC) (GLuint shader);
typedef void (GL_APIENTRYP RGLSYMGLDELETETEXTURESPROC) (GLsizei n, const GLuint *textures);
typedef void (GL_APIENTRYP RGLSYMGLDEPTHFUNCPROC) (GLenum func);
typedef void (GL_APIENTRYP RGLSYMGLDEPTHMASKPROC) (GLboolean flag);
typedef void (GL_APIENTRYP RGLSYMGLDEPTHRANGEFPROC) (GLfloat n, GLfloat f);
typedef void (GL_APIENTRYP RGLSYMGLDETACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (GL_APIENTRYP RGLSYMGLDISABLEPROC) (GLenum cap);
typedef void (GL_APIENTRYP RGLSYMGLDISABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void (GL_APIENTRYP RGLSYMGLDRAWARRAYSPROC) (GLenum mode, GLint first, GLsizei count);
typedef void (GL_APIENTRYP RGLSYMGLDRAWELEMENTSPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices);
typedef void (GL_APIENTRYP RGLSYMGLENABLEPROC) (GLenum cap);
typedef void (GL_APIENTRYP RGLSYMGLENABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void (GL_APIENTRYP RGLSYMGLFINISHPROC) (void);
typedef void (GL_APIENTRYP RGLSYMGLFLUSHPROC) (void);
typedef void (GL_APIENTRYP RGLSYMGLFRAMEBUFFERRENDERBUFFERPROC) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (GL_APIENTRYP RGLSYMGLFRAMEBUFFERTEXTURE2DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (GL_APIENTRYP RGLSYMGLFRONTFACEPROC) (GLenum mode);
typedef void (GL_APIENTRYP RGLSYMGLGENBUFFERSPROC) (GLsizei n, GLuint *buffers);
typedef void (GL_APIENTRYP RGLSYMGLGENERATEMIPMAPPROC) (GLenum target);
typedef void (GL_APIENTRYP RGLSYMGLGENFRAMEBUFFERSPROC) (GLsizei n, GLuint *framebuffers);
typedef void (GL_APIENTRYP RGLSYMGLGENRENDERBUFFERSPROC) (GLsizei n, GLuint *renderbuffers);
typedef void (GL_APIENTRYP RGLSYMGLGENTEXTURESPROC) (GLsizei n, GLuint *textures);
typedef void (GL_APIENTRYP RGLSYMGLGETACTIVEATTRIBPROC) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
typedef void (GL_APIENTRYP RGLSYMGLGETACTIVEUNIFORMPROC) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
typedef void (GL_APIENTRYP RGLSYMGLGETATTACHEDSHADERSPROC) (GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders);
typedef GLint (GL_APIENTRYP RGLSYMGLGETATTRIBLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (GL_APIENTRYP RGLSYMGLGETBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef GLenum (GL_APIENTRYP RGLSYMGLGETERRORPROC) (void);
typedef void (GL_APIENTRYP RGLSYMGLGETFLOATVPROC) (GLenum pname, GLfloat *data);
typedef void (GL_APIENTRYP RGLSYMGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) (GLenum target, GLenum attachment, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETINTEGERVPROC) (GLenum pname, GLint *data);
typedef void (GL_APIENTRYP RGLSYMGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (GL_APIENTRYP RGLSYMGLGETRENDERBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (GL_APIENTRYP RGLSYMGLGETSHADERPRECISIONFORMATPROC) (GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision);
typedef void (GL_APIENTRYP RGLSYMGLGETSHADERSOURCEPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source);
typedef const GLubyte *(GL_APIENTRYP RGLSYMGLGETSTRINGPROC) (GLenum name);
typedef void (GL_APIENTRYP RGLSYMGLGETTEXPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (GL_APIENTRYP RGLSYMGLGETTEXPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETUNIFORMFVPROC) (GLuint program, GLint location, GLfloat *params);
typedef void (GL_APIENTRYP RGLSYMGLGETUNIFORMIVPROC) (GLuint program, GLint location, GLint *params);
typedef GLint (GL_APIENTRYP RGLSYMGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (GL_APIENTRYP RGLSYMGLGETVERTEXATTRIBFVPROC) (GLuint index, GLenum pname, GLfloat *params);
typedef void (GL_APIENTRYP RGLSYMGLGETVERTEXATTRIBIVPROC) (GLuint index, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETVERTEXATTRIBPOINTERVPROC) (GLuint index, GLenum pname, void **pointer);
typedef void (GL_APIENTRYP RGLSYMGLHINTPROC) (GLenum target, GLenum mode);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISBUFFERPROC) (GLuint buffer);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISENABLEDPROC) (GLenum cap);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISFRAMEBUFFERPROC) (GLuint framebuffer);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISPROGRAMPROC) (GLuint program);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISRENDERBUFFERPROC) (GLuint renderbuffer);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISSHADERPROC) (GLuint shader);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISTEXTUREPROC) (GLuint texture);
typedef void (GL_APIENTRYP RGLSYMGLLINEWIDTHPROC) (GLfloat width);
typedef void (GL_APIENTRYP RGLSYMGLLINKPROGRAMPROC) (GLuint program);
typedef void (GL_APIENTRYP RGLSYMGLPIXELSTOREIPROC) (GLenum pname, GLint param);
typedef void (GL_APIENTRYP RGLSYMGLPOLYGONOFFSETPROC) (GLfloat factor, GLfloat units);
typedef void (GL_APIENTRYP RGLSYMGLREADPIXELSPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
typedef void (GL_APIENTRYP RGLSYMGLRELEASESHADERCOMPILERPROC) (void);
typedef void (GL_APIENTRYP RGLSYMGLRENDERBUFFERSTORAGEPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP RGLSYMGLSAMPLECOVERAGEPROC) (GLfloat value, GLboolean invert);
typedef void (GL_APIENTRYP RGLSYMGLSCISSORPROC) (GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP RGLSYMGLSHADERBINARYPROC) (GLsizei count, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length);
typedef void (GL_APIENTRYP RGLSYMGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (GL_APIENTRYP RGLSYMGLSTENCILFUNCPROC) (GLenum func, GLint ref, GLuint mask);
typedef void (GL_APIENTRYP RGLSYMGLSTENCILFUNCSEPARATEPROC) (GLenum face, GLenum func, GLint ref, GLuint mask);
typedef void (GL_APIENTRYP RGLSYMGLSTENCILMASKPROC) (GLuint mask);
typedef void (GL_APIENTRYP RGLSYMGLSTENCILMASKSEPARATEPROC) (GLenum face, GLuint mask);
typedef void (GL_APIENTRYP RGLSYMGLSTENCILOPPROC) (GLenum fail, GLenum zfail, GLenum zpass);
typedef void (GL_APIENTRYP RGLSYMGLSTENCILOPSEPARATEPROC) (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
typedef void (GL_APIENTRYP RGLSYMGLTEXIMAGE2DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (GL_APIENTRYP RGLSYMGLTEXPARAMETERFPROC) (GLenum target, GLenum pname, GLfloat param);
typedef void (GL_APIENTRYP RGLSYMGLTEXPARAMETERFVPROC) (GLenum target, GLenum pname, const GLfloat *params);
typedef void (GL_APIENTRYP RGLSYMGLTEXPARAMETERIPROC) (GLenum target, GLenum pname, GLint param);
typedef void (GL_APIENTRYP RGLSYMGLTEXPARAMETERIVPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM1FPROC) (GLint location, GLfloat v0);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM1FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM1IPROC) (GLint location, GLint v0);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM1IVPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM2FPROC) (GLint location, GLfloat v0, GLfloat v1);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM2FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM2IPROC) (GLint location, GLint v0, GLint v1);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM2IVPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM3FPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM3FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM3IPROC) (GLint location, GLint v0, GLint v1, GLint v2);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM3IVPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM4FPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM4FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM4IPROC) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM4IVPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORMMATRIX2FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORMMATRIX3FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORMMATRIX4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLUSEPROGRAMPROC) (GLuint program);
typedef void (GL_APIENTRYP RGLSYMGLVALIDATEPROGRAMPROC) (GLuint program);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIB1FPROC) (GLuint index, GLfloat x);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIB1FVPROC) (GLuint index, const GLfloat *v);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIB2FPROC) (GLuint index, GLfloat x, GLfloat y);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIB2FVPROC) (GLuint index, const GLfloat *v);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIB3FPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIB3FVPROC) (GLuint index, const GLfloat *v);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIB4FPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIB4FVPROC) (GLuint index, const GLfloat *v);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIBPOINTERPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (GL_APIENTRYP RGLSYMGLVIEWPORTPROC) (GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP RGLSYMGLREADBUFFERPROC) (GLenum src);
typedef void (GL_APIENTRYP RGLSYMGLDRAWRANGEELEMENTSPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices);
typedef void (GL_APIENTRYP RGLSYMGLTEXIMAGE3DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (GL_APIENTRYP RGLSYMGLTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
typedef void (GL_APIENTRYP RGLSYMGLCOPYTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE3DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data);
typedef void (GL_APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data);
typedef void (GL_APIENTRYP RGLSYMGLGENQUERIESPROC) (GLsizei n, GLuint *ids);
typedef void (GL_APIENTRYP RGLSYMGLDELETEQUERIESPROC) (GLsizei n, const GLuint *ids);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISQUERYPROC) (GLuint id);
typedef void (GL_APIENTRYP RGLSYMGLBEGINQUERYPROC) (GLenum target, GLuint id);
typedef void (GL_APIENTRYP RGLSYMGLENDQUERYPROC) (GLenum target);
typedef void (GL_APIENTRYP RGLSYMGLGETQUERYIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETQUERYOBJECTUIVPROC) (GLuint id, GLenum pname, GLuint *params);
typedef GLboolean (GL_APIENTRYP RGLSYMGLUNMAPBUFFERPROC) (GLenum target);
typedef void (GL_APIENTRYP RGLSYMGLGETBUFFERPOINTERVPROC) (GLenum target, GLenum pname, void **params);
typedef void (GL_APIENTRYP RGLSYMGLDRAWBUFFERSPROC) (GLsizei n, const GLenum *bufs);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORMMATRIX2X3FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORMMATRIX3X2FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORMMATRIX2X4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORMMATRIX4X2FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORMMATRIX3X4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORMMATRIX4X3FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLBLITFRAMEBUFFERPROC) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void (GL_APIENTRYP RGLSYMGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP RGLSYMGLFRAMEBUFFERTEXTURELAYERPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
typedef void *(GL_APIENTRYP RGLSYMGLMAPBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void (GL_APIENTRYP RGLSYMGLFLUSHMAPPEDBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length);
typedef void (GL_APIENTRYP RGLSYMGLBINDVERTEXARRAYPROC) (GLuint array);
typedef void (GL_APIENTRYP RGLSYMGLDELETEVERTEXARRAYSPROC) (GLsizei n, const GLuint *arrays);
typedef void (GL_APIENTRYP RGLSYMGLGENVERTEXARRAYSPROC) (GLsizei n, GLuint *arrays);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISVERTEXARRAYPROC) (GLuint array);
typedef void (GL_APIENTRYP RGLSYMGLGETINTEGERI_VPROC) (GLenum target, GLuint index, GLint *data);
typedef void (GL_APIENTRYP RGLSYMGLBEGINTRANSFORMFEEDBACKPROC) (GLenum primitiveMode);
typedef void (GL_APIENTRYP RGLSYMGLENDTRANSFORMFEEDBACKPROC) (void);
typedef void (GL_APIENTRYP RGLSYMGLBINDBUFFERRANGEPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (GL_APIENTRYP RGLSYMGLBINDBUFFERBASEPROC) (GLenum target, GLuint index, GLuint buffer);
typedef void (GL_APIENTRYP RGLSYMGLTRANSFORMFEEDBACKVARYINGSPROC) (GLuint program, GLsizei count, const GLchar *const*varyings, GLenum bufferMode);
typedef void (GL_APIENTRYP RGLSYMGLGETTRANSFORMFEEDBACKVARYINGPROC) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIBIPOINTERPROC) (GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer);
typedef void (GL_APIENTRYP RGLSYMGLGETVERTEXATTRIBIIVPROC) (GLuint index, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETVERTEXATTRIBIUIVPROC) (GLuint index, GLenum pname, GLuint *params);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIBI4IPROC) (GLuint index, GLint x, GLint y, GLint z, GLint w);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIBI4UIPROC) (GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIBI4IVPROC) (GLuint index, const GLint *v);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIBI4UIVPROC) (GLuint index, const GLuint *v);
typedef void (GL_APIENTRYP RGLSYMGLGETUNIFORMUIVPROC) (GLuint program, GLint location, GLuint *params);
typedef GLint (GL_APIENTRYP RGLSYMGLGETFRAGDATALOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM1UIPROC) (GLint location, GLuint v0);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM2UIPROC) (GLint location, GLuint v0, GLuint v1);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM3UIPROC) (GLint location, GLuint v0, GLuint v1, GLuint v2);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM4UIPROC) (GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM1UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM2UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM3UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORM4UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (GL_APIENTRYP RGLSYMGLCLEARBUFFERIVPROC) (GLenum buffer, GLint drawbuffer, const GLint *value);
typedef void (GL_APIENTRYP RGLSYMGLCLEARBUFFERUIVPROC) (GLenum buffer, GLint drawbuffer, const GLuint *value);
typedef void (GL_APIENTRYP RGLSYMGLCLEARBUFFERFVPROC) (GLenum buffer, GLint drawbuffer, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLCLEARBUFFERFIPROC) (GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
typedef const GLubyte *(GL_APIENTRYP RGLSYMGLGETSTRINGIPROC) (GLenum name, GLuint index);
typedef void (GL_APIENTRYP RGLSYMGLCOPYBUFFERSUBDATAPROC) (GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
typedef void (GL_APIENTRYP RGLSYMGLGETUNIFORMINDICESPROC) (GLuint program, GLsizei uniformCount, const GLchar *const*uniformNames, GLuint *uniformIndices);
typedef void (GL_APIENTRYP RGLSYMGLGETACTIVEUNIFORMSIVPROC) (GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params);
typedef GLuint (GL_APIENTRYP RGLSYMGLGETUNIFORMBLOCKINDEXPROC) (GLuint program, const GLchar *uniformBlockName);
typedef void (GL_APIENTRYP RGLSYMGLGETACTIVEUNIFORMBLOCKIVPROC) (GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETACTIVEUNIFORMBLOCKNAMEPROC) (GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName);
typedef void (GL_APIENTRYP RGLSYMGLUNIFORMBLOCKBINDINGPROC) (GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
typedef void (GL_APIENTRYP RGLSYMGLDRAWARRAYSINSTANCEDPROC) (GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
typedef void (GL_APIENTRYP RGLSYMGLDRAWELEMENTSINSTANCEDPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount);
typedef GLsync (GL_APIENTRYP RGLSYMGLFENCESYNCPROC) (GLenum condition, GLbitfield flags);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISSYNCPROC) (GLsync sync);
typedef void (GL_APIENTRYP RGLSYMGLDELETESYNCPROC) (GLsync sync);
typedef GLenum (GL_APIENTRYP RGLSYMGLCLIENTWAITSYNCPROC) (GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef void (GL_APIENTRYP RGLSYMGLWAITSYNCPROC) (GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef void (GL_APIENTRYP RGLSYMGLGETINTEGER64VPROC) (GLenum pname, GLint64 *data);
typedef void (GL_APIENTRYP RGLSYMGLGETSYNCIVPROC) (GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values);
typedef void (GL_APIENTRYP RGLSYMGLGETINTEGER64I_VPROC) (GLenum target, GLuint index, GLint64 *data);
typedef void (GL_APIENTRYP RGLSYMGLGETBUFFERPARAMETERI64VPROC) (GLenum target, GLenum pname, GLint64 *params);
typedef void (GL_APIENTRYP RGLSYMGLGENSAMPLERSPROC) (GLsizei count, GLuint *samplers);
typedef void (GL_APIENTRYP RGLSYMGLDELETESAMPLERSPROC) (GLsizei count, const GLuint *samplers);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISSAMPLERPROC) (GLuint sampler);
typedef void (GL_APIENTRYP RGLSYMGLBINDSAMPLERPROC) (GLuint unit, GLuint sampler);
typedef void (GL_APIENTRYP RGLSYMGLSAMPLERPARAMETERIPROC) (GLuint sampler, GLenum pname, GLint param);
typedef void (GL_APIENTRYP RGLSYMGLSAMPLERPARAMETERIVPROC) (GLuint sampler, GLenum pname, const GLint *param);
typedef void (GL_APIENTRYP RGLSYMGLSAMPLERPARAMETERFPROC) (GLuint sampler, GLenum pname, GLfloat param);
typedef void (GL_APIENTRYP RGLSYMGLSAMPLERPARAMETERFVPROC) (GLuint sampler, GLenum pname, const GLfloat *param);
typedef void (GL_APIENTRYP RGLSYMGLGETSAMPLERPARAMETERIVPROC) (GLuint sampler, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETSAMPLERPARAMETERFVPROC) (GLuint sampler, GLenum pname, GLfloat *params);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIBDIVISORPROC) (GLuint index, GLuint divisor);
typedef void (GL_APIENTRYP RGLSYMGLBINDTRANSFORMFEEDBACKPROC) (GLenum target, GLuint id);
typedef void (GL_APIENTRYP RGLSYMGLDELETETRANSFORMFEEDBACKSPROC) (GLsizei n, const GLuint *ids);
typedef void (GL_APIENTRYP RGLSYMGLGENTRANSFORMFEEDBACKSPROC) (GLsizei n, GLuint *ids);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISTRANSFORMFEEDBACKPROC) (GLuint id);
typedef void (GL_APIENTRYP RGLSYMGLPAUSETRANSFORMFEEDBACKPROC) (void);
typedef void (GL_APIENTRYP RGLSYMGLRESUMETRANSFORMFEEDBACKPROC) (void);
typedef void (GL_APIENTRYP RGLSYMGLGETPROGRAMBINARYPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMBINARYPROC) (GLuint program, GLenum binaryFormat, const void *binary, GLsizei length);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMPARAMETERIPROC) (GLuint program, GLenum pname, GLint value);
typedef void (GL_APIENTRYP RGLSYMGLINVALIDATEFRAMEBUFFERPROC) (GLenum target, GLsizei numAttachments, const GLenum *attachments);
typedef void (GL_APIENTRYP RGLSYMGLINVALIDATESUBFRAMEBUFFERPROC) (GLenum target, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP RGLSYMGLTEXSTORAGE2DPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP RGLSYMGLTEXSTORAGE3DPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
typedef void (GL_APIENTRYP RGLSYMGLGETINTERNALFORMATIVPROC) (GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLDISPATCHCOMPUTEPROC) (GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
typedef void (GL_APIENTRYP RGLSYMGLDISPATCHCOMPUTEINDIRECTPROC) (GLintptr indirect);
typedef void (GL_APIENTRYP RGLSYMGLDRAWARRAYSINDIRECTPROC) (GLenum mode, const void *indirect);
typedef void (GL_APIENTRYP RGLSYMGLDRAWELEMENTSINDIRECTPROC) (GLenum mode, GLenum type, const void *indirect);
typedef void (GL_APIENTRYP RGLSYMGLFRAMEBUFFERPARAMETERIPROC) (GLenum target, GLenum pname, GLint param);
typedef void (GL_APIENTRYP RGLSYMGLGETFRAMEBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETPROGRAMINTERFACEIVPROC) (GLuint program, GLenum programInterface, GLenum pname, GLint *params);
typedef GLuint (GL_APIENTRYP RGLSYMGLGETPROGRAMRESOURCEINDEXPROC) (GLuint program, GLenum programInterface, const GLchar *name);
typedef void (GL_APIENTRYP RGLSYMGLGETPROGRAMRESOURCENAMEPROC) (GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name);
typedef void (GL_APIENTRYP RGLSYMGLGETPROGRAMRESOURCEIVPROC) (GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei bufSize, GLsizei *length, GLint *params);
typedef GLint (GL_APIENTRYP RGLSYMGLGETPROGRAMRESOURCELOCATIONPROC) (GLuint program, GLenum programInterface, const GLchar *name);
typedef void (GL_APIENTRYP RGLSYMGLUSEPROGRAMSTAGESPROC) (GLuint pipeline, GLbitfield stages, GLuint program);
typedef void (GL_APIENTRYP RGLSYMGLACTIVESHADERPROGRAMPROC) (GLuint pipeline, GLuint program);
typedef GLuint (GL_APIENTRYP RGLSYMGLCREATESHADERPROGRAMVPROC) (GLenum type, GLsizei count, const GLchar *const*strings);
typedef void (GL_APIENTRYP RGLSYMGLBINDPROGRAMPIPELINEPROC) (GLuint pipeline);
typedef void (GL_APIENTRYP RGLSYMGLDELETEPROGRAMPIPELINESPROC) (GLsizei n, const GLuint *pipelines);
typedef void (GL_APIENTRYP RGLSYMGLGENPROGRAMPIPELINESPROC) (GLsizei n, GLuint *pipelines);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISPROGRAMPIPELINEPROC) (GLuint pipeline);
typedef void (GL_APIENTRYP RGLSYMGLGETPROGRAMPIPELINEIVPROC) (GLuint pipeline, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM1IPROC) (GLuint program, GLint location, GLint v0);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM2IPROC) (GLuint program, GLint location, GLint v0, GLint v1);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM3IPROC) (GLuint program, GLint location, GLint v0, GLint v1, GLint v2);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM4IPROC) (GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM1UIPROC) (GLuint program, GLint location, GLuint v0);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM2UIPROC) (GLuint program, GLint location, GLuint v0, GLuint v1);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM3UIPROC) (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM4UIPROC) (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM1FPROC) (GLuint program, GLint location, GLfloat v0);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM2FPROC) (GLuint program, GLint location, GLfloat v0, GLfloat v1);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM3FPROC) (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM4FPROC) (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM1IVPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM2IVPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM3IVPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM4IVPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM1UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM2UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM3UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM4UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM1FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM2FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM3FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORM4FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2X3FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3X2FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2X4FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4X2FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3X4FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4X3FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP RGLSYMGLVALIDATEPROGRAMPIPELINEPROC) (GLuint pipeline);
typedef void (GL_APIENTRYP RGLSYMGLGETPROGRAMPIPELINEINFOLOGPROC) (GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (GL_APIENTRYP RGLSYMGLBINDIMAGETEXTUREPROC) (GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);
typedef void (GL_APIENTRYP RGLSYMGLGETBOOLEANI_VPROC) (GLenum target, GLuint index, GLboolean *data);
typedef void (GL_APIENTRYP RGLSYMGLGETBOOLEANVPROC) (GLenum target, GLuint index, GLboolean *data);
typedef void (GL_APIENTRYP RGLSYMGLMEMORYBARRIERPROC) (GLbitfield barriers);
typedef void (GL_APIENTRYP RGLSYMGLMEMORYBARRIERBYREGIONPROC) (GLbitfield barriers);
typedef void (GL_APIENTRYP RGLSYMGLTEXSTORAGE2DMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void (GL_APIENTRYP RGLSYMGLGETMULTISAMPLEFVPROC) (GLenum pname, GLuint index, GLfloat *val);
typedef void (GL_APIENTRYP RGLSYMGLSAMPLEMASKIPROC) (GLuint maskNumber, GLbitfield mask);
typedef void (GL_APIENTRYP RGLSYMGLGETTEXLEVELPARAMETERIVPROC) (GLenum target, GLint level, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETTEXLEVELPARAMETERFVPROC) (GLenum target, GLint level, GLenum pname, GLfloat *params);
typedef void (GL_APIENTRYP RGLSYMGLBINDVERTEXBUFFERPROC) (GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIBFORMATPROC) (GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIBIFORMATPROC) (GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXATTRIBBINDINGPROC) (GLuint attribindex, GLuint bindingindex);
typedef void (GL_APIENTRYP RGLSYMGLVERTEXBINDINGDIVISORPROC) (GLuint bindingindex, GLuint divisor);
typedef void (GL_APIENTRYP RGLSYMGLBLENDBARRIERPROC) (void);
typedef void (GL_APIENTRYP RGLSYMGLCOPYIMAGESUBDATAPROC) (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
typedef void (GL_APIENTRYP RGLSYMGLDEBUGMESSAGECONTROLPROC) (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled);
typedef void (GL_APIENTRYP RGLSYMGLDEBUGMESSAGEINSERTPROC) (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf);
typedef void (GL_APIENTRYP RGLSYMGLDEBUGMESSAGECALLBACKPROC) (RGLGENGLDEBUGPROC callback, const void *userParam);
typedef GLuint (GL_APIENTRYP RGLSYMGLGETDEBUGMESSAGELOGPROC) (GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog);
typedef void (GL_APIENTRYP RGLSYMGLPUSHDEBUGGROUPPROC) (GLenum source, GLuint id, GLsizei length, const GLchar *message);
typedef void (GL_APIENTRYP RGLSYMGLPOPDEBUGGROUPPROC) (void);
typedef void (GL_APIENTRYP RGLSYMGLOBJECTLABELPROC) (GLenum identifier, GLuint name, GLsizei length, const GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLGETOBJECTLABELPROC) (GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLOBJECTPTRLABELPROC) (const void *ptr, GLsizei length, const GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLGETOBJECTPTRLABELPROC) (const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLGETPOINTERVPROC) (GLenum pname, void **params);
typedef void (GL_APIENTRYP RGLSYMGLENABLEIPROC) (GLenum target, GLuint index);
typedef void (GL_APIENTRYP RGLSYMGLDISABLEIPROC) (GLenum target, GLuint index);
typedef void (GL_APIENTRYP RGLSYMGLBLENDEQUATIONIPROC) (GLuint buf, GLenum mode);
typedef void (GL_APIENTRYP RGLSYMGLBLENDEQUATIONSEPARATEIPROC) (GLuint buf, GLenum modeRGB, GLenum modeAlpha);
typedef void (GL_APIENTRYP RGLSYMGLBLENDFUNCIPROC) (GLuint buf, GLenum src, GLenum dst);
typedef void (GL_APIENTRYP RGLSYMGLBLENDFUNCSEPARATEIPROC) (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
typedef void (GL_APIENTRYP RGLSYMGLCOLORMASKIPROC) (GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISENABLEDIPROC) (GLenum target, GLuint index);
typedef void (GL_APIENTRYP RGLSYMGLDRAWELEMENTSBASEVERTEXPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex);
typedef void (GL_APIENTRYP RGLSYMGLDRAWRANGEELEMENTSBASEVERTEXPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex);
typedef void (GL_APIENTRYP RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex);
typedef void (GL_APIENTRYP RGLSYMGLFRAMEBUFFERTEXTUREPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level);
typedef void (GL_APIENTRYP RGLSYMGLPRIMITIVEBOUNDINGBOXPROC) (GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW);
typedef GLenum (GL_APIENTRYP RGLSYMGLGETGRAPHICSRESETSTATUSPROC) (void);
typedef void (GL_APIENTRYP RGLSYMGLREADNPIXELSPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data);
typedef void (GL_APIENTRYP RGLSYMGLGETNUNIFORMFVPROC) (GLuint program, GLint location, GLsizei bufSize, GLfloat *params);
typedef void (GL_APIENTRYP RGLSYMGLGETNUNIFORMIVPROC) (GLuint program, GLint location, GLsizei bufSize, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETNUNIFORMUIVPROC) (GLuint program, GLint location, GLsizei bufSize, GLuint *params);
typedef void (GL_APIENTRYP RGLSYMGLMINSAMPLESHADINGPROC) (GLfloat value);
typedef void (GL_APIENTRYP RGLSYMGLPATCHPARAMETERIPROC) (GLenum pname, GLint value);
typedef void (GL_APIENTRYP RGLSYMGLTEXPARAMETERIIVPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLTEXPARAMETERIUIVPROC) (GLenum target, GLenum pname, const GLuint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETTEXPARAMETERIIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETTEXPARAMETERIUIVPROC) (GLenum target, GLenum pname, GLuint *params);
typedef void (GL_APIENTRYP RGLSYMGLSAMPLERPARAMETERIIVPROC) (GLuint sampler, GLenum pname, const GLint *param);
typedef void (GL_APIENTRYP RGLSYMGLSAMPLERPARAMETERIUIVPROC) (GLuint sampler, GLenum pname, const GLuint *param);
typedef void (GL_APIENTRYP RGLSYMGLGETSAMPLERPARAMETERIIVPROC) (GLuint sampler, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP RGLSYMGLGETSAMPLERPARAMETERIUIVPROC) (GLuint sampler, GLenum pname, GLuint *params);
typedef void (GL_APIENTRYP RGLSYMGLTEXBUFFERPROC) (GLenum target, GLenum internalformat, GLuint buffer);
typedef void (GL_APIENTRYP RGLSYMGLTEXBUFFERRANGEPROC) (GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (GL_APIENTRYP RGLSYMGLTEXSTORAGE3DMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);

#define glActiveTexture __rglgen_glActiveTexture
#define glAttachShader __rglgen_glAttachShader
#define glBindAttribLocation __rglgen_glBindAttribLocation
#define glBindBuffer __rglgen_glBindBuffer
#define glBindFramebuffer __rglgen_glBindFramebuffer
#define glBindRenderbuffer __rglgen_glBindRenderbuffer
#define glBindTexture __rglgen_glBindTexture
#define glBlendColor __rglgen_glBlendColor
#define glBlendEquation __rglgen_glBlendEquation
#define glBlendEquationSeparate __rglgen_glBlendEquationSeparate
#define glBlendFunc __rglgen_glBlendFunc
#define glBlendFuncSeparate __rglgen_glBlendFuncSeparate
#define glBufferData __rglgen_glBufferData
#define glBufferSubData __rglgen_glBufferSubData
#define glCheckFramebufferStatus __rglgen_glCheckFramebufferStatus
#define glClear __rglgen_glClear
#define glClearColor __rglgen_glClearColor
#define glClearDepthf __rglgen_glClearDepthf
#define glClearStencil __rglgen_glClearStencil
#define glColorMask __rglgen_glColorMask
#define glCompileShader __rglgen_glCompileShader
#define glCompressedTexImage2D __rglgen_glCompressedTexImage2D
#define glCompressedTexSubImage2D __rglgen_glCompressedTexSubImage2D
#define glCopyTexImage2D __rglgen_glCopyTexImage2D
#define glCopyTexSubImage2D __rglgen_glCopyTexSubImage2D
#define glCreateProgram __rglgen_glCreateProgram
#define glCreateShader __rglgen_glCreateShader
#define glCullFace __rglgen_glCullFace
#define glDeleteBuffers __rglgen_glDeleteBuffers
#define glDeleteFramebuffers __rglgen_glDeleteFramebuffers
#define glDeleteProgram __rglgen_glDeleteProgram
#define glDeleteRenderbuffers __rglgen_glDeleteRenderbuffers
#define glDeleteShader __rglgen_glDeleteShader
#define glDeleteTextures __rglgen_glDeleteTextures
#define glDepthFunc __rglgen_glDepthFunc
#define glDepthMask __rglgen_glDepthMask
#define glDepthRangef __rglgen_glDepthRangef
#define glDetachShader __rglgen_glDetachShader
#define glDisable __rglgen_glDisable
#define glDisableVertexAttribArray __rglgen_glDisableVertexAttribArray
#define glDrawArrays __rglgen_glDrawArrays
#define glDrawElements __rglgen_glDrawElements
#define glEnable __rglgen_glEnable
#define glEnableVertexAttribArray __rglgen_glEnableVertexAttribArray
#define glFinish __rglgen_glFinish
#define glFlush __rglgen_glFlush
#define glFramebufferRenderbuffer __rglgen_glFramebufferRenderbuffer
#define glFramebufferTexture2D __rglgen_glFramebufferTexture2D
#define glFrontFace __rglgen_glFrontFace
#define glGenBuffers __rglgen_glGenBuffers
#define glGenerateMipmap __rglgen_glGenerateMipmap
#define glGenFramebuffers __rglgen_glGenFramebuffers
#define glGenRenderbuffers __rglgen_glGenRenderbuffers
#define glGenTextures __rglgen_glGenTextures
#define glGetActiveAttrib __rglgen_glGetActiveAttrib
#define glGetActiveUniform __rglgen_glGetActiveUniform
#define glGetAttachedShaders __rglgen_glGetAttachedShaders
#define glGetAttribLocation __rglgen_glGetAttribLocation
#define glGetBooleanv __rglgen_glGetBooleanv
#define glGetBufferParameteriv __rglgen_glGetBufferParameteriv
#define glGetError __rglgen_glGetError
#define glGetFloatv __rglgen_glGetFloatv
#define glGetFramebufferAttachmentParameteriv __rglgen_glGetFramebufferAttachmentParameteriv
#define glGetIntegerv __rglgen_glGetIntegerv
#define glGetProgramiv __rglgen_glGetProgramiv
#define glGetProgramInfoLog __rglgen_glGetProgramInfoLog
#define glGetRenderbufferParameteriv __rglgen_glGetRenderbufferParameteriv
#define glGetShaderiv __rglgen_glGetShaderiv
#define glGetShaderInfoLog __rglgen_glGetShaderInfoLog
#define glGetShaderPrecisionFormat __rglgen_glGetShaderPrecisionFormat
#define glGetShaderSource __rglgen_glGetShaderSource
#define glGetString __rglgen_glGetString
#define glGetTexParameterfv __rglgen_glGetTexParameterfv
#define glGetTexParameteriv __rglgen_glGetTexParameteriv
#define glGetUniformfv __rglgen_glGetUniformfv
#define glGetUniformiv __rglgen_glGetUniformiv
#define glGetUniformLocation __rglgen_glGetUniformLocation
#define glGetVertexAttribfv __rglgen_glGetVertexAttribfv
#define glGetVertexAttribiv __rglgen_glGetVertexAttribiv
#define glGetVertexAttribPointerv __rglgen_glGetVertexAttribPointerv
#define glHint __rglgen_glHint
#define glIsBuffer __rglgen_glIsBuffer
#define glIsEnabled __rglgen_glIsEnabled
#define glIsFramebuffer __rglgen_glIsFramebuffer
#define glIsProgram __rglgen_glIsProgram
#define glIsRenderbuffer __rglgen_glIsRenderbuffer
#define glIsShader __rglgen_glIsShader
#define glIsTexture __rglgen_glIsTexture
#define glLineWidth __rglgen_glLineWidth
#define glLinkProgram __rglgen_glLinkProgram
#define glPixelStorei __rglgen_glPixelStorei
#define glPolygonOffset __rglgen_glPolygonOffset
#define glReadPixels __rglgen_glReadPixels
#define glReleaseShaderCompiler __rglgen_glReleaseShaderCompiler
#define glRenderbufferStorage __rglgen_glRenderbufferStorage
#define glSampleCoverage __rglgen_glSampleCoverage
#define glScissor __rglgen_glScissor
#define glShaderBinary __rglgen_glShaderBinary
#define glShaderSource __rglgen_glShaderSource
#define glStencilFunc __rglgen_glStencilFunc
#define glStencilFuncSeparate __rglgen_glStencilFuncSeparate
#define glStencilMask __rglgen_glStencilMask
#define glStencilMaskSeparate __rglgen_glStencilMaskSeparate
#define glStencilOp __rglgen_glStencilOp
#define glStencilOpSeparate __rglgen_glStencilOpSeparate
#define glTexImage2D __rglgen_glTexImage2D
#define glTexParameterf __rglgen_glTexParameterf
#define glTexParameterfv __rglgen_glTexParameterfv
#define glTexParameteri __rglgen_glTexParameteri
#define glTexParameteriv __rglgen_glTexParameteriv
#define glTexSubImage2D __rglgen_glTexSubImage2D
#define glUniform1f __rglgen_glUniform1f
#define glUniform1fv __rglgen_glUniform1fv
#define glUniform1i __rglgen_glUniform1i
#define glUniform1iv __rglgen_glUniform1iv
#define glUniform2f __rglgen_glUniform2f
#define glUniform2fv __rglgen_glUniform2fv
#define glUniform2i __rglgen_glUniform2i
#define glUniform2iv __rglgen_glUniform2iv
#define glUniform3f __rglgen_glUniform3f
#define glUniform3fv __rglgen_glUniform3fv
#define glUniform3i __rglgen_glUniform3i
#define glUniform3iv __rglgen_glUniform3iv
#define glUniform4f __rglgen_glUniform4f
#define glUniform4fv __rglgen_glUniform4fv
#define glUniform4i __rglgen_glUniform4i
#define glUniform4iv __rglgen_glUniform4iv
#define glUniformMatrix2fv __rglgen_glUniformMatrix2fv
#define glUniformMatrix3fv __rglgen_glUniformMatrix3fv
#define glUniformMatrix4fv __rglgen_glUniformMatrix4fv
#define glUseProgram __rglgen_glUseProgram
#define glValidateProgram __rglgen_glValidateProgram
#define glVertexAttrib1f __rglgen_glVertexAttrib1f
#define glVertexAttrib1fv __rglgen_glVertexAttrib1fv
#define glVertexAttrib2f __rglgen_glVertexAttrib2f
#define glVertexAttrib2fv __rglgen_glVertexAttrib2fv
#define glVertexAttrib3f __rglgen_glVertexAttrib3f
#define glVertexAttrib3fv __rglgen_glVertexAttrib3fv
#define glVertexAttrib4f __rglgen_glVertexAttrib4f
#define glVertexAttrib4fv __rglgen_glVertexAttrib4fv
#define glVertexAttribPointer __rglgen_glVertexAttribPointer
#define glViewport __rglgen_glViewport
#define glReadBuffer __rglgen_glReadBuffer
#define glDrawRangeElements __rglgen_glDrawRangeElements
#define glTexImage3D __rglgen_glTexImage3D
#define glTexSubImage3D __rglgen_glTexSubImage3D
#define glCopyTexSubImage3D __rglgen_glCopyTexSubImage3D
#define glCompressedTexImage3D __rglgen_glCompressedTexImage3D
#define glCompressedTexSubImage3D __rglgen_glCompressedTexSubImage3D
#define glGenQueries __rglgen_glGenQueries
#define glDeleteQueries __rglgen_glDeleteQueries
#define glIsQuery __rglgen_glIsQuery
#define glBeginQuery __rglgen_glBeginQuery
#define glEndQuery __rglgen_glEndQuery
#define glGetQueryiv __rglgen_glGetQueryiv
#define glGetQueryObjectuiv __rglgen_glGetQueryObjectuiv
#define glUnmapBuffer __rglgen_glUnmapBuffer
#define glGetBufferPointerv __rglgen_glGetBufferPointerv
#define glDrawBuffers __rglgen_glDrawBuffers
#define glUniformMatrix2x3fv __rglgen_glUniformMatrix2x3fv
#define glUniformMatrix3x2fv __rglgen_glUniformMatrix3x2fv
#define glUniformMatrix2x4fv __rglgen_glUniformMatrix2x4fv
#define glUniformMatrix4x2fv __rglgen_glUniformMatrix4x2fv
#define glUniformMatrix3x4fv __rglgen_glUniformMatrix3x4fv
#define glUniformMatrix4x3fv __rglgen_glUniformMatrix4x3fv
#define glBlitFramebuffer __rglgen_glBlitFramebuffer
#define glRenderbufferStorageMultisample __rglgen_glRenderbufferStorageMultisample
#define glFramebufferTextureLayer __rglgen_glFramebufferTextureLayer
#define glMapBufferRange __rglgen_glMapBufferRange
#define glFlushMappedBufferRange __rglgen_glFlushMappedBufferRange
#define glBindVertexArray __rglgen_glBindVertexArray
#define glDeleteVertexArrays __rglgen_glDeleteVertexArrays
#define glGenVertexArrays __rglgen_glGenVertexArrays
#define glIsVertexArray __rglgen_glIsVertexArray
#define glGetIntegeri_v __rglgen_glGetIntegeri_v
#define glBeginTransformFeedback __rglgen_glBeginTransformFeedback
#define glEndTransformFeedback __rglgen_glEndTransformFeedback
#define glBindBufferRange __rglgen_glBindBufferRange
#define glBindBufferBase __rglgen_glBindBufferBase
#define glTransformFeedbackVaryings __rglgen_glTransformFeedbackVaryings
#define glGetTransformFeedbackVarying __rglgen_glGetTransformFeedbackVarying
#define glVertexAttribIPointer __rglgen_glVertexAttribIPointer
#define glGetVertexAttribIiv __rglgen_glGetVertexAttribIiv
#define glGetVertexAttribIuiv __rglgen_glGetVertexAttribIuiv
#define glVertexAttribI4i __rglgen_glVertexAttribI4i
#define glVertexAttribI4ui __rglgen_glVertexAttribI4ui
#define glVertexAttribI4iv __rglgen_glVertexAttribI4iv
#define glVertexAttribI4uiv __rglgen_glVertexAttribI4uiv
#define glGetUniformuiv __rglgen_glGetUniformuiv
#define glGetFragDataLocation __rglgen_glGetFragDataLocation
#define glUniform1ui __rglgen_glUniform1ui
#define glUniform2ui __rglgen_glUniform2ui
#define glUniform3ui __rglgen_glUniform3ui
#define glUniform4ui __rglgen_glUniform4ui
#define glUniform1uiv __rglgen_glUniform1uiv
#define glUniform2uiv __rglgen_glUniform2uiv
#define glUniform3uiv __rglgen_glUniform3uiv
#define glUniform4uiv __rglgen_glUniform4uiv
#define glClearBufferiv __rglgen_glClearBufferiv
#define glClearBufferuiv __rglgen_glClearBufferuiv
#define glClearBufferfv __rglgen_glClearBufferfv
#define glClearBufferfi __rglgen_glClearBufferfi
#define glGetStringi __rglgen_glGetStringi
#define glCopyBufferSubData __rglgen_glCopyBufferSubData
#define glGetUniformIndices __rglgen_glGetUniformIndices
#define glGetActiveUniformsiv __rglgen_glGetActiveUniformsiv
#define glGetUniformBlockIndex __rglgen_glGetUniformBlockIndex
#define glGetActiveUniformBlockiv __rglgen_glGetActiveUniformBlockiv
#define glGetActiveUniformBlockName __rglgen_glGetActiveUniformBlockName
#define glUniformBlockBinding __rglgen_glUniformBlockBinding
#define glDrawArraysInstanced __rglgen_glDrawArraysInstanced
#define glDrawElementsInstanced __rglgen_glDrawElementsInstanced
#define glFenceSync __rglgen_glFenceSync
#define glIsSync __rglgen_glIsSync
#define glDeleteSync __rglgen_glDeleteSync
#define glClientWaitSync __rglgen_glClientWaitSync
#define glWaitSync __rglgen_glWaitSync
#define glGetInteger64v __rglgen_glGetInteger64v
#define glGetSynciv __rglgen_glGetSynciv
#define glGetInteger64i_v __rglgen_glGetInteger64i_v
#define glGetBufferParameteri64v __rglgen_glGetBufferParameteri64v
#define glGenSamplers __rglgen_glGenSamplers
#define glDeleteSamplers __rglgen_glDeleteSamplers
#define glIsSampler __rglgen_glIsSampler
#define glBindSampler __rglgen_glBindSampler
#define glSamplerParameteri __rglgen_glSamplerParameteri
#define glSamplerParameteriv __rglgen_glSamplerParameteriv
#define glSamplerParameterf __rglgen_glSamplerParameterf
#define glSamplerParameterfv __rglgen_glSamplerParameterfv
#define glGetSamplerParameteriv __rglgen_glGetSamplerParameteriv
#define glGetSamplerParameterfv __rglgen_glGetSamplerParameterfv
#define glVertexAttribDivisor __rglgen_glVertexAttribDivisor
#define glBindTransformFeedback __rglgen_glBindTransformFeedback
#define glDeleteTransformFeedbacks __rglgen_glDeleteTransformFeedbacks
#define glGenTransformFeedbacks __rglgen_glGenTransformFeedbacks
#define glIsTransformFeedback __rglgen_glIsTransformFeedback
#define glPauseTransformFeedback __rglgen_glPauseTransformFeedback
#define glResumeTransformFeedback __rglgen_glResumeTransformFeedback
#define glGetProgramBinary __rglgen_glGetProgramBinary
#define glProgramBinary __rglgen_glProgramBinary
#define glProgramParameteri __rglgen_glProgramParameteri
#define glInvalidateFramebuffer __rglgen_glInvalidateFramebuffer
#define glInvalidateSubFramebuffer __rglgen_glInvalidateSubFramebuffer
#define glTexStorage2D __rglgen_glTexStorage2D
#define glTexStorage3D __rglgen_glTexStorage3D
#define glGetInternalformativ __rglgen_glGetInternalformativ
#define glDispatchCompute __rglgen_glDispatchCompute
#define glDispatchComputeIndirect __rglgen_glDispatchComputeIndirect
#define glDrawArraysIndirect __rglgen_glDrawArraysIndirect
#define glDrawElementsIndirect __rglgen_glDrawElementsIndirect
#define glFramebufferParameteri __rglgen_glFramebufferParameteri
#define glGetFramebufferParameteriv __rglgen_glGetFramebufferParameteriv
#define glGetProgramInterfaceiv __rglgen_glGetProgramInterfaceiv
#define glGetProgramResourceIndex __rglgen_glGetProgramResourceIndex
#define glGetProgramResourceName __rglgen_glGetProgramResourceName
#define glGetProgramResourceiv __rglgen_glGetProgramResourceiv
#define glGetProgramResourceLocation __rglgen_glGetProgramResourceLocation
#define glUseProgramStages __rglgen_glUseProgramStages
#define glActiveShaderProgram __rglgen_glActiveShaderProgram
#define glCreateShaderProgramv __rglgen_glCreateShaderProgramv
#define glBindProgramPipeline __rglgen_glBindProgramPipeline
#define glDeleteProgramPipelines __rglgen_glDeleteProgramPipelines
#define glGenProgramPipelines __rglgen_glGenProgramPipelines
#define glIsProgramPipeline __rglgen_glIsProgramPipeline
#define glGetProgramPipelineiv __rglgen_glGetProgramPipelineiv
#define glProgramUniform1i __rglgen_glProgramUniform1i
#define glProgramUniform2i __rglgen_glProgramUniform2i
#define glProgramUniform3i __rglgen_glProgramUniform3i
#define glProgramUniform4i __rglgen_glProgramUniform4i
#define glProgramUniform1ui __rglgen_glProgramUniform1ui
#define glProgramUniform2ui __rglgen_glProgramUniform2ui
#define glProgramUniform3ui __rglgen_glProgramUniform3ui
#define glProgramUniform4ui __rglgen_glProgramUniform4ui
#define glProgramUniform1f __rglgen_glProgramUniform1f
#define glProgramUniform2f __rglgen_glProgramUniform2f
#define glProgramUniform3f __rglgen_glProgramUniform3f
#define glProgramUniform4f __rglgen_glProgramUniform4f
#define glProgramUniform1iv __rglgen_glProgramUniform1iv
#define glProgramUniform2iv __rglgen_glProgramUniform2iv
#define glProgramUniform3iv __rglgen_glProgramUniform3iv
#define glProgramUniform4iv __rglgen_glProgramUniform4iv
#define glProgramUniform1uiv __rglgen_glProgramUniform1uiv
#define glProgramUniform2uiv __rglgen_glProgramUniform2uiv
#define glProgramUniform3uiv __rglgen_glProgramUniform3uiv
#define glProgramUniform4uiv __rglgen_glProgramUniform4uiv
#define glProgramUniform1fv __rglgen_glProgramUniform1fv
#define glProgramUniform2fv __rglgen_glProgramUniform2fv
#define glProgramUniform3fv __rglgen_glProgramUniform3fv
#define glProgramUniform4fv __rglgen_glProgramUniform4fv
#define glProgramUniformMatrix2fv __rglgen_glProgramUniformMatrix2fv
#define glProgramUniformMatrix3fv __rglgen_glProgramUniformMatrix3fv
#define glProgramUniformMatrix4fv __rglgen_glProgramUniformMatrix4fv
#define glProgramUniformMatrix2x3fv __rglgen_glProgramUniformMatrix2x3fv
#define glProgramUniformMatrix3x2fv __rglgen_glProgramUniformMatrix3x2fv
#define glProgramUniformMatrix2x4fv __rglgen_glProgramUniformMatrix2x4fv
#define glProgramUniformMatrix4x2fv __rglgen_glProgramUniformMatrix4x2fv
#define glProgramUniformMatrix3x4fv __rglgen_glProgramUniformMatrix3x4fv
#define glProgramUniformMatrix4x3fv __rglgen_glProgramUniformMatrix4x3fv
#define glValidateProgramPipeline __rglgen_glValidateProgramPipeline
#define glGetProgramPipelineInfoLog __rglgen_glGetProgramPipelineInfoLog
#define glBindImageTexture __rglgen_glBindImageTexture
#define glGetBooleanv __rglgen_glGetBooleanv
#define glGetBooleani_v __rglgen_glGetBooleani_v
#define glMemoryBarrier __rglgen_glMemoryBarrier
#define glMemoryBarrierByRegion __rglgen_glMemoryBarrierByRegion
#define glTexStorage2DMultisample __rglgen_glTexStorage2DMultisample
#define glGetMultisamplefv __rglgen_glGetMultisamplefv
#define glSampleMaski __rglgen_glSampleMaski
#define glGetTexLevelParameteriv __rglgen_glGetTexLevelParameteriv
#define glGetTexLevelParameterfv __rglgen_glGetTexLevelParameterfv
#define glBindVertexBuffer __rglgen_glBindVertexBuffer
#define glVertexAttribFormat __rglgen_glVertexAttribFormat
#define glVertexAttribIFormat __rglgen_glVertexAttribIFormat
#define glVertexAttribBinding __rglgen_glVertexAttribBinding
#define glVertexBindingDivisor __rglgen_glVertexBindingDivisor
#define glBlendBarrier __rglgen_glBlendBarrier
#define glCopyImageSubData __rglgen_glCopyImageSubData
#define glDebugMessageControl __rglgen_glDebugMessageControl
#define glDebugMessageInsert __rglgen_glDebugMessageInsert
#define glDebugMessageCallback __rglgen_glDebugMessageCallback
#define glGetDebugMessageLog __rglgen_glGetDebugMessageLog
#define glPushDebugGroup __rglgen_glPushDebugGroup
#define glPopDebugGroup __rglgen_glPopDebugGroup
#define glObjectLabel __rglgen_glObjectLabel
#define glGetObjectLabel __rglgen_glGetObjectLabel
#define glObjectPtrLabel __rglgen_glObjectPtrLabel
#define glGetObjectPtrLabel __rglgen_glGetObjectPtrLabel
#define glGetPointerv __rglgen_glGetPointerv
#define glEnablei __rglgen_glEnablei
#define glDisablei __rglgen_glDisablei
#define glBlendEquationi __rglgen_glBlendEquationi
#define glBlendEquationSeparatei __rglgen_glBlendEquationSeparatei
#define glBlendFunci __rglgen_glBlendFunci
#define glBlendFuncSeparatei __rglgen_glBlendFuncSeparatei
#define glColorMaski __rglgen_glColorMaski
#define glIsEnabledi __rglgen_glIsEnabledi
#define glDrawElementsBaseVertex __rglgen_glDrawElementsBaseVertex
#define glDrawRangeElementsBaseVertex __rglgen_glDrawRangeElementsBaseVertex
#define glDrawElementsInstancedBaseVertex __rglgen_glDrawElementsInstancedBaseVertex
#define glFramebufferTexture __rglgen_glFramebufferTexture
#define glPrimitiveBoundingBox __rglgen_glPrimitiveBoundingBox
#define glGetGraphicsResetStatus __rglgen_glGetGraphicsResetStatus
#define glReadnPixels __rglgen_glReadnPixels
#define glGetnUniformfv __rglgen_glGetnUniformfv
#define glGetnUniformiv __rglgen_glGetnUniformiv
#define glGetnUniformuiv __rglgen_glGetnUniformuiv
#define glMinSampleShading __rglgen_glMinSampleShading
#define glPatchParameteri __rglgen_glPatchParameteri
#define glTexParameterIiv __rglgen_glTexParameterIiv
#define glTexParameterIuiv __rglgen_glTexParameterIuiv
#define glGetTexParameterIiv __rglgen_glGetTexParameterIiv
#define glGetTexParameterIuiv __rglgen_glGetTexParameterIuiv
#define glSamplerParameterIiv __rglgen_glSamplerParameterIiv
#define glSamplerParameterIuiv __rglgen_glSamplerParameterIuiv
#define glGetSamplerParameterIiv __rglgen_glGetSamplerParameterIiv
#define glGetSamplerParameterIuiv __rglgen_glGetSamplerParameterIuiv
#define glTexBuffer __rglgen_glTexBuffer
#define glTexBufferRange __rglgen_glTexBufferRange
#define glTexStorage3DMultisample __rglgen_glTexStorage3DMultisample

extern RGLSYMGLACTIVETEXTUREPROC __rglgen_glActiveTexture;
extern RGLSYMGLATTACHSHADERPROC __rglgen_glAttachShader;
extern RGLSYMGLBINDATTRIBLOCATIONPROC __rglgen_glBindAttribLocation;
extern RGLSYMGLBINDBUFFERPROC __rglgen_glBindBuffer;
extern RGLSYMGLBINDFRAMEBUFFERPROC __rglgen_glBindFramebuffer;
extern RGLSYMGLBINDRENDERBUFFERPROC __rglgen_glBindRenderbuffer;
extern RGLSYMGLBINDTEXTUREPROC __rglgen_glBindTexture;
extern RGLSYMGLBLENDCOLORPROC __rglgen_glBlendColor;
extern RGLSYMGLBLENDEQUATIONPROC __rglgen_glBlendEquation;
extern RGLSYMGLBLENDEQUATIONSEPARATEPROC __rglgen_glBlendEquationSeparate;
extern RGLSYMGLBLENDFUNCPROC __rglgen_glBlendFunc;
extern RGLSYMGLBLENDFUNCSEPARATEPROC __rglgen_glBlendFuncSeparate;
extern RGLSYMGLBUFFERDATAPROC __rglgen_glBufferData;
extern RGLSYMGLBUFFERSUBDATAPROC __rglgen_glBufferSubData;
extern RGLSYMGLCHECKFRAMEBUFFERSTATUSPROC __rglgen_glCheckFramebufferStatus;
extern RGLSYMGLCLEARPROC __rglgen_glClear;
extern RGLSYMGLCLEARCOLORPROC __rglgen_glClearColor;
extern RGLSYMGLCLEARDEPTHFPROC __rglgen_glClearDepthf;
extern RGLSYMGLCLEARSTENCILPROC __rglgen_glClearStencil;
extern RGLSYMGLCOLORMASKPROC __rglgen_glColorMask;
extern RGLSYMGLCOMPILESHADERPROC __rglgen_glCompileShader;
extern RGLSYMGLCOMPRESSEDTEXIMAGE2DPROC __rglgen_glCompressedTexImage2D;
extern RGLSYMGLCOMPRESSEDTEXSUBIMAGE2DPROC __rglgen_glCompressedTexSubImage2D;
extern RGLSYMGLCOPYTEXIMAGE2DPROC __rglgen_glCopyTexImage2D;
extern RGLSYMGLCOPYTEXSUBIMAGE2DPROC __rglgen_glCopyTexSubImage2D;
extern RGLSYMGLCREATEPROGRAMPROC __rglgen_glCreateProgram;
extern RGLSYMGLCREATESHADERPROC __rglgen_glCreateShader;
extern RGLSYMGLCULLFACEPROC __rglgen_glCullFace;
extern RGLSYMGLDELETEBUFFERSPROC __rglgen_glDeleteBuffers;
extern RGLSYMGLDELETEFRAMEBUFFERSPROC __rglgen_glDeleteFramebuffers;
extern RGLSYMGLDELETEPROGRAMPROC __rglgen_glDeleteProgram;
extern RGLSYMGLDELETERENDERBUFFERSPROC __rglgen_glDeleteRenderbuffers;
extern RGLSYMGLDELETESHADERPROC __rglgen_glDeleteShader;
extern RGLSYMGLDELETETEXTURESPROC __rglgen_glDeleteTextures;
extern RGLSYMGLDEPTHFUNCPROC __rglgen_glDepthFunc;
extern RGLSYMGLDEPTHMASKPROC __rglgen_glDepthMask;
extern RGLSYMGLDEPTHRANGEFPROC __rglgen_glDepthRangef;
extern RGLSYMGLDETACHSHADERPROC __rglgen_glDetachShader;
extern RGLSYMGLDISABLEPROC __rglgen_glDisable;
extern RGLSYMGLDISABLEVERTEXATTRIBARRAYPROC __rglgen_glDisableVertexAttribArray;
extern RGLSYMGLDRAWARRAYSPROC __rglgen_glDrawArrays;
extern RGLSYMGLDRAWELEMENTSPROC __rglgen_glDrawElements;
extern RGLSYMGLENABLEPROC __rglgen_glEnable;
extern RGLSYMGLENABLEVERTEXATTRIBARRAYPROC __rglgen_glEnableVertexAttribArray;
extern RGLSYMGLFINISHPROC __rglgen_glFinish;
extern RGLSYMGLFLUSHPROC __rglgen_glFlush;
extern RGLSYMGLFRAMEBUFFERRENDERBUFFERPROC __rglgen_glFramebufferRenderbuffer;
extern RGLSYMGLFRAMEBUFFERTEXTURE2DPROC __rglgen_glFramebufferTexture2D;
extern RGLSYMGLFRONTFACEPROC __rglgen_glFrontFace;
extern RGLSYMGLGENBUFFERSPROC __rglgen_glGenBuffers;
extern RGLSYMGLGENERATEMIPMAPPROC __rglgen_glGenerateMipmap;
extern RGLSYMGLGENFRAMEBUFFERSPROC __rglgen_glGenFramebuffers;
extern RGLSYMGLGENRENDERBUFFERSPROC __rglgen_glGenRenderbuffers;
extern RGLSYMGLGENTEXTURESPROC __rglgen_glGenTextures;
extern RGLSYMGLGETACTIVEATTRIBPROC __rglgen_glGetActiveAttrib;
extern RGLSYMGLGETACTIVEUNIFORMPROC __rglgen_glGetActiveUniform;
extern RGLSYMGLGETATTACHEDSHADERSPROC __rglgen_glGetAttachedShaders;
extern RGLSYMGLGETATTRIBLOCATIONPROC __rglgen_glGetAttribLocation;
extern RGLSYMGLGETBOOLEANVPROC __rglgen_glGetBooleanv;
extern RGLSYMGLGETBUFFERPARAMETERIVPROC __rglgen_glGetBufferParameteriv;
extern RGLSYMGLGETERRORPROC __rglgen_glGetError;
extern RGLSYMGLGETFLOATVPROC __rglgen_glGetFloatv;
extern RGLSYMGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC __rglgen_glGetFramebufferAttachmentParameteriv;
extern RGLSYMGLGETINTEGERVPROC __rglgen_glGetIntegerv;
extern RGLSYMGLGETPROGRAMIVPROC __rglgen_glGetProgramiv;
extern RGLSYMGLGETPROGRAMINFOLOGPROC __rglgen_glGetProgramInfoLog;
extern RGLSYMGLGETRENDERBUFFERPARAMETERIVPROC __rglgen_glGetRenderbufferParameteriv;
extern RGLSYMGLGETSHADERIVPROC __rglgen_glGetShaderiv;
extern RGLSYMGLGETSHADERINFOLOGPROC __rglgen_glGetShaderInfoLog;
extern RGLSYMGLGETSHADERPRECISIONFORMATPROC __rglgen_glGetShaderPrecisionFormat;
extern RGLSYMGLGETSHADERSOURCEPROC __rglgen_glGetShaderSource;
extern RGLSYMGLGETSTRINGPROC __rglgen_glGetString;
extern RGLSYMGLGETTEXPARAMETERFVPROC __rglgen_glGetTexParameterfv;
extern RGLSYMGLGETTEXPARAMETERIVPROC __rglgen_glGetTexParameteriv;
extern RGLSYMGLGETUNIFORMFVPROC __rglgen_glGetUniformfv;
extern RGLSYMGLGETUNIFORMIVPROC __rglgen_glGetUniformiv;
extern RGLSYMGLGETUNIFORMLOCATIONPROC __rglgen_glGetUniformLocation;
extern RGLSYMGLGETVERTEXATTRIBFVPROC __rglgen_glGetVertexAttribfv;
extern RGLSYMGLGETVERTEXATTRIBIVPROC __rglgen_glGetVertexAttribiv;
extern RGLSYMGLGETVERTEXATTRIBPOINTERVPROC __rglgen_glGetVertexAttribPointerv;
extern RGLSYMGLHINTPROC __rglgen_glHint;
extern RGLSYMGLISBUFFERPROC __rglgen_glIsBuffer;
extern RGLSYMGLISENABLEDPROC __rglgen_glIsEnabled;
extern RGLSYMGLISFRAMEBUFFERPROC __rglgen_glIsFramebuffer;
extern RGLSYMGLISPROGRAMPROC __rglgen_glIsProgram;
extern RGLSYMGLISRENDERBUFFERPROC __rglgen_glIsRenderbuffer;
extern RGLSYMGLISSHADERPROC __rglgen_glIsShader;
extern RGLSYMGLISTEXTUREPROC __rglgen_glIsTexture;
extern RGLSYMGLLINEWIDTHPROC __rglgen_glLineWidth;
extern RGLSYMGLLINKPROGRAMPROC __rglgen_glLinkProgram;
extern RGLSYMGLPIXELSTOREIPROC __rglgen_glPixelStorei;
extern RGLSYMGLPOLYGONOFFSETPROC __rglgen_glPolygonOffset;
extern RGLSYMGLREADPIXELSPROC __rglgen_glReadPixels;
extern RGLSYMGLRELEASESHADERCOMPILERPROC __rglgen_glReleaseShaderCompiler;
extern RGLSYMGLRENDERBUFFERSTORAGEPROC __rglgen_glRenderbufferStorage;
extern RGLSYMGLSAMPLECOVERAGEPROC __rglgen_glSampleCoverage;
extern RGLSYMGLSCISSORPROC __rglgen_glScissor;
extern RGLSYMGLSHADERBINARYPROC __rglgen_glShaderBinary;
extern RGLSYMGLSHADERSOURCEPROC __rglgen_glShaderSource;
extern RGLSYMGLSTENCILFUNCPROC __rglgen_glStencilFunc;
extern RGLSYMGLSTENCILFUNCSEPARATEPROC __rglgen_glStencilFuncSeparate;
extern RGLSYMGLSTENCILMASKPROC __rglgen_glStencilMask;
extern RGLSYMGLSTENCILMASKSEPARATEPROC __rglgen_glStencilMaskSeparate;
extern RGLSYMGLSTENCILOPPROC __rglgen_glStencilOp;
extern RGLSYMGLSTENCILOPSEPARATEPROC __rglgen_glStencilOpSeparate;
extern RGLSYMGLTEXIMAGE2DPROC __rglgen_glTexImage2D;
extern RGLSYMGLTEXPARAMETERFPROC __rglgen_glTexParameterf;
extern RGLSYMGLTEXPARAMETERFVPROC __rglgen_glTexParameterfv;
extern RGLSYMGLTEXPARAMETERIPROC __rglgen_glTexParameteri;
extern RGLSYMGLTEXPARAMETERIVPROC __rglgen_glTexParameteriv;
extern RGLSYMGLTEXSUBIMAGE2DPROC __rglgen_glTexSubImage2D;
extern RGLSYMGLUNIFORM1FPROC __rglgen_glUniform1f;
extern RGLSYMGLUNIFORM1FVPROC __rglgen_glUniform1fv;
extern RGLSYMGLUNIFORM1IPROC __rglgen_glUniform1i;
extern RGLSYMGLUNIFORM1IVPROC __rglgen_glUniform1iv;
extern RGLSYMGLUNIFORM2FPROC __rglgen_glUniform2f;
extern RGLSYMGLUNIFORM2FVPROC __rglgen_glUniform2fv;
extern RGLSYMGLUNIFORM2IPROC __rglgen_glUniform2i;
extern RGLSYMGLUNIFORM2IVPROC __rglgen_glUniform2iv;
extern RGLSYMGLUNIFORM3FPROC __rglgen_glUniform3f;
extern RGLSYMGLUNIFORM3FVPROC __rglgen_glUniform3fv;
extern RGLSYMGLUNIFORM3IPROC __rglgen_glUniform3i;
extern RGLSYMGLUNIFORM3IVPROC __rglgen_glUniform3iv;
extern RGLSYMGLUNIFORM4FPROC __rglgen_glUniform4f;
extern RGLSYMGLUNIFORM4FVPROC __rglgen_glUniform4fv;
extern RGLSYMGLUNIFORM4IPROC __rglgen_glUniform4i;
extern RGLSYMGLUNIFORM4IVPROC __rglgen_glUniform4iv;
extern RGLSYMGLUNIFORMMATRIX2FVPROC __rglgen_glUniformMatrix2fv;
extern RGLSYMGLUNIFORMMATRIX3FVPROC __rglgen_glUniformMatrix3fv;
extern RGLSYMGLUNIFORMMATRIX4FVPROC __rglgen_glUniformMatrix4fv;
extern RGLSYMGLUSEPROGRAMPROC __rglgen_glUseProgram;
extern RGLSYMGLVALIDATEPROGRAMPROC __rglgen_glValidateProgram;
extern RGLSYMGLVERTEXATTRIB1FPROC __rglgen_glVertexAttrib1f;
extern RGLSYMGLVERTEXATTRIB1FVPROC __rglgen_glVertexAttrib1fv;
extern RGLSYMGLVERTEXATTRIB2FPROC __rglgen_glVertexAttrib2f;
extern RGLSYMGLVERTEXATTRIB2FVPROC __rglgen_glVertexAttrib2fv;
extern RGLSYMGLVERTEXATTRIB3FPROC __rglgen_glVertexAttrib3f;
extern RGLSYMGLVERTEXATTRIB3FVPROC __rglgen_glVertexAttrib3fv;
extern RGLSYMGLVERTEXATTRIB4FPROC __rglgen_glVertexAttrib4f;
extern RGLSYMGLVERTEXATTRIB4FVPROC __rglgen_glVertexAttrib4fv;
extern RGLSYMGLVERTEXATTRIBPOINTERPROC __rglgen_glVertexAttribPointer;
extern RGLSYMGLVIEWPORTPROC __rglgen_glViewport;
extern RGLSYMGLREADBUFFERPROC __rglgen_glReadBuffer;
extern RGLSYMGLDRAWRANGEELEMENTSPROC __rglgen_glDrawRangeElements;
extern RGLSYMGLTEXIMAGE3DPROC __rglgen_glTexImage3D;
extern RGLSYMGLTEXSUBIMAGE3DPROC __rglgen_glTexSubImage3D;
extern RGLSYMGLCOPYTEXSUBIMAGE3DPROC __rglgen_glCopyTexSubImage3D;
extern RGLSYMGLCOMPRESSEDTEXIMAGE3DPROC __rglgen_glCompressedTexImage3D;
extern RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DPROC __rglgen_glCompressedTexSubImage3D;
extern RGLSYMGLGENQUERIESPROC __rglgen_glGenQueries;
extern RGLSYMGLDELETEQUERIESPROC __rglgen_glDeleteQueries;
extern RGLSYMGLISQUERYPROC __rglgen_glIsQuery;
extern RGLSYMGLBEGINQUERYPROC __rglgen_glBeginQuery;
extern RGLSYMGLENDQUERYPROC __rglgen_glEndQuery;
extern RGLSYMGLGETQUERYIVPROC __rglgen_glGetQueryiv;
extern RGLSYMGLGETQUERYOBJECTUIVPROC __rglgen_glGetQueryObjectuiv;
extern RGLSYMGLUNMAPBUFFERPROC __rglgen_glUnmapBuffer;
extern RGLSYMGLGETBUFFERPOINTERVPROC __rglgen_glGetBufferPointerv;
extern RGLSYMGLDRAWBUFFERSPROC __rglgen_glDrawBuffers;
extern RGLSYMGLUNIFORMMATRIX2X3FVPROC __rglgen_glUniformMatrix2x3fv;
extern RGLSYMGLUNIFORMMATRIX3X2FVPROC __rglgen_glUniformMatrix3x2fv;
extern RGLSYMGLUNIFORMMATRIX2X4FVPROC __rglgen_glUniformMatrix2x4fv;
extern RGLSYMGLUNIFORMMATRIX4X2FVPROC __rglgen_glUniformMatrix4x2fv;
extern RGLSYMGLUNIFORMMATRIX3X4FVPROC __rglgen_glUniformMatrix3x4fv;
extern RGLSYMGLUNIFORMMATRIX4X3FVPROC __rglgen_glUniformMatrix4x3fv;
extern RGLSYMGLBLITFRAMEBUFFERPROC __rglgen_glBlitFramebuffer;
extern RGLSYMGLRENDERBUFFERSTORAGEMULTISAMPLEPROC __rglgen_glRenderbufferStorageMultisample;
extern RGLSYMGLFRAMEBUFFERTEXTURELAYERPROC __rglgen_glFramebufferTextureLayer;
extern RGLSYMGLMAPBUFFERRANGEPROC __rglgen_glMapBufferRange;
extern RGLSYMGLFLUSHMAPPEDBUFFERRANGEPROC __rglgen_glFlushMappedBufferRange;
extern RGLSYMGLBINDVERTEXARRAYPROC __rglgen_glBindVertexArray;
extern RGLSYMGLDELETEVERTEXARRAYSPROC __rglgen_glDeleteVertexArrays;
extern RGLSYMGLGENVERTEXARRAYSPROC __rglgen_glGenVertexArrays;
extern RGLSYMGLISVERTEXARRAYPROC __rglgen_glIsVertexArray;
extern RGLSYMGLGETINTEGERI_VPROC __rglgen_glGetIntegeri_v;
extern RGLSYMGLBEGINTRANSFORMFEEDBACKPROC __rglgen_glBeginTransformFeedback;
extern RGLSYMGLENDTRANSFORMFEEDBACKPROC __rglgen_glEndTransformFeedback;
extern RGLSYMGLBINDBUFFERRANGEPROC __rglgen_glBindBufferRange;
extern RGLSYMGLBINDBUFFERBASEPROC __rglgen_glBindBufferBase;
extern RGLSYMGLTRANSFORMFEEDBACKVARYINGSPROC __rglgen_glTransformFeedbackVaryings;
extern RGLSYMGLGETTRANSFORMFEEDBACKVARYINGPROC __rglgen_glGetTransformFeedbackVarying;
extern RGLSYMGLVERTEXATTRIBIPOINTERPROC __rglgen_glVertexAttribIPointer;
extern RGLSYMGLGETVERTEXATTRIBIIVPROC __rglgen_glGetVertexAttribIiv;
extern RGLSYMGLGETVERTEXATTRIBIUIVPROC __rglgen_glGetVertexAttribIuiv;
extern RGLSYMGLVERTEXATTRIBI4IPROC __rglgen_glVertexAttribI4i;
extern RGLSYMGLVERTEXATTRIBI4UIPROC __rglgen_glVertexAttribI4ui;
extern RGLSYMGLVERTEXATTRIBI4IVPROC __rglgen_glVertexAttribI4iv;
extern RGLSYMGLVERTEXATTRIBI4UIVPROC __rglgen_glVertexAttribI4uiv;
extern RGLSYMGLGETUNIFORMUIVPROC __rglgen_glGetUniformuiv;
extern RGLSYMGLGETFRAGDATALOCATIONPROC __rglgen_glGetFragDataLocation;
extern RGLSYMGLUNIFORM1UIPROC __rglgen_glUniform1ui;
extern RGLSYMGLUNIFORM2UIPROC __rglgen_glUniform2ui;
extern RGLSYMGLUNIFORM3UIPROC __rglgen_glUniform3ui;
extern RGLSYMGLUNIFORM4UIPROC __rglgen_glUniform4ui;
extern RGLSYMGLUNIFORM1UIVPROC __rglgen_glUniform1uiv;
extern RGLSYMGLUNIFORM2UIVPROC __rglgen_glUniform2uiv;
extern RGLSYMGLUNIFORM3UIVPROC __rglgen_glUniform3uiv;
extern RGLSYMGLUNIFORM4UIVPROC __rglgen_glUniform4uiv;
extern RGLSYMGLCLEARBUFFERIVPROC __rglgen_glClearBufferiv;
extern RGLSYMGLCLEARBUFFERUIVPROC __rglgen_glClearBufferuiv;
extern RGLSYMGLCLEARBUFFERFVPROC __rglgen_glClearBufferfv;
extern RGLSYMGLCLEARBUFFERFIPROC __rglgen_glClearBufferfi;
extern RGLSYMGLGETSTRINGIPROC __rglgen_glGetStringi;
extern RGLSYMGLCOPYBUFFERSUBDATAPROC __rglgen_glCopyBufferSubData;
extern RGLSYMGLGETUNIFORMINDICESPROC __rglgen_glGetUniformIndices;
extern RGLSYMGLGETACTIVEUNIFORMSIVPROC __rglgen_glGetActiveUniformsiv;
extern RGLSYMGLGETUNIFORMBLOCKINDEXPROC __rglgen_glGetUniformBlockIndex;
extern RGLSYMGLGETACTIVEUNIFORMBLOCKIVPROC __rglgen_glGetActiveUniformBlockiv;
extern RGLSYMGLGETACTIVEUNIFORMBLOCKNAMEPROC __rglgen_glGetActiveUniformBlockName;
extern RGLSYMGLUNIFORMBLOCKBINDINGPROC __rglgen_glUniformBlockBinding;
extern RGLSYMGLDRAWARRAYSINSTANCEDPROC __rglgen_glDrawArraysInstanced;
extern RGLSYMGLDRAWELEMENTSINSTANCEDPROC __rglgen_glDrawElementsInstanced;
extern RGLSYMGLFENCESYNCPROC __rglgen_glFenceSync;
extern RGLSYMGLISSYNCPROC __rglgen_glIsSync;
extern RGLSYMGLDELETESYNCPROC __rglgen_glDeleteSync;
extern RGLSYMGLCLIENTWAITSYNCPROC __rglgen_glClientWaitSync;
extern RGLSYMGLWAITSYNCPROC __rglgen_glWaitSync;
extern RGLSYMGLGETINTEGER64VPROC __rglgen_glGetInteger64v;
extern RGLSYMGLGETSYNCIVPROC __rglgen_glGetSynciv;
extern RGLSYMGLGETINTEGER64I_VPROC __rglgen_glGetInteger64i_v;
extern RGLSYMGLGETBUFFERPARAMETERI64VPROC __rglgen_glGetBufferParameteri64v;
extern RGLSYMGLGENSAMPLERSPROC __rglgen_glGenSamplers;
extern RGLSYMGLDELETESAMPLERSPROC __rglgen_glDeleteSamplers;
extern RGLSYMGLISSAMPLERPROC __rglgen_glIsSampler;
extern RGLSYMGLBINDSAMPLERPROC __rglgen_glBindSampler;
extern RGLSYMGLSAMPLERPARAMETERIPROC __rglgen_glSamplerParameteri;
extern RGLSYMGLSAMPLERPARAMETERIVPROC __rglgen_glSamplerParameteriv;
extern RGLSYMGLSAMPLERPARAMETERFPROC __rglgen_glSamplerParameterf;
extern RGLSYMGLSAMPLERPARAMETERFVPROC __rglgen_glSamplerParameterfv;
extern RGLSYMGLGETSAMPLERPARAMETERIVPROC __rglgen_glGetSamplerParameteriv;
extern RGLSYMGLGETSAMPLERPARAMETERFVPROC __rglgen_glGetSamplerParameterfv;
extern RGLSYMGLVERTEXATTRIBDIVISORPROC __rglgen_glVertexAttribDivisor;
extern RGLSYMGLBINDTRANSFORMFEEDBACKPROC __rglgen_glBindTransformFeedback;
extern RGLSYMGLDELETETRANSFORMFEEDBACKSPROC __rglgen_glDeleteTransformFeedbacks;
extern RGLSYMGLGENTRANSFORMFEEDBACKSPROC __rglgen_glGenTransformFeedbacks;
extern RGLSYMGLISTRANSFORMFEEDBACKPROC __rglgen_glIsTransformFeedback;
extern RGLSYMGLPAUSETRANSFORMFEEDBACKPROC __rglgen_glPauseTransformFeedback;
extern RGLSYMGLRESUMETRANSFORMFEEDBACKPROC __rglgen_glResumeTransformFeedback;
extern RGLSYMGLGETPROGRAMBINARYPROC __rglgen_glGetProgramBinary;
extern RGLSYMGLPROGRAMBINARYPROC __rglgen_glProgramBinary;
extern RGLSYMGLPROGRAMPARAMETERIPROC __rglgen_glProgramParameteri;
extern RGLSYMGLINVALIDATEFRAMEBUFFERPROC __rglgen_glInvalidateFramebuffer;
extern RGLSYMGLINVALIDATESUBFRAMEBUFFERPROC __rglgen_glInvalidateSubFramebuffer;
extern RGLSYMGLTEXSTORAGE2DPROC __rglgen_glTexStorage2D;
extern RGLSYMGLTEXSTORAGE3DPROC __rglgen_glTexStorage3D;
extern RGLSYMGLGETINTERNALFORMATIVPROC __rglgen_glGetInternalformativ;
extern RGLSYMGLDISPATCHCOMPUTEPROC __rglgen_glDispatchCompute;
extern RGLSYMGLDISPATCHCOMPUTEINDIRECTPROC __rglgen_glDispatchComputeIndirect;
extern RGLSYMGLDRAWARRAYSINDIRECTPROC __rglgen_glDrawArraysIndirect;
extern RGLSYMGLDRAWELEMENTSINDIRECTPROC __rglgen_glDrawElementsIndirect;
extern RGLSYMGLFRAMEBUFFERPARAMETERIPROC __rglgen_glFramebufferParameteri;
extern RGLSYMGLGETFRAMEBUFFERPARAMETERIVPROC __rglgen_glGetFramebufferParameteriv;
extern RGLSYMGLGETPROGRAMINTERFACEIVPROC __rglgen_glGetProgramInterfaceiv;
extern RGLSYMGLGETPROGRAMRESOURCEINDEXPROC __rglgen_glGetProgramResourceIndex;
extern RGLSYMGLGETPROGRAMRESOURCENAMEPROC __rglgen_glGetProgramResourceName;
extern RGLSYMGLGETPROGRAMRESOURCEIVPROC __rglgen_glGetProgramResourceiv;
extern RGLSYMGLGETPROGRAMRESOURCELOCATIONPROC __rglgen_glGetProgramResourceLocation;
extern RGLSYMGLUSEPROGRAMSTAGESPROC __rglgen_glUseProgramStages;
extern RGLSYMGLACTIVESHADERPROGRAMPROC __rglgen_glActiveShaderProgram;
extern RGLSYMGLCREATESHADERPROGRAMVPROC __rglgen_glCreateShaderProgramv;
extern RGLSYMGLBINDPROGRAMPIPELINEPROC __rglgen_glBindProgramPipeline;
extern RGLSYMGLDELETEPROGRAMPIPELINESPROC __rglgen_glDeleteProgramPipelines;
extern RGLSYMGLGENPROGRAMPIPELINESPROC __rglgen_glGenProgramPipelines;
extern RGLSYMGLISPROGRAMPIPELINEPROC __rglgen_glIsProgramPipeline;
extern RGLSYMGLGETPROGRAMPIPELINEIVPROC __rglgen_glGetProgramPipelineiv;
extern RGLSYMGLPROGRAMUNIFORM1IPROC __rglgen_glProgramUniform1i;
extern RGLSYMGLPROGRAMUNIFORM2IPROC __rglgen_glProgramUniform2i;
extern RGLSYMGLPROGRAMUNIFORM3IPROC __rglgen_glProgramUniform3i;
extern RGLSYMGLPROGRAMUNIFORM4IPROC __rglgen_glProgramUniform4i;
extern RGLSYMGLPROGRAMUNIFORM1UIPROC __rglgen_glProgramUniform1ui;
extern RGLSYMGLPROGRAMUNIFORM2UIPROC __rglgen_glProgramUniform2ui;
extern RGLSYMGLPROGRAMUNIFORM3UIPROC __rglgen_glProgramUniform3ui;
extern RGLSYMGLPROGRAMUNIFORM4UIPROC __rglgen_glProgramUniform4ui;
extern RGLSYMGLPROGRAMUNIFORM1FPROC __rglgen_glProgramUniform1f;
extern RGLSYMGLPROGRAMUNIFORM2FPROC __rglgen_glProgramUniform2f;
extern RGLSYMGLPROGRAMUNIFORM3FPROC __rglgen_glProgramUniform3f;
extern RGLSYMGLPROGRAMUNIFORM4FPROC __rglgen_glProgramUniform4f;
extern RGLSYMGLPROGRAMUNIFORM1IVPROC __rglgen_glProgramUniform1iv;
extern RGLSYMGLPROGRAMUNIFORM2IVPROC __rglgen_glProgramUniform2iv;
extern RGLSYMGLPROGRAMUNIFORM3IVPROC __rglgen_glProgramUniform3iv;
extern RGLSYMGLPROGRAMUNIFORM4IVPROC __rglgen_glProgramUniform4iv;
extern RGLSYMGLPROGRAMUNIFORM1UIVPROC __rglgen_glProgramUniform1uiv;
extern RGLSYMGLPROGRAMUNIFORM2UIVPROC __rglgen_glProgramUniform2uiv;
extern RGLSYMGLPROGRAMUNIFORM3UIVPROC __rglgen_glProgramUniform3uiv;
extern RGLSYMGLPROGRAMUNIFORM4UIVPROC __rglgen_glProgramUniform4uiv;
extern RGLSYMGLPROGRAMUNIFORM1FVPROC __rglgen_glProgramUniform1fv;
extern RGLSYMGLPROGRAMUNIFORM2FVPROC __rglgen_glProgramUniform2fv;
extern RGLSYMGLPROGRAMUNIFORM3FVPROC __rglgen_glProgramUniform3fv;
extern RGLSYMGLPROGRAMUNIFORM4FVPROC __rglgen_glProgramUniform4fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2FVPROC __rglgen_glProgramUniformMatrix2fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3FVPROC __rglgen_glProgramUniformMatrix3fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4FVPROC __rglgen_glProgramUniformMatrix4fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2X3FVPROC __rglgen_glProgramUniformMatrix2x3fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3X2FVPROC __rglgen_glProgramUniformMatrix3x2fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2X4FVPROC __rglgen_glProgramUniformMatrix2x4fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4X2FVPROC __rglgen_glProgramUniformMatrix4x2fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3X4FVPROC __rglgen_glProgramUniformMatrix3x4fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4X3FVPROC __rglgen_glProgramUniformMatrix4x3fv;
extern RGLSYMGLVALIDATEPROGRAMPIPELINEPROC __rglgen_glValidateProgramPipeline;
extern RGLSYMGLGETPROGRAMPIPELINEINFOLOGPROC __rglgen_glGetProgramPipelineInfoLog;
extern RGLSYMGLBINDIMAGETEXTUREPROC __rglgen_glBindImageTexture;
extern RGLSYMGLGETBOOLEANI_VPROC __rglgen_glGetBooleani_v;
extern RGLSYMGLMEMORYBARRIERPROC __rglgen_glMemoryBarrier;
extern RGLSYMGLMEMORYBARRIERBYREGIONPROC __rglgen_glMemoryBarrierByRegion;
extern RGLSYMGLTEXSTORAGE2DMULTISAMPLEPROC __rglgen_glTexStorage2DMultisample;
extern RGLSYMGLGETMULTISAMPLEFVPROC __rglgen_glGetMultisamplefv;
extern RGLSYMGLSAMPLEMASKIPROC __rglgen_glSampleMaski;
extern RGLSYMGLGETTEXLEVELPARAMETERIVPROC __rglgen_glGetTexLevelParameteriv;
extern RGLSYMGLGETTEXLEVELPARAMETERFVPROC __rglgen_glGetTexLevelParameterfv;
extern RGLSYMGLBINDVERTEXBUFFERPROC __rglgen_glBindVertexBuffer;
extern RGLSYMGLVERTEXATTRIBFORMATPROC __rglgen_glVertexAttribFormat;
extern RGLSYMGLVERTEXATTRIBIFORMATPROC __rglgen_glVertexAttribIFormat;
extern RGLSYMGLVERTEXATTRIBBINDINGPROC __rglgen_glVertexAttribBinding;
extern RGLSYMGLVERTEXBINDINGDIVISORPROC __rglgen_glVertexBindingDivisor;
extern RGLSYMGLBLENDBARRIERPROC __rglgen_glBlendBarrier;
extern RGLSYMGLCOPYIMAGESUBDATAPROC __rglgen_glCopyImageSubData;
extern RGLSYMGLDEBUGMESSAGECONTROLPROC __rglgen_glDebugMessageControl;
extern RGLSYMGLDEBUGMESSAGEINSERTPROC __rglgen_glDebugMessageInsert;
extern RGLSYMGLDEBUGMESSAGECALLBACKPROC __rglgen_glDebugMessageCallback;
extern RGLSYMGLGETDEBUGMESSAGELOGPROC __rglgen_glGetDebugMessageLog;
extern RGLSYMGLPUSHDEBUGGROUPPROC __rglgen_glPushDebugGroup;
extern RGLSYMGLPOPDEBUGGROUPPROC __rglgen_glPopDebugGroup;
extern RGLSYMGLOBJECTLABELPROC __rglgen_glObjectLabel;
extern RGLSYMGLGETOBJECTLABELPROC __rglgen_glGetObjectLabel;
extern RGLSYMGLOBJECTPTRLABELPROC __rglgen_glObjectPtrLabel;
extern RGLSYMGLGETOBJECTPTRLABELPROC __rglgen_glGetObjectPtrLabel;
extern RGLSYMGLGETPOINTERVPROC __rglgen_glGetPointerv;
extern RGLSYMGLENABLEIPROC __rglgen_glEnablei;
extern RGLSYMGLDISABLEIPROC __rglgen_glDisablei;
extern RGLSYMGLBLENDEQUATIONIPROC __rglgen_glBlendEquationi;
extern RGLSYMGLBLENDEQUATIONSEPARATEIPROC __rglgen_glBlendEquationSeparatei;
extern RGLSYMGLBLENDFUNCIPROC __rglgen_glBlendFunci;
extern RGLSYMGLBLENDFUNCSEPARATEIPROC __rglgen_glBlendFuncSeparatei;
extern RGLSYMGLCOLORMASKIPROC __rglgen_glColorMaski;
extern RGLSYMGLISENABLEDIPROC __rglgen_glIsEnabledi;
extern RGLSYMGLDRAWELEMENTSBASEVERTEXPROC __rglgen_glDrawElementsBaseVertex;
extern RGLSYMGLDRAWRANGEELEMENTSBASEVERTEXPROC __rglgen_glDrawRangeElementsBaseVertex;
extern RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC __rglgen_glDrawElementsInstancedBaseVertex;
extern RGLSYMGLFRAMEBUFFERTEXTUREPROC __rglgen_glFramebufferTexture;
extern RGLSYMGLPRIMITIVEBOUNDINGBOXPROC __rglgen_glPrimitiveBoundingBox;
extern RGLSYMGLGETGRAPHICSRESETSTATUSPROC __rglgen_glGetGraphicsResetStatus;
extern RGLSYMGLREADNPIXELSPROC __rglgen_glReadnPixels;
extern RGLSYMGLGETNUNIFORMFVPROC __rglgen_glGetnUniformfv;
extern RGLSYMGLGETNUNIFORMIVPROC __rglgen_glGetnUniformiv;
extern RGLSYMGLGETNUNIFORMUIVPROC __rglgen_glGetnUniformuiv;
extern RGLSYMGLMINSAMPLESHADINGPROC __rglgen_glMinSampleShading;
extern RGLSYMGLPATCHPARAMETERIPROC __rglgen_glPatchParameteri;
extern RGLSYMGLTEXPARAMETERIIVPROC __rglgen_glTexParameterIiv;
extern RGLSYMGLTEXPARAMETERIUIVPROC __rglgen_glTexParameterIuiv;
extern RGLSYMGLGETTEXPARAMETERIIVPROC __rglgen_glGetTexParameterIiv;
extern RGLSYMGLGETTEXPARAMETERIUIVPROC __rglgen_glGetTexParameterIuiv;
extern RGLSYMGLSAMPLERPARAMETERIIVPROC __rglgen_glSamplerParameterIiv;
extern RGLSYMGLSAMPLERPARAMETERIUIVPROC __rglgen_glSamplerParameterIuiv;
extern RGLSYMGLGETSAMPLERPARAMETERIIVPROC __rglgen_glGetSamplerParameterIiv;
extern RGLSYMGLGETSAMPLERPARAMETERIUIVPROC __rglgen_glGetSamplerParameterIuiv;
extern RGLSYMGLTEXBUFFERPROC __rglgen_glTexBuffer;
extern RGLSYMGLTEXBUFFERRANGEPROC __rglgen_glTexBufferRange;
extern RGLSYMGLTEXSTORAGE3DMULTISAMPLEPROC __rglgen_glTexStorage3DMultisample;

struct rglgen_sym_map { const char *sym; void *ptr; };
extern const struct rglgen_sym_map rglgen_symbol_map[];
#ifdef __cplusplus
}
#endif
