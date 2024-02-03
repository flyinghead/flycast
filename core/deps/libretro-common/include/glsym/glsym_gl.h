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

#ifndef RGLGEN_DECL_H__
#define RGLGEN_DECL_H__
#ifdef __cplusplus
extern "C" {
#endif
#ifdef GL_APIENTRY
typedef void (GL_APIENTRY *RGLGENGLDEBUGPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
typedef void (GL_APIENTRY *RGLGENGLDEBUGPROCKHR)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
#else
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif
typedef void (APIENTRY *RGLGENGLDEBUGPROCARB)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
typedef void (APIENTRY *RGLGENGLDEBUGPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
#endif
#ifndef GL_OES_EGL_image
typedef void *GLeglImageOES;
#endif
#if !defined(GL_OES_fixed_point) && !defined(HAVE_OPENGLES2)
typedef GLint GLfixed;
#endif
#if defined(__MACH__) && !defined(OS_TARGET_IPHONE) && !defined(MAC_OS_X_VERSION_10_7)
typedef long long int GLint64;
typedef unsigned long long int GLuint64;
typedef unsigned long long int GLuint64EXT;
typedef struct __GLsync *GLsync;
#endif
typedef void (APIENTRYP RGLSYMGLCULLFACEPROC) (GLenum mode);
typedef void (APIENTRYP RGLSYMGLFRONTFACEPROC) (GLenum mode);
typedef void (APIENTRYP RGLSYMGLHINTPROC) (GLenum target, GLenum mode);
typedef void (APIENTRYP RGLSYMGLLINEWIDTHPROC) (GLfloat width);
typedef void (APIENTRYP RGLSYMGLPOINTSIZEPROC) (GLfloat size);
typedef void (APIENTRYP RGLSYMGLPOLYGONMODEPROC) (GLenum face, GLenum mode);
typedef void (APIENTRYP RGLSYMGLSCISSORPROC) (GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLTEXPARAMETERFPROC) (GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRYP RGLSYMGLTEXPARAMETERFVPROC) (GLenum target, GLenum pname, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLTEXPARAMETERIPROC) (GLenum target, GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLTEXPARAMETERIVPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLTEXIMAGE1DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLTEXIMAGE2DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLDRAWBUFFERPROC) (GLenum buf);
typedef void (APIENTRYP RGLSYMGLCLEARPROC) (GLbitfield mask);
typedef void (APIENTRYP RGLSYMGLCLEARCOLORPROC) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRYP RGLSYMGLCLEARSTENCILPROC) (GLint s);
typedef void (APIENTRYP RGLSYMGLCLEARDEPTHPROC) (GLdouble depth);
typedef void (APIENTRYP RGLSYMGLSTENCILMASKPROC) (GLuint mask);
typedef void (APIENTRYP RGLSYMGLCOLORMASKPROC) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
typedef void (APIENTRYP RGLSYMGLDEPTHMASKPROC) (GLboolean flag);
typedef void (APIENTRYP RGLSYMGLDISABLEPROC) (GLenum cap);
typedef void (APIENTRYP RGLSYMGLENABLEPROC) (GLenum cap);
typedef void (APIENTRYP RGLSYMGLFINISHPROC) (void);
typedef void (APIENTRYP RGLSYMGLFLUSHPROC) (void);
typedef void (APIENTRYP RGLSYMGLBLENDFUNCPROC) (GLenum sfactor, GLenum dfactor);
typedef void (APIENTRYP RGLSYMGLLOGICOPPROC) (GLenum opcode);
typedef void (APIENTRYP RGLSYMGLSTENCILFUNCPROC) (GLenum func, GLint ref, GLuint mask);
typedef void (APIENTRYP RGLSYMGLSTENCILOPPROC) (GLenum fail, GLenum zfail, GLenum zpass);
typedef void (APIENTRYP RGLSYMGLDEPTHFUNCPROC) (GLenum func);
typedef void (APIENTRYP RGLSYMGLPIXELSTOREFPROC) (GLenum pname, GLfloat param);
typedef void (APIENTRYP RGLSYMGLPIXELSTOREIPROC) (GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLREADBUFFERPROC) (GLenum src);
typedef void (APIENTRYP RGLSYMGLREADPIXELSPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
typedef void (APIENTRYP RGLSYMGLGETBOOLEANVPROC) (GLenum pname, GLboolean *data);
typedef void (APIENTRYP RGLSYMGLGETDOUBLEVPROC) (GLenum pname, GLdouble *data);
typedef GLenum (APIENTRYP RGLSYMGLGETERRORPROC) (void);
typedef void (APIENTRYP RGLSYMGLGETFLOATVPROC) (GLenum pname, GLfloat *data);
typedef void (APIENTRYP RGLSYMGLGETINTEGERVPROC) (GLenum pname, GLint *data);
typedef const GLubyte *(APIENTRYP RGLSYMGLGETSTRINGPROC) (GLenum name);
typedef void (APIENTRYP RGLSYMGLGETTEXIMAGEPROC) (GLenum target, GLint level, GLenum format, GLenum type, void *pixels);
typedef void (APIENTRYP RGLSYMGLGETTEXPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETTEXPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETTEXLEVELPARAMETERFVPROC) (GLenum target, GLint level, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETTEXLEVELPARAMETERIVPROC) (GLenum target, GLint level, GLenum pname, GLint *params);
typedef GLboolean (APIENTRYP RGLSYMGLISENABLEDPROC) (GLenum cap);
typedef void (APIENTRYP RGLSYMGLDEPTHRANGEPROC) (GLdouble n, GLdouble f);
typedef void (APIENTRYP RGLSYMGLVIEWPORTPROC) (GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLDRAWARRAYSPROC) (GLenum mode, GLint first, GLsizei count);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices);
typedef void (APIENTRYP RGLSYMGLGETPOINTERVPROC) (GLenum pname, void **params);
typedef void (APIENTRYP RGLSYMGLPOLYGONOFFSETPROC) (GLfloat factor, GLfloat units);
typedef void (APIENTRYP RGLSYMGLCOPYTEXIMAGE1DPROC) (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
typedef void (APIENTRYP RGLSYMGLCOPYTEXIMAGE2DPROC) (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (APIENTRYP RGLSYMGLCOPYTEXSUBIMAGE1DPROC) (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
typedef void (APIENTRYP RGLSYMGLCOPYTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLTEXSUBIMAGE1DPROC) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLBINDTEXTUREPROC) (GLenum target, GLuint texture);
typedef void (APIENTRYP RGLSYMGLDELETETEXTURESPROC) (GLsizei n, const GLuint *textures);
typedef void (APIENTRYP RGLSYMGLGENTEXTURESPROC) (GLsizei n, GLuint *textures);
typedef GLboolean (APIENTRYP RGLSYMGLISTEXTUREPROC) (GLuint texture);
typedef void (APIENTRYP RGLSYMGLDRAWRANGEELEMENTSPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices);
typedef void (APIENTRYP RGLSYMGLTEXIMAGE3DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLCOPYTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLACTIVETEXTUREPROC) (GLenum texture);
typedef void (APIENTRYP RGLSYMGLSAMPLECOVERAGEPROC) (GLfloat value, GLboolean invert);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE3DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE2DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE1DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE1DPROC) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data);
typedef void (APIENTRYP RGLSYMGLGETCOMPRESSEDTEXIMAGEPROC) (GLenum target, GLint level, void *img);
typedef void (APIENTRYP RGLSYMGLBLENDFUNCSEPARATEPROC) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWARRAYSPROC) (GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWELEMENTSPROC) (GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount);
typedef void (APIENTRYP RGLSYMGLPOINTPARAMETERFPROC) (GLenum pname, GLfloat param);
typedef void (APIENTRYP RGLSYMGLPOINTPARAMETERFVPROC) (GLenum pname, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLPOINTPARAMETERIPROC) (GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLPOINTPARAMETERIVPROC) (GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLBLENDCOLORPROC) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRYP RGLSYMGLBLENDEQUATIONPROC) (GLenum mode);
typedef void (APIENTRYP RGLSYMGLGENQUERIESPROC) (GLsizei n, GLuint *ids);
typedef void (APIENTRYP RGLSYMGLDELETEQUERIESPROC) (GLsizei n, const GLuint *ids);
typedef GLboolean (APIENTRYP RGLSYMGLISQUERYPROC) (GLuint id);
typedef void (APIENTRYP RGLSYMGLBEGINQUERYPROC) (GLenum target, GLuint id);
typedef void (APIENTRYP RGLSYMGLENDQUERYPROC) (GLenum target);
typedef void (APIENTRYP RGLSYMGLGETQUERYIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETQUERYOBJECTIVPROC) (GLuint id, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETQUERYOBJECTUIVPROC) (GLuint id, GLenum pname, GLuint *params);
typedef void (APIENTRYP RGLSYMGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRYP RGLSYMGLDELETEBUFFERSPROC) (GLsizei n, const GLuint *buffers);
typedef void (APIENTRYP RGLSYMGLGENBUFFERSPROC) (GLsizei n, GLuint *buffers);
typedef GLboolean (APIENTRYP RGLSYMGLISBUFFERPROC) (GLuint buffer);
typedef void (APIENTRYP RGLSYMGLBUFFERDATAPROC) (GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (APIENTRYP RGLSYMGLBUFFERSUBDATAPROC) (GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
typedef void (APIENTRYP RGLSYMGLGETBUFFERSUBDATAPROC) (GLenum target, GLintptr offset, GLsizeiptr size, void *data);
typedef void *(APIENTRYP RGLSYMGLMAPBUFFERPROC) (GLenum target, GLenum access);
typedef GLboolean (APIENTRYP RGLSYMGLUNMAPBUFFERPROC) (GLenum target);
typedef void (APIENTRYP RGLSYMGLGETBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETBUFFERPOINTERVPROC) (GLenum target, GLenum pname, void **params);
typedef void (APIENTRYP RGLSYMGLBLENDEQUATIONSEPARATEPROC) (GLenum modeRGB, GLenum modeAlpha);
typedef void (APIENTRYP RGLSYMGLDRAWBUFFERSPROC) (GLsizei n, const GLenum *bufs);
typedef void (APIENTRYP RGLSYMGLSTENCILOPSEPARATEPROC) (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
typedef void (APIENTRYP RGLSYMGLSTENCILFUNCSEPARATEPROC) (GLenum face, GLenum func, GLint ref, GLuint mask);
typedef void (APIENTRYP RGLSYMGLSTENCILMASKSEPARATEPROC) (GLenum face, GLuint mask);
typedef void (APIENTRYP RGLSYMGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (APIENTRYP RGLSYMGLBINDATTRIBLOCATIONPROC) (GLuint program, GLuint index, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLCOMPILESHADERPROC) (GLuint shader);
typedef GLuint (APIENTRYP RGLSYMGLCREATEPROGRAMPROC) (void);
typedef GLuint (APIENTRYP RGLSYMGLCREATESHADERPROC) (GLenum type);
typedef void (APIENTRYP RGLSYMGLDELETEPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP RGLSYMGLDELETESHADERPROC) (GLuint shader);
typedef void (APIENTRYP RGLSYMGLDETACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (APIENTRYP RGLSYMGLDISABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void (APIENTRYP RGLSYMGLENABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void (APIENTRYP RGLSYMGLGETACTIVEATTRIBPROC) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETACTIVEUNIFORMPROC) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETATTACHEDSHADERSPROC) (GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders);
typedef GLint (APIENTRYP RGLSYMGLGETATTRIBLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP RGLSYMGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP RGLSYMGLGETSHADERSOURCEPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source);
typedef GLint (APIENTRYP RGLSYMGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMFVPROC) (GLuint program, GLint location, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMIVPROC) (GLuint program, GLint location, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBDVPROC) (GLuint index, GLenum pname, GLdouble *params);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBFVPROC) (GLuint index, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBIVPROC) (GLuint index, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBPOINTERVPROC) (GLuint index, GLenum pname, void **pointer);
typedef GLboolean (APIENTRYP RGLSYMGLISPROGRAMPROC) (GLuint program);
typedef GLboolean (APIENTRYP RGLSYMGLISSHADERPROC) (GLuint shader);
typedef void (APIENTRYP RGLSYMGLLINKPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP RGLSYMGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (APIENTRYP RGLSYMGLUSEPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP RGLSYMGLUNIFORM1FPROC) (GLint location, GLfloat v0);
typedef void (APIENTRYP RGLSYMGLUNIFORM2FPROC) (GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRYP RGLSYMGLUNIFORM3FPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRYP RGLSYMGLUNIFORM4FPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRYP RGLSYMGLUNIFORM1IPROC) (GLint location, GLint v0);
typedef void (APIENTRYP RGLSYMGLUNIFORM2IPROC) (GLint location, GLint v0, GLint v1);
typedef void (APIENTRYP RGLSYMGLUNIFORM3IPROC) (GLint location, GLint v0, GLint v1, GLint v2);
typedef void (APIENTRYP RGLSYMGLUNIFORM4IPROC) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (APIENTRYP RGLSYMGLUNIFORM1FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM2FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM3FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM4FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM1IVPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM2IVPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM3IVPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM4IVPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX2FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX3FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLVALIDATEPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1DPROC) (GLuint index, GLdouble x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1FPROC) (GLuint index, GLfloat x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1FVPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1SPROC) (GLuint index, GLshort x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1SVPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2DPROC) (GLuint index, GLdouble x, GLdouble y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2FPROC) (GLuint index, GLfloat x, GLfloat y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2FVPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2SPROC) (GLuint index, GLshort x, GLshort y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2SVPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3DPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3FPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3FVPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3SPROC) (GLuint index, GLshort x, GLshort y, GLshort z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3SVPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NBVPROC) (GLuint index, const GLbyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NIVPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NSVPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NUBPROC) (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NUBVPROC) (GLuint index, const GLubyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NUIVPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NUSVPROC) (GLuint index, const GLushort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4BVPROC) (GLuint index, const GLbyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4DPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4FPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4FVPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4IVPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4SPROC) (GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4SVPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4UBVPROC) (GLuint index, const GLubyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4UIVPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4USVPROC) (GLuint index, const GLushort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBPOINTERPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX2X3FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX3X2FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX2X4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX4X2FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX3X4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX4X3FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLCOLORMASKIPROC) (GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a);
typedef void (APIENTRYP RGLSYMGLGETBOOLEANI_VPROC) (GLenum target, GLuint index, GLboolean *data);
typedef void (APIENTRYP RGLSYMGLGETINTEGERI_VPROC) (GLenum target, GLuint index, GLint *data);
typedef void (APIENTRYP RGLSYMGLENABLEIPROC) (GLenum target, GLuint index);
typedef void (APIENTRYP RGLSYMGLDISABLEIPROC) (GLenum target, GLuint index);
typedef GLboolean (APIENTRYP RGLSYMGLISENABLEDIPROC) (GLenum target, GLuint index);
typedef void (APIENTRYP RGLSYMGLBEGINTRANSFORMFEEDBACKPROC) (GLenum primitiveMode);
typedef void (APIENTRYP RGLSYMGLENDTRANSFORMFEEDBACKPROC) (void);
typedef void (APIENTRYP RGLSYMGLBINDBUFFERRANGEPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (APIENTRYP RGLSYMGLBINDBUFFERBASEPROC) (GLenum target, GLuint index, GLuint buffer);
typedef void (APIENTRYP RGLSYMGLTRANSFORMFEEDBACKVARYINGSPROC) (GLuint program, GLsizei count, const GLchar *const*varyings, GLenum bufferMode);
typedef void (APIENTRYP RGLSYMGLGETTRANSFORMFEEDBACKVARYINGPROC) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name);
typedef void (APIENTRYP RGLSYMGLCLAMPCOLORPROC) (GLenum target, GLenum clamp);
typedef void (APIENTRYP RGLSYMGLBEGINCONDITIONALRENDERPROC) (GLuint id, GLenum mode);
typedef void (APIENTRYP RGLSYMGLENDCONDITIONALRENDERPROC) (void);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBIPOINTERPROC) (GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBIIVPROC) (GLuint index, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBIUIVPROC) (GLuint index, GLenum pname, GLuint *params);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI1IPROC) (GLuint index, GLint x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI2IPROC) (GLuint index, GLint x, GLint y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI3IPROC) (GLuint index, GLint x, GLint y, GLint z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4IPROC) (GLuint index, GLint x, GLint y, GLint z, GLint w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI1UIPROC) (GLuint index, GLuint x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI2UIPROC) (GLuint index, GLuint x, GLuint y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI3UIPROC) (GLuint index, GLuint x, GLuint y, GLuint z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4UIPROC) (GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI1IVPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI2IVPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI3IVPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4IVPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI1UIVPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI2UIVPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI3UIVPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4UIVPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4BVPROC) (GLuint index, const GLbyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4SVPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4UBVPROC) (GLuint index, const GLubyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4USVPROC) (GLuint index, const GLushort *v);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMUIVPROC) (GLuint program, GLint location, GLuint *params);
typedef void (APIENTRYP RGLSYMGLBINDFRAGDATALOCATIONPROC) (GLuint program, GLuint color, const GLchar *name);
typedef GLint (APIENTRYP RGLSYMGLGETFRAGDATALOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLUNIFORM1UIPROC) (GLint location, GLuint v0);
typedef void (APIENTRYP RGLSYMGLUNIFORM2UIPROC) (GLint location, GLuint v0, GLuint v1);
typedef void (APIENTRYP RGLSYMGLUNIFORM3UIPROC) (GLint location, GLuint v0, GLuint v1, GLuint v2);
typedef void (APIENTRYP RGLSYMGLUNIFORM4UIPROC) (GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
typedef void (APIENTRYP RGLSYMGLUNIFORM1UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM2UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM3UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM4UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLTEXPARAMETERIIVPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLTEXPARAMETERIUIVPROC) (GLenum target, GLenum pname, const GLuint *params);
typedef void (APIENTRYP RGLSYMGLGETTEXPARAMETERIIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETTEXPARAMETERIUIVPROC) (GLenum target, GLenum pname, GLuint *params);
typedef void (APIENTRYP RGLSYMGLCLEARBUFFERIVPROC) (GLenum buffer, GLint drawbuffer, const GLint *value);
typedef void (APIENTRYP RGLSYMGLCLEARBUFFERUIVPROC) (GLenum buffer, GLint drawbuffer, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLCLEARBUFFERFVPROC) (GLenum buffer, GLint drawbuffer, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLCLEARBUFFERFIPROC) (GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
typedef const GLubyte *(APIENTRYP RGLSYMGLGETSTRINGIPROC) (GLenum name, GLuint index);
typedef GLboolean (APIENTRYP RGLSYMGLISRENDERBUFFERPROC) (GLuint renderbuffer);
typedef void (APIENTRYP RGLSYMGLBINDRENDERBUFFERPROC) (GLenum target, GLuint renderbuffer);
typedef void (APIENTRYP RGLSYMGLDELETERENDERBUFFERSPROC) (GLsizei n, const GLuint *renderbuffers);
typedef void (APIENTRYP RGLSYMGLGENRENDERBUFFERSPROC) (GLsizei n, GLuint *renderbuffers);
typedef void (APIENTRYP RGLSYMGLRENDERBUFFERSTORAGEPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLGETRENDERBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef GLboolean (APIENTRYP RGLSYMGLISFRAMEBUFFERPROC) (GLuint framebuffer);
typedef void (APIENTRYP RGLSYMGLBINDFRAMEBUFFERPROC) (GLenum target, GLuint framebuffer);
typedef void (APIENTRYP RGLSYMGLDELETEFRAMEBUFFERSPROC) (GLsizei n, const GLuint *framebuffers);
typedef void (APIENTRYP RGLSYMGLGENFRAMEBUFFERSPROC) (GLsizei n, GLuint *framebuffers);
typedef GLenum (APIENTRYP RGLSYMGLCHECKFRAMEBUFFERSTATUSPROC) (GLenum target);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTURE1DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTURE2DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTURE3DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERRENDERBUFFERPROC) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (APIENTRYP RGLSYMGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) (GLenum target, GLenum attachment, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGENERATEMIPMAPPROC) (GLenum target);
typedef void (APIENTRYP RGLSYMGLBLITFRAMEBUFFERPROC) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void (APIENTRYP RGLSYMGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTURELAYERPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
typedef void *(APIENTRYP RGLSYMGLMAPBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void (APIENTRYP RGLSYMGLFLUSHMAPPEDBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length);
typedef void (APIENTRYP RGLSYMGLBINDVERTEXARRAYPROC) (GLuint array);
typedef void (APIENTRYP RGLSYMGLDELETEVERTEXARRAYSPROC) (GLsizei n, const GLuint *arrays);
typedef void (APIENTRYP RGLSYMGLGENVERTEXARRAYSPROC) (GLsizei n, GLuint *arrays);
typedef GLboolean (APIENTRYP RGLSYMGLISVERTEXARRAYPROC) (GLuint array);
typedef void (APIENTRYP RGLSYMGLDRAWARRAYSINSTANCEDPROC) (GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSINSTANCEDPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount);
typedef void (APIENTRYP RGLSYMGLTEXBUFFERPROC) (GLenum target, GLenum internalformat, GLuint buffer);
typedef void (APIENTRYP RGLSYMGLPRIMITIVERESTARTINDEXPROC) (GLuint index);
typedef void (APIENTRYP RGLSYMGLCOPYBUFFERSUBDATAPROC) (GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMINDICESPROC) (GLuint program, GLsizei uniformCount, const GLchar *const*uniformNames, GLuint *uniformIndices);
typedef void (APIENTRYP RGLSYMGLGETACTIVEUNIFORMSIVPROC) (GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETACTIVEUNIFORMNAMEPROC) (GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformName);
typedef GLuint (APIENTRYP RGLSYMGLGETUNIFORMBLOCKINDEXPROC) (GLuint program, const GLchar *uniformBlockName);
typedef void (APIENTRYP RGLSYMGLGETACTIVEUNIFORMBLOCKIVPROC) (GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETACTIVEUNIFORMBLOCKNAMEPROC) (GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName);
typedef void (APIENTRYP RGLSYMGLUNIFORMBLOCKBINDINGPROC) (GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSBASEVERTEXPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex);
typedef void (APIENTRYP RGLSYMGLDRAWRANGEELEMENTSBASEVERTEXPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWELEMENTSBASEVERTEXPROC) (GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount, const GLint *basevertex);
typedef void (APIENTRYP RGLSYMGLPROVOKINGVERTEXPROC) (GLenum mode);
typedef GLsync (APIENTRYP RGLSYMGLFENCESYNCPROC) (GLenum condition, GLbitfield flags);
typedef GLboolean (APIENTRYP RGLSYMGLISSYNCPROC) (GLsync sync);
typedef void (APIENTRYP RGLSYMGLDELETESYNCPROC) (GLsync sync);
typedef GLenum (APIENTRYP RGLSYMGLCLIENTWAITSYNCPROC) (GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef void (APIENTRYP RGLSYMGLWAITSYNCPROC) (GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef void (APIENTRYP RGLSYMGLGETINTEGER64VPROC) (GLenum pname, GLint64 *data);
typedef void (APIENTRYP RGLSYMGLGETSYNCIVPROC) (GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values);
typedef void (APIENTRYP RGLSYMGLGETINTEGER64I_VPROC) (GLenum target, GLuint index, GLint64 *data);
typedef void (APIENTRYP RGLSYMGLGETBUFFERPARAMETERI64VPROC) (GLenum target, GLenum pname, GLint64 *params);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTUREPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level);
typedef void (APIENTRYP RGLSYMGLTEXIMAGE2DMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void (APIENTRYP RGLSYMGLTEXIMAGE3DMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
typedef void (APIENTRYP RGLSYMGLGETMULTISAMPLEFVPROC) (GLenum pname, GLuint index, GLfloat *val);
typedef void (APIENTRYP RGLSYMGLSAMPLEMASKIPROC) (GLuint maskNumber, GLbitfield mask);
typedef void (APIENTRYP RGLSYMGLBINDFRAGDATALOCATIONINDEXEDPROC) (GLuint program, GLuint colorNumber, GLuint index, const GLchar *name);
typedef GLint (APIENTRYP RGLSYMGLGETFRAGDATAINDEXPROC) (GLuint program, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLGENSAMPLERSPROC) (GLsizei count, GLuint *samplers);
typedef void (APIENTRYP RGLSYMGLDELETESAMPLERSPROC) (GLsizei count, const GLuint *samplers);
typedef GLboolean (APIENTRYP RGLSYMGLISSAMPLERPROC) (GLuint sampler);
typedef void (APIENTRYP RGLSYMGLBINDSAMPLERPROC) (GLuint unit, GLuint sampler);
typedef void (APIENTRYP RGLSYMGLSAMPLERPARAMETERIPROC) (GLuint sampler, GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLSAMPLERPARAMETERIVPROC) (GLuint sampler, GLenum pname, const GLint *param);
typedef void (APIENTRYP RGLSYMGLSAMPLERPARAMETERFPROC) (GLuint sampler, GLenum pname, GLfloat param);
typedef void (APIENTRYP RGLSYMGLSAMPLERPARAMETERFVPROC) (GLuint sampler, GLenum pname, const GLfloat *param);
typedef void (APIENTRYP RGLSYMGLSAMPLERPARAMETERIIVPROC) (GLuint sampler, GLenum pname, const GLint *param);
typedef void (APIENTRYP RGLSYMGLSAMPLERPARAMETERIUIVPROC) (GLuint sampler, GLenum pname, const GLuint *param);
typedef void (APIENTRYP RGLSYMGLGETSAMPLERPARAMETERIVPROC) (GLuint sampler, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETSAMPLERPARAMETERIIVPROC) (GLuint sampler, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETSAMPLERPARAMETERFVPROC) (GLuint sampler, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETSAMPLERPARAMETERIUIVPROC) (GLuint sampler, GLenum pname, GLuint *params);
typedef void (APIENTRYP RGLSYMGLQUERYCOUNTERPROC) (GLuint id, GLenum target);
typedef void (APIENTRYP RGLSYMGLGETQUERYOBJECTI64VPROC) (GLuint id, GLenum pname, GLint64 *params);
typedef void (APIENTRYP RGLSYMGLGETQUERYOBJECTUI64VPROC) (GLuint id, GLenum pname, GLuint64 *params);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBDIVISORPROC) (GLuint index, GLuint divisor);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP1UIPROC) (GLuint index, GLenum type, GLboolean normalized, GLuint value);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP1UIVPROC) (GLuint index, GLenum type, GLboolean normalized, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP2UIPROC) (GLuint index, GLenum type, GLboolean normalized, GLuint value);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP2UIVPROC) (GLuint index, GLenum type, GLboolean normalized, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP3UIPROC) (GLuint index, GLenum type, GLboolean normalized, GLuint value);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP3UIVPROC) (GLuint index, GLenum type, GLboolean normalized, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP4UIPROC) (GLuint index, GLenum type, GLboolean normalized, GLuint value);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP4UIVPROC) (GLuint index, GLenum type, GLboolean normalized, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLMINSAMPLESHADINGPROC) (GLfloat value);
typedef void (APIENTRYP RGLSYMGLBLENDEQUATIONIPROC) (GLuint buf, GLenum mode);
typedef void (APIENTRYP RGLSYMGLBLENDEQUATIONSEPARATEIPROC) (GLuint buf, GLenum modeRGB, GLenum modeAlpha);
typedef void (APIENTRYP RGLSYMGLBLENDFUNCIPROC) (GLuint buf, GLenum src, GLenum dst);
typedef void (APIENTRYP RGLSYMGLBLENDFUNCSEPARATEIPROC) (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
typedef void (APIENTRYP RGLSYMGLDRAWARRAYSINDIRECTPROC) (GLenum mode, const void *indirect);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSINDIRECTPROC) (GLenum mode, GLenum type, const void *indirect);
typedef void (APIENTRYP RGLSYMGLUNIFORM1DPROC) (GLint location, GLdouble x);
typedef void (APIENTRYP RGLSYMGLUNIFORM2DPROC) (GLint location, GLdouble x, GLdouble y);
typedef void (APIENTRYP RGLSYMGLUNIFORM3DPROC) (GLint location, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP RGLSYMGLUNIFORM4DPROC) (GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRYP RGLSYMGLUNIFORM1DVPROC) (GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM2DVPROC) (GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM3DVPROC) (GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM4DVPROC) (GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX2DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX3DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX4DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX2X3DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX2X4DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX3X2DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX3X4DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX4X2DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX4X3DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMDVPROC) (GLuint program, GLint location, GLdouble *params);
typedef GLint (APIENTRYP RGLSYMGLGETSUBROUTINEUNIFORMLOCATIONPROC) (GLuint program, GLenum shadertype, const GLchar *name);
typedef GLuint (APIENTRYP RGLSYMGLGETSUBROUTINEINDEXPROC) (GLuint program, GLenum shadertype, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETACTIVESUBROUTINEUNIFORMIVPROC) (GLuint program, GLenum shadertype, GLuint index, GLenum pname, GLint *values);
typedef void (APIENTRYP RGLSYMGLGETACTIVESUBROUTINEUNIFORMNAMEPROC) (GLuint program, GLenum shadertype, GLuint index, GLsizei bufsize, GLsizei *length, GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETACTIVESUBROUTINENAMEPROC) (GLuint program, GLenum shadertype, GLuint index, GLsizei bufsize, GLsizei *length, GLchar *name);
typedef void (APIENTRYP RGLSYMGLUNIFORMSUBROUTINESUIVPROC) (GLenum shadertype, GLsizei count, const GLuint *indices);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMSUBROUTINEUIVPROC) (GLenum shadertype, GLint location, GLuint *params);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMSTAGEIVPROC) (GLuint program, GLenum shadertype, GLenum pname, GLint *values);
typedef void (APIENTRYP RGLSYMGLPATCHPARAMETERIPROC) (GLenum pname, GLint value);
typedef void (APIENTRYP RGLSYMGLPATCHPARAMETERFVPROC) (GLenum pname, const GLfloat *values);
typedef void (APIENTRYP RGLSYMGLBINDTRANSFORMFEEDBACKPROC) (GLenum target, GLuint id);
typedef void (APIENTRYP RGLSYMGLDELETETRANSFORMFEEDBACKSPROC) (GLsizei n, const GLuint *ids);
typedef void (APIENTRYP RGLSYMGLGENTRANSFORMFEEDBACKSPROC) (GLsizei n, GLuint *ids);
typedef GLboolean (APIENTRYP RGLSYMGLISTRANSFORMFEEDBACKPROC) (GLuint id);
typedef void (APIENTRYP RGLSYMGLPAUSETRANSFORMFEEDBACKPROC) (void);
typedef void (APIENTRYP RGLSYMGLRESUMETRANSFORMFEEDBACKPROC) (void);
typedef void (APIENTRYP RGLSYMGLDRAWTRANSFORMFEEDBACKPROC) (GLenum mode, GLuint id);
typedef void (APIENTRYP RGLSYMGLDRAWTRANSFORMFEEDBACKSTREAMPROC) (GLenum mode, GLuint id, GLuint stream);
typedef void (APIENTRYP RGLSYMGLBEGINQUERYINDEXEDPROC) (GLenum target, GLuint index, GLuint id);
typedef void (APIENTRYP RGLSYMGLENDQUERYINDEXEDPROC) (GLenum target, GLuint index);
typedef void (APIENTRYP RGLSYMGLGETQUERYINDEXEDIVPROC) (GLenum target, GLuint index, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLRELEASESHADERCOMPILERPROC) (void);
typedef void (APIENTRYP RGLSYMGLSHADERBINARYPROC) (GLsizei count, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length);
typedef void (APIENTRYP RGLSYMGLGETSHADERPRECISIONFORMATPROC) (GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision);
typedef void (APIENTRYP RGLSYMGLDEPTHRANGEFPROC) (GLfloat n, GLfloat f);
typedef void (APIENTRYP RGLSYMGLCLEARDEPTHFPROC) (GLfloat d);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMBINARYPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary);
typedef void (APIENTRYP RGLSYMGLPROGRAMBINARYPROC) (GLuint program, GLenum binaryFormat, const void *binary, GLsizei length);
typedef void (APIENTRYP RGLSYMGLPROGRAMPARAMETERIPROC) (GLuint program, GLenum pname, GLint value);
typedef void (APIENTRYP RGLSYMGLUSEPROGRAMSTAGESPROC) (GLuint pipeline, GLbitfield stages, GLuint program);
typedef void (APIENTRYP RGLSYMGLACTIVESHADERPROGRAMPROC) (GLuint pipeline, GLuint program);
typedef GLuint (APIENTRYP RGLSYMGLCREATESHADERPROGRAMVPROC) (GLenum type, GLsizei count, const GLchar *const*strings);
typedef void (APIENTRYP RGLSYMGLBINDPROGRAMPIPELINEPROC) (GLuint pipeline);
typedef void (APIENTRYP RGLSYMGLDELETEPROGRAMPIPELINESPROC) (GLsizei n, const GLuint *pipelines);
typedef void (APIENTRYP RGLSYMGLGENPROGRAMPIPELINESPROC) (GLsizei n, GLuint *pipelines);
typedef GLboolean (APIENTRYP RGLSYMGLISPROGRAMPIPELINEPROC) (GLuint pipeline);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMPIPELINEIVPROC) (GLuint pipeline, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1IPROC) (GLuint program, GLint location, GLint v0);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1IVPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1FPROC) (GLuint program, GLint location, GLfloat v0);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1DPROC) (GLuint program, GLint location, GLdouble v0);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1DVPROC) (GLuint program, GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1UIPROC) (GLuint program, GLint location, GLuint v0);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2IPROC) (GLuint program, GLint location, GLint v0, GLint v1);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2IVPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2FPROC) (GLuint program, GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2DPROC) (GLuint program, GLint location, GLdouble v0, GLdouble v1);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2DVPROC) (GLuint program, GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2UIPROC) (GLuint program, GLint location, GLuint v0, GLuint v1);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3IPROC) (GLuint program, GLint location, GLint v0, GLint v1, GLint v2);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3IVPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3FPROC) (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3DPROC) (GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3DVPROC) (GLuint program, GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3UIPROC) (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4IPROC) (GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4IVPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4FPROC) (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4DPROC) (GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4DVPROC) (GLuint program, GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4UIPROC) (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2X3FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3X2FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2X4FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4X2FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3X4FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4X3FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2X3DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3X2DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2X4DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4X2DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3X4DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4X3DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLVALIDATEPROGRAMPIPELINEPROC) (GLuint pipeline);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMPIPELINEINFOLOGPROC) (GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL1DPROC) (GLuint index, GLdouble x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL2DPROC) (GLuint index, GLdouble x, GLdouble y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL3DPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL4DPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL1DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL2DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL3DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL4DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBLPOINTERPROC) (GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBLDVPROC) (GLuint index, GLenum pname, GLdouble *params);
typedef void (APIENTRYP RGLSYMGLVIEWPORTARRAYVPROC) (GLuint first, GLsizei count, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLVIEWPORTINDEXEDFPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h);
typedef void (APIENTRYP RGLSYMGLVIEWPORTINDEXEDFVPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLSCISSORARRAYVPROC) (GLuint first, GLsizei count, const GLint *v);
typedef void (APIENTRYP RGLSYMGLSCISSORINDEXEDPROC) (GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLSCISSORINDEXEDVPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLDEPTHRANGEARRAYVPROC) (GLuint first, GLsizei count, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLDEPTHRANGEINDEXEDPROC) (GLuint index, GLdouble n, GLdouble f);
typedef void (APIENTRYP RGLSYMGLGETFLOATI_VPROC) (GLenum target, GLuint index, GLfloat *data);
typedef void (APIENTRYP RGLSYMGLGETDOUBLEI_VPROC) (GLenum target, GLuint index, GLdouble *data);
typedef void (APIENTRYP RGLSYMGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC) (GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance);
typedef void (APIENTRYP RGLSYMGLGETINTERNALFORMATIVPROC) (GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETACTIVEATOMICCOUNTERBUFFERIVPROC) (GLuint program, GLuint bufferIndex, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLBINDIMAGETEXTUREPROC) (GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);
typedef void (APIENTRYP RGLSYMGLMEMORYBARRIERPROC) (GLbitfield barriers);
typedef void (APIENTRYP RGLSYMGLTEXSTORAGE1DPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
typedef void (APIENTRYP RGLSYMGLTEXSTORAGE2DPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLTEXSTORAGE3DPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
typedef void (APIENTRYP RGLSYMGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC) (GLenum mode, GLuint id, GLsizei instancecount);
typedef void (APIENTRYP RGLSYMGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC) (GLenum mode, GLuint id, GLuint stream, GLsizei instancecount);
typedef void (APIENTRYP RGLSYMGLCLEARBUFFERDATAPROC) (GLenum target, GLenum internalformat, GLenum format, GLenum type, const void *data);
typedef void (APIENTRYP RGLSYMGLCLEARBUFFERSUBDATAPROC) (GLenum target, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data);
typedef void (APIENTRYP RGLSYMGLDISPATCHCOMPUTEPROC) (GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
typedef void (APIENTRYP RGLSYMGLDISPATCHCOMPUTEINDIRECTPROC) (GLintptr indirect);
typedef void (APIENTRYP RGLSYMGLCOPYIMAGESUBDATAPROC) (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERPARAMETERIPROC) (GLenum target, GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLGETFRAMEBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETINTERNALFORMATI64VPROC) (GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint64 *params);
typedef void (APIENTRYP RGLSYMGLINVALIDATETEXSUBIMAGEPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth);
typedef void (APIENTRYP RGLSYMGLINVALIDATETEXIMAGEPROC) (GLuint texture, GLint level);
typedef void (APIENTRYP RGLSYMGLINVALIDATEBUFFERSUBDATAPROC) (GLuint buffer, GLintptr offset, GLsizeiptr length);
typedef void (APIENTRYP RGLSYMGLINVALIDATEBUFFERDATAPROC) (GLuint buffer);
typedef void (APIENTRYP RGLSYMGLINVALIDATEFRAMEBUFFERPROC) (GLenum target, GLsizei numAttachments, const GLenum *attachments);
typedef void (APIENTRYP RGLSYMGLINVALIDATESUBFRAMEBUFFERPROC) (GLenum target, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWARRAYSINDIRECTPROC) (GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWELEMENTSINDIRECTPROC) (GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMINTERFACEIVPROC) (GLuint program, GLenum programInterface, GLenum pname, GLint *params);
typedef GLuint (APIENTRYP RGLSYMGLGETPROGRAMRESOURCEINDEXPROC) (GLuint program, GLenum programInterface, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMRESOURCENAMEPROC) (GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMRESOURCEIVPROC) (GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei bufSize, GLsizei *length, GLint *params);
typedef GLint (APIENTRYP RGLSYMGLGETPROGRAMRESOURCELOCATIONPROC) (GLuint program, GLenum programInterface, const GLchar *name);
typedef GLint (APIENTRYP RGLSYMGLGETPROGRAMRESOURCELOCATIONINDEXPROC) (GLuint program, GLenum programInterface, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLSHADERSTORAGEBLOCKBINDINGPROC) (GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding);
typedef void (APIENTRYP RGLSYMGLTEXBUFFERRANGEPROC) (GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (APIENTRYP RGLSYMGLTEXSTORAGE2DMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void (APIENTRYP RGLSYMGLTEXSTORAGE3DMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
typedef void (APIENTRYP RGLSYMGLTEXTUREVIEWPROC) (GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers);
typedef void (APIENTRYP RGLSYMGLBINDVERTEXBUFFERPROC) (GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBFORMATPROC) (GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBIFORMATPROC) (GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBLFORMATPROC) (GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBBINDINGPROC) (GLuint attribindex, GLuint bindingindex);
typedef void (APIENTRYP RGLSYMGLVERTEXBINDINGDIVISORPROC) (GLuint bindingindex, GLuint divisor);
typedef void (APIENTRYP RGLSYMGLDEBUGMESSAGECONTROLPROC) (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled);
typedef void (APIENTRYP RGLSYMGLDEBUGMESSAGEINSERTPROC) (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf);
typedef void (APIENTRYP RGLSYMGLDEBUGMESSAGECALLBACKPROC) (RGLGENGLDEBUGPROC callback, const void *userParam);
typedef GLuint (APIENTRYP RGLSYMGLGETDEBUGMESSAGELOGPROC) (GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog);
typedef void (APIENTRYP RGLSYMGLPUSHDEBUGGROUPPROC) (GLenum source, GLuint id, GLsizei length, const GLchar *message);
typedef void (APIENTRYP RGLSYMGLPOPDEBUGGROUPPROC) (void);
typedef void (APIENTRYP RGLSYMGLOBJECTLABELPROC) (GLenum identifier, GLuint name, GLsizei length, const GLchar *label);
typedef void (APIENTRYP RGLSYMGLGETOBJECTLABELPROC) (GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (APIENTRYP RGLSYMGLOBJECTPTRLABELPROC) (const void *ptr, GLsizei length, const GLchar *label);
typedef void (APIENTRYP RGLSYMGLGETOBJECTPTRLABELPROC) (const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (APIENTRYP RGLSYMGLBUFFERSTORAGEPROC) (GLenum target, GLsizeiptr size, const void *data, GLbitfield flags);
typedef void (APIENTRYP RGLSYMGLCLEARTEXIMAGEPROC) (GLuint texture, GLint level, GLenum format, GLenum type, const void *data);
typedef void (APIENTRYP RGLSYMGLCLEARTEXSUBIMAGEPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *data);
typedef void (APIENTRYP RGLSYMGLBINDBUFFERSBASEPROC) (GLenum target, GLuint first, GLsizei count, const GLuint *buffers);
typedef void (APIENTRYP RGLSYMGLBINDBUFFERSRANGEPROC) (GLenum target, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizeiptr *sizes);
typedef void (APIENTRYP RGLSYMGLBINDTEXTURESPROC) (GLuint first, GLsizei count, const GLuint *textures);
typedef void (APIENTRYP RGLSYMGLBINDSAMPLERSPROC) (GLuint first, GLsizei count, const GLuint *samplers);
typedef void (APIENTRYP RGLSYMGLBINDIMAGETEXTURESPROC) (GLuint first, GLsizei count, const GLuint *textures);
typedef void (APIENTRYP RGLSYMGLBINDVERTEXBUFFERSPROC) (GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides);
typedef void (APIENTRYP RGLSYMGLCLIPCONTROLPROC) (GLenum origin, GLenum depth);
typedef void (APIENTRYP RGLSYMGLCREATETRANSFORMFEEDBACKSPROC) (GLsizei n, GLuint *ids);
typedef void (APIENTRYP RGLSYMGLTRANSFORMFEEDBACKBUFFERBASEPROC) (GLuint xfb, GLuint index, GLuint buffer);
typedef void (APIENTRYP RGLSYMGLTRANSFORMFEEDBACKBUFFERRANGEPROC) (GLuint xfb, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (APIENTRYP RGLSYMGLGETTRANSFORMFEEDBACKIVPROC) (GLuint xfb, GLenum pname, GLint *param);
typedef void (APIENTRYP RGLSYMGLGETTRANSFORMFEEDBACKI_VPROC) (GLuint xfb, GLenum pname, GLuint index, GLint *param);
typedef void (APIENTRYP RGLSYMGLGETTRANSFORMFEEDBACKI64_VPROC) (GLuint xfb, GLenum pname, GLuint index, GLint64 *param);
typedef void (APIENTRYP RGLSYMGLCREATEBUFFERSPROC) (GLsizei n, GLuint *buffers);
typedef void (APIENTRYP RGLSYMGLNAMEDBUFFERSTORAGEPROC) (GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags);
typedef void (APIENTRYP RGLSYMGLNAMEDBUFFERDATAPROC) (GLuint buffer, GLsizeiptr size, const void *data, GLenum usage);
typedef void (APIENTRYP RGLSYMGLNAMEDBUFFERSUBDATAPROC) (GLuint buffer, GLintptr offset, GLsizeiptr size, const void *data);
typedef void (APIENTRYP RGLSYMGLCOPYNAMEDBUFFERSUBDATAPROC) (GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
typedef void (APIENTRYP RGLSYMGLCLEARNAMEDBUFFERDATAPROC) (GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void *data);
typedef void (APIENTRYP RGLSYMGLCLEARNAMEDBUFFERSUBDATAPROC) (GLuint buffer, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data);
typedef void *(APIENTRYP RGLSYMGLMAPNAMEDBUFFERPROC) (GLuint buffer, GLenum access);
typedef void *(APIENTRYP RGLSYMGLMAPNAMEDBUFFERRANGEPROC) (GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef GLboolean (APIENTRYP RGLSYMGLUNMAPNAMEDBUFFERPROC) (GLuint buffer);
typedef void (APIENTRYP RGLSYMGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC) (GLuint buffer, GLintptr offset, GLsizeiptr length);
typedef void (APIENTRYP RGLSYMGLGETNAMEDBUFFERPARAMETERIVPROC) (GLuint buffer, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETNAMEDBUFFERPARAMETERI64VPROC) (GLuint buffer, GLenum pname, GLint64 *params);
typedef void (APIENTRYP RGLSYMGLGETNAMEDBUFFERPOINTERVPROC) (GLuint buffer, GLenum pname, void **params);
typedef void (APIENTRYP RGLSYMGLGETNAMEDBUFFERSUBDATAPROC) (GLuint buffer, GLintptr offset, GLsizeiptr size, void *data);
typedef void (APIENTRYP RGLSYMGLCREATEFRAMEBUFFERSPROC) (GLsizei n, GLuint *framebuffers);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERRENDERBUFFERPROC) (GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERPARAMETERIPROC) (GLuint framebuffer, GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERTEXTUREPROC) (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERTEXTURELAYERPROC) (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERDRAWBUFFERPROC) (GLuint framebuffer, GLenum buf);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC) (GLuint framebuffer, GLsizei n, const GLenum *bufs);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERREADBUFFERPROC) (GLuint framebuffer, GLenum src);
typedef void (APIENTRYP RGLSYMGLINVALIDATENAMEDFRAMEBUFFERDATAPROC) (GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments);
typedef void (APIENTRYP RGLSYMGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC) (GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLCLEARNAMEDFRAMEBUFFERIVPROC) (GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value);
typedef void (APIENTRYP RGLSYMGLCLEARNAMEDFRAMEBUFFERUIVPROC) (GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLCLEARNAMEDFRAMEBUFFERFVPROC) (GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLCLEARNAMEDFRAMEBUFFERFIPROC) (GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
typedef void (APIENTRYP RGLSYMGLBLITNAMEDFRAMEBUFFERPROC) (GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef GLenum (APIENTRYP RGLSYMGLCHECKNAMEDFRAMEBUFFERSTATUSPROC) (GLuint framebuffer, GLenum target);
typedef void (APIENTRYP RGLSYMGLGETNAMEDFRAMEBUFFERPARAMETERIVPROC) (GLuint framebuffer, GLenum pname, GLint *param);
typedef void (APIENTRYP RGLSYMGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC) (GLuint framebuffer, GLenum attachment, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLCREATERENDERBUFFERSPROC) (GLsizei n, GLuint *renderbuffers);
typedef void (APIENTRYP RGLSYMGLNAMEDRENDERBUFFERSTORAGEPROC) (GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC) (GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLGETNAMEDRENDERBUFFERPARAMETERIVPROC) (GLuint renderbuffer, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLCREATETEXTURESPROC) (GLenum target, GLsizei n, GLuint *textures);
typedef void (APIENTRYP RGLSYMGLTEXTUREBUFFERPROC) (GLuint texture, GLenum internalformat, GLuint buffer);
typedef void (APIENTRYP RGLSYMGLTEXTUREBUFFERRANGEPROC) (GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (APIENTRYP RGLSYMGLTEXTURESTORAGE1DPROC) (GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width);
typedef void (APIENTRYP RGLSYMGLTEXTURESTORAGE2DPROC) (GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLTEXTURESTORAGE3DPROC) (GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
typedef void (APIENTRYP RGLSYMGLTEXTURESTORAGE2DMULTISAMPLEPROC) (GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void (APIENTRYP RGLSYMGLTEXTURESTORAGE3DMULTISAMPLEPROC) (GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
typedef void (APIENTRYP RGLSYMGLTEXTURESUBIMAGE1DPROC) (GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLTEXTURESUBIMAGE2DPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLTEXTURESUBIMAGE3DPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE1DPROC) (GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE2DPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE3DPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data);
typedef void (APIENTRYP RGLSYMGLCOPYTEXTURESUBIMAGE1DPROC) (GLuint texture, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
typedef void (APIENTRYP RGLSYMGLCOPYTEXTURESUBIMAGE2DPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLCOPYTEXTURESUBIMAGE3DPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLTEXTUREPARAMETERFPROC) (GLuint texture, GLenum pname, GLfloat param);
typedef void (APIENTRYP RGLSYMGLTEXTUREPARAMETERFVPROC) (GLuint texture, GLenum pname, const GLfloat *param);
typedef void (APIENTRYP RGLSYMGLTEXTUREPARAMETERIPROC) (GLuint texture, GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLTEXTUREPARAMETERIIVPROC) (GLuint texture, GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLTEXTUREPARAMETERIUIVPROC) (GLuint texture, GLenum pname, const GLuint *params);
typedef void (APIENTRYP RGLSYMGLTEXTUREPARAMETERIVPROC) (GLuint texture, GLenum pname, const GLint *param);
typedef void (APIENTRYP RGLSYMGLGENERATETEXTUREMIPMAPPROC) (GLuint texture);
typedef void (APIENTRYP RGLSYMGLBINDTEXTUREUNITPROC) (GLuint unit, GLuint texture);
typedef void (APIENTRYP RGLSYMGLGETTEXTUREIMAGEPROC) (GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels);
typedef void (APIENTRYP RGLSYMGLGETCOMPRESSEDTEXTUREIMAGEPROC) (GLuint texture, GLint level, GLsizei bufSize, void *pixels);
typedef void (APIENTRYP RGLSYMGLGETTEXTURELEVELPARAMETERFVPROC) (GLuint texture, GLint level, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETTEXTURELEVELPARAMETERIVPROC) (GLuint texture, GLint level, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETTEXTUREPARAMETERFVPROC) (GLuint texture, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETTEXTUREPARAMETERIIVPROC) (GLuint texture, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETTEXTUREPARAMETERIUIVPROC) (GLuint texture, GLenum pname, GLuint *params);
typedef void (APIENTRYP RGLSYMGLGETTEXTUREPARAMETERIVPROC) (GLuint texture, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLCREATEVERTEXARRAYSPROC) (GLsizei n, GLuint *arrays);
typedef void (APIENTRYP RGLSYMGLDISABLEVERTEXARRAYATTRIBPROC) (GLuint vaobj, GLuint index);
typedef void (APIENTRYP RGLSYMGLENABLEVERTEXARRAYATTRIBPROC) (GLuint vaobj, GLuint index);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYELEMENTBUFFERPROC) (GLuint vaobj, GLuint buffer);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYVERTEXBUFFERPROC) (GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYVERTEXBUFFERSPROC) (GLuint vaobj, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYATTRIBBINDINGPROC) (GLuint vaobj, GLuint attribindex, GLuint bindingindex);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYATTRIBFORMATPROC) (GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYATTRIBIFORMATPROC) (GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYATTRIBLFORMATPROC) (GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYBINDINGDIVISORPROC) (GLuint vaobj, GLuint bindingindex, GLuint divisor);
typedef void (APIENTRYP RGLSYMGLGETVERTEXARRAYIVPROC) (GLuint vaobj, GLenum pname, GLint *param);
typedef void (APIENTRYP RGLSYMGLGETVERTEXARRAYINDEXEDIVPROC) (GLuint vaobj, GLuint index, GLenum pname, GLint *param);
typedef void (APIENTRYP RGLSYMGLGETVERTEXARRAYINDEXED64IVPROC) (GLuint vaobj, GLuint index, GLenum pname, GLint64 *param);
typedef void (APIENTRYP RGLSYMGLCREATESAMPLERSPROC) (GLsizei n, GLuint *samplers);
typedef void (APIENTRYP RGLSYMGLCREATEPROGRAMPIPELINESPROC) (GLsizei n, GLuint *pipelines);
typedef void (APIENTRYP RGLSYMGLCREATEQUERIESPROC) (GLenum target, GLsizei n, GLuint *ids);
typedef void (APIENTRYP RGLSYMGLGETQUERYBUFFEROBJECTI64VPROC) (GLuint id, GLuint buffer, GLenum pname, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLGETQUERYBUFFEROBJECTIVPROC) (GLuint id, GLuint buffer, GLenum pname, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLGETQUERYBUFFEROBJECTUI64VPROC) (GLuint id, GLuint buffer, GLenum pname, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLGETQUERYBUFFEROBJECTUIVPROC) (GLuint id, GLuint buffer, GLenum pname, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLMEMORYBARRIERBYREGIONPROC) (GLbitfield barriers);
typedef void (APIENTRYP RGLSYMGLGETTEXTURESUBIMAGEPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, GLsizei bufSize, void *pixels);
typedef void (APIENTRYP RGLSYMGLGETCOMPRESSEDTEXTURESUBIMAGEPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLsizei bufSize, void *pixels);
typedef GLenum (APIENTRYP RGLSYMGLGETGRAPHICSRESETSTATUSPROC) (void);
typedef void (APIENTRYP RGLSYMGLGETNCOMPRESSEDTEXIMAGEPROC) (GLenum target, GLint lod, GLsizei bufSize, void *pixels);
typedef void (APIENTRYP RGLSYMGLGETNTEXIMAGEPROC) (GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels);
typedef void (APIENTRYP RGLSYMGLGETNUNIFORMDVPROC) (GLuint program, GLint location, GLsizei bufSize, GLdouble *params);
typedef void (APIENTRYP RGLSYMGLGETNUNIFORMFVPROC) (GLuint program, GLint location, GLsizei bufSize, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETNUNIFORMIVPROC) (GLuint program, GLint location, GLsizei bufSize, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETNUNIFORMUIVPROC) (GLuint program, GLint location, GLsizei bufSize, GLuint *params);
typedef void (APIENTRYP RGLSYMGLREADNPIXELSPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data);
typedef void (APIENTRYP RGLSYMGLTEXTUREBARRIERPROC) (void);
typedef void (APIENTRYP RGLSYMGLSPECIALIZESHADERPROC) (GLuint shader, const GLchar *pEntryPoint, GLuint numSpecializationConstants, const GLuint *pConstantIndex, const GLuint *pConstantValue);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWARRAYSINDIRECTCOUNTPROC) (GLenum mode, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWELEMENTSINDIRECTCOUNTPROC) (GLenum mode, GLenum type, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLPOLYGONOFFSETCLAMPPROC) (GLfloat factor, GLfloat units, GLfloat clamp);
typedef void (APIENTRYP RGLSYMGLPRIMITIVEBOUNDINGBOXARBPROC) (GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW);
typedef GLuint64 (APIENTRYP RGLSYMGLGETTEXTUREHANDLEARBPROC) (GLuint texture);
typedef GLuint64 (APIENTRYP RGLSYMGLGETTEXTURESAMPLERHANDLEARBPROC) (GLuint texture, GLuint sampler);
typedef void (APIENTRYP RGLSYMGLMAKETEXTUREHANDLERESIDENTARBPROC) (GLuint64 handle);
typedef void (APIENTRYP RGLSYMGLMAKETEXTUREHANDLENONRESIDENTARBPROC) (GLuint64 handle);
typedef GLuint64 (APIENTRYP RGLSYMGLGETIMAGEHANDLEARBPROC) (GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format);
typedef void (APIENTRYP RGLSYMGLMAKEIMAGEHANDLERESIDENTARBPROC) (GLuint64 handle, GLenum access);
typedef void (APIENTRYP RGLSYMGLMAKEIMAGEHANDLENONRESIDENTARBPROC) (GLuint64 handle);
typedef void (APIENTRYP RGLSYMGLUNIFORMHANDLEUI64ARBPROC) (GLint location, GLuint64 value);
typedef void (APIENTRYP RGLSYMGLUNIFORMHANDLEUI64VARBPROC) (GLint location, GLsizei count, const GLuint64 *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMHANDLEUI64ARBPROC) (GLuint program, GLint location, GLuint64 value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMHANDLEUI64VARBPROC) (GLuint program, GLint location, GLsizei count, const GLuint64 *values);
typedef GLboolean (APIENTRYP RGLSYMGLISTEXTUREHANDLERESIDENTARBPROC) (GLuint64 handle);
typedef GLboolean (APIENTRYP RGLSYMGLISIMAGEHANDLERESIDENTARBPROC) (GLuint64 handle);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL1UI64ARBPROC) (GLuint index, GLuint64EXT x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL1UI64VARBPROC) (GLuint index, const GLuint64EXT *v);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBLUI64VARBPROC) (GLuint index, GLenum pname, GLuint64EXT *params);
typedef void (APIENTRYP RGLSYMGLDISPATCHCOMPUTEGROUPSIZEARBPROC) (GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z, GLuint group_size_x, GLuint group_size_y, GLuint group_size_z);
typedef void (APIENTRYP RGLSYMGLDEBUGMESSAGECONTROLARBPROC) (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled);
typedef void (APIENTRYP RGLSYMGLDEBUGMESSAGEINSERTARBPROC) (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf);
typedef void (APIENTRYP RGLSYMGLDEBUGMESSAGECALLBACKARBPROC) (RGLGENGLDEBUGPROCARB callback, const void *userParam);
typedef GLuint (APIENTRYP RGLSYMGLGETDEBUGMESSAGELOGARBPROC) (GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog);
typedef void (APIENTRYP RGLSYMGLBLENDEQUATIONIARBPROC) (GLuint buf, GLenum mode);
typedef void (APIENTRYP RGLSYMGLBLENDEQUATIONSEPARATEIARBPROC) (GLuint buf, GLenum modeRGB, GLenum modeAlpha);
typedef void (APIENTRYP RGLSYMGLBLENDFUNCIARBPROC) (GLuint buf, GLenum src, GLenum dst);
typedef void (APIENTRYP RGLSYMGLBLENDFUNCSEPARATEIARBPROC) (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
typedef void (APIENTRYP RGLSYMGLDRAWARRAYSINSTANCEDARBPROC) (GLenum mode, GLint first, GLsizei count, GLsizei primcount);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSINSTANCEDARBPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount);
typedef void (APIENTRYP RGLSYMGLPROGRAMPARAMETERIARBPROC) (GLuint program, GLenum pname, GLint value);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTUREARBPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTURELAYERARBPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTUREFACEARBPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLenum face);
typedef void (APIENTRYP RGLSYMGLSPECIALIZESHADERARBPROC) (GLuint shader, const GLchar *pEntryPoint, GLuint numSpecializationConstants, const GLuint *pConstantIndex, const GLuint *pConstantValue);
typedef void (APIENTRYP RGLSYMGLUNIFORM1I64ARBPROC) (GLint location, GLint64 x);
typedef void (APIENTRYP RGLSYMGLUNIFORM2I64ARBPROC) (GLint location, GLint64 x, GLint64 y);
typedef void (APIENTRYP RGLSYMGLUNIFORM3I64ARBPROC) (GLint location, GLint64 x, GLint64 y, GLint64 z);
typedef void (APIENTRYP RGLSYMGLUNIFORM4I64ARBPROC) (GLint location, GLint64 x, GLint64 y, GLint64 z, GLint64 w);
typedef void (APIENTRYP RGLSYMGLUNIFORM1I64VARBPROC) (GLint location, GLsizei count, const GLint64 *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM2I64VARBPROC) (GLint location, GLsizei count, const GLint64 *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM3I64VARBPROC) (GLint location, GLsizei count, const GLint64 *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM4I64VARBPROC) (GLint location, GLsizei count, const GLint64 *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM1UI64ARBPROC) (GLint location, GLuint64 x);
typedef void (APIENTRYP RGLSYMGLUNIFORM2UI64ARBPROC) (GLint location, GLuint64 x, GLuint64 y);
typedef void (APIENTRYP RGLSYMGLUNIFORM3UI64ARBPROC) (GLint location, GLuint64 x, GLuint64 y, GLuint64 z);
typedef void (APIENTRYP RGLSYMGLUNIFORM4UI64ARBPROC) (GLint location, GLuint64 x, GLuint64 y, GLuint64 z, GLuint64 w);
typedef void (APIENTRYP RGLSYMGLUNIFORM1UI64VARBPROC) (GLint location, GLsizei count, const GLuint64 *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM2UI64VARBPROC) (GLint location, GLsizei count, const GLuint64 *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM3UI64VARBPROC) (GLint location, GLsizei count, const GLuint64 *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM4UI64VARBPROC) (GLint location, GLsizei count, const GLuint64 *value);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMI64VARBPROC) (GLuint program, GLint location, GLint64 *params);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMUI64VARBPROC) (GLuint program, GLint location, GLuint64 *params);
typedef void (APIENTRYP RGLSYMGLGETNUNIFORMI64VARBPROC) (GLuint program, GLint location, GLsizei bufSize, GLint64 *params);
typedef void (APIENTRYP RGLSYMGLGETNUNIFORMUI64VARBPROC) (GLuint program, GLint location, GLsizei bufSize, GLuint64 *params);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1I64ARBPROC) (GLuint program, GLint location, GLint64 x);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2I64ARBPROC) (GLuint program, GLint location, GLint64 x, GLint64 y);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3I64ARBPROC) (GLuint program, GLint location, GLint64 x, GLint64 y, GLint64 z);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4I64ARBPROC) (GLuint program, GLint location, GLint64 x, GLint64 y, GLint64 z, GLint64 w);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1I64VARBPROC) (GLuint program, GLint location, GLsizei count, const GLint64 *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2I64VARBPROC) (GLuint program, GLint location, GLsizei count, const GLint64 *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3I64VARBPROC) (GLuint program, GLint location, GLsizei count, const GLint64 *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4I64VARBPROC) (GLuint program, GLint location, GLsizei count, const GLint64 *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1UI64ARBPROC) (GLuint program, GLint location, GLuint64 x);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2UI64ARBPROC) (GLuint program, GLint location, GLuint64 x, GLuint64 y);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3UI64ARBPROC) (GLuint program, GLint location, GLuint64 x, GLuint64 y, GLuint64 z);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4UI64ARBPROC) (GLuint program, GLint location, GLuint64 x, GLuint64 y, GLuint64 z, GLuint64 w);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1UI64VARBPROC) (GLuint program, GLint location, GLsizei count, const GLuint64 *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2UI64VARBPROC) (GLuint program, GLint location, GLsizei count, const GLuint64 *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3UI64VARBPROC) (GLuint program, GLint location, GLsizei count, const GLuint64 *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4UI64VARBPROC) (GLuint program, GLint location, GLsizei count, const GLuint64 *value);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWARRAYSINDIRECTCOUNTARBPROC) (GLenum mode, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWELEMENTSINDIRECTCOUNTARBPROC) (GLenum mode, GLenum type, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBDIVISORARBPROC) (GLuint index, GLuint divisor);
typedef void (APIENTRYP RGLSYMGLMAXSHADERCOMPILERTHREADSARBPROC) (GLuint count);
typedef GLenum (APIENTRYP RGLSYMGLGETGRAPHICSRESETSTATUSARBPROC) (void);
typedef void (APIENTRYP RGLSYMGLGETNTEXIMAGEARBPROC) (GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *img);
typedef void (APIENTRYP RGLSYMGLREADNPIXELSARBPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data);
typedef void (APIENTRYP RGLSYMGLGETNCOMPRESSEDTEXIMAGEARBPROC) (GLenum target, GLint lod, GLsizei bufSize, void *img);
typedef void (APIENTRYP RGLSYMGLGETNUNIFORMFVARBPROC) (GLuint program, GLint location, GLsizei bufSize, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETNUNIFORMIVARBPROC) (GLuint program, GLint location, GLsizei bufSize, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETNUNIFORMUIVARBPROC) (GLuint program, GLint location, GLsizei bufSize, GLuint *params);
typedef void (APIENTRYP RGLSYMGLGETNUNIFORMDVARBPROC) (GLuint program, GLint location, GLsizei bufSize, GLdouble *params);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERSAMPLELOCATIONSFVARBPROC) (GLenum target, GLuint start, GLsizei count, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERSAMPLELOCATIONSFVARBPROC) (GLuint framebuffer, GLuint start, GLsizei count, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLEVALUATEDEPTHVALUESARBPROC) (void);
typedef void (APIENTRYP RGLSYMGLMINSAMPLESHADINGARBPROC) (GLfloat value);
typedef void (APIENTRYP RGLSYMGLNAMEDSTRINGARBPROC) (GLenum type, GLint namelen, const GLchar *name, GLint stringlen, const GLchar *string);
typedef void (APIENTRYP RGLSYMGLDELETENAMEDSTRINGARBPROC) (GLint namelen, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLCOMPILESHADERINCLUDEARBPROC) (GLuint shader, GLsizei count, const GLchar *const*path, const GLint *length);
typedef GLboolean (APIENTRYP RGLSYMGLISNAMEDSTRINGARBPROC) (GLint namelen, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETNAMEDSTRINGARBPROC) (GLint namelen, const GLchar *name, GLsizei bufSize, GLint *stringlen, GLchar *string);
typedef void (APIENTRYP RGLSYMGLGETNAMEDSTRINGIVARBPROC) (GLint namelen, const GLchar *name, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLBUFFERPAGECOMMITMENTARBPROC) (GLenum target, GLintptr offset, GLsizeiptr size, GLboolean commit);
typedef void (APIENTRYP RGLSYMGLNAMEDBUFFERPAGECOMMITMENTEXTPROC) (GLuint buffer, GLintptr offset, GLsizeiptr size, GLboolean commit);
typedef void (APIENTRYP RGLSYMGLNAMEDBUFFERPAGECOMMITMENTARBPROC) (GLuint buffer, GLintptr offset, GLsizeiptr size, GLboolean commit);
typedef void (APIENTRYP RGLSYMGLTEXPAGECOMMITMENTARBPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLboolean commit);
typedef void (APIENTRYP RGLSYMGLTEXBUFFERARBPROC) (GLenum target, GLenum internalformat, GLuint buffer);
typedef void (APIENTRYP RGLSYMGLBLENDBARRIERKHRPROC) (void);
typedef void (APIENTRYP RGLSYMGLMAXSHADERCOMPILERTHREADSKHRPROC) (GLuint count);
typedef void (APIENTRYP RGLSYMGLEGLIMAGETARGETTEXSTORAGEEXTPROC) (GLenum target, GLeglImageOES image, const GLint* attrib_list);
typedef void (APIENTRYP RGLSYMGLEGLIMAGETARGETTEXTURESTORAGEEXTPROC) (GLuint texture, GLeglImageOES image, const GLint* attrib_list);
typedef void (APIENTRYP RGLSYMGLLABELOBJECTEXTPROC) (GLenum type, GLuint object, GLsizei length, const GLchar *label);
typedef void (APIENTRYP RGLSYMGLGETOBJECTLABELEXTPROC) (GLenum type, GLuint object, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (APIENTRYP RGLSYMGLINSERTEVENTMARKEREXTPROC) (GLsizei length, const GLchar *marker);
typedef void (APIENTRYP RGLSYMGLPUSHGROUPMARKEREXTPROC) (GLsizei length, const GLchar *marker);
typedef void (APIENTRYP RGLSYMGLPOPGROUPMARKEREXTPROC) (void);
typedef void (APIENTRYP RGLSYMGLMATRIXLOADFEXTPROC) (GLenum mode, const GLfloat *m);
typedef void (APIENTRYP RGLSYMGLMATRIXLOADDEXTPROC) (GLenum mode, const GLdouble *m);
typedef void (APIENTRYP RGLSYMGLMATRIXMULTFEXTPROC) (GLenum mode, const GLfloat *m);
typedef void (APIENTRYP RGLSYMGLMATRIXMULTDEXTPROC) (GLenum mode, const GLdouble *m);
typedef void (APIENTRYP RGLSYMGLMATRIXLOADIDENTITYEXTPROC) (GLenum mode);
typedef void (APIENTRYP RGLSYMGLMATRIXROTATEFEXTPROC) (GLenum mode, GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP RGLSYMGLMATRIXROTATEDEXTPROC) (GLenum mode, GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP RGLSYMGLMATRIXSCALEFEXTPROC) (GLenum mode, GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP RGLSYMGLMATRIXSCALEDEXTPROC) (GLenum mode, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP RGLSYMGLMATRIXTRANSLATEFEXTPROC) (GLenum mode, GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP RGLSYMGLMATRIXTRANSLATEDEXTPROC) (GLenum mode, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP RGLSYMGLMATRIXFRUSTUMEXTPROC) (GLenum mode, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
typedef void (APIENTRYP RGLSYMGLMATRIXORTHOEXTPROC) (GLenum mode, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
typedef void (APIENTRYP RGLSYMGLMATRIXPOPEXTPROC) (GLenum mode);
typedef void (APIENTRYP RGLSYMGLMATRIXPUSHEXTPROC) (GLenum mode);
typedef void (APIENTRYP RGLSYMGLCLIENTATTRIBDEFAULTEXTPROC) (GLbitfield mask);
typedef void (APIENTRYP RGLSYMGLPUSHCLIENTATTRIBDEFAULTEXTPROC) (GLbitfield mask);
typedef void (APIENTRYP RGLSYMGLTEXTUREPARAMETERFEXTPROC) (GLuint texture, GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRYP RGLSYMGLTEXTUREPARAMETERFVEXTPROC) (GLuint texture, GLenum target, GLenum pname, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLTEXTUREPARAMETERIEXTPROC) (GLuint texture, GLenum target, GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLTEXTUREPARAMETERIVEXTPROC) (GLuint texture, GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLTEXTUREIMAGE1DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLTEXTUREIMAGE2DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLTEXTURESUBIMAGE1DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLTEXTURESUBIMAGE2DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLCOPYTEXTUREIMAGE1DEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
typedef void (APIENTRYP RGLSYMGLCOPYTEXTUREIMAGE2DEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (APIENTRYP RGLSYMGLCOPYTEXTURESUBIMAGE1DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
typedef void (APIENTRYP RGLSYMGLCOPYTEXTURESUBIMAGE2DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLGETTEXTUREIMAGEEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum format, GLenum type, void *pixels);
typedef void (APIENTRYP RGLSYMGLGETTEXTUREPARAMETERFVEXTPROC) (GLuint texture, GLenum target, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETTEXTUREPARAMETERIVEXTPROC) (GLuint texture, GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETTEXTURELEVELPARAMETERFVEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETTEXTURELEVELPARAMETERIVEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLTEXTUREIMAGE3DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLTEXTURESUBIMAGE3DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLCOPYTEXTURESUBIMAGE3DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLBINDMULTITEXTUREEXTPROC) (GLenum texunit, GLenum target, GLuint texture);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORDPOINTEREXTPROC) (GLenum texunit, GLint size, GLenum type, GLsizei stride, const void *pointer);
typedef void (APIENTRYP RGLSYMGLMULTITEXENVFEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRYP RGLSYMGLMULTITEXENVFVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLMULTITEXENVIEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLMULTITEXENVIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLMULTITEXGENDEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, GLdouble param);
typedef void (APIENTRYP RGLSYMGLMULTITEXGENDVEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, const GLdouble *params);
typedef void (APIENTRYP RGLSYMGLMULTITEXGENFEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, GLfloat param);
typedef void (APIENTRYP RGLSYMGLMULTITEXGENFVEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLMULTITEXGENIEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLMULTITEXGENIVEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLGETMULTITEXENVFVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETMULTITEXENVIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETMULTITEXGENDVEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, GLdouble *params);
typedef void (APIENTRYP RGLSYMGLGETMULTITEXGENFVEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETMULTITEXGENIVEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLMULTITEXPARAMETERIEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLMULTITEXPARAMETERIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLMULTITEXPARAMETERFEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRYP RGLSYMGLMULTITEXPARAMETERFVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLMULTITEXIMAGE1DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLMULTITEXIMAGE2DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLMULTITEXSUBIMAGE1DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLMULTITEXSUBIMAGE2DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLCOPYMULTITEXIMAGE1DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
typedef void (APIENTRYP RGLSYMGLCOPYMULTITEXIMAGE2DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (APIENTRYP RGLSYMGLCOPYMULTITEXSUBIMAGE1DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
typedef void (APIENTRYP RGLSYMGLCOPYMULTITEXSUBIMAGE2DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLGETMULTITEXIMAGEEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum format, GLenum type, void *pixels);
typedef void (APIENTRYP RGLSYMGLGETMULTITEXPARAMETERFVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETMULTITEXPARAMETERIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETMULTITEXLEVELPARAMETERFVEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETMULTITEXLEVELPARAMETERIVEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLMULTITEXIMAGE3DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLMULTITEXSUBIMAGE3DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP RGLSYMGLCOPYMULTITEXSUBIMAGE3DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLENABLECLIENTSTATEINDEXEDEXTPROC) (GLenum array, GLuint index);
typedef void (APIENTRYP RGLSYMGLDISABLECLIENTSTATEINDEXEDEXTPROC) (GLenum array, GLuint index);
typedef void (APIENTRYP RGLSYMGLGETFLOATINDEXEDVEXTPROC) (GLenum target, GLuint index, GLfloat *data);
typedef void (APIENTRYP RGLSYMGLGETDOUBLEINDEXEDVEXTPROC) (GLenum target, GLuint index, GLdouble *data);
typedef void (APIENTRYP RGLSYMGLGETPOINTERINDEXEDVEXTPROC) (GLenum target, GLuint index, void **data);
typedef void (APIENTRYP RGLSYMGLENABLEINDEXEDEXTPROC) (GLenum target, GLuint index);
typedef void (APIENTRYP RGLSYMGLDISABLEINDEXEDEXTPROC) (GLenum target, GLuint index);
typedef GLboolean (APIENTRYP RGLSYMGLISENABLEDINDEXEDEXTPROC) (GLenum target, GLuint index);
typedef void (APIENTRYP RGLSYMGLGETINTEGERINDEXEDVEXTPROC) (GLenum target, GLuint index, GLint *data);
typedef void (APIENTRYP RGLSYMGLGETBOOLEANINDEXEDVEXTPROC) (GLenum target, GLuint index, GLboolean *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *bits);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *bits);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *bits);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *bits);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *bits);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *bits);
typedef void (APIENTRYP RGLSYMGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC) (GLuint texture, GLenum target, GLint lod, void *img);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDMULTITEXIMAGE3DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *bits);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDMULTITEXIMAGE2DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *bits);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDMULTITEXIMAGE1DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *bits);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDMULTITEXSUBIMAGE3DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *bits);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDMULTITEXSUBIMAGE2DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *bits);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDMULTITEXSUBIMAGE1DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *bits);
typedef void (APIENTRYP RGLSYMGLGETCOMPRESSEDMULTITEXIMAGEEXTPROC) (GLenum texunit, GLenum target, GLint lod, void *img);
typedef void (APIENTRYP RGLSYMGLMATRIXLOADTRANSPOSEFEXTPROC) (GLenum mode, const GLfloat *m);
typedef void (APIENTRYP RGLSYMGLMATRIXLOADTRANSPOSEDEXTPROC) (GLenum mode, const GLdouble *m);
typedef void (APIENTRYP RGLSYMGLMATRIXMULTTRANSPOSEFEXTPROC) (GLenum mode, const GLfloat *m);
typedef void (APIENTRYP RGLSYMGLMATRIXMULTTRANSPOSEDEXTPROC) (GLenum mode, const GLdouble *m);
typedef void (APIENTRYP RGLSYMGLNAMEDBUFFERDATAEXTPROC) (GLuint buffer, GLsizeiptr size, const void *data, GLenum usage);
typedef void (APIENTRYP RGLSYMGLNAMEDBUFFERSUBDATAEXTPROC) (GLuint buffer, GLintptr offset, GLsizeiptr size, const void *data);
typedef void *(APIENTRYP RGLSYMGLMAPNAMEDBUFFEREXTPROC) (GLuint buffer, GLenum access);
typedef GLboolean (APIENTRYP RGLSYMGLUNMAPNAMEDBUFFEREXTPROC) (GLuint buffer);
typedef void (APIENTRYP RGLSYMGLGETNAMEDBUFFERPARAMETERIVEXTPROC) (GLuint buffer, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETNAMEDBUFFERPOINTERVEXTPROC) (GLuint buffer, GLenum pname, void **params);
typedef void (APIENTRYP RGLSYMGLGETNAMEDBUFFERSUBDATAEXTPROC) (GLuint buffer, GLintptr offset, GLsizeiptr size, void *data);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1FEXTPROC) (GLuint program, GLint location, GLfloat v0);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2FEXTPROC) (GLuint program, GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3FEXTPROC) (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4FEXTPROC) (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1IEXTPROC) (GLuint program, GLint location, GLint v0);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2IEXTPROC) (GLuint program, GLint location, GLint v0, GLint v1);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3IEXTPROC) (GLuint program, GLint location, GLint v0, GLint v1, GLint v2);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4IEXTPROC) (GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLTEXTUREBUFFEREXTPROC) (GLuint texture, GLenum target, GLenum internalformat, GLuint buffer);
typedef void (APIENTRYP RGLSYMGLMULTITEXBUFFEREXTPROC) (GLenum texunit, GLenum target, GLenum internalformat, GLuint buffer);
typedef void (APIENTRYP RGLSYMGLTEXTUREPARAMETERIIVEXTPROC) (GLuint texture, GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLTEXTUREPARAMETERIUIVEXTPROC) (GLuint texture, GLenum target, GLenum pname, const GLuint *params);
typedef void (APIENTRYP RGLSYMGLGETTEXTUREPARAMETERIIVEXTPROC) (GLuint texture, GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETTEXTUREPARAMETERIUIVEXTPROC) (GLuint texture, GLenum target, GLenum pname, GLuint *params);
typedef void (APIENTRYP RGLSYMGLMULTITEXPARAMETERIIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLMULTITEXPARAMETERIUIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, const GLuint *params);
typedef void (APIENTRYP RGLSYMGLGETMULTITEXPARAMETERIIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETMULTITEXPARAMETERIUIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLuint *params);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1UIEXTPROC) (GLuint program, GLint location, GLuint v0);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2UIEXTPROC) (GLuint program, GLint location, GLuint v0, GLuint v1);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3UIEXTPROC) (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4UIEXTPROC) (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1UIVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2UIVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3UIVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4UIVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLNAMEDPROGRAMLOCALPARAMETERS4FVEXTPROC) (GLuint program, GLenum target, GLuint index, GLsizei count, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLNAMEDPROGRAMLOCALPARAMETERI4IEXTPROC) (GLuint program, GLenum target, GLuint index, GLint x, GLint y, GLint z, GLint w);
typedef void (APIENTRYP RGLSYMGLNAMEDPROGRAMLOCALPARAMETERI4IVEXTPROC) (GLuint program, GLenum target, GLuint index, const GLint *params);
typedef void (APIENTRYP RGLSYMGLNAMEDPROGRAMLOCALPARAMETERSI4IVEXTPROC) (GLuint program, GLenum target, GLuint index, GLsizei count, const GLint *params);
typedef void (APIENTRYP RGLSYMGLNAMEDPROGRAMLOCALPARAMETERI4UIEXTPROC) (GLuint program, GLenum target, GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
typedef void (APIENTRYP RGLSYMGLNAMEDPROGRAMLOCALPARAMETERI4UIVEXTPROC) (GLuint program, GLenum target, GLuint index, const GLuint *params);
typedef void (APIENTRYP RGLSYMGLNAMEDPROGRAMLOCALPARAMETERSI4UIVEXTPROC) (GLuint program, GLenum target, GLuint index, GLsizei count, const GLuint *params);
typedef void (APIENTRYP RGLSYMGLGETNAMEDPROGRAMLOCALPARAMETERIIVEXTPROC) (GLuint program, GLenum target, GLuint index, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETNAMEDPROGRAMLOCALPARAMETERIUIVEXTPROC) (GLuint program, GLenum target, GLuint index, GLuint *params);
typedef void (APIENTRYP RGLSYMGLENABLECLIENTSTATEIEXTPROC) (GLenum array, GLuint index);
typedef void (APIENTRYP RGLSYMGLDISABLECLIENTSTATEIEXTPROC) (GLenum array, GLuint index);
typedef void (APIENTRYP RGLSYMGLGETFLOATI_VEXTPROC) (GLenum pname, GLuint index, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETDOUBLEI_VEXTPROC) (GLenum pname, GLuint index, GLdouble *params);
typedef void (APIENTRYP RGLSYMGLGETPOINTERI_VEXTPROC) (GLenum pname, GLuint index, void **params);
typedef void (APIENTRYP RGLSYMGLNAMEDPROGRAMSTRINGEXTPROC) (GLuint program, GLenum target, GLenum format, GLsizei len, const void *string);
typedef void (APIENTRYP RGLSYMGLNAMEDPROGRAMLOCALPARAMETER4DEXTPROC) (GLuint program, GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRYP RGLSYMGLNAMEDPROGRAMLOCALPARAMETER4DVEXTPROC) (GLuint program, GLenum target, GLuint index, const GLdouble *params);
typedef void (APIENTRYP RGLSYMGLNAMEDPROGRAMLOCALPARAMETER4FEXTPROC) (GLuint program, GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRYP RGLSYMGLNAMEDPROGRAMLOCALPARAMETER4FVEXTPROC) (GLuint program, GLenum target, GLuint index, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETNAMEDPROGRAMLOCALPARAMETERDVEXTPROC) (GLuint program, GLenum target, GLuint index, GLdouble *params);
typedef void (APIENTRYP RGLSYMGLGETNAMEDPROGRAMLOCALPARAMETERFVEXTPROC) (GLuint program, GLenum target, GLuint index, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETNAMEDPROGRAMIVEXTPROC) (GLuint program, GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETNAMEDPROGRAMSTRINGEXTPROC) (GLuint program, GLenum target, GLenum pname, void *string);
typedef void (APIENTRYP RGLSYMGLNAMEDRENDERBUFFERSTORAGEEXTPROC) (GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLGETNAMEDRENDERBUFFERPARAMETERIVEXTPROC) (GLuint renderbuffer, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) (GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLECOVERAGEEXTPROC) (GLuint renderbuffer, GLsizei coverageSamples, GLsizei colorSamples, GLenum internalformat, GLsizei width, GLsizei height);
typedef GLenum (APIENTRYP RGLSYMGLCHECKNAMEDFRAMEBUFFERSTATUSEXTPROC) (GLuint framebuffer, GLenum target);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERTEXTURE1DEXTPROC) (GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC) (GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERTEXTURE3DEXTPROC) (GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERRENDERBUFFEREXTPROC) (GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (APIENTRYP RGLSYMGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC) (GLuint framebuffer, GLenum attachment, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGENERATETEXTUREMIPMAPEXTPROC) (GLuint texture, GLenum target);
typedef void (APIENTRYP RGLSYMGLGENERATEMULTITEXMIPMAPEXTPROC) (GLenum texunit, GLenum target);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERDRAWBUFFEREXTPROC) (GLuint framebuffer, GLenum mode);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERDRAWBUFFERSEXTPROC) (GLuint framebuffer, GLsizei n, const GLenum *bufs);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERREADBUFFEREXTPROC) (GLuint framebuffer, GLenum mode);
typedef void (APIENTRYP RGLSYMGLGETFRAMEBUFFERPARAMETERIVEXTPROC) (GLuint framebuffer, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLNAMEDCOPYBUFFERSUBDATAEXTPROC) (GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERTEXTUREEXTPROC) (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC) (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERTEXTUREFACEEXTPROC) (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLenum face);
typedef void (APIENTRYP RGLSYMGLTEXTURERENDERBUFFEREXTPROC) (GLuint texture, GLenum target, GLuint renderbuffer);
typedef void (APIENTRYP RGLSYMGLMULTITEXRENDERBUFFEREXTPROC) (GLenum texunit, GLenum target, GLuint renderbuffer);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYVERTEXOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYCOLOROFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYEDGEFLAGOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLsizei stride, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYINDEXOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYNORMALOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYTEXCOORDOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYMULTITEXCOORDOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLenum texunit, GLint size, GLenum type, GLsizei stride, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYFOGCOORDOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYSECONDARYCOLOROFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLENABLEVERTEXARRAYEXTPROC) (GLuint vaobj, GLenum array);
typedef void (APIENTRYP RGLSYMGLDISABLEVERTEXARRAYEXTPROC) (GLuint vaobj, GLenum array);
typedef void (APIENTRYP RGLSYMGLENABLEVERTEXARRAYATTRIBEXTPROC) (GLuint vaobj, GLuint index);
typedef void (APIENTRYP RGLSYMGLDISABLEVERTEXARRAYATTRIBEXTPROC) (GLuint vaobj, GLuint index);
typedef void (APIENTRYP RGLSYMGLGETVERTEXARRAYINTEGERVEXTPROC) (GLuint vaobj, GLenum pname, GLint *param);
typedef void (APIENTRYP RGLSYMGLGETVERTEXARRAYPOINTERVEXTPROC) (GLuint vaobj, GLenum pname, void **param);
typedef void (APIENTRYP RGLSYMGLGETVERTEXARRAYINTEGERI_VEXTPROC) (GLuint vaobj, GLuint index, GLenum pname, GLint *param);
typedef void (APIENTRYP RGLSYMGLGETVERTEXARRAYPOINTERI_VEXTPROC) (GLuint vaobj, GLuint index, GLenum pname, void **param);
typedef void *(APIENTRYP RGLSYMGLMAPNAMEDBUFFERRANGEEXTPROC) (GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void (APIENTRYP RGLSYMGLFLUSHMAPPEDNAMEDBUFFERRANGEEXTPROC) (GLuint buffer, GLintptr offset, GLsizeiptr length);
typedef void (APIENTRYP RGLSYMGLNAMEDBUFFERSTORAGEEXTPROC) (GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags);
typedef void (APIENTRYP RGLSYMGLCLEARNAMEDBUFFERDATAEXTPROC) (GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void *data);
typedef void (APIENTRYP RGLSYMGLCLEARNAMEDBUFFERSUBDATAEXTPROC) (GLuint buffer, GLenum internalformat, GLsizeiptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERPARAMETERIEXTPROC) (GLuint framebuffer, GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLGETNAMEDFRAMEBUFFERPARAMETERIVEXTPROC) (GLuint framebuffer, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1DEXTPROC) (GLuint program, GLint location, GLdouble x);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2DEXTPROC) (GLuint program, GLint location, GLdouble x, GLdouble y);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3DEXTPROC) (GLuint program, GLint location, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4DEXTPROC) (GLuint program, GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1DVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2DVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3DVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4DVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2X3DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2X4DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3X2DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3X4DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4X2DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4X3DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLTEXTUREBUFFERRANGEEXTPROC) (GLuint texture, GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (APIENTRYP RGLSYMGLTEXTURESTORAGE1DEXTPROC) (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
typedef void (APIENTRYP RGLSYMGLTEXTURESTORAGE2DEXTPROC) (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLTEXTURESTORAGE3DEXTPROC) (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
typedef void (APIENTRYP RGLSYMGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC) (GLuint texture, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void (APIENTRYP RGLSYMGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC) (GLuint texture, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYBINDVERTEXBUFFEREXTPROC) (GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYVERTEXATTRIBFORMATEXTPROC) (GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYVERTEXATTRIBIFORMATEXTPROC) (GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYVERTEXATTRIBLFORMATEXTPROC) (GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYVERTEXATTRIBBINDINGEXTPROC) (GLuint vaobj, GLuint attribindex, GLuint bindingindex);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYVERTEXBINDINGDIVISOREXTPROC) (GLuint vaobj, GLuint bindingindex, GLuint divisor);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYVERTEXATTRIBLOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset);
typedef void (APIENTRYP RGLSYMGLTEXTUREPAGECOMMITMENTEXTPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLboolean commit);
typedef void (APIENTRYP RGLSYMGLVERTEXARRAYVERTEXATTRIBDIVISOREXTPROC) (GLuint vaobj, GLuint index, GLuint divisor);
typedef void (APIENTRYP RGLSYMGLDRAWARRAYSINSTANCEDEXTPROC) (GLenum mode, GLint start, GLsizei count, GLsizei primcount);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSINSTANCEDEXTPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount);
typedef void (APIENTRYP RGLSYMGLPOLYGONOFFSETCLAMPEXTPROC) (GLfloat factor, GLfloat units, GLfloat clamp);
typedef void (APIENTRYP RGLSYMGLRASTERSAMPLESEXTPROC) (GLuint samples, GLboolean fixedsamplelocations);
typedef void (APIENTRYP RGLSYMGLUSESHADERPROGRAMEXTPROC) (GLenum type, GLuint program);
typedef void (APIENTRYP RGLSYMGLACTIVEPROGRAMEXTPROC) (GLuint program);
typedef GLuint (APIENTRYP RGLSYMGLCREATESHADERPROGRAMEXTPROC) (GLenum type, const GLchar *string);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERFETCHBARRIEREXTPROC) (void);
typedef void (APIENTRYP RGLSYMGLWINDOWRECTANGLESEXTPROC) (GLenum mode, GLsizei count, const GLint *box);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWARRAYSINDIRECTBINDLESSNVPROC) (GLenum mode, const void *indirect, GLsizei drawCount, GLsizei stride, GLint vertexBufferCount);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWELEMENTSINDIRECTBINDLESSNVPROC) (GLenum mode, GLenum type, const void *indirect, GLsizei drawCount, GLsizei stride, GLint vertexBufferCount);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWARRAYSINDIRECTBINDLESSCOUNTNVPROC) (GLenum mode, const void *indirect, GLsizei drawCount, GLsizei maxDrawCount, GLsizei stride, GLint vertexBufferCount);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWELEMENTSINDIRECTBINDLESSCOUNTNVPROC) (GLenum mode, GLenum type, const void *indirect, GLsizei drawCount, GLsizei maxDrawCount, GLsizei stride, GLint vertexBufferCount);
typedef GLuint64 (APIENTRYP RGLSYMGLGETTEXTUREHANDLENVPROC) (GLuint texture);
typedef GLuint64 (APIENTRYP RGLSYMGLGETTEXTURESAMPLERHANDLENVPROC) (GLuint texture, GLuint sampler);
typedef void (APIENTRYP RGLSYMGLMAKETEXTUREHANDLERESIDENTNVPROC) (GLuint64 handle);
typedef void (APIENTRYP RGLSYMGLMAKETEXTUREHANDLENONRESIDENTNVPROC) (GLuint64 handle);
typedef GLuint64 (APIENTRYP RGLSYMGLGETIMAGEHANDLENVPROC) (GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format);
typedef void (APIENTRYP RGLSYMGLMAKEIMAGEHANDLERESIDENTNVPROC) (GLuint64 handle, GLenum access);
typedef void (APIENTRYP RGLSYMGLMAKEIMAGEHANDLENONRESIDENTNVPROC) (GLuint64 handle);
typedef void (APIENTRYP RGLSYMGLUNIFORMHANDLEUI64NVPROC) (GLint location, GLuint64 value);
typedef void (APIENTRYP RGLSYMGLUNIFORMHANDLEUI64VNVPROC) (GLint location, GLsizei count, const GLuint64 *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMHANDLEUI64NVPROC) (GLuint program, GLint location, GLuint64 value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMHANDLEUI64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLuint64 *values);
typedef GLboolean (APIENTRYP RGLSYMGLISTEXTUREHANDLERESIDENTNVPROC) (GLuint64 handle);
typedef GLboolean (APIENTRYP RGLSYMGLISIMAGEHANDLERESIDENTNVPROC) (GLuint64 handle);
typedef void (APIENTRYP RGLSYMGLBLENDPARAMETERINVPROC) (GLenum pname, GLint value);
typedef void (APIENTRYP RGLSYMGLBLENDBARRIERNVPROC) (void);
typedef void (APIENTRYP RGLSYMGLVIEWPORTPOSITIONWSCALENVPROC) (GLuint index, GLfloat xcoeff, GLfloat ycoeff);
typedef void (APIENTRYP RGLSYMGLCREATESTATESNVPROC) (GLsizei n, GLuint *states);
typedef void (APIENTRYP RGLSYMGLDELETESTATESNVPROC) (GLsizei n, const GLuint *states);
typedef GLboolean (APIENTRYP RGLSYMGLISSTATENVPROC) (GLuint state);
typedef void (APIENTRYP RGLSYMGLSTATECAPTURENVPROC) (GLuint state, GLenum mode);
typedef GLuint (APIENTRYP RGLSYMGLGETCOMMANDHEADERNVPROC) (GLenum tokenID, GLuint size);
typedef GLushort (APIENTRYP RGLSYMGLGETSTAGEINDEXNVPROC) (GLenum shadertype);
typedef void (APIENTRYP RGLSYMGLDRAWCOMMANDSNVPROC) (GLenum primitiveMode, GLuint buffer, const GLintptr *indirects, const GLsizei *sizes, GLuint count);
typedef void (APIENTRYP RGLSYMGLDRAWCOMMANDSADDRESSNVPROC) (GLenum primitiveMode, const GLuint64 *indirects, const GLsizei *sizes, GLuint count);
typedef void (APIENTRYP RGLSYMGLDRAWCOMMANDSSTATESNVPROC) (GLuint buffer, const GLintptr *indirects, const GLsizei *sizes, const GLuint *states, const GLuint *fbos, GLuint count);
typedef void (APIENTRYP RGLSYMGLDRAWCOMMANDSSTATESADDRESSNVPROC) (const GLuint64 *indirects, const GLsizei *sizes, const GLuint *states, const GLuint *fbos, GLuint count);
typedef void (APIENTRYP RGLSYMGLCREATECOMMANDLISTSNVPROC) (GLsizei n, GLuint *lists);
typedef void (APIENTRYP RGLSYMGLDELETECOMMANDLISTSNVPROC) (GLsizei n, const GLuint *lists);
typedef GLboolean (APIENTRYP RGLSYMGLISCOMMANDLISTNVPROC) (GLuint list);
typedef void (APIENTRYP RGLSYMGLLISTDRAWCOMMANDSSTATESCLIENTNVPROC) (GLuint list, GLuint segment, const void **indirects, const GLsizei *sizes, const GLuint *states, const GLuint *fbos, GLuint count);
typedef void (APIENTRYP RGLSYMGLCOMMANDLISTSEGMENTSNVPROC) (GLuint list, GLuint segments);
typedef void (APIENTRYP RGLSYMGLCOMPILECOMMANDLISTNVPROC) (GLuint list);
typedef void (APIENTRYP RGLSYMGLCALLCOMMANDLISTNVPROC) (GLuint list);
typedef void (APIENTRYP RGLSYMGLBEGINCONDITIONALRENDERNVPROC) (GLuint id, GLenum mode);
typedef void (APIENTRYP RGLSYMGLENDCONDITIONALRENDERNVPROC) (void);
typedef void (APIENTRYP RGLSYMGLSUBPIXELPRECISIONBIASNVPROC) (GLuint xbits, GLuint ybits);
typedef void (APIENTRYP RGLSYMGLCONSERVATIVERASTERPARAMETERFNVPROC) (GLenum pname, GLfloat value);
typedef void (APIENTRYP RGLSYMGLCONSERVATIVERASTERPARAMETERINVPROC) (GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLDRAWVKIMAGENVPROC) (GLuint64 vkImage, GLuint sampler, GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1, GLfloat z, GLfloat s0, GLfloat t0, GLfloat s1, GLfloat t1);
typedef void (APIENTRYP RGLSYMGLWAITVKSEMAPHORENVPROC) (GLuint64 vkSemaphore);
typedef void (APIENTRYP RGLSYMGLSIGNALVKSEMAPHORENVPROC) (GLuint64 vkSemaphore);
typedef void (APIENTRYP RGLSYMGLSIGNALVKFENCENVPROC) (GLuint64 vkFence);
typedef void (APIENTRYP RGLSYMGLFRAGMENTCOVERAGECOLORNVPROC) (GLuint color);
typedef void (APIENTRYP RGLSYMGLCOVERAGEMODULATIONTABLENVPROC) (GLsizei n, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLGETCOVERAGEMODULATIONTABLENVPROC) (GLsizei bufsize, GLfloat *v);
typedef void (APIENTRYP RGLSYMGLCOVERAGEMODULATIONNVPROC) (GLenum components);
typedef void (APIENTRYP RGLSYMGLRENDERBUFFERSTORAGEMULTISAMPLECOVERAGENVPROC) (GLenum target, GLsizei coverageSamples, GLsizei colorSamples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLUNIFORM1I64NVPROC) (GLint location, GLint64EXT x);
typedef void (APIENTRYP RGLSYMGLUNIFORM2I64NVPROC) (GLint location, GLint64EXT x, GLint64EXT y);
typedef void (APIENTRYP RGLSYMGLUNIFORM3I64NVPROC) (GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z);
typedef void (APIENTRYP RGLSYMGLUNIFORM4I64NVPROC) (GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z, GLint64EXT w);
typedef void (APIENTRYP RGLSYMGLUNIFORM1I64VNVPROC) (GLint location, GLsizei count, const GLint64EXT *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM2I64VNVPROC) (GLint location, GLsizei count, const GLint64EXT *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM3I64VNVPROC) (GLint location, GLsizei count, const GLint64EXT *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM4I64VNVPROC) (GLint location, GLsizei count, const GLint64EXT *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM1UI64NVPROC) (GLint location, GLuint64EXT x);
typedef void (APIENTRYP RGLSYMGLUNIFORM2UI64NVPROC) (GLint location, GLuint64EXT x, GLuint64EXT y);
typedef void (APIENTRYP RGLSYMGLUNIFORM3UI64NVPROC) (GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z);
typedef void (APIENTRYP RGLSYMGLUNIFORM4UI64NVPROC) (GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z, GLuint64EXT w);
typedef void (APIENTRYP RGLSYMGLUNIFORM1UI64VNVPROC) (GLint location, GLsizei count, const GLuint64EXT *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM2UI64VNVPROC) (GLint location, GLsizei count, const GLuint64EXT *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM3UI64VNVPROC) (GLint location, GLsizei count, const GLuint64EXT *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM4UI64VNVPROC) (GLint location, GLsizei count, const GLuint64EXT *value);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMI64VNVPROC) (GLuint program, GLint location, GLint64EXT *params);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1I64NVPROC) (GLuint program, GLint location, GLint64EXT x);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2I64NVPROC) (GLuint program, GLint location, GLint64EXT x, GLint64EXT y);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3I64NVPROC) (GLuint program, GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4I64NVPROC) (GLuint program, GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z, GLint64EXT w);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1I64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLint64EXT *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2I64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLint64EXT *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3I64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLint64EXT *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4I64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLint64EXT *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1UI64NVPROC) (GLuint program, GLint location, GLuint64EXT x);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2UI64NVPROC) (GLuint program, GLint location, GLuint64EXT x, GLuint64EXT y);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3UI64NVPROC) (GLuint program, GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4UI64NVPROC) (GLuint program, GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z, GLuint64EXT w);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1UI64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLuint64EXT *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2UI64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLuint64EXT *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3UI64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLuint64EXT *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4UI64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLuint64EXT *value);
typedef void (APIENTRYP RGLSYMGLGETINTERNALFORMATSAMPLEIVNVPROC) (GLenum target, GLenum internalformat, GLsizei samples, GLenum pname, GLsizei bufSize, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETMEMORYOBJECTDETACHEDRESOURCESUIVNVPROC) (GLuint memory, GLenum pname, GLint first, GLsizei count, GLuint *params);
typedef void (APIENTRYP RGLSYMGLRESETMEMORYOBJECTPARAMETERNVPROC) (GLuint memory, GLenum pname);
typedef void (APIENTRYP RGLSYMGLTEXATTACHMEMORYNVPROC) (GLenum target, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP RGLSYMGLBUFFERATTACHMEMORYNVPROC) (GLenum target, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP RGLSYMGLTEXTUREATTACHMEMORYNVPROC) (GLuint texture, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP RGLSYMGLNAMEDBUFFERATTACHMEMORYNVPROC) (GLuint buffer, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP RGLSYMGLDRAWMESHTASKSNVPROC) (GLuint first, GLuint count);
typedef void (APIENTRYP RGLSYMGLDRAWMESHTASKSINDIRECTNVPROC) (GLintptr indirect);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWMESHTASKSINDIRECTNVPROC) (GLintptr indirect, GLsizei drawcount, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWMESHTASKSINDIRECTCOUNTNVPROC) (GLintptr indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride);
typedef GLuint (APIENTRYP RGLSYMGLGENPATHSNVPROC) (GLsizei range);
typedef void (APIENTRYP RGLSYMGLDELETEPATHSNVPROC) (GLuint path, GLsizei range);
typedef GLboolean (APIENTRYP RGLSYMGLISPATHNVPROC) (GLuint path);
typedef void (APIENTRYP RGLSYMGLPATHCOMMANDSNVPROC) (GLuint path, GLsizei numCommands, const GLubyte *commands, GLsizei numCoords, GLenum coordType, const void *coords);
typedef void (APIENTRYP RGLSYMGLPATHCOORDSNVPROC) (GLuint path, GLsizei numCoords, GLenum coordType, const void *coords);
typedef void (APIENTRYP RGLSYMGLPATHSUBCOMMANDSNVPROC) (GLuint path, GLsizei commandStart, GLsizei commandsToDelete, GLsizei numCommands, const GLubyte *commands, GLsizei numCoords, GLenum coordType, const void *coords);
typedef void (APIENTRYP RGLSYMGLPATHSUBCOORDSNVPROC) (GLuint path, GLsizei coordStart, GLsizei numCoords, GLenum coordType, const void *coords);
typedef void (APIENTRYP RGLSYMGLPATHSTRINGNVPROC) (GLuint path, GLenum format, GLsizei length, const void *pathString);
typedef void (APIENTRYP RGLSYMGLPATHGLYPHSNVPROC) (GLuint firstPathName, GLenum fontTarget, const void *fontName, GLbitfield fontStyle, GLsizei numGlyphs, GLenum type, const void *charcodes, GLenum handleMissingGlyphs, GLuint pathParameterTemplate, GLfloat emScale);
typedef void (APIENTRYP RGLSYMGLPATHGLYPHRANGENVPROC) (GLuint firstPathName, GLenum fontTarget, const void *fontName, GLbitfield fontStyle, GLuint firstGlyph, GLsizei numGlyphs, GLenum handleMissingGlyphs, GLuint pathParameterTemplate, GLfloat emScale);
typedef void (APIENTRYP RGLSYMGLWEIGHTPATHSNVPROC) (GLuint resultPath, GLsizei numPaths, const GLuint *paths, const GLfloat *weights);
typedef void (APIENTRYP RGLSYMGLCOPYPATHNVPROC) (GLuint resultPath, GLuint srcPath);
typedef void (APIENTRYP RGLSYMGLINTERPOLATEPATHSNVPROC) (GLuint resultPath, GLuint pathA, GLuint pathB, GLfloat weight);
typedef void (APIENTRYP RGLSYMGLTRANSFORMPATHNVPROC) (GLuint resultPath, GLuint srcPath, GLenum transformType, const GLfloat *transformValues);
typedef void (APIENTRYP RGLSYMGLPATHPARAMETERIVNVPROC) (GLuint path, GLenum pname, const GLint *value);
typedef void (APIENTRYP RGLSYMGLPATHPARAMETERINVPROC) (GLuint path, GLenum pname, GLint value);
typedef void (APIENTRYP RGLSYMGLPATHPARAMETERFVNVPROC) (GLuint path, GLenum pname, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPATHPARAMETERFNVPROC) (GLuint path, GLenum pname, GLfloat value);
typedef void (APIENTRYP RGLSYMGLPATHDASHARRAYNVPROC) (GLuint path, GLsizei dashCount, const GLfloat *dashArray);
typedef void (APIENTRYP RGLSYMGLPATHSTENCILFUNCNVPROC) (GLenum func, GLint ref, GLuint mask);
typedef void (APIENTRYP RGLSYMGLPATHSTENCILDEPTHOFFSETNVPROC) (GLfloat factor, GLfloat units);
typedef void (APIENTRYP RGLSYMGLSTENCILFILLPATHNVPROC) (GLuint path, GLenum fillMode, GLuint mask);
typedef void (APIENTRYP RGLSYMGLSTENCILSTROKEPATHNVPROC) (GLuint path, GLint reference, GLuint mask);
typedef void (APIENTRYP RGLSYMGLSTENCILFILLPATHINSTANCEDNVPROC) (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLenum fillMode, GLuint mask, GLenum transformType, const GLfloat *transformValues);
typedef void (APIENTRYP RGLSYMGLSTENCILSTROKEPATHINSTANCEDNVPROC) (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLint reference, GLuint mask, GLenum transformType, const GLfloat *transformValues);
typedef void (APIENTRYP RGLSYMGLPATHCOVERDEPTHFUNCNVPROC) (GLenum func);
typedef void (APIENTRYP RGLSYMGLCOVERFILLPATHNVPROC) (GLuint path, GLenum coverMode);
typedef void (APIENTRYP RGLSYMGLCOVERSTROKEPATHNVPROC) (GLuint path, GLenum coverMode);
typedef void (APIENTRYP RGLSYMGLCOVERFILLPATHINSTANCEDNVPROC) (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLenum coverMode, GLenum transformType, const GLfloat *transformValues);
typedef void (APIENTRYP RGLSYMGLCOVERSTROKEPATHINSTANCEDNVPROC) (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLenum coverMode, GLenum transformType, const GLfloat *transformValues);
typedef void (APIENTRYP RGLSYMGLGETPATHPARAMETERIVNVPROC) (GLuint path, GLenum pname, GLint *value);
typedef void (APIENTRYP RGLSYMGLGETPATHPARAMETERFVNVPROC) (GLuint path, GLenum pname, GLfloat *value);
typedef void (APIENTRYP RGLSYMGLGETPATHCOMMANDSNVPROC) (GLuint path, GLubyte *commands);
typedef void (APIENTRYP RGLSYMGLGETPATHCOORDSNVPROC) (GLuint path, GLfloat *coords);
typedef void (APIENTRYP RGLSYMGLGETPATHDASHARRAYNVPROC) (GLuint path, GLfloat *dashArray);
typedef void (APIENTRYP RGLSYMGLGETPATHMETRICSNVPROC) (GLbitfield metricQueryMask, GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLsizei stride, GLfloat *metrics);
typedef void (APIENTRYP RGLSYMGLGETPATHMETRICRANGENVPROC) (GLbitfield metricQueryMask, GLuint firstPathName, GLsizei numPaths, GLsizei stride, GLfloat *metrics);
typedef void (APIENTRYP RGLSYMGLGETPATHSPACINGNVPROC) (GLenum pathListMode, GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLfloat advanceScale, GLfloat kerningScale, GLenum transformType, GLfloat *returnedSpacing);
typedef GLboolean (APIENTRYP RGLSYMGLISPOINTINFILLPATHNVPROC) (GLuint path, GLuint mask, GLfloat x, GLfloat y);
typedef GLboolean (APIENTRYP RGLSYMGLISPOINTINSTROKEPATHNVPROC) (GLuint path, GLfloat x, GLfloat y);
typedef GLfloat (APIENTRYP RGLSYMGLGETPATHLENGTHNVPROC) (GLuint path, GLsizei startSegment, GLsizei numSegments);
typedef GLboolean (APIENTRYP RGLSYMGLPOINTALONGPATHNVPROC) (GLuint path, GLsizei startSegment, GLsizei numSegments, GLfloat distance, GLfloat *x, GLfloat *y, GLfloat *tangentX, GLfloat *tangentY);
typedef void (APIENTRYP RGLSYMGLMATRIXLOAD3X2FNVPROC) (GLenum matrixMode, const GLfloat *m);
typedef void (APIENTRYP RGLSYMGLMATRIXLOAD3X3FNVPROC) (GLenum matrixMode, const GLfloat *m);
typedef void (APIENTRYP RGLSYMGLMATRIXLOADTRANSPOSE3X3FNVPROC) (GLenum matrixMode, const GLfloat *m);
typedef void (APIENTRYP RGLSYMGLMATRIXMULT3X2FNVPROC) (GLenum matrixMode, const GLfloat *m);
typedef void (APIENTRYP RGLSYMGLMATRIXMULT3X3FNVPROC) (GLenum matrixMode, const GLfloat *m);
typedef void (APIENTRYP RGLSYMGLMATRIXMULTTRANSPOSE3X3FNVPROC) (GLenum matrixMode, const GLfloat *m);
typedef void (APIENTRYP RGLSYMGLSTENCILTHENCOVERFILLPATHNVPROC) (GLuint path, GLenum fillMode, GLuint mask, GLenum coverMode);
typedef void (APIENTRYP RGLSYMGLSTENCILTHENCOVERSTROKEPATHNVPROC) (GLuint path, GLint reference, GLuint mask, GLenum coverMode);
typedef void (APIENTRYP RGLSYMGLSTENCILTHENCOVERFILLPATHINSTANCEDNVPROC) (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLenum fillMode, GLuint mask, GLenum coverMode, GLenum transformType, const GLfloat *transformValues);
typedef void (APIENTRYP RGLSYMGLSTENCILTHENCOVERSTROKEPATHINSTANCEDNVPROC) (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLint reference, GLuint mask, GLenum coverMode, GLenum transformType, const GLfloat *transformValues);
typedef GLenum (APIENTRYP RGLSYMGLPATHGLYPHINDEXRANGENVPROC) (GLenum fontTarget, const void *fontName, GLbitfield fontStyle, GLuint pathParameterTemplate, GLfloat emScale, GLuint baseAndCount[2]);
typedef GLenum (APIENTRYP RGLSYMGLPATHGLYPHINDEXARRAYNVPROC) (GLuint firstPathName, GLenum fontTarget, const void *fontName, GLbitfield fontStyle, GLuint firstGlyphIndex, GLsizei numGlyphs, GLuint pathParameterTemplate, GLfloat emScale);
typedef GLenum (APIENTRYP RGLSYMGLPATHMEMORYGLYPHINDEXARRAYNVPROC) (GLuint firstPathName, GLenum fontTarget, GLsizeiptr fontSize, const void *fontData, GLsizei faceIndex, GLuint firstGlyphIndex, GLsizei numGlyphs, GLuint pathParameterTemplate, GLfloat emScale);
typedef void (APIENTRYP RGLSYMGLPROGRAMPATHFRAGMENTINPUTGENNVPROC) (GLuint program, GLint location, GLenum genMode, GLint components, const GLfloat *coeffs);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMRESOURCEFVNVPROC) (GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei bufSize, GLsizei *length, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERSAMPLELOCATIONSFVNVPROC) (GLenum target, GLuint start, GLsizei count, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLNAMEDFRAMEBUFFERSAMPLELOCATIONSFVNVPROC) (GLuint framebuffer, GLuint start, GLsizei count, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLRESOLVEDEPTHVALUESNVPROC) (void);
typedef void (APIENTRYP RGLSYMGLSCISSOREXCLUSIVENVPROC) (GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLSCISSOREXCLUSIVEARRAYVNVPROC) (GLuint first, GLsizei count, const GLint *v);
typedef void (APIENTRYP RGLSYMGLMAKEBUFFERRESIDENTNVPROC) (GLenum target, GLenum access);
typedef void (APIENTRYP RGLSYMGLMAKEBUFFERNONRESIDENTNVPROC) (GLenum target);
typedef GLboolean (APIENTRYP RGLSYMGLISBUFFERRESIDENTNVPROC) (GLenum target);
typedef void (APIENTRYP RGLSYMGLMAKENAMEDBUFFERRESIDENTNVPROC) (GLuint buffer, GLenum access);
typedef void (APIENTRYP RGLSYMGLMAKENAMEDBUFFERNONRESIDENTNVPROC) (GLuint buffer);
typedef GLboolean (APIENTRYP RGLSYMGLISNAMEDBUFFERRESIDENTNVPROC) (GLuint buffer);
typedef void (APIENTRYP RGLSYMGLGETBUFFERPARAMETERUI64VNVPROC) (GLenum target, GLenum pname, GLuint64EXT *params);
typedef void (APIENTRYP RGLSYMGLGETNAMEDBUFFERPARAMETERUI64VNVPROC) (GLuint buffer, GLenum pname, GLuint64EXT *params);
typedef void (APIENTRYP RGLSYMGLGETINTEGERUI64VNVPROC) (GLenum value, GLuint64EXT *result);
typedef void (APIENTRYP RGLSYMGLUNIFORMUI64NVPROC) (GLint location, GLuint64EXT value);
typedef void (APIENTRYP RGLSYMGLUNIFORMUI64VNVPROC) (GLint location, GLsizei count, const GLuint64EXT *value);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMUI64VNVPROC) (GLuint program, GLint location, GLuint64EXT *params);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMUI64NVPROC) (GLuint program, GLint location, GLuint64EXT value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMUI64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLuint64EXT *value);
typedef void (APIENTRYP RGLSYMGLBINDSHADINGRATEIMAGENVPROC) (GLuint texture);
typedef void (APIENTRYP RGLSYMGLGETSHADINGRATEIMAGEPALETTENVPROC) (GLuint viewport, GLuint entry, GLenum *rate);
typedef void (APIENTRYP RGLSYMGLGETSHADINGRATESAMPLELOCATIONIVNVPROC) (GLenum rate, GLuint samples, GLuint index, GLint *location);
typedef void (APIENTRYP RGLSYMGLSHADINGRATEIMAGEBARRIERNVPROC) (GLboolean synchronize);
typedef void (APIENTRYP RGLSYMGLSHADINGRATEIMAGEPALETTENVPROC) (GLuint viewport, GLuint first, GLsizei count, const GLenum *rates);
typedef void (APIENTRYP RGLSYMGLSHADINGRATESAMPLEORDERNVPROC) (GLenum order);
typedef void (APIENTRYP RGLSYMGLSHADINGRATESAMPLEORDERCUSTOMNVPROC) (GLenum rate, GLuint samples, const GLint *locations);
typedef void (APIENTRYP RGLSYMGLTEXTUREBARRIERNVPROC) (void);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL1I64NVPROC) (GLuint index, GLint64EXT x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL2I64NVPROC) (GLuint index, GLint64EXT x, GLint64EXT y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL3I64NVPROC) (GLuint index, GLint64EXT x, GLint64EXT y, GLint64EXT z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL4I64NVPROC) (GLuint index, GLint64EXT x, GLint64EXT y, GLint64EXT z, GLint64EXT w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL1I64VNVPROC) (GLuint index, const GLint64EXT *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL2I64VNVPROC) (GLuint index, const GLint64EXT *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL3I64VNVPROC) (GLuint index, const GLint64EXT *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL4I64VNVPROC) (GLuint index, const GLint64EXT *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL1UI64NVPROC) (GLuint index, GLuint64EXT x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL2UI64NVPROC) (GLuint index, GLuint64EXT x, GLuint64EXT y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL3UI64NVPROC) (GLuint index, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL4UI64NVPROC) (GLuint index, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z, GLuint64EXT w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL1UI64VNVPROC) (GLuint index, const GLuint64EXT *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL2UI64VNVPROC) (GLuint index, const GLuint64EXT *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL3UI64VNVPROC) (GLuint index, const GLuint64EXT *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL4UI64VNVPROC) (GLuint index, const GLuint64EXT *v);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBLI64VNVPROC) (GLuint index, GLenum pname, GLint64EXT *params);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBLUI64VNVPROC) (GLuint index, GLenum pname, GLuint64EXT *params);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBLFORMATNVPROC) (GLuint index, GLint size, GLenum type, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLBUFFERADDRESSRANGENVPROC) (GLenum pname, GLuint index, GLuint64EXT address, GLsizeiptr length);
typedef void (APIENTRYP RGLSYMGLVERTEXFORMATNVPROC) (GLint size, GLenum type, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLNORMALFORMATNVPROC) (GLenum type, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLCOLORFORMATNVPROC) (GLint size, GLenum type, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLINDEXFORMATNVPROC) (GLenum type, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLTEXCOORDFORMATNVPROC) (GLint size, GLenum type, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLEDGEFLAGFORMATNVPROC) (GLsizei stride);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLORFORMATNVPROC) (GLint size, GLenum type, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLFOGCOORDFORMATNVPROC) (GLenum type, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBFORMATNVPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBIFORMATNVPROC) (GLuint index, GLint size, GLenum type, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLGETINTEGERUI64I_VNVPROC) (GLenum value, GLuint index, GLuint64EXT *result);
typedef void (APIENTRYP RGLSYMGLVIEWPORTSWIZZLENVPROC) (GLuint index, GLenum swizzlex, GLenum swizzley, GLenum swizzlez, GLenum swizzlew);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint baseViewIndex, GLsizei numViews);

#define glCullFace __rglgen_glCullFace
#define glFrontFace __rglgen_glFrontFace
#define glHint __rglgen_glHint
#define glLineWidth __rglgen_glLineWidth
#define glPointSize __rglgen_glPointSize
#define glPolygonMode __rglgen_glPolygonMode
#define glScissor __rglgen_glScissor
#define glTexParameterf __rglgen_glTexParameterf
#define glTexParameterfv __rglgen_glTexParameterfv
#define glTexParameteri __rglgen_glTexParameteri
#define glTexParameteriv __rglgen_glTexParameteriv
#define glTexImage1D __rglgen_glTexImage1D
#define glTexImage2D __rglgen_glTexImage2D
#define glDrawBuffer __rglgen_glDrawBuffer
#define glClear __rglgen_glClear
#define glClearColor __rglgen_glClearColor
#define glClearStencil __rglgen_glClearStencil
#define glClearDepth __rglgen_glClearDepth
#define glStencilMask __rglgen_glStencilMask
#define glColorMask __rglgen_glColorMask
#define glDepthMask __rglgen_glDepthMask
#define glDisable __rglgen_glDisable
#define glEnable __rglgen_glEnable
#define glFinish __rglgen_glFinish
#define glFlush __rglgen_glFlush
#define glBlendFunc __rglgen_glBlendFunc
#define glLogicOp __rglgen_glLogicOp
#define glStencilFunc __rglgen_glStencilFunc
#define glStencilOp __rglgen_glStencilOp
#define glDepthFunc __rglgen_glDepthFunc
#define glPixelStoref __rglgen_glPixelStoref
#define glPixelStorei __rglgen_glPixelStorei
#define glReadBuffer __rglgen_glReadBuffer
#define glReadPixels __rglgen_glReadPixels
#define glGetBooleanv __rglgen_glGetBooleanv
#define glGetDoublev __rglgen_glGetDoublev
#define glGetError __rglgen_glGetError
#define glGetFloatv __rglgen_glGetFloatv
#define glGetIntegerv __rglgen_glGetIntegerv
#define glGetString __rglgen_glGetString
#define glGetTexImage __rglgen_glGetTexImage
#define glGetTexParameterfv __rglgen_glGetTexParameterfv
#define glGetTexParameteriv __rglgen_glGetTexParameteriv
#define glGetTexLevelParameterfv __rglgen_glGetTexLevelParameterfv
#define glGetTexLevelParameteriv __rglgen_glGetTexLevelParameteriv
#define glIsEnabled __rglgen_glIsEnabled
#define glDepthRange __rglgen_glDepthRange
#define glViewport __rglgen_glViewport
#define glDrawArrays __rglgen_glDrawArrays
#define glDrawElements __rglgen_glDrawElements
#define glGetPointerv __rglgen_glGetPointerv
#define glPolygonOffset __rglgen_glPolygonOffset
#define glCopyTexImage1D __rglgen_glCopyTexImage1D
#define glCopyTexImage2D __rglgen_glCopyTexImage2D
#define glCopyTexSubImage1D __rglgen_glCopyTexSubImage1D
#define glCopyTexSubImage2D __rglgen_glCopyTexSubImage2D
#define glTexSubImage1D __rglgen_glTexSubImage1D
#define glTexSubImage2D __rglgen_glTexSubImage2D
#define glBindTexture __rglgen_glBindTexture
#define glDeleteTextures __rglgen_glDeleteTextures
#define glGenTextures __rglgen_glGenTextures
#define glIsTexture __rglgen_glIsTexture
#define glDrawRangeElements __rglgen_glDrawRangeElements
#define glTexImage3D __rglgen_glTexImage3D
#define glTexSubImage3D __rglgen_glTexSubImage3D
#define glCopyTexSubImage3D __rglgen_glCopyTexSubImage3D
#define glActiveTexture __rglgen_glActiveTexture
#define glSampleCoverage __rglgen_glSampleCoverage
#define glCompressedTexImage3D __rglgen_glCompressedTexImage3D
#define glCompressedTexImage2D __rglgen_glCompressedTexImage2D
#define glCompressedTexImage1D __rglgen_glCompressedTexImage1D
#define glCompressedTexSubImage3D __rglgen_glCompressedTexSubImage3D
#define glCompressedTexSubImage2D __rglgen_glCompressedTexSubImage2D
#define glCompressedTexSubImage1D __rglgen_glCompressedTexSubImage1D
#define glGetCompressedTexImage __rglgen_glGetCompressedTexImage
#define glBlendFuncSeparate __rglgen_glBlendFuncSeparate
#define glMultiDrawArrays __rglgen_glMultiDrawArrays
#define glMultiDrawElements __rglgen_glMultiDrawElements
#define glPointParameterf __rglgen_glPointParameterf
#define glPointParameterfv __rglgen_glPointParameterfv
#define glPointParameteri __rglgen_glPointParameteri
#define glPointParameteriv __rglgen_glPointParameteriv
#define glBlendColor __rglgen_glBlendColor
#define glBlendEquation __rglgen_glBlendEquation
#define glGenQueries __rglgen_glGenQueries
#define glDeleteQueries __rglgen_glDeleteQueries
#define glIsQuery __rglgen_glIsQuery
#define glBeginQuery __rglgen_glBeginQuery
#define glEndQuery __rglgen_glEndQuery
#define glGetQueryiv __rglgen_glGetQueryiv
#define glGetQueryObjectiv __rglgen_glGetQueryObjectiv
#define glGetQueryObjectuiv __rglgen_glGetQueryObjectuiv
#define glBindBuffer __rglgen_glBindBuffer
#define glDeleteBuffers __rglgen_glDeleteBuffers
#define glGenBuffers __rglgen_glGenBuffers
#define glIsBuffer __rglgen_glIsBuffer
#define glBufferData __rglgen_glBufferData
#define glBufferSubData __rglgen_glBufferSubData
#define glGetBufferSubData __rglgen_glGetBufferSubData
#define glMapBuffer __rglgen_glMapBuffer
#define glUnmapBuffer __rglgen_glUnmapBuffer
#define glGetBufferParameteriv __rglgen_glGetBufferParameteriv
#define glGetBufferPointerv __rglgen_glGetBufferPointerv
#define glBlendEquationSeparate __rglgen_glBlendEquationSeparate
#define glDrawBuffers __rglgen_glDrawBuffers
#define glStencilOpSeparate __rglgen_glStencilOpSeparate
#define glStencilFuncSeparate __rglgen_glStencilFuncSeparate
#define glStencilMaskSeparate __rglgen_glStencilMaskSeparate
#define glAttachShader __rglgen_glAttachShader
#define glBindAttribLocation __rglgen_glBindAttribLocation
#define glCompileShader __rglgen_glCompileShader
#define glCreateProgram __rglgen_glCreateProgram
#define glCreateShader __rglgen_glCreateShader
#define glDeleteProgram __rglgen_glDeleteProgram
#define glDeleteShader __rglgen_glDeleteShader
#define glDetachShader __rglgen_glDetachShader
#define glDisableVertexAttribArray __rglgen_glDisableVertexAttribArray
#define glEnableVertexAttribArray __rglgen_glEnableVertexAttribArray
#define glGetActiveAttrib __rglgen_glGetActiveAttrib
#define glGetActiveUniform __rglgen_glGetActiveUniform
#define glGetAttachedShaders __rglgen_glGetAttachedShaders
#define glGetAttribLocation __rglgen_glGetAttribLocation
#define glGetProgramiv __rglgen_glGetProgramiv
#define glGetProgramInfoLog __rglgen_glGetProgramInfoLog
#define glGetShaderiv __rglgen_glGetShaderiv
#define glGetShaderInfoLog __rglgen_glGetShaderInfoLog
#define glGetShaderSource __rglgen_glGetShaderSource
#define glGetUniformLocation __rglgen_glGetUniformLocation
#define glGetUniformfv __rglgen_glGetUniformfv
#define glGetUniformiv __rglgen_glGetUniformiv
#define glGetVertexAttribdv __rglgen_glGetVertexAttribdv
#define glGetVertexAttribfv __rglgen_glGetVertexAttribfv
#define glGetVertexAttribiv __rglgen_glGetVertexAttribiv
#define glGetVertexAttribPointerv __rglgen_glGetVertexAttribPointerv
#define glIsProgram __rglgen_glIsProgram
#define glIsShader __rglgen_glIsShader
#define glLinkProgram __rglgen_glLinkProgram
#define glShaderSource __rglgen_glShaderSource
#define glUseProgram __rglgen_glUseProgram
#define glUniform1f __rglgen_glUniform1f
#define glUniform2f __rglgen_glUniform2f
#define glUniform3f __rglgen_glUniform3f
#define glUniform4f __rglgen_glUniform4f
#define glUniform1i __rglgen_glUniform1i
#define glUniform2i __rglgen_glUniform2i
#define glUniform3i __rglgen_glUniform3i
#define glUniform4i __rglgen_glUniform4i
#define glUniform1fv __rglgen_glUniform1fv
#define glUniform2fv __rglgen_glUniform2fv
#define glUniform3fv __rglgen_glUniform3fv
#define glUniform4fv __rglgen_glUniform4fv
#define glUniform1iv __rglgen_glUniform1iv
#define glUniform2iv __rglgen_glUniform2iv
#define glUniform3iv __rglgen_glUniform3iv
#define glUniform4iv __rglgen_glUniform4iv
#define glUniformMatrix2fv __rglgen_glUniformMatrix2fv
#define glUniformMatrix3fv __rglgen_glUniformMatrix3fv
#define glUniformMatrix4fv __rglgen_glUniformMatrix4fv
#define glValidateProgram __rglgen_glValidateProgram
#define glVertexAttrib1d __rglgen_glVertexAttrib1d
#define glVertexAttrib1dv __rglgen_glVertexAttrib1dv
#define glVertexAttrib1f __rglgen_glVertexAttrib1f
#define glVertexAttrib1fv __rglgen_glVertexAttrib1fv
#define glVertexAttrib1s __rglgen_glVertexAttrib1s
#define glVertexAttrib1sv __rglgen_glVertexAttrib1sv
#define glVertexAttrib2d __rglgen_glVertexAttrib2d
#define glVertexAttrib2dv __rglgen_glVertexAttrib2dv
#define glVertexAttrib2f __rglgen_glVertexAttrib2f
#define glVertexAttrib2fv __rglgen_glVertexAttrib2fv
#define glVertexAttrib2s __rglgen_glVertexAttrib2s
#define glVertexAttrib2sv __rglgen_glVertexAttrib2sv
#define glVertexAttrib3d __rglgen_glVertexAttrib3d
#define glVertexAttrib3dv __rglgen_glVertexAttrib3dv
#define glVertexAttrib3f __rglgen_glVertexAttrib3f
#define glVertexAttrib3fv __rglgen_glVertexAttrib3fv
#define glVertexAttrib3s __rglgen_glVertexAttrib3s
#define glVertexAttrib3sv __rglgen_glVertexAttrib3sv
#define glVertexAttrib4Nbv __rglgen_glVertexAttrib4Nbv
#define glVertexAttrib4Niv __rglgen_glVertexAttrib4Niv
#define glVertexAttrib4Nsv __rglgen_glVertexAttrib4Nsv
#define glVertexAttrib4Nub __rglgen_glVertexAttrib4Nub
#define glVertexAttrib4Nubv __rglgen_glVertexAttrib4Nubv
#define glVertexAttrib4Nuiv __rglgen_glVertexAttrib4Nuiv
#define glVertexAttrib4Nusv __rglgen_glVertexAttrib4Nusv
#define glVertexAttrib4bv __rglgen_glVertexAttrib4bv
#define glVertexAttrib4d __rglgen_glVertexAttrib4d
#define glVertexAttrib4dv __rglgen_glVertexAttrib4dv
#define glVertexAttrib4f __rglgen_glVertexAttrib4f
#define glVertexAttrib4fv __rglgen_glVertexAttrib4fv
#define glVertexAttrib4iv __rglgen_glVertexAttrib4iv
#define glVertexAttrib4s __rglgen_glVertexAttrib4s
#define glVertexAttrib4sv __rglgen_glVertexAttrib4sv
#define glVertexAttrib4ubv __rglgen_glVertexAttrib4ubv
#define glVertexAttrib4uiv __rglgen_glVertexAttrib4uiv
#define glVertexAttrib4usv __rglgen_glVertexAttrib4usv
#define glVertexAttribPointer __rglgen_glVertexAttribPointer
#define glUniformMatrix2x3fv __rglgen_glUniformMatrix2x3fv
#define glUniformMatrix3x2fv __rglgen_glUniformMatrix3x2fv
#define glUniformMatrix2x4fv __rglgen_glUniformMatrix2x4fv
#define glUniformMatrix4x2fv __rglgen_glUniformMatrix4x2fv
#define glUniformMatrix3x4fv __rglgen_glUniformMatrix3x4fv
#define glUniformMatrix4x3fv __rglgen_glUniformMatrix4x3fv
#define glColorMaski __rglgen_glColorMaski
#define glGetBooleani_v __rglgen_glGetBooleani_v
#define glGetIntegeri_v __rglgen_glGetIntegeri_v
#define glEnablei __rglgen_glEnablei
#define glDisablei __rglgen_glDisablei
#define glIsEnabledi __rglgen_glIsEnabledi
#define glBeginTransformFeedback __rglgen_glBeginTransformFeedback
#define glEndTransformFeedback __rglgen_glEndTransformFeedback
#define glBindBufferRange __rglgen_glBindBufferRange
#define glBindBufferBase __rglgen_glBindBufferBase
#define glTransformFeedbackVaryings __rglgen_glTransformFeedbackVaryings
#define glGetTransformFeedbackVarying __rglgen_glGetTransformFeedbackVarying
#define glClampColor __rglgen_glClampColor
#define glBeginConditionalRender __rglgen_glBeginConditionalRender
#define glEndConditionalRender __rglgen_glEndConditionalRender
#define glVertexAttribIPointer __rglgen_glVertexAttribIPointer
#define glGetVertexAttribIiv __rglgen_glGetVertexAttribIiv
#define glGetVertexAttribIuiv __rglgen_glGetVertexAttribIuiv
#define glVertexAttribI1i __rglgen_glVertexAttribI1i
#define glVertexAttribI2i __rglgen_glVertexAttribI2i
#define glVertexAttribI3i __rglgen_glVertexAttribI3i
#define glVertexAttribI4i __rglgen_glVertexAttribI4i
#define glVertexAttribI1ui __rglgen_glVertexAttribI1ui
#define glVertexAttribI2ui __rglgen_glVertexAttribI2ui
#define glVertexAttribI3ui __rglgen_glVertexAttribI3ui
#define glVertexAttribI4ui __rglgen_glVertexAttribI4ui
#define glVertexAttribI1iv __rglgen_glVertexAttribI1iv
#define glVertexAttribI2iv __rglgen_glVertexAttribI2iv
#define glVertexAttribI3iv __rglgen_glVertexAttribI3iv
#define glVertexAttribI4iv __rglgen_glVertexAttribI4iv
#define glVertexAttribI1uiv __rglgen_glVertexAttribI1uiv
#define glVertexAttribI2uiv __rglgen_glVertexAttribI2uiv
#define glVertexAttribI3uiv __rglgen_glVertexAttribI3uiv
#define glVertexAttribI4uiv __rglgen_glVertexAttribI4uiv
#define glVertexAttribI4bv __rglgen_glVertexAttribI4bv
#define glVertexAttribI4sv __rglgen_glVertexAttribI4sv
#define glVertexAttribI4ubv __rglgen_glVertexAttribI4ubv
#define glVertexAttribI4usv __rglgen_glVertexAttribI4usv
#define glGetUniformuiv __rglgen_glGetUniformuiv
#define glBindFragDataLocation __rglgen_glBindFragDataLocation
#define glGetFragDataLocation __rglgen_glGetFragDataLocation
#define glUniform1ui __rglgen_glUniform1ui
#define glUniform2ui __rglgen_glUniform2ui
#define glUniform3ui __rglgen_glUniform3ui
#define glUniform4ui __rglgen_glUniform4ui
#define glUniform1uiv __rglgen_glUniform1uiv
#define glUniform2uiv __rglgen_glUniform2uiv
#define glUniform3uiv __rglgen_glUniform3uiv
#define glUniform4uiv __rglgen_glUniform4uiv
#define glTexParameterIiv __rglgen_glTexParameterIiv
#define glTexParameterIuiv __rglgen_glTexParameterIuiv
#define glGetTexParameterIiv __rglgen_glGetTexParameterIiv
#define glGetTexParameterIuiv __rglgen_glGetTexParameterIuiv
#define glClearBufferiv __rglgen_glClearBufferiv
#define glClearBufferuiv __rglgen_glClearBufferuiv
#define glClearBufferfv __rglgen_glClearBufferfv
#define glClearBufferfi __rglgen_glClearBufferfi
#define glGetStringi __rglgen_glGetStringi
#define glIsRenderbuffer __rglgen_glIsRenderbuffer
#define glBindRenderbuffer __rglgen_glBindRenderbuffer
#define glDeleteRenderbuffers __rglgen_glDeleteRenderbuffers
#define glGenRenderbuffers __rglgen_glGenRenderbuffers
#define glRenderbufferStorage __rglgen_glRenderbufferStorage
#define glGetRenderbufferParameteriv __rglgen_glGetRenderbufferParameteriv
#define glIsFramebuffer __rglgen_glIsFramebuffer
#define glBindFramebuffer __rglgen_glBindFramebuffer
#define glDeleteFramebuffers __rglgen_glDeleteFramebuffers
#define glGenFramebuffers __rglgen_glGenFramebuffers
#define glCheckFramebufferStatus __rglgen_glCheckFramebufferStatus
#define glFramebufferTexture1D __rglgen_glFramebufferTexture1D
#define glFramebufferTexture2D __rglgen_glFramebufferTexture2D
#define glFramebufferTexture3D __rglgen_glFramebufferTexture3D
#define glFramebufferRenderbuffer __rglgen_glFramebufferRenderbuffer
#define glGetFramebufferAttachmentParameteriv __rglgen_glGetFramebufferAttachmentParameteriv
#define glGenerateMipmap __rglgen_glGenerateMipmap
#define glBlitFramebuffer __rglgen_glBlitFramebuffer
#define glRenderbufferStorageMultisample __rglgen_glRenderbufferStorageMultisample
#define glFramebufferTextureLayer __rglgen_glFramebufferTextureLayer
#define glMapBufferRange __rglgen_glMapBufferRange
#define glFlushMappedBufferRange __rglgen_glFlushMappedBufferRange
#define glBindVertexArray __rglgen_glBindVertexArray
#define glDeleteVertexArrays __rglgen_glDeleteVertexArrays
#define glGenVertexArrays __rglgen_glGenVertexArrays
#define glIsVertexArray __rglgen_glIsVertexArray
#define glDrawArraysInstanced __rglgen_glDrawArraysInstanced
#define glDrawElementsInstanced __rglgen_glDrawElementsInstanced
#define glTexBuffer __rglgen_glTexBuffer
#define glPrimitiveRestartIndex __rglgen_glPrimitiveRestartIndex
#define glCopyBufferSubData __rglgen_glCopyBufferSubData
#define glGetUniformIndices __rglgen_glGetUniformIndices
#define glGetActiveUniformsiv __rglgen_glGetActiveUniformsiv
#define glGetActiveUniformName __rglgen_glGetActiveUniformName
#define glGetUniformBlockIndex __rglgen_glGetUniformBlockIndex
#define glGetActiveUniformBlockiv __rglgen_glGetActiveUniformBlockiv
#define glGetActiveUniformBlockName __rglgen_glGetActiveUniformBlockName
#define glUniformBlockBinding __rglgen_glUniformBlockBinding
#define glDrawElementsBaseVertex __rglgen_glDrawElementsBaseVertex
#define glDrawRangeElementsBaseVertex __rglgen_glDrawRangeElementsBaseVertex
#define glDrawElementsInstancedBaseVertex __rglgen_glDrawElementsInstancedBaseVertex
#define glMultiDrawElementsBaseVertex __rglgen_glMultiDrawElementsBaseVertex
#define glProvokingVertex __rglgen_glProvokingVertex
#define glFenceSync __rglgen_glFenceSync
#define glIsSync __rglgen_glIsSync
#define glDeleteSync __rglgen_glDeleteSync
#define glClientWaitSync __rglgen_glClientWaitSync
#define glWaitSync __rglgen_glWaitSync
#define glGetInteger64v __rglgen_glGetInteger64v
#define glGetSynciv __rglgen_glGetSynciv
#define glGetInteger64i_v __rglgen_glGetInteger64i_v
#define glGetBufferParameteri64v __rglgen_glGetBufferParameteri64v
#define glFramebufferTexture __rglgen_glFramebufferTexture
#define glTexImage2DMultisample __rglgen_glTexImage2DMultisample
#define glTexImage3DMultisample __rglgen_glTexImage3DMultisample
#define glGetMultisamplefv __rglgen_glGetMultisamplefv
#define glSampleMaski __rglgen_glSampleMaski
#define glBindFragDataLocationIndexed __rglgen_glBindFragDataLocationIndexed
#define glGetFragDataIndex __rglgen_glGetFragDataIndex
#define glGenSamplers __rglgen_glGenSamplers
#define glDeleteSamplers __rglgen_glDeleteSamplers
#define glIsSampler __rglgen_glIsSampler
#define glBindSampler __rglgen_glBindSampler
#define glSamplerParameteri __rglgen_glSamplerParameteri
#define glSamplerParameteriv __rglgen_glSamplerParameteriv
#define glSamplerParameterf __rglgen_glSamplerParameterf
#define glSamplerParameterfv __rglgen_glSamplerParameterfv
#define glSamplerParameterIiv __rglgen_glSamplerParameterIiv
#define glSamplerParameterIuiv __rglgen_glSamplerParameterIuiv
#define glGetSamplerParameteriv __rglgen_glGetSamplerParameteriv
#define glGetSamplerParameterIiv __rglgen_glGetSamplerParameterIiv
#define glGetSamplerParameterfv __rglgen_glGetSamplerParameterfv
#define glGetSamplerParameterIuiv __rglgen_glGetSamplerParameterIuiv
#define glQueryCounter __rglgen_glQueryCounter
#define glGetQueryObjecti64v __rglgen_glGetQueryObjecti64v
#define glGetQueryObjectui64v __rglgen_glGetQueryObjectui64v
#define glVertexAttribDivisor __rglgen_glVertexAttribDivisor
#define glVertexAttribP1ui __rglgen_glVertexAttribP1ui
#define glVertexAttribP1uiv __rglgen_glVertexAttribP1uiv
#define glVertexAttribP2ui __rglgen_glVertexAttribP2ui
#define glVertexAttribP2uiv __rglgen_glVertexAttribP2uiv
#define glVertexAttribP3ui __rglgen_glVertexAttribP3ui
#define glVertexAttribP3uiv __rglgen_glVertexAttribP3uiv
#define glVertexAttribP4ui __rglgen_glVertexAttribP4ui
#define glVertexAttribP4uiv __rglgen_glVertexAttribP4uiv
#define glMinSampleShading __rglgen_glMinSampleShading
#define glBlendEquationi __rglgen_glBlendEquationi
#define glBlendEquationSeparatei __rglgen_glBlendEquationSeparatei
#define glBlendFunci __rglgen_glBlendFunci
#define glBlendFuncSeparatei __rglgen_glBlendFuncSeparatei
#define glDrawArraysIndirect __rglgen_glDrawArraysIndirect
#define glDrawElementsIndirect __rglgen_glDrawElementsIndirect
#define glUniform1d __rglgen_glUniform1d
#define glUniform2d __rglgen_glUniform2d
#define glUniform3d __rglgen_glUniform3d
#define glUniform4d __rglgen_glUniform4d
#define glUniform1dv __rglgen_glUniform1dv
#define glUniform2dv __rglgen_glUniform2dv
#define glUniform3dv __rglgen_glUniform3dv
#define glUniform4dv __rglgen_glUniform4dv
#define glUniformMatrix2dv __rglgen_glUniformMatrix2dv
#define glUniformMatrix3dv __rglgen_glUniformMatrix3dv
#define glUniformMatrix4dv __rglgen_glUniformMatrix4dv
#define glUniformMatrix2x3dv __rglgen_glUniformMatrix2x3dv
#define glUniformMatrix2x4dv __rglgen_glUniformMatrix2x4dv
#define glUniformMatrix3x2dv __rglgen_glUniformMatrix3x2dv
#define glUniformMatrix3x4dv __rglgen_glUniformMatrix3x4dv
#define glUniformMatrix4x2dv __rglgen_glUniformMatrix4x2dv
#define glUniformMatrix4x3dv __rglgen_glUniformMatrix4x3dv
#define glGetUniformdv __rglgen_glGetUniformdv
#define glGetSubroutineUniformLocation __rglgen_glGetSubroutineUniformLocation
#define glGetSubroutineIndex __rglgen_glGetSubroutineIndex
#define glGetActiveSubroutineUniformiv __rglgen_glGetActiveSubroutineUniformiv
#define glGetActiveSubroutineUniformName __rglgen_glGetActiveSubroutineUniformName
#define glGetActiveSubroutineName __rglgen_glGetActiveSubroutineName
#define glUniformSubroutinesuiv __rglgen_glUniformSubroutinesuiv
#define glGetUniformSubroutineuiv __rglgen_glGetUniformSubroutineuiv
#define glGetProgramStageiv __rglgen_glGetProgramStageiv
#define glPatchParameteri __rglgen_glPatchParameteri
#define glPatchParameterfv __rglgen_glPatchParameterfv
#define glBindTransformFeedback __rglgen_glBindTransformFeedback
#define glDeleteTransformFeedbacks __rglgen_glDeleteTransformFeedbacks
#define glGenTransformFeedbacks __rglgen_glGenTransformFeedbacks
#define glIsTransformFeedback __rglgen_glIsTransformFeedback
#define glPauseTransformFeedback __rglgen_glPauseTransformFeedback
#define glResumeTransformFeedback __rglgen_glResumeTransformFeedback
#define glDrawTransformFeedback __rglgen_glDrawTransformFeedback
#define glDrawTransformFeedbackStream __rglgen_glDrawTransformFeedbackStream
#define glBeginQueryIndexed __rglgen_glBeginQueryIndexed
#define glEndQueryIndexed __rglgen_glEndQueryIndexed
#define glGetQueryIndexediv __rglgen_glGetQueryIndexediv
#define glReleaseShaderCompiler __rglgen_glReleaseShaderCompiler
#define glShaderBinary __rglgen_glShaderBinary
#define glGetShaderPrecisionFormat __rglgen_glGetShaderPrecisionFormat
#define glDepthRangef __rglgen_glDepthRangef
#define glClearDepthf __rglgen_glClearDepthf
#define glGetProgramBinary __rglgen_glGetProgramBinary
#define glProgramBinary __rglgen_glProgramBinary
#define glProgramParameteri __rglgen_glProgramParameteri
#define glUseProgramStages __rglgen_glUseProgramStages
#define glActiveShaderProgram __rglgen_glActiveShaderProgram
#define glCreateShaderProgramv __rglgen_glCreateShaderProgramv
#define glBindProgramPipeline __rglgen_glBindProgramPipeline
#define glDeleteProgramPipelines __rglgen_glDeleteProgramPipelines
#define glGenProgramPipelines __rglgen_glGenProgramPipelines
#define glIsProgramPipeline __rglgen_glIsProgramPipeline
#define glGetProgramPipelineiv __rglgen_glGetProgramPipelineiv
#define glProgramUniform1i __rglgen_glProgramUniform1i
#define glProgramUniform1iv __rglgen_glProgramUniform1iv
#define glProgramUniform1f __rglgen_glProgramUniform1f
#define glProgramUniform1fv __rglgen_glProgramUniform1fv
#define glProgramUniform1d __rglgen_glProgramUniform1d
#define glProgramUniform1dv __rglgen_glProgramUniform1dv
#define glProgramUniform1ui __rglgen_glProgramUniform1ui
#define glProgramUniform1uiv __rglgen_glProgramUniform1uiv
#define glProgramUniform2i __rglgen_glProgramUniform2i
#define glProgramUniform2iv __rglgen_glProgramUniform2iv
#define glProgramUniform2f __rglgen_glProgramUniform2f
#define glProgramUniform2fv __rglgen_glProgramUniform2fv
#define glProgramUniform2d __rglgen_glProgramUniform2d
#define glProgramUniform2dv __rglgen_glProgramUniform2dv
#define glProgramUniform2ui __rglgen_glProgramUniform2ui
#define glProgramUniform2uiv __rglgen_glProgramUniform2uiv
#define glProgramUniform3i __rglgen_glProgramUniform3i
#define glProgramUniform3iv __rglgen_glProgramUniform3iv
#define glProgramUniform3f __rglgen_glProgramUniform3f
#define glProgramUniform3fv __rglgen_glProgramUniform3fv
#define glProgramUniform3d __rglgen_glProgramUniform3d
#define glProgramUniform3dv __rglgen_glProgramUniform3dv
#define glProgramUniform3ui __rglgen_glProgramUniform3ui
#define glProgramUniform3uiv __rglgen_glProgramUniform3uiv
#define glProgramUniform4i __rglgen_glProgramUniform4i
#define glProgramUniform4iv __rglgen_glProgramUniform4iv
#define glProgramUniform4f __rglgen_glProgramUniform4f
#define glProgramUniform4fv __rglgen_glProgramUniform4fv
#define glProgramUniform4d __rglgen_glProgramUniform4d
#define glProgramUniform4dv __rglgen_glProgramUniform4dv
#define glProgramUniform4ui __rglgen_glProgramUniform4ui
#define glProgramUniform4uiv __rglgen_glProgramUniform4uiv
#define glProgramUniformMatrix2fv __rglgen_glProgramUniformMatrix2fv
#define glProgramUniformMatrix3fv __rglgen_glProgramUniformMatrix3fv
#define glProgramUniformMatrix4fv __rglgen_glProgramUniformMatrix4fv
#define glProgramUniformMatrix2dv __rglgen_glProgramUniformMatrix2dv
#define glProgramUniformMatrix3dv __rglgen_glProgramUniformMatrix3dv
#define glProgramUniformMatrix4dv __rglgen_glProgramUniformMatrix4dv
#define glProgramUniformMatrix2x3fv __rglgen_glProgramUniformMatrix2x3fv
#define glProgramUniformMatrix3x2fv __rglgen_glProgramUniformMatrix3x2fv
#define glProgramUniformMatrix2x4fv __rglgen_glProgramUniformMatrix2x4fv
#define glProgramUniformMatrix4x2fv __rglgen_glProgramUniformMatrix4x2fv
#define glProgramUniformMatrix3x4fv __rglgen_glProgramUniformMatrix3x4fv
#define glProgramUniformMatrix4x3fv __rglgen_glProgramUniformMatrix4x3fv
#define glProgramUniformMatrix2x3dv __rglgen_glProgramUniformMatrix2x3dv
#define glProgramUniformMatrix3x2dv __rglgen_glProgramUniformMatrix3x2dv
#define glProgramUniformMatrix2x4dv __rglgen_glProgramUniformMatrix2x4dv
#define glProgramUniformMatrix4x2dv __rglgen_glProgramUniformMatrix4x2dv
#define glProgramUniformMatrix3x4dv __rglgen_glProgramUniformMatrix3x4dv
#define glProgramUniformMatrix4x3dv __rglgen_glProgramUniformMatrix4x3dv
#define glValidateProgramPipeline __rglgen_glValidateProgramPipeline
#define glGetProgramPipelineInfoLog __rglgen_glGetProgramPipelineInfoLog
#define glVertexAttribL1d __rglgen_glVertexAttribL1d
#define glVertexAttribL2d __rglgen_glVertexAttribL2d
#define glVertexAttribL3d __rglgen_glVertexAttribL3d
#define glVertexAttribL4d __rglgen_glVertexAttribL4d
#define glVertexAttribL1dv __rglgen_glVertexAttribL1dv
#define glVertexAttribL2dv __rglgen_glVertexAttribL2dv
#define glVertexAttribL3dv __rglgen_glVertexAttribL3dv
#define glVertexAttribL4dv __rglgen_glVertexAttribL4dv
#define glVertexAttribLPointer __rglgen_glVertexAttribLPointer
#define glGetVertexAttribLdv __rglgen_glGetVertexAttribLdv
#define glViewportArrayv __rglgen_glViewportArrayv
#define glViewportIndexedf __rglgen_glViewportIndexedf
#define glViewportIndexedfv __rglgen_glViewportIndexedfv
#define glScissorArrayv __rglgen_glScissorArrayv
#define glScissorIndexed __rglgen_glScissorIndexed
#define glScissorIndexedv __rglgen_glScissorIndexedv
#define glDepthRangeArrayv __rglgen_glDepthRangeArrayv
#define glDepthRangeIndexed __rglgen_glDepthRangeIndexed
#define glGetFloati_v __rglgen_glGetFloati_v
#define glGetDoublei_v __rglgen_glGetDoublei_v
#define glDrawArraysInstancedBaseInstance __rglgen_glDrawArraysInstancedBaseInstance
#define glDrawElementsInstancedBaseInstance __rglgen_glDrawElementsInstancedBaseInstance
#define glDrawElementsInstancedBaseVertexBaseInstance __rglgen_glDrawElementsInstancedBaseVertexBaseInstance
#define glGetInternalformativ __rglgen_glGetInternalformativ
#define glGetActiveAtomicCounterBufferiv __rglgen_glGetActiveAtomicCounterBufferiv
#define glBindImageTexture __rglgen_glBindImageTexture
#define glMemoryBarrier __rglgen_glMemoryBarrier
#define glTexStorage1D __rglgen_glTexStorage1D
#define glTexStorage2D __rglgen_glTexStorage2D
#define glTexStorage3D __rglgen_glTexStorage3D
#define glDrawTransformFeedbackInstanced __rglgen_glDrawTransformFeedbackInstanced
#define glDrawTransformFeedbackStreamInstanced __rglgen_glDrawTransformFeedbackStreamInstanced
#define glClearBufferData __rglgen_glClearBufferData
#define glClearBufferSubData __rglgen_glClearBufferSubData
#define glDispatchCompute __rglgen_glDispatchCompute
#define glDispatchComputeIndirect __rglgen_glDispatchComputeIndirect
#define glCopyImageSubData __rglgen_glCopyImageSubData
#define glFramebufferParameteri __rglgen_glFramebufferParameteri
#define glGetFramebufferParameteriv __rglgen_glGetFramebufferParameteriv
#define glGetInternalformati64v __rglgen_glGetInternalformati64v
#define glInvalidateTexSubImage __rglgen_glInvalidateTexSubImage
#define glInvalidateTexImage __rglgen_glInvalidateTexImage
#define glInvalidateBufferSubData __rglgen_glInvalidateBufferSubData
#define glInvalidateBufferData __rglgen_glInvalidateBufferData
#define glInvalidateFramebuffer __rglgen_glInvalidateFramebuffer
#define glInvalidateSubFramebuffer __rglgen_glInvalidateSubFramebuffer
#define glMultiDrawArraysIndirect __rglgen_glMultiDrawArraysIndirect
#define glMultiDrawElementsIndirect __rglgen_glMultiDrawElementsIndirect
#define glGetProgramInterfaceiv __rglgen_glGetProgramInterfaceiv
#define glGetProgramResourceIndex __rglgen_glGetProgramResourceIndex
#define glGetProgramResourceName __rglgen_glGetProgramResourceName
#define glGetProgramResourceiv __rglgen_glGetProgramResourceiv
#define glGetProgramResourceLocation __rglgen_glGetProgramResourceLocation
#define glGetProgramResourceLocationIndex __rglgen_glGetProgramResourceLocationIndex
#define glShaderStorageBlockBinding __rglgen_glShaderStorageBlockBinding
#define glTexBufferRange __rglgen_glTexBufferRange
#define glTexStorage2DMultisample __rglgen_glTexStorage2DMultisample
#define glTexStorage3DMultisample __rglgen_glTexStorage3DMultisample
#define glTextureView __rglgen_glTextureView
#define glBindVertexBuffer __rglgen_glBindVertexBuffer
#define glVertexAttribFormat __rglgen_glVertexAttribFormat
#define glVertexAttribIFormat __rglgen_glVertexAttribIFormat
#define glVertexAttribLFormat __rglgen_glVertexAttribLFormat
#define glVertexAttribBinding __rglgen_glVertexAttribBinding
#define glVertexBindingDivisor __rglgen_glVertexBindingDivisor
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
#define glBufferStorage __rglgen_glBufferStorage
#define glClearTexImage __rglgen_glClearTexImage
#define glClearTexSubImage __rglgen_glClearTexSubImage
#define glBindBuffersBase __rglgen_glBindBuffersBase
#define glBindBuffersRange __rglgen_glBindBuffersRange
#define glBindTextures __rglgen_glBindTextures
#define glBindSamplers __rglgen_glBindSamplers
#define glBindImageTextures __rglgen_glBindImageTextures
#define glBindVertexBuffers __rglgen_glBindVertexBuffers
#define glClipControl __rglgen_glClipControl
#define glCreateTransformFeedbacks __rglgen_glCreateTransformFeedbacks
#define glTransformFeedbackBufferBase __rglgen_glTransformFeedbackBufferBase
#define glTransformFeedbackBufferRange __rglgen_glTransformFeedbackBufferRange
#define glGetTransformFeedbackiv __rglgen_glGetTransformFeedbackiv
#define glGetTransformFeedbacki_v __rglgen_glGetTransformFeedbacki_v
#define glGetTransformFeedbacki64_v __rglgen_glGetTransformFeedbacki64_v
#define glCreateBuffers __rglgen_glCreateBuffers
#define glNamedBufferStorage __rglgen_glNamedBufferStorage
#define glNamedBufferData __rglgen_glNamedBufferData
#define glNamedBufferSubData __rglgen_glNamedBufferSubData
#define glCopyNamedBufferSubData __rglgen_glCopyNamedBufferSubData
#define glClearNamedBufferData __rglgen_glClearNamedBufferData
#define glClearNamedBufferSubData __rglgen_glClearNamedBufferSubData
#define glMapNamedBuffer __rglgen_glMapNamedBuffer
#define glMapNamedBufferRange __rglgen_glMapNamedBufferRange
#define glUnmapNamedBuffer __rglgen_glUnmapNamedBuffer
#define glFlushMappedNamedBufferRange __rglgen_glFlushMappedNamedBufferRange
#define glGetNamedBufferParameteriv __rglgen_glGetNamedBufferParameteriv
#define glGetNamedBufferParameteri64v __rglgen_glGetNamedBufferParameteri64v
#define glGetNamedBufferPointerv __rglgen_glGetNamedBufferPointerv
#define glGetNamedBufferSubData __rglgen_glGetNamedBufferSubData
#define glCreateFramebuffers __rglgen_glCreateFramebuffers
#define glNamedFramebufferRenderbuffer __rglgen_glNamedFramebufferRenderbuffer
#define glNamedFramebufferParameteri __rglgen_glNamedFramebufferParameteri
#define glNamedFramebufferTexture __rglgen_glNamedFramebufferTexture
#define glNamedFramebufferTextureLayer __rglgen_glNamedFramebufferTextureLayer
#define glNamedFramebufferDrawBuffer __rglgen_glNamedFramebufferDrawBuffer
#define glNamedFramebufferDrawBuffers __rglgen_glNamedFramebufferDrawBuffers
#define glNamedFramebufferReadBuffer __rglgen_glNamedFramebufferReadBuffer
#define glInvalidateNamedFramebufferData __rglgen_glInvalidateNamedFramebufferData
#define glInvalidateNamedFramebufferSubData __rglgen_glInvalidateNamedFramebufferSubData
#define glClearNamedFramebufferiv __rglgen_glClearNamedFramebufferiv
#define glClearNamedFramebufferuiv __rglgen_glClearNamedFramebufferuiv
#define glClearNamedFramebufferfv __rglgen_glClearNamedFramebufferfv
#define glClearNamedFramebufferfi __rglgen_glClearNamedFramebufferfi
#define glBlitNamedFramebuffer __rglgen_glBlitNamedFramebuffer
#define glCheckNamedFramebufferStatus __rglgen_glCheckNamedFramebufferStatus
#define glGetNamedFramebufferParameteriv __rglgen_glGetNamedFramebufferParameteriv
#define glGetNamedFramebufferAttachmentParameteriv __rglgen_glGetNamedFramebufferAttachmentParameteriv
#define glCreateRenderbuffers __rglgen_glCreateRenderbuffers
#define glNamedRenderbufferStorage __rglgen_glNamedRenderbufferStorage
#define glNamedRenderbufferStorageMultisample __rglgen_glNamedRenderbufferStorageMultisample
#define glGetNamedRenderbufferParameteriv __rglgen_glGetNamedRenderbufferParameteriv
#define glCreateTextures __rglgen_glCreateTextures
#define glTextureBuffer __rglgen_glTextureBuffer
#define glTextureBufferRange __rglgen_glTextureBufferRange
#define glTextureStorage1D __rglgen_glTextureStorage1D
#define glTextureStorage2D __rglgen_glTextureStorage2D
#define glTextureStorage3D __rglgen_glTextureStorage3D
#define glTextureStorage2DMultisample __rglgen_glTextureStorage2DMultisample
#define glTextureStorage3DMultisample __rglgen_glTextureStorage3DMultisample
#define glTextureSubImage1D __rglgen_glTextureSubImage1D
#define glTextureSubImage2D __rglgen_glTextureSubImage2D
#define glTextureSubImage3D __rglgen_glTextureSubImage3D
#define glCompressedTextureSubImage1D __rglgen_glCompressedTextureSubImage1D
#define glCompressedTextureSubImage2D __rglgen_glCompressedTextureSubImage2D
#define glCompressedTextureSubImage3D __rglgen_glCompressedTextureSubImage3D
#define glCopyTextureSubImage1D __rglgen_glCopyTextureSubImage1D
#define glCopyTextureSubImage2D __rglgen_glCopyTextureSubImage2D
#define glCopyTextureSubImage3D __rglgen_glCopyTextureSubImage3D
#define glTextureParameterf __rglgen_glTextureParameterf
#define glTextureParameterfv __rglgen_glTextureParameterfv
#define glTextureParameteri __rglgen_glTextureParameteri
#define glTextureParameterIiv __rglgen_glTextureParameterIiv
#define glTextureParameterIuiv __rglgen_glTextureParameterIuiv
#define glTextureParameteriv __rglgen_glTextureParameteriv
#define glGenerateTextureMipmap __rglgen_glGenerateTextureMipmap
#define glBindTextureUnit __rglgen_glBindTextureUnit
#define glGetTextureImage __rglgen_glGetTextureImage
#define glGetCompressedTextureImage __rglgen_glGetCompressedTextureImage
#define glGetTextureLevelParameterfv __rglgen_glGetTextureLevelParameterfv
#define glGetTextureLevelParameteriv __rglgen_glGetTextureLevelParameteriv
#define glGetTextureParameterfv __rglgen_glGetTextureParameterfv
#define glGetTextureParameterIiv __rglgen_glGetTextureParameterIiv
#define glGetTextureParameterIuiv __rglgen_glGetTextureParameterIuiv
#define glGetTextureParameteriv __rglgen_glGetTextureParameteriv
#define glCreateVertexArrays __rglgen_glCreateVertexArrays
#define glDisableVertexArrayAttrib __rglgen_glDisableVertexArrayAttrib
#define glEnableVertexArrayAttrib __rglgen_glEnableVertexArrayAttrib
#define glVertexArrayElementBuffer __rglgen_glVertexArrayElementBuffer
#define glVertexArrayVertexBuffer __rglgen_glVertexArrayVertexBuffer
#define glVertexArrayVertexBuffers __rglgen_glVertexArrayVertexBuffers
#define glVertexArrayAttribBinding __rglgen_glVertexArrayAttribBinding
#define glVertexArrayAttribFormat __rglgen_glVertexArrayAttribFormat
#define glVertexArrayAttribIFormat __rglgen_glVertexArrayAttribIFormat
#define glVertexArrayAttribLFormat __rglgen_glVertexArrayAttribLFormat
#define glVertexArrayBindingDivisor __rglgen_glVertexArrayBindingDivisor
#define glGetVertexArrayiv __rglgen_glGetVertexArrayiv
#define glGetVertexArrayIndexediv __rglgen_glGetVertexArrayIndexediv
#define glGetVertexArrayIndexed64iv __rglgen_glGetVertexArrayIndexed64iv
#define glCreateSamplers __rglgen_glCreateSamplers
#define glCreateProgramPipelines __rglgen_glCreateProgramPipelines
#define glCreateQueries __rglgen_glCreateQueries
#define glGetQueryBufferObjecti64v __rglgen_glGetQueryBufferObjecti64v
#define glGetQueryBufferObjectiv __rglgen_glGetQueryBufferObjectiv
#define glGetQueryBufferObjectui64v __rglgen_glGetQueryBufferObjectui64v
#define glGetQueryBufferObjectuiv __rglgen_glGetQueryBufferObjectuiv
#define glMemoryBarrierByRegion __rglgen_glMemoryBarrierByRegion
#define glGetTextureSubImage __rglgen_glGetTextureSubImage
#define glGetCompressedTextureSubImage __rglgen_glGetCompressedTextureSubImage
#define glGetGraphicsResetStatus __rglgen_glGetGraphicsResetStatus
#define glGetnCompressedTexImage __rglgen_glGetnCompressedTexImage
#define glGetnTexImage __rglgen_glGetnTexImage
#define glGetnUniformdv __rglgen_glGetnUniformdv
#define glGetnUniformfv __rglgen_glGetnUniformfv
#define glGetnUniformiv __rglgen_glGetnUniformiv
#define glGetnUniformuiv __rglgen_glGetnUniformuiv
#define glReadnPixels __rglgen_glReadnPixels
#define glTextureBarrier __rglgen_glTextureBarrier
#define glSpecializeShader __rglgen_glSpecializeShader
#define glMultiDrawArraysIndirectCount __rglgen_glMultiDrawArraysIndirectCount
#define glMultiDrawElementsIndirectCount __rglgen_glMultiDrawElementsIndirectCount
#define glPolygonOffsetClamp __rglgen_glPolygonOffsetClamp
#define glPrimitiveBoundingBoxARB __rglgen_glPrimitiveBoundingBoxARB
#define glGetTextureHandleARB __rglgen_glGetTextureHandleARB
#define glGetTextureSamplerHandleARB __rglgen_glGetTextureSamplerHandleARB
#define glMakeTextureHandleResidentARB __rglgen_glMakeTextureHandleResidentARB
#define glMakeTextureHandleNonResidentARB __rglgen_glMakeTextureHandleNonResidentARB
#define glGetImageHandleARB __rglgen_glGetImageHandleARB
#define glMakeImageHandleResidentARB __rglgen_glMakeImageHandleResidentARB
#define glMakeImageHandleNonResidentARB __rglgen_glMakeImageHandleNonResidentARB
#define glUniformHandleui64ARB __rglgen_glUniformHandleui64ARB
#define glUniformHandleui64vARB __rglgen_glUniformHandleui64vARB
#define glProgramUniformHandleui64ARB __rglgen_glProgramUniformHandleui64ARB
#define glProgramUniformHandleui64vARB __rglgen_glProgramUniformHandleui64vARB
#define glIsTextureHandleResidentARB __rglgen_glIsTextureHandleResidentARB
#define glIsImageHandleResidentARB __rglgen_glIsImageHandleResidentARB
#define glVertexAttribL1ui64ARB __rglgen_glVertexAttribL1ui64ARB
#define glVertexAttribL1ui64vARB __rglgen_glVertexAttribL1ui64vARB
#define glGetVertexAttribLui64vARB __rglgen_glGetVertexAttribLui64vARB
#define glCreateSyncFromCLeventARB __rglgen_glCreateSyncFromCLeventARB
#define glDispatchComputeGroupSizeARB __rglgen_glDispatchComputeGroupSizeARB
#define glDebugMessageControlARB __rglgen_glDebugMessageControlARB
#define glDebugMessageInsertARB __rglgen_glDebugMessageInsertARB
#define glDebugMessageCallbackARB __rglgen_glDebugMessageCallbackARB
#define glGetDebugMessageLogARB __rglgen_glGetDebugMessageLogARB
#define glBlendEquationiARB __rglgen_glBlendEquationiARB
#define glBlendEquationSeparateiARB __rglgen_glBlendEquationSeparateiARB
#define glBlendFunciARB __rglgen_glBlendFunciARB
#define glBlendFuncSeparateiARB __rglgen_glBlendFuncSeparateiARB
#define glDrawArraysInstancedARB __rglgen_glDrawArraysInstancedARB
#define glDrawElementsInstancedARB __rglgen_glDrawElementsInstancedARB
#define glProgramParameteriARB __rglgen_glProgramParameteriARB
#define glFramebufferTextureARB __rglgen_glFramebufferTextureARB
#define glFramebufferTextureLayerARB __rglgen_glFramebufferTextureLayerARB
#define glFramebufferTextureFaceARB __rglgen_glFramebufferTextureFaceARB
#define glSpecializeShaderARB __rglgen_glSpecializeShaderARB
#define glUniform1i64ARB __rglgen_glUniform1i64ARB
#define glUniform2i64ARB __rglgen_glUniform2i64ARB
#define glUniform3i64ARB __rglgen_glUniform3i64ARB
#define glUniform4i64ARB __rglgen_glUniform4i64ARB
#define glUniform1i64vARB __rglgen_glUniform1i64vARB
#define glUniform2i64vARB __rglgen_glUniform2i64vARB
#define glUniform3i64vARB __rglgen_glUniform3i64vARB
#define glUniform4i64vARB __rglgen_glUniform4i64vARB
#define glUniform1ui64ARB __rglgen_glUniform1ui64ARB
#define glUniform2ui64ARB __rglgen_glUniform2ui64ARB
#define glUniform3ui64ARB __rglgen_glUniform3ui64ARB
#define glUniform4ui64ARB __rglgen_glUniform4ui64ARB
#define glUniform1ui64vARB __rglgen_glUniform1ui64vARB
#define glUniform2ui64vARB __rglgen_glUniform2ui64vARB
#define glUniform3ui64vARB __rglgen_glUniform3ui64vARB
#define glUniform4ui64vARB __rglgen_glUniform4ui64vARB
#define glGetUniformi64vARB __rglgen_glGetUniformi64vARB
#define glGetUniformui64vARB __rglgen_glGetUniformui64vARB
#define glGetnUniformi64vARB __rglgen_glGetnUniformi64vARB
#define glGetnUniformui64vARB __rglgen_glGetnUniformui64vARB
#define glProgramUniform1i64ARB __rglgen_glProgramUniform1i64ARB
#define glProgramUniform2i64ARB __rglgen_glProgramUniform2i64ARB
#define glProgramUniform3i64ARB __rglgen_glProgramUniform3i64ARB
#define glProgramUniform4i64ARB __rglgen_glProgramUniform4i64ARB
#define glProgramUniform1i64vARB __rglgen_glProgramUniform1i64vARB
#define glProgramUniform2i64vARB __rglgen_glProgramUniform2i64vARB
#define glProgramUniform3i64vARB __rglgen_glProgramUniform3i64vARB
#define glProgramUniform4i64vARB __rglgen_glProgramUniform4i64vARB
#define glProgramUniform1ui64ARB __rglgen_glProgramUniform1ui64ARB
#define glProgramUniform2ui64ARB __rglgen_glProgramUniform2ui64ARB
#define glProgramUniform3ui64ARB __rglgen_glProgramUniform3ui64ARB
#define glProgramUniform4ui64ARB __rglgen_glProgramUniform4ui64ARB
#define glProgramUniform1ui64vARB __rglgen_glProgramUniform1ui64vARB
#define glProgramUniform2ui64vARB __rglgen_glProgramUniform2ui64vARB
#define glProgramUniform3ui64vARB __rglgen_glProgramUniform3ui64vARB
#define glProgramUniform4ui64vARB __rglgen_glProgramUniform4ui64vARB
#define glMultiDrawArraysIndirectCountARB __rglgen_glMultiDrawArraysIndirectCountARB
#define glMultiDrawElementsIndirectCountARB __rglgen_glMultiDrawElementsIndirectCountARB
#define glVertexAttribDivisorARB __rglgen_glVertexAttribDivisorARB
#define glMaxShaderCompilerThreadsARB __rglgen_glMaxShaderCompilerThreadsARB
#define glGetGraphicsResetStatusARB __rglgen_glGetGraphicsResetStatusARB
#define glGetnTexImageARB __rglgen_glGetnTexImageARB
#define glReadnPixelsARB __rglgen_glReadnPixelsARB
#define glGetnCompressedTexImageARB __rglgen_glGetnCompressedTexImageARB
#define glGetnUniformfvARB __rglgen_glGetnUniformfvARB
#define glGetnUniformivARB __rglgen_glGetnUniformivARB
#define glGetnUniformuivARB __rglgen_glGetnUniformuivARB
#define glGetnUniformdvARB __rglgen_glGetnUniformdvARB
#define glFramebufferSampleLocationsfvARB __rglgen_glFramebufferSampleLocationsfvARB
#define glNamedFramebufferSampleLocationsfvARB __rglgen_glNamedFramebufferSampleLocationsfvARB
#define glEvaluateDepthValuesARB __rglgen_glEvaluateDepthValuesARB
#define glMinSampleShadingARB __rglgen_glMinSampleShadingARB
#define glNamedStringARB __rglgen_glNamedStringARB
#define glDeleteNamedStringARB __rglgen_glDeleteNamedStringARB
#define glCompileShaderIncludeARB __rglgen_glCompileShaderIncludeARB
#define glIsNamedStringARB __rglgen_glIsNamedStringARB
#define glGetNamedStringARB __rglgen_glGetNamedStringARB
#define glGetNamedStringivARB __rglgen_glGetNamedStringivARB
#define glBufferPageCommitmentARB __rglgen_glBufferPageCommitmentARB
#define glNamedBufferPageCommitmentEXT __rglgen_glNamedBufferPageCommitmentEXT
#define glNamedBufferPageCommitmentARB __rglgen_glNamedBufferPageCommitmentARB
#define glTexPageCommitmentARB __rglgen_glTexPageCommitmentARB
#define glTexBufferARB __rglgen_glTexBufferARB
#define glBlendBarrierKHR __rglgen_glBlendBarrierKHR
#define glMaxShaderCompilerThreadsKHR __rglgen_glMaxShaderCompilerThreadsKHR
#define glEGLImageTargetTexStorageEXT __rglgen_glEGLImageTargetTexStorageEXT
#define glEGLImageTargetTextureStorageEXT __rglgen_glEGLImageTargetTextureStorageEXT
#define glLabelObjectEXT __rglgen_glLabelObjectEXT
#define glGetObjectLabelEXT __rglgen_glGetObjectLabelEXT
#define glInsertEventMarkerEXT __rglgen_glInsertEventMarkerEXT
#define glPushGroupMarkerEXT __rglgen_glPushGroupMarkerEXT
#define glPopGroupMarkerEXT __rglgen_glPopGroupMarkerEXT
#define glMatrixLoadfEXT __rglgen_glMatrixLoadfEXT
#define glMatrixLoaddEXT __rglgen_glMatrixLoaddEXT
#define glMatrixMultfEXT __rglgen_glMatrixMultfEXT
#define glMatrixMultdEXT __rglgen_glMatrixMultdEXT
#define glMatrixLoadIdentityEXT __rglgen_glMatrixLoadIdentityEXT
#define glMatrixRotatefEXT __rglgen_glMatrixRotatefEXT
#define glMatrixRotatedEXT __rglgen_glMatrixRotatedEXT
#define glMatrixScalefEXT __rglgen_glMatrixScalefEXT
#define glMatrixScaledEXT __rglgen_glMatrixScaledEXT
#define glMatrixTranslatefEXT __rglgen_glMatrixTranslatefEXT
#define glMatrixTranslatedEXT __rglgen_glMatrixTranslatedEXT
#define glMatrixFrustumEXT __rglgen_glMatrixFrustumEXT
#define glMatrixOrthoEXT __rglgen_glMatrixOrthoEXT
#define glMatrixPopEXT __rglgen_glMatrixPopEXT
#define glMatrixPushEXT __rglgen_glMatrixPushEXT
#define glClientAttribDefaultEXT __rglgen_glClientAttribDefaultEXT
#define glPushClientAttribDefaultEXT __rglgen_glPushClientAttribDefaultEXT
#define glTextureParameterfEXT __rglgen_glTextureParameterfEXT
#define glTextureParameterfvEXT __rglgen_glTextureParameterfvEXT
#define glTextureParameteriEXT __rglgen_glTextureParameteriEXT
#define glTextureParameterivEXT __rglgen_glTextureParameterivEXT
#define glTextureImage1DEXT __rglgen_glTextureImage1DEXT
#define glTextureImage2DEXT __rglgen_glTextureImage2DEXT
#define glTextureSubImage1DEXT __rglgen_glTextureSubImage1DEXT
#define glTextureSubImage2DEXT __rglgen_glTextureSubImage2DEXT
#define glCopyTextureImage1DEXT __rglgen_glCopyTextureImage1DEXT
#define glCopyTextureImage2DEXT __rglgen_glCopyTextureImage2DEXT
#define glCopyTextureSubImage1DEXT __rglgen_glCopyTextureSubImage1DEXT
#define glCopyTextureSubImage2DEXT __rglgen_glCopyTextureSubImage2DEXT
#define glGetTextureImageEXT __rglgen_glGetTextureImageEXT
#define glGetTextureParameterfvEXT __rglgen_glGetTextureParameterfvEXT
#define glGetTextureParameterivEXT __rglgen_glGetTextureParameterivEXT
#define glGetTextureLevelParameterfvEXT __rglgen_glGetTextureLevelParameterfvEXT
#define glGetTextureLevelParameterivEXT __rglgen_glGetTextureLevelParameterivEXT
#define glTextureImage3DEXT __rglgen_glTextureImage3DEXT
#define glTextureSubImage3DEXT __rglgen_glTextureSubImage3DEXT
#define glCopyTextureSubImage3DEXT __rglgen_glCopyTextureSubImage3DEXT
#define glBindMultiTextureEXT __rglgen_glBindMultiTextureEXT
#define glMultiTexCoordPointerEXT __rglgen_glMultiTexCoordPointerEXT
#define glMultiTexEnvfEXT __rglgen_glMultiTexEnvfEXT
#define glMultiTexEnvfvEXT __rglgen_glMultiTexEnvfvEXT
#define glMultiTexEnviEXT __rglgen_glMultiTexEnviEXT
#define glMultiTexEnvivEXT __rglgen_glMultiTexEnvivEXT
#define glMultiTexGendEXT __rglgen_glMultiTexGendEXT
#define glMultiTexGendvEXT __rglgen_glMultiTexGendvEXT
#define glMultiTexGenfEXT __rglgen_glMultiTexGenfEXT
#define glMultiTexGenfvEXT __rglgen_glMultiTexGenfvEXT
#define glMultiTexGeniEXT __rglgen_glMultiTexGeniEXT
#define glMultiTexGenivEXT __rglgen_glMultiTexGenivEXT
#define glGetMultiTexEnvfvEXT __rglgen_glGetMultiTexEnvfvEXT
#define glGetMultiTexEnvivEXT __rglgen_glGetMultiTexEnvivEXT
#define glGetMultiTexGendvEXT __rglgen_glGetMultiTexGendvEXT
#define glGetMultiTexGenfvEXT __rglgen_glGetMultiTexGenfvEXT
#define glGetMultiTexGenivEXT __rglgen_glGetMultiTexGenivEXT
#define glMultiTexParameteriEXT __rglgen_glMultiTexParameteriEXT
#define glMultiTexParameterivEXT __rglgen_glMultiTexParameterivEXT
#define glMultiTexParameterfEXT __rglgen_glMultiTexParameterfEXT
#define glMultiTexParameterfvEXT __rglgen_glMultiTexParameterfvEXT
#define glMultiTexImage1DEXT __rglgen_glMultiTexImage1DEXT
#define glMultiTexImage2DEXT __rglgen_glMultiTexImage2DEXT
#define glMultiTexSubImage1DEXT __rglgen_glMultiTexSubImage1DEXT
#define glMultiTexSubImage2DEXT __rglgen_glMultiTexSubImage2DEXT
#define glCopyMultiTexImage1DEXT __rglgen_glCopyMultiTexImage1DEXT
#define glCopyMultiTexImage2DEXT __rglgen_glCopyMultiTexImage2DEXT
#define glCopyMultiTexSubImage1DEXT __rglgen_glCopyMultiTexSubImage1DEXT
#define glCopyMultiTexSubImage2DEXT __rglgen_glCopyMultiTexSubImage2DEXT
#define glGetMultiTexImageEXT __rglgen_glGetMultiTexImageEXT
#define glGetMultiTexParameterfvEXT __rglgen_glGetMultiTexParameterfvEXT
#define glGetMultiTexParameterivEXT __rglgen_glGetMultiTexParameterivEXT
#define glGetMultiTexLevelParameterfvEXT __rglgen_glGetMultiTexLevelParameterfvEXT
#define glGetMultiTexLevelParameterivEXT __rglgen_glGetMultiTexLevelParameterivEXT
#define glMultiTexImage3DEXT __rglgen_glMultiTexImage3DEXT
#define glMultiTexSubImage3DEXT __rglgen_glMultiTexSubImage3DEXT
#define glCopyMultiTexSubImage3DEXT __rglgen_glCopyMultiTexSubImage3DEXT
#define glEnableClientStateIndexedEXT __rglgen_glEnableClientStateIndexedEXT
#define glDisableClientStateIndexedEXT __rglgen_glDisableClientStateIndexedEXT
#define glGetFloatIndexedvEXT __rglgen_glGetFloatIndexedvEXT
#define glGetDoubleIndexedvEXT __rglgen_glGetDoubleIndexedvEXT
#define glGetPointerIndexedvEXT __rglgen_glGetPointerIndexedvEXT
#define glEnableIndexedEXT __rglgen_glEnableIndexedEXT
#define glDisableIndexedEXT __rglgen_glDisableIndexedEXT
#define glIsEnabledIndexedEXT __rglgen_glIsEnabledIndexedEXT
#define glGetIntegerIndexedvEXT __rglgen_glGetIntegerIndexedvEXT
#define glGetBooleanIndexedvEXT __rglgen_glGetBooleanIndexedvEXT
#define glCompressedTextureImage3DEXT __rglgen_glCompressedTextureImage3DEXT
#define glCompressedTextureImage2DEXT __rglgen_glCompressedTextureImage2DEXT
#define glCompressedTextureImage1DEXT __rglgen_glCompressedTextureImage1DEXT
#define glCompressedTextureSubImage3DEXT __rglgen_glCompressedTextureSubImage3DEXT
#define glCompressedTextureSubImage2DEXT __rglgen_glCompressedTextureSubImage2DEXT
#define glCompressedTextureSubImage1DEXT __rglgen_glCompressedTextureSubImage1DEXT
#define glGetCompressedTextureImageEXT __rglgen_glGetCompressedTextureImageEXT
#define glCompressedMultiTexImage3DEXT __rglgen_glCompressedMultiTexImage3DEXT
#define glCompressedMultiTexImage2DEXT __rglgen_glCompressedMultiTexImage2DEXT
#define glCompressedMultiTexImage1DEXT __rglgen_glCompressedMultiTexImage1DEXT
#define glCompressedMultiTexSubImage3DEXT __rglgen_glCompressedMultiTexSubImage3DEXT
#define glCompressedMultiTexSubImage2DEXT __rglgen_glCompressedMultiTexSubImage2DEXT
#define glCompressedMultiTexSubImage1DEXT __rglgen_glCompressedMultiTexSubImage1DEXT
#define glGetCompressedMultiTexImageEXT __rglgen_glGetCompressedMultiTexImageEXT
#define glMatrixLoadTransposefEXT __rglgen_glMatrixLoadTransposefEXT
#define glMatrixLoadTransposedEXT __rglgen_glMatrixLoadTransposedEXT
#define glMatrixMultTransposefEXT __rglgen_glMatrixMultTransposefEXT
#define glMatrixMultTransposedEXT __rglgen_glMatrixMultTransposedEXT
#define glNamedBufferDataEXT __rglgen_glNamedBufferDataEXT
#define glNamedBufferSubDataEXT __rglgen_glNamedBufferSubDataEXT
#define glMapNamedBufferEXT __rglgen_glMapNamedBufferEXT
#define glUnmapNamedBufferEXT __rglgen_glUnmapNamedBufferEXT
#define glGetNamedBufferParameterivEXT __rglgen_glGetNamedBufferParameterivEXT
#define glGetNamedBufferPointervEXT __rglgen_glGetNamedBufferPointervEXT
#define glGetNamedBufferSubDataEXT __rglgen_glGetNamedBufferSubDataEXT
#define glProgramUniform1fEXT __rglgen_glProgramUniform1fEXT
#define glProgramUniform2fEXT __rglgen_glProgramUniform2fEXT
#define glProgramUniform3fEXT __rglgen_glProgramUniform3fEXT
#define glProgramUniform4fEXT __rglgen_glProgramUniform4fEXT
#define glProgramUniform1iEXT __rglgen_glProgramUniform1iEXT
#define glProgramUniform2iEXT __rglgen_glProgramUniform2iEXT
#define glProgramUniform3iEXT __rglgen_glProgramUniform3iEXT
#define glProgramUniform4iEXT __rglgen_glProgramUniform4iEXT
#define glProgramUniform1fvEXT __rglgen_glProgramUniform1fvEXT
#define glProgramUniform2fvEXT __rglgen_glProgramUniform2fvEXT
#define glProgramUniform3fvEXT __rglgen_glProgramUniform3fvEXT
#define glProgramUniform4fvEXT __rglgen_glProgramUniform4fvEXT
#define glProgramUniform1ivEXT __rglgen_glProgramUniform1ivEXT
#define glProgramUniform2ivEXT __rglgen_glProgramUniform2ivEXT
#define glProgramUniform3ivEXT __rglgen_glProgramUniform3ivEXT
#define glProgramUniform4ivEXT __rglgen_glProgramUniform4ivEXT
#define glProgramUniformMatrix2fvEXT __rglgen_glProgramUniformMatrix2fvEXT
#define glProgramUniformMatrix3fvEXT __rglgen_glProgramUniformMatrix3fvEXT
#define glProgramUniformMatrix4fvEXT __rglgen_glProgramUniformMatrix4fvEXT
#define glProgramUniformMatrix2x3fvEXT __rglgen_glProgramUniformMatrix2x3fvEXT
#define glProgramUniformMatrix3x2fvEXT __rglgen_glProgramUniformMatrix3x2fvEXT
#define glProgramUniformMatrix2x4fvEXT __rglgen_glProgramUniformMatrix2x4fvEXT
#define glProgramUniformMatrix4x2fvEXT __rglgen_glProgramUniformMatrix4x2fvEXT
#define glProgramUniformMatrix3x4fvEXT __rglgen_glProgramUniformMatrix3x4fvEXT
#define glProgramUniformMatrix4x3fvEXT __rglgen_glProgramUniformMatrix4x3fvEXT
#define glTextureBufferEXT __rglgen_glTextureBufferEXT
#define glMultiTexBufferEXT __rglgen_glMultiTexBufferEXT
#define glTextureParameterIivEXT __rglgen_glTextureParameterIivEXT
#define glTextureParameterIuivEXT __rglgen_glTextureParameterIuivEXT
#define glGetTextureParameterIivEXT __rglgen_glGetTextureParameterIivEXT
#define glGetTextureParameterIuivEXT __rglgen_glGetTextureParameterIuivEXT
#define glMultiTexParameterIivEXT __rglgen_glMultiTexParameterIivEXT
#define glMultiTexParameterIuivEXT __rglgen_glMultiTexParameterIuivEXT
#define glGetMultiTexParameterIivEXT __rglgen_glGetMultiTexParameterIivEXT
#define glGetMultiTexParameterIuivEXT __rglgen_glGetMultiTexParameterIuivEXT
#define glProgramUniform1uiEXT __rglgen_glProgramUniform1uiEXT
#define glProgramUniform2uiEXT __rglgen_glProgramUniform2uiEXT
#define glProgramUniform3uiEXT __rglgen_glProgramUniform3uiEXT
#define glProgramUniform4uiEXT __rglgen_glProgramUniform4uiEXT
#define glProgramUniform1uivEXT __rglgen_glProgramUniform1uivEXT
#define glProgramUniform2uivEXT __rglgen_glProgramUniform2uivEXT
#define glProgramUniform3uivEXT __rglgen_glProgramUniform3uivEXT
#define glProgramUniform4uivEXT __rglgen_glProgramUniform4uivEXT
#define glNamedProgramLocalParameters4fvEXT __rglgen_glNamedProgramLocalParameters4fvEXT
#define glNamedProgramLocalParameterI4iEXT __rglgen_glNamedProgramLocalParameterI4iEXT
#define glNamedProgramLocalParameterI4ivEXT __rglgen_glNamedProgramLocalParameterI4ivEXT
#define glNamedProgramLocalParametersI4ivEXT __rglgen_glNamedProgramLocalParametersI4ivEXT
#define glNamedProgramLocalParameterI4uiEXT __rglgen_glNamedProgramLocalParameterI4uiEXT
#define glNamedProgramLocalParameterI4uivEXT __rglgen_glNamedProgramLocalParameterI4uivEXT
#define glNamedProgramLocalParametersI4uivEXT __rglgen_glNamedProgramLocalParametersI4uivEXT
#define glGetNamedProgramLocalParameterIivEXT __rglgen_glGetNamedProgramLocalParameterIivEXT
#define glGetNamedProgramLocalParameterIuivEXT __rglgen_glGetNamedProgramLocalParameterIuivEXT
#define glEnableClientStateiEXT __rglgen_glEnableClientStateiEXT
#define glDisableClientStateiEXT __rglgen_glDisableClientStateiEXT
#define glGetFloati_vEXT __rglgen_glGetFloati_vEXT
#define glGetDoublei_vEXT __rglgen_glGetDoublei_vEXT
#define glGetPointeri_vEXT __rglgen_glGetPointeri_vEXT
#define glNamedProgramStringEXT __rglgen_glNamedProgramStringEXT
#define glNamedProgramLocalParameter4dEXT __rglgen_glNamedProgramLocalParameter4dEXT
#define glNamedProgramLocalParameter4dvEXT __rglgen_glNamedProgramLocalParameter4dvEXT
#define glNamedProgramLocalParameter4fEXT __rglgen_glNamedProgramLocalParameter4fEXT
#define glNamedProgramLocalParameter4fvEXT __rglgen_glNamedProgramLocalParameter4fvEXT
#define glGetNamedProgramLocalParameterdvEXT __rglgen_glGetNamedProgramLocalParameterdvEXT
#define glGetNamedProgramLocalParameterfvEXT __rglgen_glGetNamedProgramLocalParameterfvEXT
#define glGetNamedProgramivEXT __rglgen_glGetNamedProgramivEXT
#define glGetNamedProgramStringEXT __rglgen_glGetNamedProgramStringEXT
#define glNamedRenderbufferStorageEXT __rglgen_glNamedRenderbufferStorageEXT
#define glGetNamedRenderbufferParameterivEXT __rglgen_glGetNamedRenderbufferParameterivEXT
#define glNamedRenderbufferStorageMultisampleEXT __rglgen_glNamedRenderbufferStorageMultisampleEXT
#define glNamedRenderbufferStorageMultisampleCoverageEXT __rglgen_glNamedRenderbufferStorageMultisampleCoverageEXT
#define glCheckNamedFramebufferStatusEXT __rglgen_glCheckNamedFramebufferStatusEXT
#define glNamedFramebufferTexture1DEXT __rglgen_glNamedFramebufferTexture1DEXT
#define glNamedFramebufferTexture2DEXT __rglgen_glNamedFramebufferTexture2DEXT
#define glNamedFramebufferTexture3DEXT __rglgen_glNamedFramebufferTexture3DEXT
#define glNamedFramebufferRenderbufferEXT __rglgen_glNamedFramebufferRenderbufferEXT
#define glGetNamedFramebufferAttachmentParameterivEXT __rglgen_glGetNamedFramebufferAttachmentParameterivEXT
#define glGenerateTextureMipmapEXT __rglgen_glGenerateTextureMipmapEXT
#define glGenerateMultiTexMipmapEXT __rglgen_glGenerateMultiTexMipmapEXT
#define glFramebufferDrawBufferEXT __rglgen_glFramebufferDrawBufferEXT
#define glFramebufferDrawBuffersEXT __rglgen_glFramebufferDrawBuffersEXT
#define glFramebufferReadBufferEXT __rglgen_glFramebufferReadBufferEXT
#define glGetFramebufferParameterivEXT __rglgen_glGetFramebufferParameterivEXT
#define glNamedCopyBufferSubDataEXT __rglgen_glNamedCopyBufferSubDataEXT
#define glNamedFramebufferTextureEXT __rglgen_glNamedFramebufferTextureEXT
#define glNamedFramebufferTextureLayerEXT __rglgen_glNamedFramebufferTextureLayerEXT
#define glNamedFramebufferTextureFaceEXT __rglgen_glNamedFramebufferTextureFaceEXT
#define glTextureRenderbufferEXT __rglgen_glTextureRenderbufferEXT
#define glMultiTexRenderbufferEXT __rglgen_glMultiTexRenderbufferEXT
#define glVertexArrayVertexOffsetEXT __rglgen_glVertexArrayVertexOffsetEXT
#define glVertexArrayColorOffsetEXT __rglgen_glVertexArrayColorOffsetEXT
#define glVertexArrayEdgeFlagOffsetEXT __rglgen_glVertexArrayEdgeFlagOffsetEXT
#define glVertexArrayIndexOffsetEXT __rglgen_glVertexArrayIndexOffsetEXT
#define glVertexArrayNormalOffsetEXT __rglgen_glVertexArrayNormalOffsetEXT
#define glVertexArrayTexCoordOffsetEXT __rglgen_glVertexArrayTexCoordOffsetEXT
#define glVertexArrayMultiTexCoordOffsetEXT __rglgen_glVertexArrayMultiTexCoordOffsetEXT
#define glVertexArrayFogCoordOffsetEXT __rglgen_glVertexArrayFogCoordOffsetEXT
#define glVertexArraySecondaryColorOffsetEXT __rglgen_glVertexArraySecondaryColorOffsetEXT
#define glVertexArrayVertexAttribOffsetEXT __rglgen_glVertexArrayVertexAttribOffsetEXT
#define glVertexArrayVertexAttribIOffsetEXT __rglgen_glVertexArrayVertexAttribIOffsetEXT
#define glEnableVertexArrayEXT __rglgen_glEnableVertexArrayEXT
#define glDisableVertexArrayEXT __rglgen_glDisableVertexArrayEXT
#define glEnableVertexArrayAttribEXT __rglgen_glEnableVertexArrayAttribEXT
#define glDisableVertexArrayAttribEXT __rglgen_glDisableVertexArrayAttribEXT
#define glGetVertexArrayIntegervEXT __rglgen_glGetVertexArrayIntegervEXT
#define glGetVertexArrayPointervEXT __rglgen_glGetVertexArrayPointervEXT
#define glGetVertexArrayIntegeri_vEXT __rglgen_glGetVertexArrayIntegeri_vEXT
#define glGetVertexArrayPointeri_vEXT __rglgen_glGetVertexArrayPointeri_vEXT
#define glMapNamedBufferRangeEXT __rglgen_glMapNamedBufferRangeEXT
#define glFlushMappedNamedBufferRangeEXT __rglgen_glFlushMappedNamedBufferRangeEXT
#define glNamedBufferStorageEXT __rglgen_glNamedBufferStorageEXT
#define glClearNamedBufferDataEXT __rglgen_glClearNamedBufferDataEXT
#define glClearNamedBufferSubDataEXT __rglgen_glClearNamedBufferSubDataEXT
#define glNamedFramebufferParameteriEXT __rglgen_glNamedFramebufferParameteriEXT
#define glGetNamedFramebufferParameterivEXT __rglgen_glGetNamedFramebufferParameterivEXT
#define glProgramUniform1dEXT __rglgen_glProgramUniform1dEXT
#define glProgramUniform2dEXT __rglgen_glProgramUniform2dEXT
#define glProgramUniform3dEXT __rglgen_glProgramUniform3dEXT
#define glProgramUniform4dEXT __rglgen_glProgramUniform4dEXT
#define glProgramUniform1dvEXT __rglgen_glProgramUniform1dvEXT
#define glProgramUniform2dvEXT __rglgen_glProgramUniform2dvEXT
#define glProgramUniform3dvEXT __rglgen_glProgramUniform3dvEXT
#define glProgramUniform4dvEXT __rglgen_glProgramUniform4dvEXT
#define glProgramUniformMatrix2dvEXT __rglgen_glProgramUniformMatrix2dvEXT
#define glProgramUniformMatrix3dvEXT __rglgen_glProgramUniformMatrix3dvEXT
#define glProgramUniformMatrix4dvEXT __rglgen_glProgramUniformMatrix4dvEXT
#define glProgramUniformMatrix2x3dvEXT __rglgen_glProgramUniformMatrix2x3dvEXT
#define glProgramUniformMatrix2x4dvEXT __rglgen_glProgramUniformMatrix2x4dvEXT
#define glProgramUniformMatrix3x2dvEXT __rglgen_glProgramUniformMatrix3x2dvEXT
#define glProgramUniformMatrix3x4dvEXT __rglgen_glProgramUniformMatrix3x4dvEXT
#define glProgramUniformMatrix4x2dvEXT __rglgen_glProgramUniformMatrix4x2dvEXT
#define glProgramUniformMatrix4x3dvEXT __rglgen_glProgramUniformMatrix4x3dvEXT
#define glTextureBufferRangeEXT __rglgen_glTextureBufferRangeEXT
#define glTextureStorage1DEXT __rglgen_glTextureStorage1DEXT
#define glTextureStorage2DEXT __rglgen_glTextureStorage2DEXT
#define glTextureStorage3DEXT __rglgen_glTextureStorage3DEXT
#define glTextureStorage2DMultisampleEXT __rglgen_glTextureStorage2DMultisampleEXT
#define glTextureStorage3DMultisampleEXT __rglgen_glTextureStorage3DMultisampleEXT
#define glVertexArrayBindVertexBufferEXT __rglgen_glVertexArrayBindVertexBufferEXT
#define glVertexArrayVertexAttribFormatEXT __rglgen_glVertexArrayVertexAttribFormatEXT
#define glVertexArrayVertexAttribIFormatEXT __rglgen_glVertexArrayVertexAttribIFormatEXT
#define glVertexArrayVertexAttribLFormatEXT __rglgen_glVertexArrayVertexAttribLFormatEXT
#define glVertexArrayVertexAttribBindingEXT __rglgen_glVertexArrayVertexAttribBindingEXT
#define glVertexArrayVertexBindingDivisorEXT __rglgen_glVertexArrayVertexBindingDivisorEXT
#define glVertexArrayVertexAttribLOffsetEXT __rglgen_glVertexArrayVertexAttribLOffsetEXT
#define glTexturePageCommitmentEXT __rglgen_glTexturePageCommitmentEXT
#define glVertexArrayVertexAttribDivisorEXT __rglgen_glVertexArrayVertexAttribDivisorEXT
#define glDrawArraysInstancedEXT __rglgen_glDrawArraysInstancedEXT
#define glDrawElementsInstancedEXT __rglgen_glDrawElementsInstancedEXT
#define glPolygonOffsetClampEXT __rglgen_glPolygonOffsetClampEXT
#define glRasterSamplesEXT __rglgen_glRasterSamplesEXT
#define glUseShaderProgramEXT __rglgen_glUseShaderProgramEXT
#define glActiveProgramEXT __rglgen_glActiveProgramEXT
#define glCreateShaderProgramEXT __rglgen_glCreateShaderProgramEXT
#define glFramebufferFetchBarrierEXT __rglgen_glFramebufferFetchBarrierEXT
#define glWindowRectanglesEXT __rglgen_glWindowRectanglesEXT
#define glMultiDrawArraysIndirectBindlessNV __rglgen_glMultiDrawArraysIndirectBindlessNV
#define glMultiDrawElementsIndirectBindlessNV __rglgen_glMultiDrawElementsIndirectBindlessNV
#define glMultiDrawArraysIndirectBindlessCountNV __rglgen_glMultiDrawArraysIndirectBindlessCountNV
#define glMultiDrawElementsIndirectBindlessCountNV __rglgen_glMultiDrawElementsIndirectBindlessCountNV
#define glGetTextureHandleNV __rglgen_glGetTextureHandleNV
#define glGetTextureSamplerHandleNV __rglgen_glGetTextureSamplerHandleNV
#define glMakeTextureHandleResidentNV __rglgen_glMakeTextureHandleResidentNV
#define glMakeTextureHandleNonResidentNV __rglgen_glMakeTextureHandleNonResidentNV
#define glGetImageHandleNV __rglgen_glGetImageHandleNV
#define glMakeImageHandleResidentNV __rglgen_glMakeImageHandleResidentNV
#define glMakeImageHandleNonResidentNV __rglgen_glMakeImageHandleNonResidentNV
#define glUniformHandleui64NV __rglgen_glUniformHandleui64NV
#define glUniformHandleui64vNV __rglgen_glUniformHandleui64vNV
#define glProgramUniformHandleui64NV __rglgen_glProgramUniformHandleui64NV
#define glProgramUniformHandleui64vNV __rglgen_glProgramUniformHandleui64vNV
#define glIsTextureHandleResidentNV __rglgen_glIsTextureHandleResidentNV
#define glIsImageHandleResidentNV __rglgen_glIsImageHandleResidentNV
#define glBlendParameteriNV __rglgen_glBlendParameteriNV
#define glBlendBarrierNV __rglgen_glBlendBarrierNV
#define glViewportPositionWScaleNV __rglgen_glViewportPositionWScaleNV
#define glCreateStatesNV __rglgen_glCreateStatesNV
#define glDeleteStatesNV __rglgen_glDeleteStatesNV
#define glIsStateNV __rglgen_glIsStateNV
#define glStateCaptureNV __rglgen_glStateCaptureNV
#define glGetCommandHeaderNV __rglgen_glGetCommandHeaderNV
#define glGetStageIndexNV __rglgen_glGetStageIndexNV
#define glDrawCommandsNV __rglgen_glDrawCommandsNV
#define glDrawCommandsAddressNV __rglgen_glDrawCommandsAddressNV
#define glDrawCommandsStatesNV __rglgen_glDrawCommandsStatesNV
#define glDrawCommandsStatesAddressNV __rglgen_glDrawCommandsStatesAddressNV
#define glCreateCommandListsNV __rglgen_glCreateCommandListsNV
#define glDeleteCommandListsNV __rglgen_glDeleteCommandListsNV
#define glIsCommandListNV __rglgen_glIsCommandListNV
#define glListDrawCommandsStatesClientNV __rglgen_glListDrawCommandsStatesClientNV
#define glCommandListSegmentsNV __rglgen_glCommandListSegmentsNV
#define glCompileCommandListNV __rglgen_glCompileCommandListNV
#define glCallCommandListNV __rglgen_glCallCommandListNV
#define glBeginConditionalRenderNV __rglgen_glBeginConditionalRenderNV
#define glEndConditionalRenderNV __rglgen_glEndConditionalRenderNV
#define glSubpixelPrecisionBiasNV __rglgen_glSubpixelPrecisionBiasNV
#define glConservativeRasterParameterfNV __rglgen_glConservativeRasterParameterfNV
#define glConservativeRasterParameteriNV __rglgen_glConservativeRasterParameteriNV
#define glDrawVkImageNV __rglgen_glDrawVkImageNV
#define glGetVkProcAddrNV __rglgen_glGetVkProcAddrNV
#define glWaitVkSemaphoreNV __rglgen_glWaitVkSemaphoreNV
#define glSignalVkSemaphoreNV __rglgen_glSignalVkSemaphoreNV
#define glSignalVkFenceNV __rglgen_glSignalVkFenceNV
#define glFragmentCoverageColorNV __rglgen_glFragmentCoverageColorNV
#define glCoverageModulationTableNV __rglgen_glCoverageModulationTableNV
#define glGetCoverageModulationTableNV __rglgen_glGetCoverageModulationTableNV
#define glCoverageModulationNV __rglgen_glCoverageModulationNV
#define glRenderbufferStorageMultisampleCoverageNV __rglgen_glRenderbufferStorageMultisampleCoverageNV
#define glUniform1i64NV __rglgen_glUniform1i64NV
#define glUniform2i64NV __rglgen_glUniform2i64NV
#define glUniform3i64NV __rglgen_glUniform3i64NV
#define glUniform4i64NV __rglgen_glUniform4i64NV
#define glUniform1i64vNV __rglgen_glUniform1i64vNV
#define glUniform2i64vNV __rglgen_glUniform2i64vNV
#define glUniform3i64vNV __rglgen_glUniform3i64vNV
#define glUniform4i64vNV __rglgen_glUniform4i64vNV
#define glUniform1ui64NV __rglgen_glUniform1ui64NV
#define glUniform2ui64NV __rglgen_glUniform2ui64NV
#define glUniform3ui64NV __rglgen_glUniform3ui64NV
#define glUniform4ui64NV __rglgen_glUniform4ui64NV
#define glUniform1ui64vNV __rglgen_glUniform1ui64vNV
#define glUniform2ui64vNV __rglgen_glUniform2ui64vNV
#define glUniform3ui64vNV __rglgen_glUniform3ui64vNV
#define glUniform4ui64vNV __rglgen_glUniform4ui64vNV
#define glGetUniformi64vNV __rglgen_glGetUniformi64vNV
#define glProgramUniform1i64NV __rglgen_glProgramUniform1i64NV
#define glProgramUniform2i64NV __rglgen_glProgramUniform2i64NV
#define glProgramUniform3i64NV __rglgen_glProgramUniform3i64NV
#define glProgramUniform4i64NV __rglgen_glProgramUniform4i64NV
#define glProgramUniform1i64vNV __rglgen_glProgramUniform1i64vNV
#define glProgramUniform2i64vNV __rglgen_glProgramUniform2i64vNV
#define glProgramUniform3i64vNV __rglgen_glProgramUniform3i64vNV
#define glProgramUniform4i64vNV __rglgen_glProgramUniform4i64vNV
#define glProgramUniform1ui64NV __rglgen_glProgramUniform1ui64NV
#define glProgramUniform2ui64NV __rglgen_glProgramUniform2ui64NV
#define glProgramUniform3ui64NV __rglgen_glProgramUniform3ui64NV
#define glProgramUniform4ui64NV __rglgen_glProgramUniform4ui64NV
#define glProgramUniform1ui64vNV __rglgen_glProgramUniform1ui64vNV
#define glProgramUniform2ui64vNV __rglgen_glProgramUniform2ui64vNV
#define glProgramUniform3ui64vNV __rglgen_glProgramUniform3ui64vNV
#define glProgramUniform4ui64vNV __rglgen_glProgramUniform4ui64vNV
#define glGetInternalformatSampleivNV __rglgen_glGetInternalformatSampleivNV
#define glGetMemoryObjectDetachedResourcesuivNV __rglgen_glGetMemoryObjectDetachedResourcesuivNV
#define glResetMemoryObjectParameterNV __rglgen_glResetMemoryObjectParameterNV
#define glTexAttachMemoryNV __rglgen_glTexAttachMemoryNV
#define glBufferAttachMemoryNV __rglgen_glBufferAttachMemoryNV
#define glTextureAttachMemoryNV __rglgen_glTextureAttachMemoryNV
#define glNamedBufferAttachMemoryNV __rglgen_glNamedBufferAttachMemoryNV
#define glDrawMeshTasksNV __rglgen_glDrawMeshTasksNV
#define glDrawMeshTasksIndirectNV __rglgen_glDrawMeshTasksIndirectNV
#define glMultiDrawMeshTasksIndirectNV __rglgen_glMultiDrawMeshTasksIndirectNV
#define glMultiDrawMeshTasksIndirectCountNV __rglgen_glMultiDrawMeshTasksIndirectCountNV
#define glGenPathsNV __rglgen_glGenPathsNV
#define glDeletePathsNV __rglgen_glDeletePathsNV
#define glIsPathNV __rglgen_glIsPathNV
#define glPathCommandsNV __rglgen_glPathCommandsNV
#define glPathCoordsNV __rglgen_glPathCoordsNV
#define glPathSubCommandsNV __rglgen_glPathSubCommandsNV
#define glPathSubCoordsNV __rglgen_glPathSubCoordsNV
#define glPathStringNV __rglgen_glPathStringNV
#define glPathGlyphsNV __rglgen_glPathGlyphsNV
#define glPathGlyphRangeNV __rglgen_glPathGlyphRangeNV
#define glWeightPathsNV __rglgen_glWeightPathsNV
#define glCopyPathNV __rglgen_glCopyPathNV
#define glInterpolatePathsNV __rglgen_glInterpolatePathsNV
#define glTransformPathNV __rglgen_glTransformPathNV
#define glPathParameterivNV __rglgen_glPathParameterivNV
#define glPathParameteriNV __rglgen_glPathParameteriNV
#define glPathParameterfvNV __rglgen_glPathParameterfvNV
#define glPathParameterfNV __rglgen_glPathParameterfNV
#define glPathDashArrayNV __rglgen_glPathDashArrayNV
#define glPathStencilFuncNV __rglgen_glPathStencilFuncNV
#define glPathStencilDepthOffsetNV __rglgen_glPathStencilDepthOffsetNV
#define glStencilFillPathNV __rglgen_glStencilFillPathNV
#define glStencilStrokePathNV __rglgen_glStencilStrokePathNV
#define glStencilFillPathInstancedNV __rglgen_glStencilFillPathInstancedNV
#define glStencilStrokePathInstancedNV __rglgen_glStencilStrokePathInstancedNV
#define glPathCoverDepthFuncNV __rglgen_glPathCoverDepthFuncNV
#define glCoverFillPathNV __rglgen_glCoverFillPathNV
#define glCoverStrokePathNV __rglgen_glCoverStrokePathNV
#define glCoverFillPathInstancedNV __rglgen_glCoverFillPathInstancedNV
#define glCoverStrokePathInstancedNV __rglgen_glCoverStrokePathInstancedNV
#define glGetPathParameterivNV __rglgen_glGetPathParameterivNV
#define glGetPathParameterfvNV __rglgen_glGetPathParameterfvNV
#define glGetPathCommandsNV __rglgen_glGetPathCommandsNV
#define glGetPathCoordsNV __rglgen_glGetPathCoordsNV
#define glGetPathDashArrayNV __rglgen_glGetPathDashArrayNV
#define glGetPathMetricsNV __rglgen_glGetPathMetricsNV
#define glGetPathMetricRangeNV __rglgen_glGetPathMetricRangeNV
#define glGetPathSpacingNV __rglgen_glGetPathSpacingNV
#define glIsPointInFillPathNV __rglgen_glIsPointInFillPathNV
#define glIsPointInStrokePathNV __rglgen_glIsPointInStrokePathNV
#define glGetPathLengthNV __rglgen_glGetPathLengthNV
#define glPointAlongPathNV __rglgen_glPointAlongPathNV
#define glMatrixLoad3x2fNV __rglgen_glMatrixLoad3x2fNV
#define glMatrixLoad3x3fNV __rglgen_glMatrixLoad3x3fNV
#define glMatrixLoadTranspose3x3fNV __rglgen_glMatrixLoadTranspose3x3fNV
#define glMatrixMult3x2fNV __rglgen_glMatrixMult3x2fNV
#define glMatrixMult3x3fNV __rglgen_glMatrixMult3x3fNV
#define glMatrixMultTranspose3x3fNV __rglgen_glMatrixMultTranspose3x3fNV
#define glStencilThenCoverFillPathNV __rglgen_glStencilThenCoverFillPathNV
#define glStencilThenCoverStrokePathNV __rglgen_glStencilThenCoverStrokePathNV
#define glStencilThenCoverFillPathInstancedNV __rglgen_glStencilThenCoverFillPathInstancedNV
#define glStencilThenCoverStrokePathInstancedNV __rglgen_glStencilThenCoverStrokePathInstancedNV
#define glPathGlyphIndexRangeNV __rglgen_glPathGlyphIndexRangeNV
#define glPathGlyphIndexArrayNV __rglgen_glPathGlyphIndexArrayNV
#define glPathMemoryGlyphIndexArrayNV __rglgen_glPathMemoryGlyphIndexArrayNV
#define glProgramPathFragmentInputGenNV __rglgen_glProgramPathFragmentInputGenNV
#define glGetProgramResourcefvNV __rglgen_glGetProgramResourcefvNV
#define glFramebufferSampleLocationsfvNV __rglgen_glFramebufferSampleLocationsfvNV
#define glNamedFramebufferSampleLocationsfvNV __rglgen_glNamedFramebufferSampleLocationsfvNV
#define glResolveDepthValuesNV __rglgen_glResolveDepthValuesNV
#define glScissorExclusiveNV __rglgen_glScissorExclusiveNV
#define glScissorExclusiveArrayvNV __rglgen_glScissorExclusiveArrayvNV
#define glMakeBufferResidentNV __rglgen_glMakeBufferResidentNV
#define glMakeBufferNonResidentNV __rglgen_glMakeBufferNonResidentNV
#define glIsBufferResidentNV __rglgen_glIsBufferResidentNV
#define glMakeNamedBufferResidentNV __rglgen_glMakeNamedBufferResidentNV
#define glMakeNamedBufferNonResidentNV __rglgen_glMakeNamedBufferNonResidentNV
#define glIsNamedBufferResidentNV __rglgen_glIsNamedBufferResidentNV
#define glGetBufferParameterui64vNV __rglgen_glGetBufferParameterui64vNV
#define glGetNamedBufferParameterui64vNV __rglgen_glGetNamedBufferParameterui64vNV
#define glGetIntegerui64vNV __rglgen_glGetIntegerui64vNV
#define glUniformui64NV __rglgen_glUniformui64NV
#define glUniformui64vNV __rglgen_glUniformui64vNV
#define glGetUniformui64vNV __rglgen_glGetUniformui64vNV
#define glProgramUniformui64NV __rglgen_glProgramUniformui64NV
#define glProgramUniformui64vNV __rglgen_glProgramUniformui64vNV
#define glBindShadingRateImageNV __rglgen_glBindShadingRateImageNV
#define glGetShadingRateImagePaletteNV __rglgen_glGetShadingRateImagePaletteNV
#define glGetShadingRateSampleLocationivNV __rglgen_glGetShadingRateSampleLocationivNV
#define glShadingRateImageBarrierNV __rglgen_glShadingRateImageBarrierNV
#define glShadingRateImagePaletteNV __rglgen_glShadingRateImagePaletteNV
#define glShadingRateSampleOrderNV __rglgen_glShadingRateSampleOrderNV
#define glShadingRateSampleOrderCustomNV __rglgen_glShadingRateSampleOrderCustomNV
#define glTextureBarrierNV __rglgen_glTextureBarrierNV
#define glVertexAttribL1i64NV __rglgen_glVertexAttribL1i64NV
#define glVertexAttribL2i64NV __rglgen_glVertexAttribL2i64NV
#define glVertexAttribL3i64NV __rglgen_glVertexAttribL3i64NV
#define glVertexAttribL4i64NV __rglgen_glVertexAttribL4i64NV
#define glVertexAttribL1i64vNV __rglgen_glVertexAttribL1i64vNV
#define glVertexAttribL2i64vNV __rglgen_glVertexAttribL2i64vNV
#define glVertexAttribL3i64vNV __rglgen_glVertexAttribL3i64vNV
#define glVertexAttribL4i64vNV __rglgen_glVertexAttribL4i64vNV
#define glVertexAttribL1ui64NV __rglgen_glVertexAttribL1ui64NV
#define glVertexAttribL2ui64NV __rglgen_glVertexAttribL2ui64NV
#define glVertexAttribL3ui64NV __rglgen_glVertexAttribL3ui64NV
#define glVertexAttribL4ui64NV __rglgen_glVertexAttribL4ui64NV
#define glVertexAttribL1ui64vNV __rglgen_glVertexAttribL1ui64vNV
#define glVertexAttribL2ui64vNV __rglgen_glVertexAttribL2ui64vNV
#define glVertexAttribL3ui64vNV __rglgen_glVertexAttribL3ui64vNV
#define glVertexAttribL4ui64vNV __rglgen_glVertexAttribL4ui64vNV
#define glGetVertexAttribLi64vNV __rglgen_glGetVertexAttribLi64vNV
#define glGetVertexAttribLui64vNV __rglgen_glGetVertexAttribLui64vNV
#define glVertexAttribLFormatNV __rglgen_glVertexAttribLFormatNV
#define glBufferAddressRangeNV __rglgen_glBufferAddressRangeNV
#define glVertexFormatNV __rglgen_glVertexFormatNV
#define glNormalFormatNV __rglgen_glNormalFormatNV
#define glColorFormatNV __rglgen_glColorFormatNV
#define glIndexFormatNV __rglgen_glIndexFormatNV
#define glTexCoordFormatNV __rglgen_glTexCoordFormatNV
#define glEdgeFlagFormatNV __rglgen_glEdgeFlagFormatNV
#define glSecondaryColorFormatNV __rglgen_glSecondaryColorFormatNV
#define glFogCoordFormatNV __rglgen_glFogCoordFormatNV
#define glVertexAttribFormatNV __rglgen_glVertexAttribFormatNV
#define glVertexAttribIFormatNV __rglgen_glVertexAttribIFormatNV
#define glGetIntegerui64i_vNV __rglgen_glGetIntegerui64i_vNV
#define glViewportSwizzleNV __rglgen_glViewportSwizzleNV
#define glFramebufferTextureMultiviewOVR __rglgen_glFramebufferTextureMultiviewOVR

extern RGLSYMGLCULLFACEPROC __rglgen_glCullFace;
extern RGLSYMGLFRONTFACEPROC __rglgen_glFrontFace;
extern RGLSYMGLHINTPROC __rglgen_glHint;
extern RGLSYMGLLINEWIDTHPROC __rglgen_glLineWidth;
extern RGLSYMGLPOINTSIZEPROC __rglgen_glPointSize;
extern RGLSYMGLPOLYGONMODEPROC __rglgen_glPolygonMode;
extern RGLSYMGLSCISSORPROC __rglgen_glScissor;
extern RGLSYMGLTEXPARAMETERFPROC __rglgen_glTexParameterf;
extern RGLSYMGLTEXPARAMETERFVPROC __rglgen_glTexParameterfv;
extern RGLSYMGLTEXPARAMETERIPROC __rglgen_glTexParameteri;
extern RGLSYMGLTEXPARAMETERIVPROC __rglgen_glTexParameteriv;
extern RGLSYMGLTEXIMAGE1DPROC __rglgen_glTexImage1D;
extern RGLSYMGLTEXIMAGE2DPROC __rglgen_glTexImage2D;
extern RGLSYMGLDRAWBUFFERPROC __rglgen_glDrawBuffer;
extern RGLSYMGLCLEARPROC __rglgen_glClear;
extern RGLSYMGLCLEARCOLORPROC __rglgen_glClearColor;
extern RGLSYMGLCLEARSTENCILPROC __rglgen_glClearStencil;
extern RGLSYMGLCLEARDEPTHPROC __rglgen_glClearDepth;
extern RGLSYMGLSTENCILMASKPROC __rglgen_glStencilMask;
extern RGLSYMGLCOLORMASKPROC __rglgen_glColorMask;
extern RGLSYMGLDEPTHMASKPROC __rglgen_glDepthMask;
extern RGLSYMGLDISABLEPROC __rglgen_glDisable;
extern RGLSYMGLENABLEPROC __rglgen_glEnable;
extern RGLSYMGLFINISHPROC __rglgen_glFinish;
extern RGLSYMGLFLUSHPROC __rglgen_glFlush;
extern RGLSYMGLBLENDFUNCPROC __rglgen_glBlendFunc;
extern RGLSYMGLLOGICOPPROC __rglgen_glLogicOp;
extern RGLSYMGLSTENCILFUNCPROC __rglgen_glStencilFunc;
extern RGLSYMGLSTENCILOPPROC __rglgen_glStencilOp;
extern RGLSYMGLDEPTHFUNCPROC __rglgen_glDepthFunc;
extern RGLSYMGLPIXELSTOREFPROC __rglgen_glPixelStoref;
extern RGLSYMGLPIXELSTOREIPROC __rglgen_glPixelStorei;
extern RGLSYMGLREADBUFFERPROC __rglgen_glReadBuffer;
extern RGLSYMGLREADPIXELSPROC __rglgen_glReadPixels;
extern RGLSYMGLGETBOOLEANVPROC __rglgen_glGetBooleanv;
extern RGLSYMGLGETDOUBLEVPROC __rglgen_glGetDoublev;
extern RGLSYMGLGETERRORPROC __rglgen_glGetError;
extern RGLSYMGLGETFLOATVPROC __rglgen_glGetFloatv;
extern RGLSYMGLGETINTEGERVPROC __rglgen_glGetIntegerv;
extern RGLSYMGLGETSTRINGPROC __rglgen_glGetString;
extern RGLSYMGLGETTEXIMAGEPROC __rglgen_glGetTexImage;
extern RGLSYMGLGETTEXPARAMETERFVPROC __rglgen_glGetTexParameterfv;
extern RGLSYMGLGETTEXPARAMETERIVPROC __rglgen_glGetTexParameteriv;
extern RGLSYMGLGETTEXLEVELPARAMETERFVPROC __rglgen_glGetTexLevelParameterfv;
extern RGLSYMGLGETTEXLEVELPARAMETERIVPROC __rglgen_glGetTexLevelParameteriv;
extern RGLSYMGLISENABLEDPROC __rglgen_glIsEnabled;
extern RGLSYMGLDEPTHRANGEPROC __rglgen_glDepthRange;
extern RGLSYMGLVIEWPORTPROC __rglgen_glViewport;
extern RGLSYMGLDRAWARRAYSPROC __rglgen_glDrawArrays;
extern RGLSYMGLDRAWELEMENTSPROC __rglgen_glDrawElements;
extern RGLSYMGLGETPOINTERVPROC __rglgen_glGetPointerv;
extern RGLSYMGLPOLYGONOFFSETPROC __rglgen_glPolygonOffset;
extern RGLSYMGLCOPYTEXIMAGE1DPROC __rglgen_glCopyTexImage1D;
extern RGLSYMGLCOPYTEXIMAGE2DPROC __rglgen_glCopyTexImage2D;
extern RGLSYMGLCOPYTEXSUBIMAGE1DPROC __rglgen_glCopyTexSubImage1D;
extern RGLSYMGLCOPYTEXSUBIMAGE2DPROC __rglgen_glCopyTexSubImage2D;
extern RGLSYMGLTEXSUBIMAGE1DPROC __rglgen_glTexSubImage1D;
extern RGLSYMGLTEXSUBIMAGE2DPROC __rglgen_glTexSubImage2D;
extern RGLSYMGLBINDTEXTUREPROC __rglgen_glBindTexture;
extern RGLSYMGLDELETETEXTURESPROC __rglgen_glDeleteTextures;
extern RGLSYMGLGENTEXTURESPROC __rglgen_glGenTextures;
extern RGLSYMGLISTEXTUREPROC __rglgen_glIsTexture;
extern RGLSYMGLDRAWRANGEELEMENTSPROC __rglgen_glDrawRangeElements;
extern RGLSYMGLTEXIMAGE3DPROC __rglgen_glTexImage3D;
extern RGLSYMGLTEXSUBIMAGE3DPROC __rglgen_glTexSubImage3D;
extern RGLSYMGLCOPYTEXSUBIMAGE3DPROC __rglgen_glCopyTexSubImage3D;
extern RGLSYMGLACTIVETEXTUREPROC __rglgen_glActiveTexture;
extern RGLSYMGLSAMPLECOVERAGEPROC __rglgen_glSampleCoverage;
extern RGLSYMGLCOMPRESSEDTEXIMAGE3DPROC __rglgen_glCompressedTexImage3D;
extern RGLSYMGLCOMPRESSEDTEXIMAGE2DPROC __rglgen_glCompressedTexImage2D;
extern RGLSYMGLCOMPRESSEDTEXIMAGE1DPROC __rglgen_glCompressedTexImage1D;
extern RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DPROC __rglgen_glCompressedTexSubImage3D;
extern RGLSYMGLCOMPRESSEDTEXSUBIMAGE2DPROC __rglgen_glCompressedTexSubImage2D;
extern RGLSYMGLCOMPRESSEDTEXSUBIMAGE1DPROC __rglgen_glCompressedTexSubImage1D;
extern RGLSYMGLGETCOMPRESSEDTEXIMAGEPROC __rglgen_glGetCompressedTexImage;
extern RGLSYMGLBLENDFUNCSEPARATEPROC __rglgen_glBlendFuncSeparate;
extern RGLSYMGLMULTIDRAWARRAYSPROC __rglgen_glMultiDrawArrays;
extern RGLSYMGLMULTIDRAWELEMENTSPROC __rglgen_glMultiDrawElements;
extern RGLSYMGLPOINTPARAMETERFPROC __rglgen_glPointParameterf;
extern RGLSYMGLPOINTPARAMETERFVPROC __rglgen_glPointParameterfv;
extern RGLSYMGLPOINTPARAMETERIPROC __rglgen_glPointParameteri;
extern RGLSYMGLPOINTPARAMETERIVPROC __rglgen_glPointParameteriv;
extern RGLSYMGLBLENDCOLORPROC __rglgen_glBlendColor;
extern RGLSYMGLBLENDEQUATIONPROC __rglgen_glBlendEquation;
extern RGLSYMGLGENQUERIESPROC __rglgen_glGenQueries;
extern RGLSYMGLDELETEQUERIESPROC __rglgen_glDeleteQueries;
extern RGLSYMGLISQUERYPROC __rglgen_glIsQuery;
extern RGLSYMGLBEGINQUERYPROC __rglgen_glBeginQuery;
extern RGLSYMGLENDQUERYPROC __rglgen_glEndQuery;
extern RGLSYMGLGETQUERYIVPROC __rglgen_glGetQueryiv;
extern RGLSYMGLGETQUERYOBJECTIVPROC __rglgen_glGetQueryObjectiv;
extern RGLSYMGLGETQUERYOBJECTUIVPROC __rglgen_glGetQueryObjectuiv;
extern RGLSYMGLBINDBUFFERPROC __rglgen_glBindBuffer;
extern RGLSYMGLDELETEBUFFERSPROC __rglgen_glDeleteBuffers;
extern RGLSYMGLGENBUFFERSPROC __rglgen_glGenBuffers;
extern RGLSYMGLISBUFFERPROC __rglgen_glIsBuffer;
extern RGLSYMGLBUFFERDATAPROC __rglgen_glBufferData;
extern RGLSYMGLBUFFERSUBDATAPROC __rglgen_glBufferSubData;
extern RGLSYMGLGETBUFFERSUBDATAPROC __rglgen_glGetBufferSubData;
extern RGLSYMGLMAPBUFFERPROC __rglgen_glMapBuffer;
extern RGLSYMGLUNMAPBUFFERPROC __rglgen_glUnmapBuffer;
extern RGLSYMGLGETBUFFERPARAMETERIVPROC __rglgen_glGetBufferParameteriv;
extern RGLSYMGLGETBUFFERPOINTERVPROC __rglgen_glGetBufferPointerv;
extern RGLSYMGLBLENDEQUATIONSEPARATEPROC __rglgen_glBlendEquationSeparate;
extern RGLSYMGLDRAWBUFFERSPROC __rglgen_glDrawBuffers;
extern RGLSYMGLSTENCILOPSEPARATEPROC __rglgen_glStencilOpSeparate;
extern RGLSYMGLSTENCILFUNCSEPARATEPROC __rglgen_glStencilFuncSeparate;
extern RGLSYMGLSTENCILMASKSEPARATEPROC __rglgen_glStencilMaskSeparate;
extern RGLSYMGLATTACHSHADERPROC __rglgen_glAttachShader;
extern RGLSYMGLBINDATTRIBLOCATIONPROC __rglgen_glBindAttribLocation;
extern RGLSYMGLCOMPILESHADERPROC __rglgen_glCompileShader;
extern RGLSYMGLCREATEPROGRAMPROC __rglgen_glCreateProgram;
extern RGLSYMGLCREATESHADERPROC __rglgen_glCreateShader;
extern RGLSYMGLDELETEPROGRAMPROC __rglgen_glDeleteProgram;
extern RGLSYMGLDELETESHADERPROC __rglgen_glDeleteShader;
extern RGLSYMGLDETACHSHADERPROC __rglgen_glDetachShader;
extern RGLSYMGLDISABLEVERTEXATTRIBARRAYPROC __rglgen_glDisableVertexAttribArray;
extern RGLSYMGLENABLEVERTEXATTRIBARRAYPROC __rglgen_glEnableVertexAttribArray;
extern RGLSYMGLGETACTIVEATTRIBPROC __rglgen_glGetActiveAttrib;
extern RGLSYMGLGETACTIVEUNIFORMPROC __rglgen_glGetActiveUniform;
extern RGLSYMGLGETATTACHEDSHADERSPROC __rglgen_glGetAttachedShaders;
extern RGLSYMGLGETATTRIBLOCATIONPROC __rglgen_glGetAttribLocation;
extern RGLSYMGLGETPROGRAMIVPROC __rglgen_glGetProgramiv;
extern RGLSYMGLGETPROGRAMINFOLOGPROC __rglgen_glGetProgramInfoLog;
extern RGLSYMGLGETSHADERIVPROC __rglgen_glGetShaderiv;
extern RGLSYMGLGETSHADERINFOLOGPROC __rglgen_glGetShaderInfoLog;
extern RGLSYMGLGETSHADERSOURCEPROC __rglgen_glGetShaderSource;
extern RGLSYMGLGETUNIFORMLOCATIONPROC __rglgen_glGetUniformLocation;
extern RGLSYMGLGETUNIFORMFVPROC __rglgen_glGetUniformfv;
extern RGLSYMGLGETUNIFORMIVPROC __rglgen_glGetUniformiv;
extern RGLSYMGLGETVERTEXATTRIBDVPROC __rglgen_glGetVertexAttribdv;
extern RGLSYMGLGETVERTEXATTRIBFVPROC __rglgen_glGetVertexAttribfv;
extern RGLSYMGLGETVERTEXATTRIBIVPROC __rglgen_glGetVertexAttribiv;
extern RGLSYMGLGETVERTEXATTRIBPOINTERVPROC __rglgen_glGetVertexAttribPointerv;
extern RGLSYMGLISPROGRAMPROC __rglgen_glIsProgram;
extern RGLSYMGLISSHADERPROC __rglgen_glIsShader;
extern RGLSYMGLLINKPROGRAMPROC __rglgen_glLinkProgram;
extern RGLSYMGLSHADERSOURCEPROC __rglgen_glShaderSource;
extern RGLSYMGLUSEPROGRAMPROC __rglgen_glUseProgram;
extern RGLSYMGLUNIFORM1FPROC __rglgen_glUniform1f;
extern RGLSYMGLUNIFORM2FPROC __rglgen_glUniform2f;
extern RGLSYMGLUNIFORM3FPROC __rglgen_glUniform3f;
extern RGLSYMGLUNIFORM4FPROC __rglgen_glUniform4f;
extern RGLSYMGLUNIFORM1IPROC __rglgen_glUniform1i;
extern RGLSYMGLUNIFORM2IPROC __rglgen_glUniform2i;
extern RGLSYMGLUNIFORM3IPROC __rglgen_glUniform3i;
extern RGLSYMGLUNIFORM4IPROC __rglgen_glUniform4i;
extern RGLSYMGLUNIFORM1FVPROC __rglgen_glUniform1fv;
extern RGLSYMGLUNIFORM2FVPROC __rglgen_glUniform2fv;
extern RGLSYMGLUNIFORM3FVPROC __rglgen_glUniform3fv;
extern RGLSYMGLUNIFORM4FVPROC __rglgen_glUniform4fv;
extern RGLSYMGLUNIFORM1IVPROC __rglgen_glUniform1iv;
extern RGLSYMGLUNIFORM2IVPROC __rglgen_glUniform2iv;
extern RGLSYMGLUNIFORM3IVPROC __rglgen_glUniform3iv;
extern RGLSYMGLUNIFORM4IVPROC __rglgen_glUniform4iv;
extern RGLSYMGLUNIFORMMATRIX2FVPROC __rglgen_glUniformMatrix2fv;
extern RGLSYMGLUNIFORMMATRIX3FVPROC __rglgen_glUniformMatrix3fv;
extern RGLSYMGLUNIFORMMATRIX4FVPROC __rglgen_glUniformMatrix4fv;
extern RGLSYMGLVALIDATEPROGRAMPROC __rglgen_glValidateProgram;
extern RGLSYMGLVERTEXATTRIB1DPROC __rglgen_glVertexAttrib1d;
extern RGLSYMGLVERTEXATTRIB1DVPROC __rglgen_glVertexAttrib1dv;
extern RGLSYMGLVERTEXATTRIB1FPROC __rglgen_glVertexAttrib1f;
extern RGLSYMGLVERTEXATTRIB1FVPROC __rglgen_glVertexAttrib1fv;
extern RGLSYMGLVERTEXATTRIB1SPROC __rglgen_glVertexAttrib1s;
extern RGLSYMGLVERTEXATTRIB1SVPROC __rglgen_glVertexAttrib1sv;
extern RGLSYMGLVERTEXATTRIB2DPROC __rglgen_glVertexAttrib2d;
extern RGLSYMGLVERTEXATTRIB2DVPROC __rglgen_glVertexAttrib2dv;
extern RGLSYMGLVERTEXATTRIB2FPROC __rglgen_glVertexAttrib2f;
extern RGLSYMGLVERTEXATTRIB2FVPROC __rglgen_glVertexAttrib2fv;
extern RGLSYMGLVERTEXATTRIB2SPROC __rglgen_glVertexAttrib2s;
extern RGLSYMGLVERTEXATTRIB2SVPROC __rglgen_glVertexAttrib2sv;
extern RGLSYMGLVERTEXATTRIB3DPROC __rglgen_glVertexAttrib3d;
extern RGLSYMGLVERTEXATTRIB3DVPROC __rglgen_glVertexAttrib3dv;
extern RGLSYMGLVERTEXATTRIB3FPROC __rglgen_glVertexAttrib3f;
extern RGLSYMGLVERTEXATTRIB3FVPROC __rglgen_glVertexAttrib3fv;
extern RGLSYMGLVERTEXATTRIB3SPROC __rglgen_glVertexAttrib3s;
extern RGLSYMGLVERTEXATTRIB3SVPROC __rglgen_glVertexAttrib3sv;
extern RGLSYMGLVERTEXATTRIB4NBVPROC __rglgen_glVertexAttrib4Nbv;
extern RGLSYMGLVERTEXATTRIB4NIVPROC __rglgen_glVertexAttrib4Niv;
extern RGLSYMGLVERTEXATTRIB4NSVPROC __rglgen_glVertexAttrib4Nsv;
extern RGLSYMGLVERTEXATTRIB4NUBPROC __rglgen_glVertexAttrib4Nub;
extern RGLSYMGLVERTEXATTRIB4NUBVPROC __rglgen_glVertexAttrib4Nubv;
extern RGLSYMGLVERTEXATTRIB4NUIVPROC __rglgen_glVertexAttrib4Nuiv;
extern RGLSYMGLVERTEXATTRIB4NUSVPROC __rglgen_glVertexAttrib4Nusv;
extern RGLSYMGLVERTEXATTRIB4BVPROC __rglgen_glVertexAttrib4bv;
extern RGLSYMGLVERTEXATTRIB4DPROC __rglgen_glVertexAttrib4d;
extern RGLSYMGLVERTEXATTRIB4DVPROC __rglgen_glVertexAttrib4dv;
extern RGLSYMGLVERTEXATTRIB4FPROC __rglgen_glVertexAttrib4f;
extern RGLSYMGLVERTEXATTRIB4FVPROC __rglgen_glVertexAttrib4fv;
extern RGLSYMGLVERTEXATTRIB4IVPROC __rglgen_glVertexAttrib4iv;
extern RGLSYMGLVERTEXATTRIB4SPROC __rglgen_glVertexAttrib4s;
extern RGLSYMGLVERTEXATTRIB4SVPROC __rglgen_glVertexAttrib4sv;
extern RGLSYMGLVERTEXATTRIB4UBVPROC __rglgen_glVertexAttrib4ubv;
extern RGLSYMGLVERTEXATTRIB4UIVPROC __rglgen_glVertexAttrib4uiv;
extern RGLSYMGLVERTEXATTRIB4USVPROC __rglgen_glVertexAttrib4usv;
extern RGLSYMGLVERTEXATTRIBPOINTERPROC __rglgen_glVertexAttribPointer;
extern RGLSYMGLUNIFORMMATRIX2X3FVPROC __rglgen_glUniformMatrix2x3fv;
extern RGLSYMGLUNIFORMMATRIX3X2FVPROC __rglgen_glUniformMatrix3x2fv;
extern RGLSYMGLUNIFORMMATRIX2X4FVPROC __rglgen_glUniformMatrix2x4fv;
extern RGLSYMGLUNIFORMMATRIX4X2FVPROC __rglgen_glUniformMatrix4x2fv;
extern RGLSYMGLUNIFORMMATRIX3X4FVPROC __rglgen_glUniformMatrix3x4fv;
extern RGLSYMGLUNIFORMMATRIX4X3FVPROC __rglgen_glUniformMatrix4x3fv;
extern RGLSYMGLCOLORMASKIPROC __rglgen_glColorMaski;
extern RGLSYMGLGETBOOLEANI_VPROC __rglgen_glGetBooleani_v;
extern RGLSYMGLGETINTEGERI_VPROC __rglgen_glGetIntegeri_v;
extern RGLSYMGLENABLEIPROC __rglgen_glEnablei;
extern RGLSYMGLDISABLEIPROC __rglgen_glDisablei;
extern RGLSYMGLISENABLEDIPROC __rglgen_glIsEnabledi;
extern RGLSYMGLBEGINTRANSFORMFEEDBACKPROC __rglgen_glBeginTransformFeedback;
extern RGLSYMGLENDTRANSFORMFEEDBACKPROC __rglgen_glEndTransformFeedback;
extern RGLSYMGLBINDBUFFERRANGEPROC __rglgen_glBindBufferRange;
extern RGLSYMGLBINDBUFFERBASEPROC __rglgen_glBindBufferBase;
extern RGLSYMGLTRANSFORMFEEDBACKVARYINGSPROC __rglgen_glTransformFeedbackVaryings;
extern RGLSYMGLGETTRANSFORMFEEDBACKVARYINGPROC __rglgen_glGetTransformFeedbackVarying;
extern RGLSYMGLCLAMPCOLORPROC __rglgen_glClampColor;
extern RGLSYMGLBEGINCONDITIONALRENDERPROC __rglgen_glBeginConditionalRender;
extern RGLSYMGLENDCONDITIONALRENDERPROC __rglgen_glEndConditionalRender;
extern RGLSYMGLVERTEXATTRIBIPOINTERPROC __rglgen_glVertexAttribIPointer;
extern RGLSYMGLGETVERTEXATTRIBIIVPROC __rglgen_glGetVertexAttribIiv;
extern RGLSYMGLGETVERTEXATTRIBIUIVPROC __rglgen_glGetVertexAttribIuiv;
extern RGLSYMGLVERTEXATTRIBI1IPROC __rglgen_glVertexAttribI1i;
extern RGLSYMGLVERTEXATTRIBI2IPROC __rglgen_glVertexAttribI2i;
extern RGLSYMGLVERTEXATTRIBI3IPROC __rglgen_glVertexAttribI3i;
extern RGLSYMGLVERTEXATTRIBI4IPROC __rglgen_glVertexAttribI4i;
extern RGLSYMGLVERTEXATTRIBI1UIPROC __rglgen_glVertexAttribI1ui;
extern RGLSYMGLVERTEXATTRIBI2UIPROC __rglgen_glVertexAttribI2ui;
extern RGLSYMGLVERTEXATTRIBI3UIPROC __rglgen_glVertexAttribI3ui;
extern RGLSYMGLVERTEXATTRIBI4UIPROC __rglgen_glVertexAttribI4ui;
extern RGLSYMGLVERTEXATTRIBI1IVPROC __rglgen_glVertexAttribI1iv;
extern RGLSYMGLVERTEXATTRIBI2IVPROC __rglgen_glVertexAttribI2iv;
extern RGLSYMGLVERTEXATTRIBI3IVPROC __rglgen_glVertexAttribI3iv;
extern RGLSYMGLVERTEXATTRIBI4IVPROC __rglgen_glVertexAttribI4iv;
extern RGLSYMGLVERTEXATTRIBI1UIVPROC __rglgen_glVertexAttribI1uiv;
extern RGLSYMGLVERTEXATTRIBI2UIVPROC __rglgen_glVertexAttribI2uiv;
extern RGLSYMGLVERTEXATTRIBI3UIVPROC __rglgen_glVertexAttribI3uiv;
extern RGLSYMGLVERTEXATTRIBI4UIVPROC __rglgen_glVertexAttribI4uiv;
extern RGLSYMGLVERTEXATTRIBI4BVPROC __rglgen_glVertexAttribI4bv;
extern RGLSYMGLVERTEXATTRIBI4SVPROC __rglgen_glVertexAttribI4sv;
extern RGLSYMGLVERTEXATTRIBI4UBVPROC __rglgen_glVertexAttribI4ubv;
extern RGLSYMGLVERTEXATTRIBI4USVPROC __rglgen_glVertexAttribI4usv;
extern RGLSYMGLGETUNIFORMUIVPROC __rglgen_glGetUniformuiv;
extern RGLSYMGLBINDFRAGDATALOCATIONPROC __rglgen_glBindFragDataLocation;
extern RGLSYMGLGETFRAGDATALOCATIONPROC __rglgen_glGetFragDataLocation;
extern RGLSYMGLUNIFORM1UIPROC __rglgen_glUniform1ui;
extern RGLSYMGLUNIFORM2UIPROC __rglgen_glUniform2ui;
extern RGLSYMGLUNIFORM3UIPROC __rglgen_glUniform3ui;
extern RGLSYMGLUNIFORM4UIPROC __rglgen_glUniform4ui;
extern RGLSYMGLUNIFORM1UIVPROC __rglgen_glUniform1uiv;
extern RGLSYMGLUNIFORM2UIVPROC __rglgen_glUniform2uiv;
extern RGLSYMGLUNIFORM3UIVPROC __rglgen_glUniform3uiv;
extern RGLSYMGLUNIFORM4UIVPROC __rglgen_glUniform4uiv;
extern RGLSYMGLTEXPARAMETERIIVPROC __rglgen_glTexParameterIiv;
extern RGLSYMGLTEXPARAMETERIUIVPROC __rglgen_glTexParameterIuiv;
extern RGLSYMGLGETTEXPARAMETERIIVPROC __rglgen_glGetTexParameterIiv;
extern RGLSYMGLGETTEXPARAMETERIUIVPROC __rglgen_glGetTexParameterIuiv;
extern RGLSYMGLCLEARBUFFERIVPROC __rglgen_glClearBufferiv;
extern RGLSYMGLCLEARBUFFERUIVPROC __rglgen_glClearBufferuiv;
extern RGLSYMGLCLEARBUFFERFVPROC __rglgen_glClearBufferfv;
extern RGLSYMGLCLEARBUFFERFIPROC __rglgen_glClearBufferfi;
extern RGLSYMGLGETSTRINGIPROC __rglgen_glGetStringi;
extern RGLSYMGLISRENDERBUFFERPROC __rglgen_glIsRenderbuffer;
extern RGLSYMGLBINDRENDERBUFFERPROC __rglgen_glBindRenderbuffer;
extern RGLSYMGLDELETERENDERBUFFERSPROC __rglgen_glDeleteRenderbuffers;
extern RGLSYMGLGENRENDERBUFFERSPROC __rglgen_glGenRenderbuffers;
extern RGLSYMGLRENDERBUFFERSTORAGEPROC __rglgen_glRenderbufferStorage;
extern RGLSYMGLGETRENDERBUFFERPARAMETERIVPROC __rglgen_glGetRenderbufferParameteriv;
extern RGLSYMGLISFRAMEBUFFERPROC __rglgen_glIsFramebuffer;
extern RGLSYMGLBINDFRAMEBUFFERPROC __rglgen_glBindFramebuffer;
extern RGLSYMGLDELETEFRAMEBUFFERSPROC __rglgen_glDeleteFramebuffers;
extern RGLSYMGLGENFRAMEBUFFERSPROC __rglgen_glGenFramebuffers;
extern RGLSYMGLCHECKFRAMEBUFFERSTATUSPROC __rglgen_glCheckFramebufferStatus;
extern RGLSYMGLFRAMEBUFFERTEXTURE1DPROC __rglgen_glFramebufferTexture1D;
extern RGLSYMGLFRAMEBUFFERTEXTURE2DPROC __rglgen_glFramebufferTexture2D;
extern RGLSYMGLFRAMEBUFFERTEXTURE3DPROC __rglgen_glFramebufferTexture3D;
extern RGLSYMGLFRAMEBUFFERRENDERBUFFERPROC __rglgen_glFramebufferRenderbuffer;
extern RGLSYMGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC __rglgen_glGetFramebufferAttachmentParameteriv;
extern RGLSYMGLGENERATEMIPMAPPROC __rglgen_glGenerateMipmap;
extern RGLSYMGLBLITFRAMEBUFFERPROC __rglgen_glBlitFramebuffer;
extern RGLSYMGLRENDERBUFFERSTORAGEMULTISAMPLEPROC __rglgen_glRenderbufferStorageMultisample;
extern RGLSYMGLFRAMEBUFFERTEXTURELAYERPROC __rglgen_glFramebufferTextureLayer;
extern RGLSYMGLMAPBUFFERRANGEPROC __rglgen_glMapBufferRange;
extern RGLSYMGLFLUSHMAPPEDBUFFERRANGEPROC __rglgen_glFlushMappedBufferRange;
extern RGLSYMGLBINDVERTEXARRAYPROC __rglgen_glBindVertexArray;
extern RGLSYMGLDELETEVERTEXARRAYSPROC __rglgen_glDeleteVertexArrays;
extern RGLSYMGLGENVERTEXARRAYSPROC __rglgen_glGenVertexArrays;
extern RGLSYMGLISVERTEXARRAYPROC __rglgen_glIsVertexArray;
extern RGLSYMGLDRAWARRAYSINSTANCEDPROC __rglgen_glDrawArraysInstanced;
extern RGLSYMGLDRAWELEMENTSINSTANCEDPROC __rglgen_glDrawElementsInstanced;
extern RGLSYMGLTEXBUFFERPROC __rglgen_glTexBuffer;
extern RGLSYMGLPRIMITIVERESTARTINDEXPROC __rglgen_glPrimitiveRestartIndex;
extern RGLSYMGLCOPYBUFFERSUBDATAPROC __rglgen_glCopyBufferSubData;
extern RGLSYMGLGETUNIFORMINDICESPROC __rglgen_glGetUniformIndices;
extern RGLSYMGLGETACTIVEUNIFORMSIVPROC __rglgen_glGetActiveUniformsiv;
extern RGLSYMGLGETACTIVEUNIFORMNAMEPROC __rglgen_glGetActiveUniformName;
extern RGLSYMGLGETUNIFORMBLOCKINDEXPROC __rglgen_glGetUniformBlockIndex;
extern RGLSYMGLGETACTIVEUNIFORMBLOCKIVPROC __rglgen_glGetActiveUniformBlockiv;
extern RGLSYMGLGETACTIVEUNIFORMBLOCKNAMEPROC __rglgen_glGetActiveUniformBlockName;
extern RGLSYMGLUNIFORMBLOCKBINDINGPROC __rglgen_glUniformBlockBinding;
extern RGLSYMGLDRAWELEMENTSBASEVERTEXPROC __rglgen_glDrawElementsBaseVertex;
extern RGLSYMGLDRAWRANGEELEMENTSBASEVERTEXPROC __rglgen_glDrawRangeElementsBaseVertex;
extern RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC __rglgen_glDrawElementsInstancedBaseVertex;
extern RGLSYMGLMULTIDRAWELEMENTSBASEVERTEXPROC __rglgen_glMultiDrawElementsBaseVertex;
extern RGLSYMGLPROVOKINGVERTEXPROC __rglgen_glProvokingVertex;
extern RGLSYMGLFENCESYNCPROC __rglgen_glFenceSync;
extern RGLSYMGLISSYNCPROC __rglgen_glIsSync;
extern RGLSYMGLDELETESYNCPROC __rglgen_glDeleteSync;
extern RGLSYMGLCLIENTWAITSYNCPROC __rglgen_glClientWaitSync;
extern RGLSYMGLWAITSYNCPROC __rglgen_glWaitSync;
extern RGLSYMGLGETINTEGER64VPROC __rglgen_glGetInteger64v;
extern RGLSYMGLGETSYNCIVPROC __rglgen_glGetSynciv;
extern RGLSYMGLGETINTEGER64I_VPROC __rglgen_glGetInteger64i_v;
extern RGLSYMGLGETBUFFERPARAMETERI64VPROC __rglgen_glGetBufferParameteri64v;
extern RGLSYMGLFRAMEBUFFERTEXTUREPROC __rglgen_glFramebufferTexture;
extern RGLSYMGLTEXIMAGE2DMULTISAMPLEPROC __rglgen_glTexImage2DMultisample;
extern RGLSYMGLTEXIMAGE3DMULTISAMPLEPROC __rglgen_glTexImage3DMultisample;
extern RGLSYMGLGETMULTISAMPLEFVPROC __rglgen_glGetMultisamplefv;
extern RGLSYMGLSAMPLEMASKIPROC __rglgen_glSampleMaski;
extern RGLSYMGLBINDFRAGDATALOCATIONINDEXEDPROC __rglgen_glBindFragDataLocationIndexed;
extern RGLSYMGLGETFRAGDATAINDEXPROC __rglgen_glGetFragDataIndex;
extern RGLSYMGLGENSAMPLERSPROC __rglgen_glGenSamplers;
extern RGLSYMGLDELETESAMPLERSPROC __rglgen_glDeleteSamplers;
extern RGLSYMGLISSAMPLERPROC __rglgen_glIsSampler;
extern RGLSYMGLBINDSAMPLERPROC __rglgen_glBindSampler;
extern RGLSYMGLSAMPLERPARAMETERIPROC __rglgen_glSamplerParameteri;
extern RGLSYMGLSAMPLERPARAMETERIVPROC __rglgen_glSamplerParameteriv;
extern RGLSYMGLSAMPLERPARAMETERFPROC __rglgen_glSamplerParameterf;
extern RGLSYMGLSAMPLERPARAMETERFVPROC __rglgen_glSamplerParameterfv;
extern RGLSYMGLSAMPLERPARAMETERIIVPROC __rglgen_glSamplerParameterIiv;
extern RGLSYMGLSAMPLERPARAMETERIUIVPROC __rglgen_glSamplerParameterIuiv;
extern RGLSYMGLGETSAMPLERPARAMETERIVPROC __rglgen_glGetSamplerParameteriv;
extern RGLSYMGLGETSAMPLERPARAMETERIIVPROC __rglgen_glGetSamplerParameterIiv;
extern RGLSYMGLGETSAMPLERPARAMETERFVPROC __rglgen_glGetSamplerParameterfv;
extern RGLSYMGLGETSAMPLERPARAMETERIUIVPROC __rglgen_glGetSamplerParameterIuiv;
extern RGLSYMGLQUERYCOUNTERPROC __rglgen_glQueryCounter;
extern RGLSYMGLGETQUERYOBJECTI64VPROC __rglgen_glGetQueryObjecti64v;
extern RGLSYMGLGETQUERYOBJECTUI64VPROC __rglgen_glGetQueryObjectui64v;
extern RGLSYMGLVERTEXATTRIBDIVISORPROC __rglgen_glVertexAttribDivisor;
extern RGLSYMGLVERTEXATTRIBP1UIPROC __rglgen_glVertexAttribP1ui;
extern RGLSYMGLVERTEXATTRIBP1UIVPROC __rglgen_glVertexAttribP1uiv;
extern RGLSYMGLVERTEXATTRIBP2UIPROC __rglgen_glVertexAttribP2ui;
extern RGLSYMGLVERTEXATTRIBP2UIVPROC __rglgen_glVertexAttribP2uiv;
extern RGLSYMGLVERTEXATTRIBP3UIPROC __rglgen_glVertexAttribP3ui;
extern RGLSYMGLVERTEXATTRIBP3UIVPROC __rglgen_glVertexAttribP3uiv;
extern RGLSYMGLVERTEXATTRIBP4UIPROC __rglgen_glVertexAttribP4ui;
extern RGLSYMGLVERTEXATTRIBP4UIVPROC __rglgen_glVertexAttribP4uiv;
extern RGLSYMGLMINSAMPLESHADINGPROC __rglgen_glMinSampleShading;
extern RGLSYMGLBLENDEQUATIONIPROC __rglgen_glBlendEquationi;
extern RGLSYMGLBLENDEQUATIONSEPARATEIPROC __rglgen_glBlendEquationSeparatei;
extern RGLSYMGLBLENDFUNCIPROC __rglgen_glBlendFunci;
extern RGLSYMGLBLENDFUNCSEPARATEIPROC __rglgen_glBlendFuncSeparatei;
extern RGLSYMGLDRAWARRAYSINDIRECTPROC __rglgen_glDrawArraysIndirect;
extern RGLSYMGLDRAWELEMENTSINDIRECTPROC __rglgen_glDrawElementsIndirect;
extern RGLSYMGLUNIFORM1DPROC __rglgen_glUniform1d;
extern RGLSYMGLUNIFORM2DPROC __rglgen_glUniform2d;
extern RGLSYMGLUNIFORM3DPROC __rglgen_glUniform3d;
extern RGLSYMGLUNIFORM4DPROC __rglgen_glUniform4d;
extern RGLSYMGLUNIFORM1DVPROC __rglgen_glUniform1dv;
extern RGLSYMGLUNIFORM2DVPROC __rglgen_glUniform2dv;
extern RGLSYMGLUNIFORM3DVPROC __rglgen_glUniform3dv;
extern RGLSYMGLUNIFORM4DVPROC __rglgen_glUniform4dv;
extern RGLSYMGLUNIFORMMATRIX2DVPROC __rglgen_glUniformMatrix2dv;
extern RGLSYMGLUNIFORMMATRIX3DVPROC __rglgen_glUniformMatrix3dv;
extern RGLSYMGLUNIFORMMATRIX4DVPROC __rglgen_glUniformMatrix4dv;
extern RGLSYMGLUNIFORMMATRIX2X3DVPROC __rglgen_glUniformMatrix2x3dv;
extern RGLSYMGLUNIFORMMATRIX2X4DVPROC __rglgen_glUniformMatrix2x4dv;
extern RGLSYMGLUNIFORMMATRIX3X2DVPROC __rglgen_glUniformMatrix3x2dv;
extern RGLSYMGLUNIFORMMATRIX3X4DVPROC __rglgen_glUniformMatrix3x4dv;
extern RGLSYMGLUNIFORMMATRIX4X2DVPROC __rglgen_glUniformMatrix4x2dv;
extern RGLSYMGLUNIFORMMATRIX4X3DVPROC __rglgen_glUniformMatrix4x3dv;
extern RGLSYMGLGETUNIFORMDVPROC __rglgen_glGetUniformdv;
extern RGLSYMGLGETSUBROUTINEUNIFORMLOCATIONPROC __rglgen_glGetSubroutineUniformLocation;
extern RGLSYMGLGETSUBROUTINEINDEXPROC __rglgen_glGetSubroutineIndex;
extern RGLSYMGLGETACTIVESUBROUTINEUNIFORMIVPROC __rglgen_glGetActiveSubroutineUniformiv;
extern RGLSYMGLGETACTIVESUBROUTINEUNIFORMNAMEPROC __rglgen_glGetActiveSubroutineUniformName;
extern RGLSYMGLGETACTIVESUBROUTINENAMEPROC __rglgen_glGetActiveSubroutineName;
extern RGLSYMGLUNIFORMSUBROUTINESUIVPROC __rglgen_glUniformSubroutinesuiv;
extern RGLSYMGLGETUNIFORMSUBROUTINEUIVPROC __rglgen_glGetUniformSubroutineuiv;
extern RGLSYMGLGETPROGRAMSTAGEIVPROC __rglgen_glGetProgramStageiv;
extern RGLSYMGLPATCHPARAMETERIPROC __rglgen_glPatchParameteri;
extern RGLSYMGLPATCHPARAMETERFVPROC __rglgen_glPatchParameterfv;
extern RGLSYMGLBINDTRANSFORMFEEDBACKPROC __rglgen_glBindTransformFeedback;
extern RGLSYMGLDELETETRANSFORMFEEDBACKSPROC __rglgen_glDeleteTransformFeedbacks;
extern RGLSYMGLGENTRANSFORMFEEDBACKSPROC __rglgen_glGenTransformFeedbacks;
extern RGLSYMGLISTRANSFORMFEEDBACKPROC __rglgen_glIsTransformFeedback;
extern RGLSYMGLPAUSETRANSFORMFEEDBACKPROC __rglgen_glPauseTransformFeedback;
extern RGLSYMGLRESUMETRANSFORMFEEDBACKPROC __rglgen_glResumeTransformFeedback;
extern RGLSYMGLDRAWTRANSFORMFEEDBACKPROC __rglgen_glDrawTransformFeedback;
extern RGLSYMGLDRAWTRANSFORMFEEDBACKSTREAMPROC __rglgen_glDrawTransformFeedbackStream;
extern RGLSYMGLBEGINQUERYINDEXEDPROC __rglgen_glBeginQueryIndexed;
extern RGLSYMGLENDQUERYINDEXEDPROC __rglgen_glEndQueryIndexed;
extern RGLSYMGLGETQUERYINDEXEDIVPROC __rglgen_glGetQueryIndexediv;
extern RGLSYMGLRELEASESHADERCOMPILERPROC __rglgen_glReleaseShaderCompiler;
extern RGLSYMGLSHADERBINARYPROC __rglgen_glShaderBinary;
extern RGLSYMGLGETSHADERPRECISIONFORMATPROC __rglgen_glGetShaderPrecisionFormat;
extern RGLSYMGLDEPTHRANGEFPROC __rglgen_glDepthRangef;
extern RGLSYMGLCLEARDEPTHFPROC __rglgen_glClearDepthf;
extern RGLSYMGLGETPROGRAMBINARYPROC __rglgen_glGetProgramBinary;
extern RGLSYMGLPROGRAMBINARYPROC __rglgen_glProgramBinary;
extern RGLSYMGLPROGRAMPARAMETERIPROC __rglgen_glProgramParameteri;
extern RGLSYMGLUSEPROGRAMSTAGESPROC __rglgen_glUseProgramStages;
extern RGLSYMGLACTIVESHADERPROGRAMPROC __rglgen_glActiveShaderProgram;
extern RGLSYMGLCREATESHADERPROGRAMVPROC __rglgen_glCreateShaderProgramv;
extern RGLSYMGLBINDPROGRAMPIPELINEPROC __rglgen_glBindProgramPipeline;
extern RGLSYMGLDELETEPROGRAMPIPELINESPROC __rglgen_glDeleteProgramPipelines;
extern RGLSYMGLGENPROGRAMPIPELINESPROC __rglgen_glGenProgramPipelines;
extern RGLSYMGLISPROGRAMPIPELINEPROC __rglgen_glIsProgramPipeline;
extern RGLSYMGLGETPROGRAMPIPELINEIVPROC __rglgen_glGetProgramPipelineiv;
extern RGLSYMGLPROGRAMUNIFORM1IPROC __rglgen_glProgramUniform1i;
extern RGLSYMGLPROGRAMUNIFORM1IVPROC __rglgen_glProgramUniform1iv;
extern RGLSYMGLPROGRAMUNIFORM1FPROC __rglgen_glProgramUniform1f;
extern RGLSYMGLPROGRAMUNIFORM1FVPROC __rglgen_glProgramUniform1fv;
extern RGLSYMGLPROGRAMUNIFORM1DPROC __rglgen_glProgramUniform1d;
extern RGLSYMGLPROGRAMUNIFORM1DVPROC __rglgen_glProgramUniform1dv;
extern RGLSYMGLPROGRAMUNIFORM1UIPROC __rglgen_glProgramUniform1ui;
extern RGLSYMGLPROGRAMUNIFORM1UIVPROC __rglgen_glProgramUniform1uiv;
extern RGLSYMGLPROGRAMUNIFORM2IPROC __rglgen_glProgramUniform2i;
extern RGLSYMGLPROGRAMUNIFORM2IVPROC __rglgen_glProgramUniform2iv;
extern RGLSYMGLPROGRAMUNIFORM2FPROC __rglgen_glProgramUniform2f;
extern RGLSYMGLPROGRAMUNIFORM2FVPROC __rglgen_glProgramUniform2fv;
extern RGLSYMGLPROGRAMUNIFORM2DPROC __rglgen_glProgramUniform2d;
extern RGLSYMGLPROGRAMUNIFORM2DVPROC __rglgen_glProgramUniform2dv;
extern RGLSYMGLPROGRAMUNIFORM2UIPROC __rglgen_glProgramUniform2ui;
extern RGLSYMGLPROGRAMUNIFORM2UIVPROC __rglgen_glProgramUniform2uiv;
extern RGLSYMGLPROGRAMUNIFORM3IPROC __rglgen_glProgramUniform3i;
extern RGLSYMGLPROGRAMUNIFORM3IVPROC __rglgen_glProgramUniform3iv;
extern RGLSYMGLPROGRAMUNIFORM3FPROC __rglgen_glProgramUniform3f;
extern RGLSYMGLPROGRAMUNIFORM3FVPROC __rglgen_glProgramUniform3fv;
extern RGLSYMGLPROGRAMUNIFORM3DPROC __rglgen_glProgramUniform3d;
extern RGLSYMGLPROGRAMUNIFORM3DVPROC __rglgen_glProgramUniform3dv;
extern RGLSYMGLPROGRAMUNIFORM3UIPROC __rglgen_glProgramUniform3ui;
extern RGLSYMGLPROGRAMUNIFORM3UIVPROC __rglgen_glProgramUniform3uiv;
extern RGLSYMGLPROGRAMUNIFORM4IPROC __rglgen_glProgramUniform4i;
extern RGLSYMGLPROGRAMUNIFORM4IVPROC __rglgen_glProgramUniform4iv;
extern RGLSYMGLPROGRAMUNIFORM4FPROC __rglgen_glProgramUniform4f;
extern RGLSYMGLPROGRAMUNIFORM4FVPROC __rglgen_glProgramUniform4fv;
extern RGLSYMGLPROGRAMUNIFORM4DPROC __rglgen_glProgramUniform4d;
extern RGLSYMGLPROGRAMUNIFORM4DVPROC __rglgen_glProgramUniform4dv;
extern RGLSYMGLPROGRAMUNIFORM4UIPROC __rglgen_glProgramUniform4ui;
extern RGLSYMGLPROGRAMUNIFORM4UIVPROC __rglgen_glProgramUniform4uiv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2FVPROC __rglgen_glProgramUniformMatrix2fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3FVPROC __rglgen_glProgramUniformMatrix3fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4FVPROC __rglgen_glProgramUniformMatrix4fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2DVPROC __rglgen_glProgramUniformMatrix2dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3DVPROC __rglgen_glProgramUniformMatrix3dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4DVPROC __rglgen_glProgramUniformMatrix4dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2X3FVPROC __rglgen_glProgramUniformMatrix2x3fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3X2FVPROC __rglgen_glProgramUniformMatrix3x2fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2X4FVPROC __rglgen_glProgramUniformMatrix2x4fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4X2FVPROC __rglgen_glProgramUniformMatrix4x2fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3X4FVPROC __rglgen_glProgramUniformMatrix3x4fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4X3FVPROC __rglgen_glProgramUniformMatrix4x3fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2X3DVPROC __rglgen_glProgramUniformMatrix2x3dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3X2DVPROC __rglgen_glProgramUniformMatrix3x2dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2X4DVPROC __rglgen_glProgramUniformMatrix2x4dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4X2DVPROC __rglgen_glProgramUniformMatrix4x2dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3X4DVPROC __rglgen_glProgramUniformMatrix3x4dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4X3DVPROC __rglgen_glProgramUniformMatrix4x3dv;
extern RGLSYMGLVALIDATEPROGRAMPIPELINEPROC __rglgen_glValidateProgramPipeline;
extern RGLSYMGLGETPROGRAMPIPELINEINFOLOGPROC __rglgen_glGetProgramPipelineInfoLog;
extern RGLSYMGLVERTEXATTRIBL1DPROC __rglgen_glVertexAttribL1d;
extern RGLSYMGLVERTEXATTRIBL2DPROC __rglgen_glVertexAttribL2d;
extern RGLSYMGLVERTEXATTRIBL3DPROC __rglgen_glVertexAttribL3d;
extern RGLSYMGLVERTEXATTRIBL4DPROC __rglgen_glVertexAttribL4d;
extern RGLSYMGLVERTEXATTRIBL1DVPROC __rglgen_glVertexAttribL1dv;
extern RGLSYMGLVERTEXATTRIBL2DVPROC __rglgen_glVertexAttribL2dv;
extern RGLSYMGLVERTEXATTRIBL3DVPROC __rglgen_glVertexAttribL3dv;
extern RGLSYMGLVERTEXATTRIBL4DVPROC __rglgen_glVertexAttribL4dv;
extern RGLSYMGLVERTEXATTRIBLPOINTERPROC __rglgen_glVertexAttribLPointer;
extern RGLSYMGLGETVERTEXATTRIBLDVPROC __rglgen_glGetVertexAttribLdv;
extern RGLSYMGLVIEWPORTARRAYVPROC __rglgen_glViewportArrayv;
extern RGLSYMGLVIEWPORTINDEXEDFPROC __rglgen_glViewportIndexedf;
extern RGLSYMGLVIEWPORTINDEXEDFVPROC __rglgen_glViewportIndexedfv;
extern RGLSYMGLSCISSORARRAYVPROC __rglgen_glScissorArrayv;
extern RGLSYMGLSCISSORINDEXEDPROC __rglgen_glScissorIndexed;
extern RGLSYMGLSCISSORINDEXEDVPROC __rglgen_glScissorIndexedv;
extern RGLSYMGLDEPTHRANGEARRAYVPROC __rglgen_glDepthRangeArrayv;
extern RGLSYMGLDEPTHRANGEINDEXEDPROC __rglgen_glDepthRangeIndexed;
extern RGLSYMGLGETFLOATI_VPROC __rglgen_glGetFloati_v;
extern RGLSYMGLGETDOUBLEI_VPROC __rglgen_glGetDoublei_v;
extern RGLSYMGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC __rglgen_glDrawArraysInstancedBaseInstance;
extern RGLSYMGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC __rglgen_glDrawElementsInstancedBaseInstance;
extern RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC __rglgen_glDrawElementsInstancedBaseVertexBaseInstance;
extern RGLSYMGLGETINTERNALFORMATIVPROC __rglgen_glGetInternalformativ;
extern RGLSYMGLGETACTIVEATOMICCOUNTERBUFFERIVPROC __rglgen_glGetActiveAtomicCounterBufferiv;
extern RGLSYMGLBINDIMAGETEXTUREPROC __rglgen_glBindImageTexture;
extern RGLSYMGLMEMORYBARRIERPROC __rglgen_glMemoryBarrier;
extern RGLSYMGLTEXSTORAGE1DPROC __rglgen_glTexStorage1D;
extern RGLSYMGLTEXSTORAGE2DPROC __rglgen_glTexStorage2D;
extern RGLSYMGLTEXSTORAGE3DPROC __rglgen_glTexStorage3D;
extern RGLSYMGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC __rglgen_glDrawTransformFeedbackInstanced;
extern RGLSYMGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC __rglgen_glDrawTransformFeedbackStreamInstanced;
extern RGLSYMGLCLEARBUFFERDATAPROC __rglgen_glClearBufferData;
extern RGLSYMGLCLEARBUFFERSUBDATAPROC __rglgen_glClearBufferSubData;
extern RGLSYMGLDISPATCHCOMPUTEPROC __rglgen_glDispatchCompute;
extern RGLSYMGLDISPATCHCOMPUTEINDIRECTPROC __rglgen_glDispatchComputeIndirect;
extern RGLSYMGLCOPYIMAGESUBDATAPROC __rglgen_glCopyImageSubData;
extern RGLSYMGLFRAMEBUFFERPARAMETERIPROC __rglgen_glFramebufferParameteri;
extern RGLSYMGLGETFRAMEBUFFERPARAMETERIVPROC __rglgen_glGetFramebufferParameteriv;
extern RGLSYMGLGETINTERNALFORMATI64VPROC __rglgen_glGetInternalformati64v;
extern RGLSYMGLINVALIDATETEXSUBIMAGEPROC __rglgen_glInvalidateTexSubImage;
extern RGLSYMGLINVALIDATETEXIMAGEPROC __rglgen_glInvalidateTexImage;
extern RGLSYMGLINVALIDATEBUFFERSUBDATAPROC __rglgen_glInvalidateBufferSubData;
extern RGLSYMGLINVALIDATEBUFFERDATAPROC __rglgen_glInvalidateBufferData;
extern RGLSYMGLINVALIDATEFRAMEBUFFERPROC __rglgen_glInvalidateFramebuffer;
extern RGLSYMGLINVALIDATESUBFRAMEBUFFERPROC __rglgen_glInvalidateSubFramebuffer;
extern RGLSYMGLMULTIDRAWARRAYSINDIRECTPROC __rglgen_glMultiDrawArraysIndirect;
extern RGLSYMGLMULTIDRAWELEMENTSINDIRECTPROC __rglgen_glMultiDrawElementsIndirect;
extern RGLSYMGLGETPROGRAMINTERFACEIVPROC __rglgen_glGetProgramInterfaceiv;
extern RGLSYMGLGETPROGRAMRESOURCEINDEXPROC __rglgen_glGetProgramResourceIndex;
extern RGLSYMGLGETPROGRAMRESOURCENAMEPROC __rglgen_glGetProgramResourceName;
extern RGLSYMGLGETPROGRAMRESOURCEIVPROC __rglgen_glGetProgramResourceiv;
extern RGLSYMGLGETPROGRAMRESOURCELOCATIONPROC __rglgen_glGetProgramResourceLocation;
extern RGLSYMGLGETPROGRAMRESOURCELOCATIONINDEXPROC __rglgen_glGetProgramResourceLocationIndex;
extern RGLSYMGLSHADERSTORAGEBLOCKBINDINGPROC __rglgen_glShaderStorageBlockBinding;
extern RGLSYMGLTEXBUFFERRANGEPROC __rglgen_glTexBufferRange;
extern RGLSYMGLTEXSTORAGE2DMULTISAMPLEPROC __rglgen_glTexStorage2DMultisample;
extern RGLSYMGLTEXSTORAGE3DMULTISAMPLEPROC __rglgen_glTexStorage3DMultisample;
extern RGLSYMGLTEXTUREVIEWPROC __rglgen_glTextureView;
extern RGLSYMGLBINDVERTEXBUFFERPROC __rglgen_glBindVertexBuffer;
extern RGLSYMGLVERTEXATTRIBFORMATPROC __rglgen_glVertexAttribFormat;
extern RGLSYMGLVERTEXATTRIBIFORMATPROC __rglgen_glVertexAttribIFormat;
extern RGLSYMGLVERTEXATTRIBLFORMATPROC __rglgen_glVertexAttribLFormat;
extern RGLSYMGLVERTEXATTRIBBINDINGPROC __rglgen_glVertexAttribBinding;
extern RGLSYMGLVERTEXBINDINGDIVISORPROC __rglgen_glVertexBindingDivisor;
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
extern RGLSYMGLBUFFERSTORAGEPROC __rglgen_glBufferStorage;
extern RGLSYMGLCLEARTEXIMAGEPROC __rglgen_glClearTexImage;
extern RGLSYMGLCLEARTEXSUBIMAGEPROC __rglgen_glClearTexSubImage;
extern RGLSYMGLBINDBUFFERSBASEPROC __rglgen_glBindBuffersBase;
extern RGLSYMGLBINDBUFFERSRANGEPROC __rglgen_glBindBuffersRange;
extern RGLSYMGLBINDTEXTURESPROC __rglgen_glBindTextures;
extern RGLSYMGLBINDSAMPLERSPROC __rglgen_glBindSamplers;
extern RGLSYMGLBINDIMAGETEXTURESPROC __rglgen_glBindImageTextures;
extern RGLSYMGLBINDVERTEXBUFFERSPROC __rglgen_glBindVertexBuffers;
extern RGLSYMGLCLIPCONTROLPROC __rglgen_glClipControl;
extern RGLSYMGLCREATETRANSFORMFEEDBACKSPROC __rglgen_glCreateTransformFeedbacks;
extern RGLSYMGLTRANSFORMFEEDBACKBUFFERBASEPROC __rglgen_glTransformFeedbackBufferBase;
extern RGLSYMGLTRANSFORMFEEDBACKBUFFERRANGEPROC __rglgen_glTransformFeedbackBufferRange;
extern RGLSYMGLGETTRANSFORMFEEDBACKIVPROC __rglgen_glGetTransformFeedbackiv;
extern RGLSYMGLGETTRANSFORMFEEDBACKI_VPROC __rglgen_glGetTransformFeedbacki_v;
extern RGLSYMGLGETTRANSFORMFEEDBACKI64_VPROC __rglgen_glGetTransformFeedbacki64_v;
extern RGLSYMGLCREATEBUFFERSPROC __rglgen_glCreateBuffers;
extern RGLSYMGLNAMEDBUFFERSTORAGEPROC __rglgen_glNamedBufferStorage;
extern RGLSYMGLNAMEDBUFFERDATAPROC __rglgen_glNamedBufferData;
extern RGLSYMGLNAMEDBUFFERSUBDATAPROC __rglgen_glNamedBufferSubData;
extern RGLSYMGLCOPYNAMEDBUFFERSUBDATAPROC __rglgen_glCopyNamedBufferSubData;
extern RGLSYMGLCLEARNAMEDBUFFERDATAPROC __rglgen_glClearNamedBufferData;
extern RGLSYMGLCLEARNAMEDBUFFERSUBDATAPROC __rglgen_glClearNamedBufferSubData;
extern RGLSYMGLMAPNAMEDBUFFERPROC __rglgen_glMapNamedBuffer;
extern RGLSYMGLMAPNAMEDBUFFERRANGEPROC __rglgen_glMapNamedBufferRange;
extern RGLSYMGLUNMAPNAMEDBUFFERPROC __rglgen_glUnmapNamedBuffer;
extern RGLSYMGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC __rglgen_glFlushMappedNamedBufferRange;
extern RGLSYMGLGETNAMEDBUFFERPARAMETERIVPROC __rglgen_glGetNamedBufferParameteriv;
extern RGLSYMGLGETNAMEDBUFFERPARAMETERI64VPROC __rglgen_glGetNamedBufferParameteri64v;
extern RGLSYMGLGETNAMEDBUFFERPOINTERVPROC __rglgen_glGetNamedBufferPointerv;
extern RGLSYMGLGETNAMEDBUFFERSUBDATAPROC __rglgen_glGetNamedBufferSubData;
extern RGLSYMGLCREATEFRAMEBUFFERSPROC __rglgen_glCreateFramebuffers;
extern RGLSYMGLNAMEDFRAMEBUFFERRENDERBUFFERPROC __rglgen_glNamedFramebufferRenderbuffer;
extern RGLSYMGLNAMEDFRAMEBUFFERPARAMETERIPROC __rglgen_glNamedFramebufferParameteri;
extern RGLSYMGLNAMEDFRAMEBUFFERTEXTUREPROC __rglgen_glNamedFramebufferTexture;
extern RGLSYMGLNAMEDFRAMEBUFFERTEXTURELAYERPROC __rglgen_glNamedFramebufferTextureLayer;
extern RGLSYMGLNAMEDFRAMEBUFFERDRAWBUFFERPROC __rglgen_glNamedFramebufferDrawBuffer;
extern RGLSYMGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC __rglgen_glNamedFramebufferDrawBuffers;
extern RGLSYMGLNAMEDFRAMEBUFFERREADBUFFERPROC __rglgen_glNamedFramebufferReadBuffer;
extern RGLSYMGLINVALIDATENAMEDFRAMEBUFFERDATAPROC __rglgen_glInvalidateNamedFramebufferData;
extern RGLSYMGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC __rglgen_glInvalidateNamedFramebufferSubData;
extern RGLSYMGLCLEARNAMEDFRAMEBUFFERIVPROC __rglgen_glClearNamedFramebufferiv;
extern RGLSYMGLCLEARNAMEDFRAMEBUFFERUIVPROC __rglgen_glClearNamedFramebufferuiv;
extern RGLSYMGLCLEARNAMEDFRAMEBUFFERFVPROC __rglgen_glClearNamedFramebufferfv;
extern RGLSYMGLCLEARNAMEDFRAMEBUFFERFIPROC __rglgen_glClearNamedFramebufferfi;
extern RGLSYMGLBLITNAMEDFRAMEBUFFERPROC __rglgen_glBlitNamedFramebuffer;
extern RGLSYMGLCHECKNAMEDFRAMEBUFFERSTATUSPROC __rglgen_glCheckNamedFramebufferStatus;
extern RGLSYMGLGETNAMEDFRAMEBUFFERPARAMETERIVPROC __rglgen_glGetNamedFramebufferParameteriv;
extern RGLSYMGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC __rglgen_glGetNamedFramebufferAttachmentParameteriv;
extern RGLSYMGLCREATERENDERBUFFERSPROC __rglgen_glCreateRenderbuffers;
extern RGLSYMGLNAMEDRENDERBUFFERSTORAGEPROC __rglgen_glNamedRenderbufferStorage;
extern RGLSYMGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC __rglgen_glNamedRenderbufferStorageMultisample;
extern RGLSYMGLGETNAMEDRENDERBUFFERPARAMETERIVPROC __rglgen_glGetNamedRenderbufferParameteriv;
extern RGLSYMGLCREATETEXTURESPROC __rglgen_glCreateTextures;
extern RGLSYMGLTEXTUREBUFFERPROC __rglgen_glTextureBuffer;
extern RGLSYMGLTEXTUREBUFFERRANGEPROC __rglgen_glTextureBufferRange;
extern RGLSYMGLTEXTURESTORAGE1DPROC __rglgen_glTextureStorage1D;
extern RGLSYMGLTEXTURESTORAGE2DPROC __rglgen_glTextureStorage2D;
extern RGLSYMGLTEXTURESTORAGE3DPROC __rglgen_glTextureStorage3D;
extern RGLSYMGLTEXTURESTORAGE2DMULTISAMPLEPROC __rglgen_glTextureStorage2DMultisample;
extern RGLSYMGLTEXTURESTORAGE3DMULTISAMPLEPROC __rglgen_glTextureStorage3DMultisample;
extern RGLSYMGLTEXTURESUBIMAGE1DPROC __rglgen_glTextureSubImage1D;
extern RGLSYMGLTEXTURESUBIMAGE2DPROC __rglgen_glTextureSubImage2D;
extern RGLSYMGLTEXTURESUBIMAGE3DPROC __rglgen_glTextureSubImage3D;
extern RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE1DPROC __rglgen_glCompressedTextureSubImage1D;
extern RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE2DPROC __rglgen_glCompressedTextureSubImage2D;
extern RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE3DPROC __rglgen_glCompressedTextureSubImage3D;
extern RGLSYMGLCOPYTEXTURESUBIMAGE1DPROC __rglgen_glCopyTextureSubImage1D;
extern RGLSYMGLCOPYTEXTURESUBIMAGE2DPROC __rglgen_glCopyTextureSubImage2D;
extern RGLSYMGLCOPYTEXTURESUBIMAGE3DPROC __rglgen_glCopyTextureSubImage3D;
extern RGLSYMGLTEXTUREPARAMETERFPROC __rglgen_glTextureParameterf;
extern RGLSYMGLTEXTUREPARAMETERFVPROC __rglgen_glTextureParameterfv;
extern RGLSYMGLTEXTUREPARAMETERIPROC __rglgen_glTextureParameteri;
extern RGLSYMGLTEXTUREPARAMETERIIVPROC __rglgen_glTextureParameterIiv;
extern RGLSYMGLTEXTUREPARAMETERIUIVPROC __rglgen_glTextureParameterIuiv;
extern RGLSYMGLTEXTUREPARAMETERIVPROC __rglgen_glTextureParameteriv;
extern RGLSYMGLGENERATETEXTUREMIPMAPPROC __rglgen_glGenerateTextureMipmap;
extern RGLSYMGLBINDTEXTUREUNITPROC __rglgen_glBindTextureUnit;
extern RGLSYMGLGETTEXTUREIMAGEPROC __rglgen_glGetTextureImage;
extern RGLSYMGLGETCOMPRESSEDTEXTUREIMAGEPROC __rglgen_glGetCompressedTextureImage;
extern RGLSYMGLGETTEXTURELEVELPARAMETERFVPROC __rglgen_glGetTextureLevelParameterfv;
extern RGLSYMGLGETTEXTURELEVELPARAMETERIVPROC __rglgen_glGetTextureLevelParameteriv;
extern RGLSYMGLGETTEXTUREPARAMETERFVPROC __rglgen_glGetTextureParameterfv;
extern RGLSYMGLGETTEXTUREPARAMETERIIVPROC __rglgen_glGetTextureParameterIiv;
extern RGLSYMGLGETTEXTUREPARAMETERIUIVPROC __rglgen_glGetTextureParameterIuiv;
extern RGLSYMGLGETTEXTUREPARAMETERIVPROC __rglgen_glGetTextureParameteriv;
extern RGLSYMGLCREATEVERTEXARRAYSPROC __rglgen_glCreateVertexArrays;
extern RGLSYMGLDISABLEVERTEXARRAYATTRIBPROC __rglgen_glDisableVertexArrayAttrib;
extern RGLSYMGLENABLEVERTEXARRAYATTRIBPROC __rglgen_glEnableVertexArrayAttrib;
extern RGLSYMGLVERTEXARRAYELEMENTBUFFERPROC __rglgen_glVertexArrayElementBuffer;
extern RGLSYMGLVERTEXARRAYVERTEXBUFFERPROC __rglgen_glVertexArrayVertexBuffer;
extern RGLSYMGLVERTEXARRAYVERTEXBUFFERSPROC __rglgen_glVertexArrayVertexBuffers;
extern RGLSYMGLVERTEXARRAYATTRIBBINDINGPROC __rglgen_glVertexArrayAttribBinding;
extern RGLSYMGLVERTEXARRAYATTRIBFORMATPROC __rglgen_glVertexArrayAttribFormat;
extern RGLSYMGLVERTEXARRAYATTRIBIFORMATPROC __rglgen_glVertexArrayAttribIFormat;
extern RGLSYMGLVERTEXARRAYATTRIBLFORMATPROC __rglgen_glVertexArrayAttribLFormat;
extern RGLSYMGLVERTEXARRAYBINDINGDIVISORPROC __rglgen_glVertexArrayBindingDivisor;
extern RGLSYMGLGETVERTEXARRAYIVPROC __rglgen_glGetVertexArrayiv;
extern RGLSYMGLGETVERTEXARRAYINDEXEDIVPROC __rglgen_glGetVertexArrayIndexediv;
extern RGLSYMGLGETVERTEXARRAYINDEXED64IVPROC __rglgen_glGetVertexArrayIndexed64iv;
extern RGLSYMGLCREATESAMPLERSPROC __rglgen_glCreateSamplers;
extern RGLSYMGLCREATEPROGRAMPIPELINESPROC __rglgen_glCreateProgramPipelines;
extern RGLSYMGLCREATEQUERIESPROC __rglgen_glCreateQueries;
extern RGLSYMGLGETQUERYBUFFEROBJECTI64VPROC __rglgen_glGetQueryBufferObjecti64v;
extern RGLSYMGLGETQUERYBUFFEROBJECTIVPROC __rglgen_glGetQueryBufferObjectiv;
extern RGLSYMGLGETQUERYBUFFEROBJECTUI64VPROC __rglgen_glGetQueryBufferObjectui64v;
extern RGLSYMGLGETQUERYBUFFEROBJECTUIVPROC __rglgen_glGetQueryBufferObjectuiv;
extern RGLSYMGLMEMORYBARRIERBYREGIONPROC __rglgen_glMemoryBarrierByRegion;
extern RGLSYMGLGETTEXTURESUBIMAGEPROC __rglgen_glGetTextureSubImage;
extern RGLSYMGLGETCOMPRESSEDTEXTURESUBIMAGEPROC __rglgen_glGetCompressedTextureSubImage;
extern RGLSYMGLGETGRAPHICSRESETSTATUSPROC __rglgen_glGetGraphicsResetStatus;
extern RGLSYMGLGETNCOMPRESSEDTEXIMAGEPROC __rglgen_glGetnCompressedTexImage;
extern RGLSYMGLGETNTEXIMAGEPROC __rglgen_glGetnTexImage;
extern RGLSYMGLGETNUNIFORMDVPROC __rglgen_glGetnUniformdv;
extern RGLSYMGLGETNUNIFORMFVPROC __rglgen_glGetnUniformfv;
extern RGLSYMGLGETNUNIFORMIVPROC __rglgen_glGetnUniformiv;
extern RGLSYMGLGETNUNIFORMUIVPROC __rglgen_glGetnUniformuiv;
extern RGLSYMGLREADNPIXELSPROC __rglgen_glReadnPixels;
extern RGLSYMGLTEXTUREBARRIERPROC __rglgen_glTextureBarrier;
extern RGLSYMGLSPECIALIZESHADERPROC __rglgen_glSpecializeShader;
extern RGLSYMGLMULTIDRAWARRAYSINDIRECTCOUNTPROC __rglgen_glMultiDrawArraysIndirectCount;
extern RGLSYMGLMULTIDRAWELEMENTSINDIRECTCOUNTPROC __rglgen_glMultiDrawElementsIndirectCount;
extern RGLSYMGLPOLYGONOFFSETCLAMPPROC __rglgen_glPolygonOffsetClamp;
extern RGLSYMGLPRIMITIVEBOUNDINGBOXARBPROC __rglgen_glPrimitiveBoundingBoxARB;
extern RGLSYMGLGETTEXTUREHANDLEARBPROC __rglgen_glGetTextureHandleARB;
extern RGLSYMGLGETTEXTURESAMPLERHANDLEARBPROC __rglgen_glGetTextureSamplerHandleARB;
extern RGLSYMGLMAKETEXTUREHANDLERESIDENTARBPROC __rglgen_glMakeTextureHandleResidentARB;
extern RGLSYMGLMAKETEXTUREHANDLENONRESIDENTARBPROC __rglgen_glMakeTextureHandleNonResidentARB;
extern RGLSYMGLGETIMAGEHANDLEARBPROC __rglgen_glGetImageHandleARB;
extern RGLSYMGLMAKEIMAGEHANDLERESIDENTARBPROC __rglgen_glMakeImageHandleResidentARB;
extern RGLSYMGLMAKEIMAGEHANDLENONRESIDENTARBPROC __rglgen_glMakeImageHandleNonResidentARB;
extern RGLSYMGLUNIFORMHANDLEUI64ARBPROC __rglgen_glUniformHandleui64ARB;
extern RGLSYMGLUNIFORMHANDLEUI64VARBPROC __rglgen_glUniformHandleui64vARB;
extern RGLSYMGLPROGRAMUNIFORMHANDLEUI64ARBPROC __rglgen_glProgramUniformHandleui64ARB;
extern RGLSYMGLPROGRAMUNIFORMHANDLEUI64VARBPROC __rglgen_glProgramUniformHandleui64vARB;
extern RGLSYMGLISTEXTUREHANDLERESIDENTARBPROC __rglgen_glIsTextureHandleResidentARB;
extern RGLSYMGLISIMAGEHANDLERESIDENTARBPROC __rglgen_glIsImageHandleResidentARB;
extern RGLSYMGLVERTEXATTRIBL1UI64ARBPROC __rglgen_glVertexAttribL1ui64ARB;
extern RGLSYMGLVERTEXATTRIBL1UI64VARBPROC __rglgen_glVertexAttribL1ui64vARB;
extern RGLSYMGLGETVERTEXATTRIBLUI64VARBPROC __rglgen_glGetVertexAttribLui64vARB;
extern RGLSYMGLDISPATCHCOMPUTEGROUPSIZEARBPROC __rglgen_glDispatchComputeGroupSizeARB;
extern RGLSYMGLDEBUGMESSAGECONTROLARBPROC __rglgen_glDebugMessageControlARB;
extern RGLSYMGLDEBUGMESSAGEINSERTARBPROC __rglgen_glDebugMessageInsertARB;
extern RGLSYMGLDEBUGMESSAGECALLBACKARBPROC __rglgen_glDebugMessageCallbackARB;
extern RGLSYMGLGETDEBUGMESSAGELOGARBPROC __rglgen_glGetDebugMessageLogARB;
extern RGLSYMGLBLENDEQUATIONIARBPROC __rglgen_glBlendEquationiARB;
extern RGLSYMGLBLENDEQUATIONSEPARATEIARBPROC __rglgen_glBlendEquationSeparateiARB;
extern RGLSYMGLBLENDFUNCIARBPROC __rglgen_glBlendFunciARB;
extern RGLSYMGLBLENDFUNCSEPARATEIARBPROC __rglgen_glBlendFuncSeparateiARB;
extern RGLSYMGLDRAWARRAYSINSTANCEDARBPROC __rglgen_glDrawArraysInstancedARB;
extern RGLSYMGLDRAWELEMENTSINSTANCEDARBPROC __rglgen_glDrawElementsInstancedARB;
extern RGLSYMGLPROGRAMPARAMETERIARBPROC __rglgen_glProgramParameteriARB;
extern RGLSYMGLFRAMEBUFFERTEXTUREARBPROC __rglgen_glFramebufferTextureARB;
extern RGLSYMGLFRAMEBUFFERTEXTURELAYERARBPROC __rglgen_glFramebufferTextureLayerARB;
extern RGLSYMGLFRAMEBUFFERTEXTUREFACEARBPROC __rglgen_glFramebufferTextureFaceARB;
extern RGLSYMGLSPECIALIZESHADERARBPROC __rglgen_glSpecializeShaderARB;
extern RGLSYMGLUNIFORM1I64ARBPROC __rglgen_glUniform1i64ARB;
extern RGLSYMGLUNIFORM2I64ARBPROC __rglgen_glUniform2i64ARB;
extern RGLSYMGLUNIFORM3I64ARBPROC __rglgen_glUniform3i64ARB;
extern RGLSYMGLUNIFORM4I64ARBPROC __rglgen_glUniform4i64ARB;
extern RGLSYMGLUNIFORM1I64VARBPROC __rglgen_glUniform1i64vARB;
extern RGLSYMGLUNIFORM2I64VARBPROC __rglgen_glUniform2i64vARB;
extern RGLSYMGLUNIFORM3I64VARBPROC __rglgen_glUniform3i64vARB;
extern RGLSYMGLUNIFORM4I64VARBPROC __rglgen_glUniform4i64vARB;
extern RGLSYMGLUNIFORM1UI64ARBPROC __rglgen_glUniform1ui64ARB;
extern RGLSYMGLUNIFORM2UI64ARBPROC __rglgen_glUniform2ui64ARB;
extern RGLSYMGLUNIFORM3UI64ARBPROC __rglgen_glUniform3ui64ARB;
extern RGLSYMGLUNIFORM4UI64ARBPROC __rglgen_glUniform4ui64ARB;
extern RGLSYMGLUNIFORM1UI64VARBPROC __rglgen_glUniform1ui64vARB;
extern RGLSYMGLUNIFORM2UI64VARBPROC __rglgen_glUniform2ui64vARB;
extern RGLSYMGLUNIFORM3UI64VARBPROC __rglgen_glUniform3ui64vARB;
extern RGLSYMGLUNIFORM4UI64VARBPROC __rglgen_glUniform4ui64vARB;
extern RGLSYMGLGETUNIFORMI64VARBPROC __rglgen_glGetUniformi64vARB;
extern RGLSYMGLGETUNIFORMUI64VARBPROC __rglgen_glGetUniformui64vARB;
extern RGLSYMGLGETNUNIFORMI64VARBPROC __rglgen_glGetnUniformi64vARB;
extern RGLSYMGLGETNUNIFORMUI64VARBPROC __rglgen_glGetnUniformui64vARB;
extern RGLSYMGLPROGRAMUNIFORM1I64ARBPROC __rglgen_glProgramUniform1i64ARB;
extern RGLSYMGLPROGRAMUNIFORM2I64ARBPROC __rglgen_glProgramUniform2i64ARB;
extern RGLSYMGLPROGRAMUNIFORM3I64ARBPROC __rglgen_glProgramUniform3i64ARB;
extern RGLSYMGLPROGRAMUNIFORM4I64ARBPROC __rglgen_glProgramUniform4i64ARB;
extern RGLSYMGLPROGRAMUNIFORM1I64VARBPROC __rglgen_glProgramUniform1i64vARB;
extern RGLSYMGLPROGRAMUNIFORM2I64VARBPROC __rglgen_glProgramUniform2i64vARB;
extern RGLSYMGLPROGRAMUNIFORM3I64VARBPROC __rglgen_glProgramUniform3i64vARB;
extern RGLSYMGLPROGRAMUNIFORM4I64VARBPROC __rglgen_glProgramUniform4i64vARB;
extern RGLSYMGLPROGRAMUNIFORM1UI64ARBPROC __rglgen_glProgramUniform1ui64ARB;
extern RGLSYMGLPROGRAMUNIFORM2UI64ARBPROC __rglgen_glProgramUniform2ui64ARB;
extern RGLSYMGLPROGRAMUNIFORM3UI64ARBPROC __rglgen_glProgramUniform3ui64ARB;
extern RGLSYMGLPROGRAMUNIFORM4UI64ARBPROC __rglgen_glProgramUniform4ui64ARB;
extern RGLSYMGLPROGRAMUNIFORM1UI64VARBPROC __rglgen_glProgramUniform1ui64vARB;
extern RGLSYMGLPROGRAMUNIFORM2UI64VARBPROC __rglgen_glProgramUniform2ui64vARB;
extern RGLSYMGLPROGRAMUNIFORM3UI64VARBPROC __rglgen_glProgramUniform3ui64vARB;
extern RGLSYMGLPROGRAMUNIFORM4UI64VARBPROC __rglgen_glProgramUniform4ui64vARB;
extern RGLSYMGLMULTIDRAWARRAYSINDIRECTCOUNTARBPROC __rglgen_glMultiDrawArraysIndirectCountARB;
extern RGLSYMGLMULTIDRAWELEMENTSINDIRECTCOUNTARBPROC __rglgen_glMultiDrawElementsIndirectCountARB;
extern RGLSYMGLVERTEXATTRIBDIVISORARBPROC __rglgen_glVertexAttribDivisorARB;
extern RGLSYMGLMAXSHADERCOMPILERTHREADSARBPROC __rglgen_glMaxShaderCompilerThreadsARB;
extern RGLSYMGLGETGRAPHICSRESETSTATUSARBPROC __rglgen_glGetGraphicsResetStatusARB;
extern RGLSYMGLGETNTEXIMAGEARBPROC __rglgen_glGetnTexImageARB;
extern RGLSYMGLREADNPIXELSARBPROC __rglgen_glReadnPixelsARB;
extern RGLSYMGLGETNCOMPRESSEDTEXIMAGEARBPROC __rglgen_glGetnCompressedTexImageARB;
extern RGLSYMGLGETNUNIFORMFVARBPROC __rglgen_glGetnUniformfvARB;
extern RGLSYMGLGETNUNIFORMIVARBPROC __rglgen_glGetnUniformivARB;
extern RGLSYMGLGETNUNIFORMUIVARBPROC __rglgen_glGetnUniformuivARB;
extern RGLSYMGLGETNUNIFORMDVARBPROC __rglgen_glGetnUniformdvARB;
extern RGLSYMGLFRAMEBUFFERSAMPLELOCATIONSFVARBPROC __rglgen_glFramebufferSampleLocationsfvARB;
extern RGLSYMGLNAMEDFRAMEBUFFERSAMPLELOCATIONSFVARBPROC __rglgen_glNamedFramebufferSampleLocationsfvARB;
extern RGLSYMGLEVALUATEDEPTHVALUESARBPROC __rglgen_glEvaluateDepthValuesARB;
extern RGLSYMGLMINSAMPLESHADINGARBPROC __rglgen_glMinSampleShadingARB;
extern RGLSYMGLNAMEDSTRINGARBPROC __rglgen_glNamedStringARB;
extern RGLSYMGLDELETENAMEDSTRINGARBPROC __rglgen_glDeleteNamedStringARB;
extern RGLSYMGLCOMPILESHADERINCLUDEARBPROC __rglgen_glCompileShaderIncludeARB;
extern RGLSYMGLISNAMEDSTRINGARBPROC __rglgen_glIsNamedStringARB;
extern RGLSYMGLGETNAMEDSTRINGARBPROC __rglgen_glGetNamedStringARB;
extern RGLSYMGLGETNAMEDSTRINGIVARBPROC __rglgen_glGetNamedStringivARB;
extern RGLSYMGLBUFFERPAGECOMMITMENTARBPROC __rglgen_glBufferPageCommitmentARB;
extern RGLSYMGLNAMEDBUFFERPAGECOMMITMENTEXTPROC __rglgen_glNamedBufferPageCommitmentEXT;
extern RGLSYMGLNAMEDBUFFERPAGECOMMITMENTARBPROC __rglgen_glNamedBufferPageCommitmentARB;
extern RGLSYMGLTEXPAGECOMMITMENTARBPROC __rglgen_glTexPageCommitmentARB;
extern RGLSYMGLTEXBUFFERARBPROC __rglgen_glTexBufferARB;
extern RGLSYMGLBLENDBARRIERKHRPROC __rglgen_glBlendBarrierKHR;
extern RGLSYMGLMAXSHADERCOMPILERTHREADSKHRPROC __rglgen_glMaxShaderCompilerThreadsKHR;
extern RGLSYMGLEGLIMAGETARGETTEXSTORAGEEXTPROC __rglgen_glEGLImageTargetTexStorageEXT;
extern RGLSYMGLEGLIMAGETARGETTEXTURESTORAGEEXTPROC __rglgen_glEGLImageTargetTextureStorageEXT;
extern RGLSYMGLLABELOBJECTEXTPROC __rglgen_glLabelObjectEXT;
extern RGLSYMGLGETOBJECTLABELEXTPROC __rglgen_glGetObjectLabelEXT;
extern RGLSYMGLINSERTEVENTMARKEREXTPROC __rglgen_glInsertEventMarkerEXT;
extern RGLSYMGLPUSHGROUPMARKEREXTPROC __rglgen_glPushGroupMarkerEXT;
extern RGLSYMGLPOPGROUPMARKEREXTPROC __rglgen_glPopGroupMarkerEXT;
extern RGLSYMGLMATRIXLOADFEXTPROC __rglgen_glMatrixLoadfEXT;
extern RGLSYMGLMATRIXLOADDEXTPROC __rglgen_glMatrixLoaddEXT;
extern RGLSYMGLMATRIXMULTFEXTPROC __rglgen_glMatrixMultfEXT;
extern RGLSYMGLMATRIXMULTDEXTPROC __rglgen_glMatrixMultdEXT;
extern RGLSYMGLMATRIXLOADIDENTITYEXTPROC __rglgen_glMatrixLoadIdentityEXT;
extern RGLSYMGLMATRIXROTATEFEXTPROC __rglgen_glMatrixRotatefEXT;
extern RGLSYMGLMATRIXROTATEDEXTPROC __rglgen_glMatrixRotatedEXT;
extern RGLSYMGLMATRIXSCALEFEXTPROC __rglgen_glMatrixScalefEXT;
extern RGLSYMGLMATRIXSCALEDEXTPROC __rglgen_glMatrixScaledEXT;
extern RGLSYMGLMATRIXTRANSLATEFEXTPROC __rglgen_glMatrixTranslatefEXT;
extern RGLSYMGLMATRIXTRANSLATEDEXTPROC __rglgen_glMatrixTranslatedEXT;
extern RGLSYMGLMATRIXFRUSTUMEXTPROC __rglgen_glMatrixFrustumEXT;
extern RGLSYMGLMATRIXORTHOEXTPROC __rglgen_glMatrixOrthoEXT;
extern RGLSYMGLMATRIXPOPEXTPROC __rglgen_glMatrixPopEXT;
extern RGLSYMGLMATRIXPUSHEXTPROC __rglgen_glMatrixPushEXT;
extern RGLSYMGLCLIENTATTRIBDEFAULTEXTPROC __rglgen_glClientAttribDefaultEXT;
extern RGLSYMGLPUSHCLIENTATTRIBDEFAULTEXTPROC __rglgen_glPushClientAttribDefaultEXT;
extern RGLSYMGLTEXTUREPARAMETERFEXTPROC __rglgen_glTextureParameterfEXT;
extern RGLSYMGLTEXTUREPARAMETERFVEXTPROC __rglgen_glTextureParameterfvEXT;
extern RGLSYMGLTEXTUREPARAMETERIEXTPROC __rglgen_glTextureParameteriEXT;
extern RGLSYMGLTEXTUREPARAMETERIVEXTPROC __rglgen_glTextureParameterivEXT;
extern RGLSYMGLTEXTUREIMAGE1DEXTPROC __rglgen_glTextureImage1DEXT;
extern RGLSYMGLTEXTUREIMAGE2DEXTPROC __rglgen_glTextureImage2DEXT;
extern RGLSYMGLTEXTURESUBIMAGE1DEXTPROC __rglgen_glTextureSubImage1DEXT;
extern RGLSYMGLTEXTURESUBIMAGE2DEXTPROC __rglgen_glTextureSubImage2DEXT;
extern RGLSYMGLCOPYTEXTUREIMAGE1DEXTPROC __rglgen_glCopyTextureImage1DEXT;
extern RGLSYMGLCOPYTEXTUREIMAGE2DEXTPROC __rglgen_glCopyTextureImage2DEXT;
extern RGLSYMGLCOPYTEXTURESUBIMAGE1DEXTPROC __rglgen_glCopyTextureSubImage1DEXT;
extern RGLSYMGLCOPYTEXTURESUBIMAGE2DEXTPROC __rglgen_glCopyTextureSubImage2DEXT;
extern RGLSYMGLGETTEXTUREIMAGEEXTPROC __rglgen_glGetTextureImageEXT;
extern RGLSYMGLGETTEXTUREPARAMETERFVEXTPROC __rglgen_glGetTextureParameterfvEXT;
extern RGLSYMGLGETTEXTUREPARAMETERIVEXTPROC __rglgen_glGetTextureParameterivEXT;
extern RGLSYMGLGETTEXTURELEVELPARAMETERFVEXTPROC __rglgen_glGetTextureLevelParameterfvEXT;
extern RGLSYMGLGETTEXTURELEVELPARAMETERIVEXTPROC __rglgen_glGetTextureLevelParameterivEXT;
extern RGLSYMGLTEXTUREIMAGE3DEXTPROC __rglgen_glTextureImage3DEXT;
extern RGLSYMGLTEXTURESUBIMAGE3DEXTPROC __rglgen_glTextureSubImage3DEXT;
extern RGLSYMGLCOPYTEXTURESUBIMAGE3DEXTPROC __rglgen_glCopyTextureSubImage3DEXT;
extern RGLSYMGLBINDMULTITEXTUREEXTPROC __rglgen_glBindMultiTextureEXT;
extern RGLSYMGLMULTITEXCOORDPOINTEREXTPROC __rglgen_glMultiTexCoordPointerEXT;
extern RGLSYMGLMULTITEXENVFEXTPROC __rglgen_glMultiTexEnvfEXT;
extern RGLSYMGLMULTITEXENVFVEXTPROC __rglgen_glMultiTexEnvfvEXT;
extern RGLSYMGLMULTITEXENVIEXTPROC __rglgen_glMultiTexEnviEXT;
extern RGLSYMGLMULTITEXENVIVEXTPROC __rglgen_glMultiTexEnvivEXT;
extern RGLSYMGLMULTITEXGENDEXTPROC __rglgen_glMultiTexGendEXT;
extern RGLSYMGLMULTITEXGENDVEXTPROC __rglgen_glMultiTexGendvEXT;
extern RGLSYMGLMULTITEXGENFEXTPROC __rglgen_glMultiTexGenfEXT;
extern RGLSYMGLMULTITEXGENFVEXTPROC __rglgen_glMultiTexGenfvEXT;
extern RGLSYMGLMULTITEXGENIEXTPROC __rglgen_glMultiTexGeniEXT;
extern RGLSYMGLMULTITEXGENIVEXTPROC __rglgen_glMultiTexGenivEXT;
extern RGLSYMGLGETMULTITEXENVFVEXTPROC __rglgen_glGetMultiTexEnvfvEXT;
extern RGLSYMGLGETMULTITEXENVIVEXTPROC __rglgen_glGetMultiTexEnvivEXT;
extern RGLSYMGLGETMULTITEXGENDVEXTPROC __rglgen_glGetMultiTexGendvEXT;
extern RGLSYMGLGETMULTITEXGENFVEXTPROC __rglgen_glGetMultiTexGenfvEXT;
extern RGLSYMGLGETMULTITEXGENIVEXTPROC __rglgen_glGetMultiTexGenivEXT;
extern RGLSYMGLMULTITEXPARAMETERIEXTPROC __rglgen_glMultiTexParameteriEXT;
extern RGLSYMGLMULTITEXPARAMETERIVEXTPROC __rglgen_glMultiTexParameterivEXT;
extern RGLSYMGLMULTITEXPARAMETERFEXTPROC __rglgen_glMultiTexParameterfEXT;
extern RGLSYMGLMULTITEXPARAMETERFVEXTPROC __rglgen_glMultiTexParameterfvEXT;
extern RGLSYMGLMULTITEXIMAGE1DEXTPROC __rglgen_glMultiTexImage1DEXT;
extern RGLSYMGLMULTITEXIMAGE2DEXTPROC __rglgen_glMultiTexImage2DEXT;
extern RGLSYMGLMULTITEXSUBIMAGE1DEXTPROC __rglgen_glMultiTexSubImage1DEXT;
extern RGLSYMGLMULTITEXSUBIMAGE2DEXTPROC __rglgen_glMultiTexSubImage2DEXT;
extern RGLSYMGLCOPYMULTITEXIMAGE1DEXTPROC __rglgen_glCopyMultiTexImage1DEXT;
extern RGLSYMGLCOPYMULTITEXIMAGE2DEXTPROC __rglgen_glCopyMultiTexImage2DEXT;
extern RGLSYMGLCOPYMULTITEXSUBIMAGE1DEXTPROC __rglgen_glCopyMultiTexSubImage1DEXT;
extern RGLSYMGLCOPYMULTITEXSUBIMAGE2DEXTPROC __rglgen_glCopyMultiTexSubImage2DEXT;
extern RGLSYMGLGETMULTITEXIMAGEEXTPROC __rglgen_glGetMultiTexImageEXT;
extern RGLSYMGLGETMULTITEXPARAMETERFVEXTPROC __rglgen_glGetMultiTexParameterfvEXT;
extern RGLSYMGLGETMULTITEXPARAMETERIVEXTPROC __rglgen_glGetMultiTexParameterivEXT;
extern RGLSYMGLGETMULTITEXLEVELPARAMETERFVEXTPROC __rglgen_glGetMultiTexLevelParameterfvEXT;
extern RGLSYMGLGETMULTITEXLEVELPARAMETERIVEXTPROC __rglgen_glGetMultiTexLevelParameterivEXT;
extern RGLSYMGLMULTITEXIMAGE3DEXTPROC __rglgen_glMultiTexImage3DEXT;
extern RGLSYMGLMULTITEXSUBIMAGE3DEXTPROC __rglgen_glMultiTexSubImage3DEXT;
extern RGLSYMGLCOPYMULTITEXSUBIMAGE3DEXTPROC __rglgen_glCopyMultiTexSubImage3DEXT;
extern RGLSYMGLENABLECLIENTSTATEINDEXEDEXTPROC __rglgen_glEnableClientStateIndexedEXT;
extern RGLSYMGLDISABLECLIENTSTATEINDEXEDEXTPROC __rglgen_glDisableClientStateIndexedEXT;
extern RGLSYMGLGETFLOATINDEXEDVEXTPROC __rglgen_glGetFloatIndexedvEXT;
extern RGLSYMGLGETDOUBLEINDEXEDVEXTPROC __rglgen_glGetDoubleIndexedvEXT;
extern RGLSYMGLGETPOINTERINDEXEDVEXTPROC __rglgen_glGetPointerIndexedvEXT;
extern RGLSYMGLENABLEINDEXEDEXTPROC __rglgen_glEnableIndexedEXT;
extern RGLSYMGLDISABLEINDEXEDEXTPROC __rglgen_glDisableIndexedEXT;
extern RGLSYMGLISENABLEDINDEXEDEXTPROC __rglgen_glIsEnabledIndexedEXT;
extern RGLSYMGLGETINTEGERINDEXEDVEXTPROC __rglgen_glGetIntegerIndexedvEXT;
extern RGLSYMGLGETBOOLEANINDEXEDVEXTPROC __rglgen_glGetBooleanIndexedvEXT;
extern RGLSYMGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC __rglgen_glCompressedTextureImage3DEXT;
extern RGLSYMGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC __rglgen_glCompressedTextureImage2DEXT;
extern RGLSYMGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC __rglgen_glCompressedTextureImage1DEXT;
extern RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC __rglgen_glCompressedTextureSubImage3DEXT;
extern RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC __rglgen_glCompressedTextureSubImage2DEXT;
extern RGLSYMGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC __rglgen_glCompressedTextureSubImage1DEXT;
extern RGLSYMGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC __rglgen_glGetCompressedTextureImageEXT;
extern RGLSYMGLCOMPRESSEDMULTITEXIMAGE3DEXTPROC __rglgen_glCompressedMultiTexImage3DEXT;
extern RGLSYMGLCOMPRESSEDMULTITEXIMAGE2DEXTPROC __rglgen_glCompressedMultiTexImage2DEXT;
extern RGLSYMGLCOMPRESSEDMULTITEXIMAGE1DEXTPROC __rglgen_glCompressedMultiTexImage1DEXT;
extern RGLSYMGLCOMPRESSEDMULTITEXSUBIMAGE3DEXTPROC __rglgen_glCompressedMultiTexSubImage3DEXT;
extern RGLSYMGLCOMPRESSEDMULTITEXSUBIMAGE2DEXTPROC __rglgen_glCompressedMultiTexSubImage2DEXT;
extern RGLSYMGLCOMPRESSEDMULTITEXSUBIMAGE1DEXTPROC __rglgen_glCompressedMultiTexSubImage1DEXT;
extern RGLSYMGLGETCOMPRESSEDMULTITEXIMAGEEXTPROC __rglgen_glGetCompressedMultiTexImageEXT;
extern RGLSYMGLMATRIXLOADTRANSPOSEFEXTPROC __rglgen_glMatrixLoadTransposefEXT;
extern RGLSYMGLMATRIXLOADTRANSPOSEDEXTPROC __rglgen_glMatrixLoadTransposedEXT;
extern RGLSYMGLMATRIXMULTTRANSPOSEFEXTPROC __rglgen_glMatrixMultTransposefEXT;
extern RGLSYMGLMATRIXMULTTRANSPOSEDEXTPROC __rglgen_glMatrixMultTransposedEXT;
extern RGLSYMGLNAMEDBUFFERDATAEXTPROC __rglgen_glNamedBufferDataEXT;
extern RGLSYMGLNAMEDBUFFERSUBDATAEXTPROC __rglgen_glNamedBufferSubDataEXT;
extern RGLSYMGLMAPNAMEDBUFFEREXTPROC __rglgen_glMapNamedBufferEXT;
extern RGLSYMGLUNMAPNAMEDBUFFEREXTPROC __rglgen_glUnmapNamedBufferEXT;
extern RGLSYMGLGETNAMEDBUFFERPARAMETERIVEXTPROC __rglgen_glGetNamedBufferParameterivEXT;
extern RGLSYMGLGETNAMEDBUFFERPOINTERVEXTPROC __rglgen_glGetNamedBufferPointervEXT;
extern RGLSYMGLGETNAMEDBUFFERSUBDATAEXTPROC __rglgen_glGetNamedBufferSubDataEXT;
extern RGLSYMGLPROGRAMUNIFORM1FEXTPROC __rglgen_glProgramUniform1fEXT;
extern RGLSYMGLPROGRAMUNIFORM2FEXTPROC __rglgen_glProgramUniform2fEXT;
extern RGLSYMGLPROGRAMUNIFORM3FEXTPROC __rglgen_glProgramUniform3fEXT;
extern RGLSYMGLPROGRAMUNIFORM4FEXTPROC __rglgen_glProgramUniform4fEXT;
extern RGLSYMGLPROGRAMUNIFORM1IEXTPROC __rglgen_glProgramUniform1iEXT;
extern RGLSYMGLPROGRAMUNIFORM2IEXTPROC __rglgen_glProgramUniform2iEXT;
extern RGLSYMGLPROGRAMUNIFORM3IEXTPROC __rglgen_glProgramUniform3iEXT;
extern RGLSYMGLPROGRAMUNIFORM4IEXTPROC __rglgen_glProgramUniform4iEXT;
extern RGLSYMGLPROGRAMUNIFORM1FVEXTPROC __rglgen_glProgramUniform1fvEXT;
extern RGLSYMGLPROGRAMUNIFORM2FVEXTPROC __rglgen_glProgramUniform2fvEXT;
extern RGLSYMGLPROGRAMUNIFORM3FVEXTPROC __rglgen_glProgramUniform3fvEXT;
extern RGLSYMGLPROGRAMUNIFORM4FVEXTPROC __rglgen_glProgramUniform4fvEXT;
extern RGLSYMGLPROGRAMUNIFORM1IVEXTPROC __rglgen_glProgramUniform1ivEXT;
extern RGLSYMGLPROGRAMUNIFORM2IVEXTPROC __rglgen_glProgramUniform2ivEXT;
extern RGLSYMGLPROGRAMUNIFORM3IVEXTPROC __rglgen_glProgramUniform3ivEXT;
extern RGLSYMGLPROGRAMUNIFORM4IVEXTPROC __rglgen_glProgramUniform4ivEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2FVEXTPROC __rglgen_glProgramUniformMatrix2fvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3FVEXTPROC __rglgen_glProgramUniformMatrix3fvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4FVEXTPROC __rglgen_glProgramUniformMatrix4fvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC __rglgen_glProgramUniformMatrix2x3fvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC __rglgen_glProgramUniformMatrix3x2fvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC __rglgen_glProgramUniformMatrix2x4fvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC __rglgen_glProgramUniformMatrix4x2fvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC __rglgen_glProgramUniformMatrix3x4fvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC __rglgen_glProgramUniformMatrix4x3fvEXT;
extern RGLSYMGLTEXTUREBUFFEREXTPROC __rglgen_glTextureBufferEXT;
extern RGLSYMGLMULTITEXBUFFEREXTPROC __rglgen_glMultiTexBufferEXT;
extern RGLSYMGLTEXTUREPARAMETERIIVEXTPROC __rglgen_glTextureParameterIivEXT;
extern RGLSYMGLTEXTUREPARAMETERIUIVEXTPROC __rglgen_glTextureParameterIuivEXT;
extern RGLSYMGLGETTEXTUREPARAMETERIIVEXTPROC __rglgen_glGetTextureParameterIivEXT;
extern RGLSYMGLGETTEXTUREPARAMETERIUIVEXTPROC __rglgen_glGetTextureParameterIuivEXT;
extern RGLSYMGLMULTITEXPARAMETERIIVEXTPROC __rglgen_glMultiTexParameterIivEXT;
extern RGLSYMGLMULTITEXPARAMETERIUIVEXTPROC __rglgen_glMultiTexParameterIuivEXT;
extern RGLSYMGLGETMULTITEXPARAMETERIIVEXTPROC __rglgen_glGetMultiTexParameterIivEXT;
extern RGLSYMGLGETMULTITEXPARAMETERIUIVEXTPROC __rglgen_glGetMultiTexParameterIuivEXT;
extern RGLSYMGLPROGRAMUNIFORM1UIEXTPROC __rglgen_glProgramUniform1uiEXT;
extern RGLSYMGLPROGRAMUNIFORM2UIEXTPROC __rglgen_glProgramUniform2uiEXT;
extern RGLSYMGLPROGRAMUNIFORM3UIEXTPROC __rglgen_glProgramUniform3uiEXT;
extern RGLSYMGLPROGRAMUNIFORM4UIEXTPROC __rglgen_glProgramUniform4uiEXT;
extern RGLSYMGLPROGRAMUNIFORM1UIVEXTPROC __rglgen_glProgramUniform1uivEXT;
extern RGLSYMGLPROGRAMUNIFORM2UIVEXTPROC __rglgen_glProgramUniform2uivEXT;
extern RGLSYMGLPROGRAMUNIFORM3UIVEXTPROC __rglgen_glProgramUniform3uivEXT;
extern RGLSYMGLPROGRAMUNIFORM4UIVEXTPROC __rglgen_glProgramUniform4uivEXT;
extern RGLSYMGLNAMEDPROGRAMLOCALPARAMETERS4FVEXTPROC __rglgen_glNamedProgramLocalParameters4fvEXT;
extern RGLSYMGLNAMEDPROGRAMLOCALPARAMETERI4IEXTPROC __rglgen_glNamedProgramLocalParameterI4iEXT;
extern RGLSYMGLNAMEDPROGRAMLOCALPARAMETERI4IVEXTPROC __rglgen_glNamedProgramLocalParameterI4ivEXT;
extern RGLSYMGLNAMEDPROGRAMLOCALPARAMETERSI4IVEXTPROC __rglgen_glNamedProgramLocalParametersI4ivEXT;
extern RGLSYMGLNAMEDPROGRAMLOCALPARAMETERI4UIEXTPROC __rglgen_glNamedProgramLocalParameterI4uiEXT;
extern RGLSYMGLNAMEDPROGRAMLOCALPARAMETERI4UIVEXTPROC __rglgen_glNamedProgramLocalParameterI4uivEXT;
extern RGLSYMGLNAMEDPROGRAMLOCALPARAMETERSI4UIVEXTPROC __rglgen_glNamedProgramLocalParametersI4uivEXT;
extern RGLSYMGLGETNAMEDPROGRAMLOCALPARAMETERIIVEXTPROC __rglgen_glGetNamedProgramLocalParameterIivEXT;
extern RGLSYMGLGETNAMEDPROGRAMLOCALPARAMETERIUIVEXTPROC __rglgen_glGetNamedProgramLocalParameterIuivEXT;
extern RGLSYMGLENABLECLIENTSTATEIEXTPROC __rglgen_glEnableClientStateiEXT;
extern RGLSYMGLDISABLECLIENTSTATEIEXTPROC __rglgen_glDisableClientStateiEXT;
extern RGLSYMGLGETFLOATI_VEXTPROC __rglgen_glGetFloati_vEXT;
extern RGLSYMGLGETDOUBLEI_VEXTPROC __rglgen_glGetDoublei_vEXT;
extern RGLSYMGLGETPOINTERI_VEXTPROC __rglgen_glGetPointeri_vEXT;
extern RGLSYMGLNAMEDPROGRAMSTRINGEXTPROC __rglgen_glNamedProgramStringEXT;
extern RGLSYMGLNAMEDPROGRAMLOCALPARAMETER4DEXTPROC __rglgen_glNamedProgramLocalParameter4dEXT;
extern RGLSYMGLNAMEDPROGRAMLOCALPARAMETER4DVEXTPROC __rglgen_glNamedProgramLocalParameter4dvEXT;
extern RGLSYMGLNAMEDPROGRAMLOCALPARAMETER4FEXTPROC __rglgen_glNamedProgramLocalParameter4fEXT;
extern RGLSYMGLNAMEDPROGRAMLOCALPARAMETER4FVEXTPROC __rglgen_glNamedProgramLocalParameter4fvEXT;
extern RGLSYMGLGETNAMEDPROGRAMLOCALPARAMETERDVEXTPROC __rglgen_glGetNamedProgramLocalParameterdvEXT;
extern RGLSYMGLGETNAMEDPROGRAMLOCALPARAMETERFVEXTPROC __rglgen_glGetNamedProgramLocalParameterfvEXT;
extern RGLSYMGLGETNAMEDPROGRAMIVEXTPROC __rglgen_glGetNamedProgramivEXT;
extern RGLSYMGLGETNAMEDPROGRAMSTRINGEXTPROC __rglgen_glGetNamedProgramStringEXT;
extern RGLSYMGLNAMEDRENDERBUFFERSTORAGEEXTPROC __rglgen_glNamedRenderbufferStorageEXT;
extern RGLSYMGLGETNAMEDRENDERBUFFERPARAMETERIVEXTPROC __rglgen_glGetNamedRenderbufferParameterivEXT;
extern RGLSYMGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC __rglgen_glNamedRenderbufferStorageMultisampleEXT;
extern RGLSYMGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLECOVERAGEEXTPROC __rglgen_glNamedRenderbufferStorageMultisampleCoverageEXT;
extern RGLSYMGLCHECKNAMEDFRAMEBUFFERSTATUSEXTPROC __rglgen_glCheckNamedFramebufferStatusEXT;
extern RGLSYMGLNAMEDFRAMEBUFFERTEXTURE1DEXTPROC __rglgen_glNamedFramebufferTexture1DEXT;
extern RGLSYMGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC __rglgen_glNamedFramebufferTexture2DEXT;
extern RGLSYMGLNAMEDFRAMEBUFFERTEXTURE3DEXTPROC __rglgen_glNamedFramebufferTexture3DEXT;
extern RGLSYMGLNAMEDFRAMEBUFFERRENDERBUFFEREXTPROC __rglgen_glNamedFramebufferRenderbufferEXT;
extern RGLSYMGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC __rglgen_glGetNamedFramebufferAttachmentParameterivEXT;
extern RGLSYMGLGENERATETEXTUREMIPMAPEXTPROC __rglgen_glGenerateTextureMipmapEXT;
extern RGLSYMGLGENERATEMULTITEXMIPMAPEXTPROC __rglgen_glGenerateMultiTexMipmapEXT;
extern RGLSYMGLFRAMEBUFFERDRAWBUFFEREXTPROC __rglgen_glFramebufferDrawBufferEXT;
extern RGLSYMGLFRAMEBUFFERDRAWBUFFERSEXTPROC __rglgen_glFramebufferDrawBuffersEXT;
extern RGLSYMGLFRAMEBUFFERREADBUFFEREXTPROC __rglgen_glFramebufferReadBufferEXT;
extern RGLSYMGLGETFRAMEBUFFERPARAMETERIVEXTPROC __rglgen_glGetFramebufferParameterivEXT;
extern RGLSYMGLNAMEDCOPYBUFFERSUBDATAEXTPROC __rglgen_glNamedCopyBufferSubDataEXT;
extern RGLSYMGLNAMEDFRAMEBUFFERTEXTUREEXTPROC __rglgen_glNamedFramebufferTextureEXT;
extern RGLSYMGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC __rglgen_glNamedFramebufferTextureLayerEXT;
extern RGLSYMGLNAMEDFRAMEBUFFERTEXTUREFACEEXTPROC __rglgen_glNamedFramebufferTextureFaceEXT;
extern RGLSYMGLTEXTURERENDERBUFFEREXTPROC __rglgen_glTextureRenderbufferEXT;
extern RGLSYMGLMULTITEXRENDERBUFFEREXTPROC __rglgen_glMultiTexRenderbufferEXT;
extern RGLSYMGLVERTEXARRAYVERTEXOFFSETEXTPROC __rglgen_glVertexArrayVertexOffsetEXT;
extern RGLSYMGLVERTEXARRAYCOLOROFFSETEXTPROC __rglgen_glVertexArrayColorOffsetEXT;
extern RGLSYMGLVERTEXARRAYEDGEFLAGOFFSETEXTPROC __rglgen_glVertexArrayEdgeFlagOffsetEXT;
extern RGLSYMGLVERTEXARRAYINDEXOFFSETEXTPROC __rglgen_glVertexArrayIndexOffsetEXT;
extern RGLSYMGLVERTEXARRAYNORMALOFFSETEXTPROC __rglgen_glVertexArrayNormalOffsetEXT;
extern RGLSYMGLVERTEXARRAYTEXCOORDOFFSETEXTPROC __rglgen_glVertexArrayTexCoordOffsetEXT;
extern RGLSYMGLVERTEXARRAYMULTITEXCOORDOFFSETEXTPROC __rglgen_glVertexArrayMultiTexCoordOffsetEXT;
extern RGLSYMGLVERTEXARRAYFOGCOORDOFFSETEXTPROC __rglgen_glVertexArrayFogCoordOffsetEXT;
extern RGLSYMGLVERTEXARRAYSECONDARYCOLOROFFSETEXTPROC __rglgen_glVertexArraySecondaryColorOffsetEXT;
extern RGLSYMGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC __rglgen_glVertexArrayVertexAttribOffsetEXT;
extern RGLSYMGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC __rglgen_glVertexArrayVertexAttribIOffsetEXT;
extern RGLSYMGLENABLEVERTEXARRAYEXTPROC __rglgen_glEnableVertexArrayEXT;
extern RGLSYMGLDISABLEVERTEXARRAYEXTPROC __rglgen_glDisableVertexArrayEXT;
extern RGLSYMGLENABLEVERTEXARRAYATTRIBEXTPROC __rglgen_glEnableVertexArrayAttribEXT;
extern RGLSYMGLDISABLEVERTEXARRAYATTRIBEXTPROC __rglgen_glDisableVertexArrayAttribEXT;
extern RGLSYMGLGETVERTEXARRAYINTEGERVEXTPROC __rglgen_glGetVertexArrayIntegervEXT;
extern RGLSYMGLGETVERTEXARRAYPOINTERVEXTPROC __rglgen_glGetVertexArrayPointervEXT;
extern RGLSYMGLGETVERTEXARRAYINTEGERI_VEXTPROC __rglgen_glGetVertexArrayIntegeri_vEXT;
extern RGLSYMGLGETVERTEXARRAYPOINTERI_VEXTPROC __rglgen_glGetVertexArrayPointeri_vEXT;
extern RGLSYMGLMAPNAMEDBUFFERRANGEEXTPROC __rglgen_glMapNamedBufferRangeEXT;
extern RGLSYMGLFLUSHMAPPEDNAMEDBUFFERRANGEEXTPROC __rglgen_glFlushMappedNamedBufferRangeEXT;
extern RGLSYMGLNAMEDBUFFERSTORAGEEXTPROC __rglgen_glNamedBufferStorageEXT;
extern RGLSYMGLCLEARNAMEDBUFFERDATAEXTPROC __rglgen_glClearNamedBufferDataEXT;
extern RGLSYMGLCLEARNAMEDBUFFERSUBDATAEXTPROC __rglgen_glClearNamedBufferSubDataEXT;
extern RGLSYMGLNAMEDFRAMEBUFFERPARAMETERIEXTPROC __rglgen_glNamedFramebufferParameteriEXT;
extern RGLSYMGLGETNAMEDFRAMEBUFFERPARAMETERIVEXTPROC __rglgen_glGetNamedFramebufferParameterivEXT;
extern RGLSYMGLPROGRAMUNIFORM1DEXTPROC __rglgen_glProgramUniform1dEXT;
extern RGLSYMGLPROGRAMUNIFORM2DEXTPROC __rglgen_glProgramUniform2dEXT;
extern RGLSYMGLPROGRAMUNIFORM3DEXTPROC __rglgen_glProgramUniform3dEXT;
extern RGLSYMGLPROGRAMUNIFORM4DEXTPROC __rglgen_glProgramUniform4dEXT;
extern RGLSYMGLPROGRAMUNIFORM1DVEXTPROC __rglgen_glProgramUniform1dvEXT;
extern RGLSYMGLPROGRAMUNIFORM2DVEXTPROC __rglgen_glProgramUniform2dvEXT;
extern RGLSYMGLPROGRAMUNIFORM3DVEXTPROC __rglgen_glProgramUniform3dvEXT;
extern RGLSYMGLPROGRAMUNIFORM4DVEXTPROC __rglgen_glProgramUniform4dvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2DVEXTPROC __rglgen_glProgramUniformMatrix2dvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3DVEXTPROC __rglgen_glProgramUniformMatrix3dvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4DVEXTPROC __rglgen_glProgramUniformMatrix4dvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2X3DVEXTPROC __rglgen_glProgramUniformMatrix2x3dvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2X4DVEXTPROC __rglgen_glProgramUniformMatrix2x4dvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3X2DVEXTPROC __rglgen_glProgramUniformMatrix3x2dvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3X4DVEXTPROC __rglgen_glProgramUniformMatrix3x4dvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4X2DVEXTPROC __rglgen_glProgramUniformMatrix4x2dvEXT;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4X3DVEXTPROC __rglgen_glProgramUniformMatrix4x3dvEXT;
extern RGLSYMGLTEXTUREBUFFERRANGEEXTPROC __rglgen_glTextureBufferRangeEXT;
extern RGLSYMGLTEXTURESTORAGE1DEXTPROC __rglgen_glTextureStorage1DEXT;
extern RGLSYMGLTEXTURESTORAGE2DEXTPROC __rglgen_glTextureStorage2DEXT;
extern RGLSYMGLTEXTURESTORAGE3DEXTPROC __rglgen_glTextureStorage3DEXT;
extern RGLSYMGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC __rglgen_glTextureStorage2DMultisampleEXT;
extern RGLSYMGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC __rglgen_glTextureStorage3DMultisampleEXT;
extern RGLSYMGLVERTEXARRAYBINDVERTEXBUFFEREXTPROC __rglgen_glVertexArrayBindVertexBufferEXT;
extern RGLSYMGLVERTEXARRAYVERTEXATTRIBFORMATEXTPROC __rglgen_glVertexArrayVertexAttribFormatEXT;
extern RGLSYMGLVERTEXARRAYVERTEXATTRIBIFORMATEXTPROC __rglgen_glVertexArrayVertexAttribIFormatEXT;
extern RGLSYMGLVERTEXARRAYVERTEXATTRIBLFORMATEXTPROC __rglgen_glVertexArrayVertexAttribLFormatEXT;
extern RGLSYMGLVERTEXARRAYVERTEXATTRIBBINDINGEXTPROC __rglgen_glVertexArrayVertexAttribBindingEXT;
extern RGLSYMGLVERTEXARRAYVERTEXBINDINGDIVISOREXTPROC __rglgen_glVertexArrayVertexBindingDivisorEXT;
extern RGLSYMGLVERTEXARRAYVERTEXATTRIBLOFFSETEXTPROC __rglgen_glVertexArrayVertexAttribLOffsetEXT;
extern RGLSYMGLTEXTUREPAGECOMMITMENTEXTPROC __rglgen_glTexturePageCommitmentEXT;
extern RGLSYMGLVERTEXARRAYVERTEXATTRIBDIVISOREXTPROC __rglgen_glVertexArrayVertexAttribDivisorEXT;
extern RGLSYMGLDRAWARRAYSINSTANCEDEXTPROC __rglgen_glDrawArraysInstancedEXT;
extern RGLSYMGLDRAWELEMENTSINSTANCEDEXTPROC __rglgen_glDrawElementsInstancedEXT;
extern RGLSYMGLPOLYGONOFFSETCLAMPEXTPROC __rglgen_glPolygonOffsetClampEXT;
extern RGLSYMGLRASTERSAMPLESEXTPROC __rglgen_glRasterSamplesEXT;
extern RGLSYMGLUSESHADERPROGRAMEXTPROC __rglgen_glUseShaderProgramEXT;
extern RGLSYMGLACTIVEPROGRAMEXTPROC __rglgen_glActiveProgramEXT;
extern RGLSYMGLCREATESHADERPROGRAMEXTPROC __rglgen_glCreateShaderProgramEXT;
extern RGLSYMGLFRAMEBUFFERFETCHBARRIEREXTPROC __rglgen_glFramebufferFetchBarrierEXT;
extern RGLSYMGLWINDOWRECTANGLESEXTPROC __rglgen_glWindowRectanglesEXT;
extern RGLSYMGLMULTIDRAWARRAYSINDIRECTBINDLESSNVPROC __rglgen_glMultiDrawArraysIndirectBindlessNV;
extern RGLSYMGLMULTIDRAWELEMENTSINDIRECTBINDLESSNVPROC __rglgen_glMultiDrawElementsIndirectBindlessNV;
extern RGLSYMGLMULTIDRAWARRAYSINDIRECTBINDLESSCOUNTNVPROC __rglgen_glMultiDrawArraysIndirectBindlessCountNV;
extern RGLSYMGLMULTIDRAWELEMENTSINDIRECTBINDLESSCOUNTNVPROC __rglgen_glMultiDrawElementsIndirectBindlessCountNV;
extern RGLSYMGLGETTEXTUREHANDLENVPROC __rglgen_glGetTextureHandleNV;
extern RGLSYMGLGETTEXTURESAMPLERHANDLENVPROC __rglgen_glGetTextureSamplerHandleNV;
extern RGLSYMGLMAKETEXTUREHANDLERESIDENTNVPROC __rglgen_glMakeTextureHandleResidentNV;
extern RGLSYMGLMAKETEXTUREHANDLENONRESIDENTNVPROC __rglgen_glMakeTextureHandleNonResidentNV;
extern RGLSYMGLGETIMAGEHANDLENVPROC __rglgen_glGetImageHandleNV;
extern RGLSYMGLMAKEIMAGEHANDLERESIDENTNVPROC __rglgen_glMakeImageHandleResidentNV;
extern RGLSYMGLMAKEIMAGEHANDLENONRESIDENTNVPROC __rglgen_glMakeImageHandleNonResidentNV;
extern RGLSYMGLUNIFORMHANDLEUI64NVPROC __rglgen_glUniformHandleui64NV;
extern RGLSYMGLUNIFORMHANDLEUI64VNVPROC __rglgen_glUniformHandleui64vNV;
extern RGLSYMGLPROGRAMUNIFORMHANDLEUI64NVPROC __rglgen_glProgramUniformHandleui64NV;
extern RGLSYMGLPROGRAMUNIFORMHANDLEUI64VNVPROC __rglgen_glProgramUniformHandleui64vNV;
extern RGLSYMGLISTEXTUREHANDLERESIDENTNVPROC __rglgen_glIsTextureHandleResidentNV;
extern RGLSYMGLISIMAGEHANDLERESIDENTNVPROC __rglgen_glIsImageHandleResidentNV;
extern RGLSYMGLBLENDPARAMETERINVPROC __rglgen_glBlendParameteriNV;
extern RGLSYMGLBLENDBARRIERNVPROC __rglgen_glBlendBarrierNV;
extern RGLSYMGLVIEWPORTPOSITIONWSCALENVPROC __rglgen_glViewportPositionWScaleNV;
extern RGLSYMGLCREATESTATESNVPROC __rglgen_glCreateStatesNV;
extern RGLSYMGLDELETESTATESNVPROC __rglgen_glDeleteStatesNV;
extern RGLSYMGLISSTATENVPROC __rglgen_glIsStateNV;
extern RGLSYMGLSTATECAPTURENVPROC __rglgen_glStateCaptureNV;
extern RGLSYMGLGETCOMMANDHEADERNVPROC __rglgen_glGetCommandHeaderNV;
extern RGLSYMGLGETSTAGEINDEXNVPROC __rglgen_glGetStageIndexNV;
extern RGLSYMGLDRAWCOMMANDSNVPROC __rglgen_glDrawCommandsNV;
extern RGLSYMGLDRAWCOMMANDSADDRESSNVPROC __rglgen_glDrawCommandsAddressNV;
extern RGLSYMGLDRAWCOMMANDSSTATESNVPROC __rglgen_glDrawCommandsStatesNV;
extern RGLSYMGLDRAWCOMMANDSSTATESADDRESSNVPROC __rglgen_glDrawCommandsStatesAddressNV;
extern RGLSYMGLCREATECOMMANDLISTSNVPROC __rglgen_glCreateCommandListsNV;
extern RGLSYMGLDELETECOMMANDLISTSNVPROC __rglgen_glDeleteCommandListsNV;
extern RGLSYMGLISCOMMANDLISTNVPROC __rglgen_glIsCommandListNV;
extern RGLSYMGLLISTDRAWCOMMANDSSTATESCLIENTNVPROC __rglgen_glListDrawCommandsStatesClientNV;
extern RGLSYMGLCOMMANDLISTSEGMENTSNVPROC __rglgen_glCommandListSegmentsNV;
extern RGLSYMGLCOMPILECOMMANDLISTNVPROC __rglgen_glCompileCommandListNV;
extern RGLSYMGLCALLCOMMANDLISTNVPROC __rglgen_glCallCommandListNV;
extern RGLSYMGLBEGINCONDITIONALRENDERNVPROC __rglgen_glBeginConditionalRenderNV;
extern RGLSYMGLENDCONDITIONALRENDERNVPROC __rglgen_glEndConditionalRenderNV;
extern RGLSYMGLSUBPIXELPRECISIONBIASNVPROC __rglgen_glSubpixelPrecisionBiasNV;
extern RGLSYMGLCONSERVATIVERASTERPARAMETERFNVPROC __rglgen_glConservativeRasterParameterfNV;
extern RGLSYMGLCONSERVATIVERASTERPARAMETERINVPROC __rglgen_glConservativeRasterParameteriNV;
extern RGLSYMGLDRAWVKIMAGENVPROC __rglgen_glDrawVkImageNV;
extern RGLSYMGLWAITVKSEMAPHORENVPROC __rglgen_glWaitVkSemaphoreNV;
extern RGLSYMGLSIGNALVKSEMAPHORENVPROC __rglgen_glSignalVkSemaphoreNV;
extern RGLSYMGLSIGNALVKFENCENVPROC __rglgen_glSignalVkFenceNV;
extern RGLSYMGLFRAGMENTCOVERAGECOLORNVPROC __rglgen_glFragmentCoverageColorNV;
extern RGLSYMGLCOVERAGEMODULATIONTABLENVPROC __rglgen_glCoverageModulationTableNV;
extern RGLSYMGLGETCOVERAGEMODULATIONTABLENVPROC __rglgen_glGetCoverageModulationTableNV;
extern RGLSYMGLCOVERAGEMODULATIONNVPROC __rglgen_glCoverageModulationNV;
extern RGLSYMGLRENDERBUFFERSTORAGEMULTISAMPLECOVERAGENVPROC __rglgen_glRenderbufferStorageMultisampleCoverageNV;
extern RGLSYMGLUNIFORM1I64NVPROC __rglgen_glUniform1i64NV;
extern RGLSYMGLUNIFORM2I64NVPROC __rglgen_glUniform2i64NV;
extern RGLSYMGLUNIFORM3I64NVPROC __rglgen_glUniform3i64NV;
extern RGLSYMGLUNIFORM4I64NVPROC __rglgen_glUniform4i64NV;
extern RGLSYMGLUNIFORM1I64VNVPROC __rglgen_glUniform1i64vNV;
extern RGLSYMGLUNIFORM2I64VNVPROC __rglgen_glUniform2i64vNV;
extern RGLSYMGLUNIFORM3I64VNVPROC __rglgen_glUniform3i64vNV;
extern RGLSYMGLUNIFORM4I64VNVPROC __rglgen_glUniform4i64vNV;
extern RGLSYMGLUNIFORM1UI64NVPROC __rglgen_glUniform1ui64NV;
extern RGLSYMGLUNIFORM2UI64NVPROC __rglgen_glUniform2ui64NV;
extern RGLSYMGLUNIFORM3UI64NVPROC __rglgen_glUniform3ui64NV;
extern RGLSYMGLUNIFORM4UI64NVPROC __rglgen_glUniform4ui64NV;
extern RGLSYMGLUNIFORM1UI64VNVPROC __rglgen_glUniform1ui64vNV;
extern RGLSYMGLUNIFORM2UI64VNVPROC __rglgen_glUniform2ui64vNV;
extern RGLSYMGLUNIFORM3UI64VNVPROC __rglgen_glUniform3ui64vNV;
extern RGLSYMGLUNIFORM4UI64VNVPROC __rglgen_glUniform4ui64vNV;
extern RGLSYMGLGETUNIFORMI64VNVPROC __rglgen_glGetUniformi64vNV;
extern RGLSYMGLPROGRAMUNIFORM1I64NVPROC __rglgen_glProgramUniform1i64NV;
extern RGLSYMGLPROGRAMUNIFORM2I64NVPROC __rglgen_glProgramUniform2i64NV;
extern RGLSYMGLPROGRAMUNIFORM3I64NVPROC __rglgen_glProgramUniform3i64NV;
extern RGLSYMGLPROGRAMUNIFORM4I64NVPROC __rglgen_glProgramUniform4i64NV;
extern RGLSYMGLPROGRAMUNIFORM1I64VNVPROC __rglgen_glProgramUniform1i64vNV;
extern RGLSYMGLPROGRAMUNIFORM2I64VNVPROC __rglgen_glProgramUniform2i64vNV;
extern RGLSYMGLPROGRAMUNIFORM3I64VNVPROC __rglgen_glProgramUniform3i64vNV;
extern RGLSYMGLPROGRAMUNIFORM4I64VNVPROC __rglgen_glProgramUniform4i64vNV;
extern RGLSYMGLPROGRAMUNIFORM1UI64NVPROC __rglgen_glProgramUniform1ui64NV;
extern RGLSYMGLPROGRAMUNIFORM2UI64NVPROC __rglgen_glProgramUniform2ui64NV;
extern RGLSYMGLPROGRAMUNIFORM3UI64NVPROC __rglgen_glProgramUniform3ui64NV;
extern RGLSYMGLPROGRAMUNIFORM4UI64NVPROC __rglgen_glProgramUniform4ui64NV;
extern RGLSYMGLPROGRAMUNIFORM1UI64VNVPROC __rglgen_glProgramUniform1ui64vNV;
extern RGLSYMGLPROGRAMUNIFORM2UI64VNVPROC __rglgen_glProgramUniform2ui64vNV;
extern RGLSYMGLPROGRAMUNIFORM3UI64VNVPROC __rglgen_glProgramUniform3ui64vNV;
extern RGLSYMGLPROGRAMUNIFORM4UI64VNVPROC __rglgen_glProgramUniform4ui64vNV;
extern RGLSYMGLGETINTERNALFORMATSAMPLEIVNVPROC __rglgen_glGetInternalformatSampleivNV;
extern RGLSYMGLGETMEMORYOBJECTDETACHEDRESOURCESUIVNVPROC __rglgen_glGetMemoryObjectDetachedResourcesuivNV;
extern RGLSYMGLRESETMEMORYOBJECTPARAMETERNVPROC __rglgen_glResetMemoryObjectParameterNV;
extern RGLSYMGLTEXATTACHMEMORYNVPROC __rglgen_glTexAttachMemoryNV;
extern RGLSYMGLBUFFERATTACHMEMORYNVPROC __rglgen_glBufferAttachMemoryNV;
extern RGLSYMGLTEXTUREATTACHMEMORYNVPROC __rglgen_glTextureAttachMemoryNV;
extern RGLSYMGLNAMEDBUFFERATTACHMEMORYNVPROC __rglgen_glNamedBufferAttachMemoryNV;
extern RGLSYMGLDRAWMESHTASKSNVPROC __rglgen_glDrawMeshTasksNV;
extern RGLSYMGLDRAWMESHTASKSINDIRECTNVPROC __rglgen_glDrawMeshTasksIndirectNV;
extern RGLSYMGLMULTIDRAWMESHTASKSINDIRECTNVPROC __rglgen_glMultiDrawMeshTasksIndirectNV;
extern RGLSYMGLMULTIDRAWMESHTASKSINDIRECTCOUNTNVPROC __rglgen_glMultiDrawMeshTasksIndirectCountNV;
extern RGLSYMGLGENPATHSNVPROC __rglgen_glGenPathsNV;
extern RGLSYMGLDELETEPATHSNVPROC __rglgen_glDeletePathsNV;
extern RGLSYMGLISPATHNVPROC __rglgen_glIsPathNV;
extern RGLSYMGLPATHCOMMANDSNVPROC __rglgen_glPathCommandsNV;
extern RGLSYMGLPATHCOORDSNVPROC __rglgen_glPathCoordsNV;
extern RGLSYMGLPATHSUBCOMMANDSNVPROC __rglgen_glPathSubCommandsNV;
extern RGLSYMGLPATHSUBCOORDSNVPROC __rglgen_glPathSubCoordsNV;
extern RGLSYMGLPATHSTRINGNVPROC __rglgen_glPathStringNV;
extern RGLSYMGLPATHGLYPHSNVPROC __rglgen_glPathGlyphsNV;
extern RGLSYMGLPATHGLYPHRANGENVPROC __rglgen_glPathGlyphRangeNV;
extern RGLSYMGLWEIGHTPATHSNVPROC __rglgen_glWeightPathsNV;
extern RGLSYMGLCOPYPATHNVPROC __rglgen_glCopyPathNV;
extern RGLSYMGLINTERPOLATEPATHSNVPROC __rglgen_glInterpolatePathsNV;
extern RGLSYMGLTRANSFORMPATHNVPROC __rglgen_glTransformPathNV;
extern RGLSYMGLPATHPARAMETERIVNVPROC __rglgen_glPathParameterivNV;
extern RGLSYMGLPATHPARAMETERINVPROC __rglgen_glPathParameteriNV;
extern RGLSYMGLPATHPARAMETERFVNVPROC __rglgen_glPathParameterfvNV;
extern RGLSYMGLPATHPARAMETERFNVPROC __rglgen_glPathParameterfNV;
extern RGLSYMGLPATHDASHARRAYNVPROC __rglgen_glPathDashArrayNV;
extern RGLSYMGLPATHSTENCILFUNCNVPROC __rglgen_glPathStencilFuncNV;
extern RGLSYMGLPATHSTENCILDEPTHOFFSETNVPROC __rglgen_glPathStencilDepthOffsetNV;
extern RGLSYMGLSTENCILFILLPATHNVPROC __rglgen_glStencilFillPathNV;
extern RGLSYMGLSTENCILSTROKEPATHNVPROC __rglgen_glStencilStrokePathNV;
extern RGLSYMGLSTENCILFILLPATHINSTANCEDNVPROC __rglgen_glStencilFillPathInstancedNV;
extern RGLSYMGLSTENCILSTROKEPATHINSTANCEDNVPROC __rglgen_glStencilStrokePathInstancedNV;
extern RGLSYMGLPATHCOVERDEPTHFUNCNVPROC __rglgen_glPathCoverDepthFuncNV;
extern RGLSYMGLCOVERFILLPATHNVPROC __rglgen_glCoverFillPathNV;
extern RGLSYMGLCOVERSTROKEPATHNVPROC __rglgen_glCoverStrokePathNV;
extern RGLSYMGLCOVERFILLPATHINSTANCEDNVPROC __rglgen_glCoverFillPathInstancedNV;
extern RGLSYMGLCOVERSTROKEPATHINSTANCEDNVPROC __rglgen_glCoverStrokePathInstancedNV;
extern RGLSYMGLGETPATHPARAMETERIVNVPROC __rglgen_glGetPathParameterivNV;
extern RGLSYMGLGETPATHPARAMETERFVNVPROC __rglgen_glGetPathParameterfvNV;
extern RGLSYMGLGETPATHCOMMANDSNVPROC __rglgen_glGetPathCommandsNV;
extern RGLSYMGLGETPATHCOORDSNVPROC __rglgen_glGetPathCoordsNV;
extern RGLSYMGLGETPATHDASHARRAYNVPROC __rglgen_glGetPathDashArrayNV;
extern RGLSYMGLGETPATHMETRICSNVPROC __rglgen_glGetPathMetricsNV;
extern RGLSYMGLGETPATHMETRICRANGENVPROC __rglgen_glGetPathMetricRangeNV;
extern RGLSYMGLGETPATHSPACINGNVPROC __rglgen_glGetPathSpacingNV;
extern RGLSYMGLISPOINTINFILLPATHNVPROC __rglgen_glIsPointInFillPathNV;
extern RGLSYMGLISPOINTINSTROKEPATHNVPROC __rglgen_glIsPointInStrokePathNV;
extern RGLSYMGLGETPATHLENGTHNVPROC __rglgen_glGetPathLengthNV;
extern RGLSYMGLPOINTALONGPATHNVPROC __rglgen_glPointAlongPathNV;
extern RGLSYMGLMATRIXLOAD3X2FNVPROC __rglgen_glMatrixLoad3x2fNV;
extern RGLSYMGLMATRIXLOAD3X3FNVPROC __rglgen_glMatrixLoad3x3fNV;
extern RGLSYMGLMATRIXLOADTRANSPOSE3X3FNVPROC __rglgen_glMatrixLoadTranspose3x3fNV;
extern RGLSYMGLMATRIXMULT3X2FNVPROC __rglgen_glMatrixMult3x2fNV;
extern RGLSYMGLMATRIXMULT3X3FNVPROC __rglgen_glMatrixMult3x3fNV;
extern RGLSYMGLMATRIXMULTTRANSPOSE3X3FNVPROC __rglgen_glMatrixMultTranspose3x3fNV;
extern RGLSYMGLSTENCILTHENCOVERFILLPATHNVPROC __rglgen_glStencilThenCoverFillPathNV;
extern RGLSYMGLSTENCILTHENCOVERSTROKEPATHNVPROC __rglgen_glStencilThenCoverStrokePathNV;
extern RGLSYMGLSTENCILTHENCOVERFILLPATHINSTANCEDNVPROC __rglgen_glStencilThenCoverFillPathInstancedNV;
extern RGLSYMGLSTENCILTHENCOVERSTROKEPATHINSTANCEDNVPROC __rglgen_glStencilThenCoverStrokePathInstancedNV;
extern RGLSYMGLPATHGLYPHINDEXRANGENVPROC __rglgen_glPathGlyphIndexRangeNV;
extern RGLSYMGLPATHGLYPHINDEXARRAYNVPROC __rglgen_glPathGlyphIndexArrayNV;
extern RGLSYMGLPATHMEMORYGLYPHINDEXARRAYNVPROC __rglgen_glPathMemoryGlyphIndexArrayNV;
extern RGLSYMGLPROGRAMPATHFRAGMENTINPUTGENNVPROC __rglgen_glProgramPathFragmentInputGenNV;
extern RGLSYMGLGETPROGRAMRESOURCEFVNVPROC __rglgen_glGetProgramResourcefvNV;
extern RGLSYMGLFRAMEBUFFERSAMPLELOCATIONSFVNVPROC __rglgen_glFramebufferSampleLocationsfvNV;
extern RGLSYMGLNAMEDFRAMEBUFFERSAMPLELOCATIONSFVNVPROC __rglgen_glNamedFramebufferSampleLocationsfvNV;
extern RGLSYMGLRESOLVEDEPTHVALUESNVPROC __rglgen_glResolveDepthValuesNV;
extern RGLSYMGLSCISSOREXCLUSIVENVPROC __rglgen_glScissorExclusiveNV;
extern RGLSYMGLSCISSOREXCLUSIVEARRAYVNVPROC __rglgen_glScissorExclusiveArrayvNV;
extern RGLSYMGLMAKEBUFFERRESIDENTNVPROC __rglgen_glMakeBufferResidentNV;
extern RGLSYMGLMAKEBUFFERNONRESIDENTNVPROC __rglgen_glMakeBufferNonResidentNV;
extern RGLSYMGLISBUFFERRESIDENTNVPROC __rglgen_glIsBufferResidentNV;
extern RGLSYMGLMAKENAMEDBUFFERRESIDENTNVPROC __rglgen_glMakeNamedBufferResidentNV;
extern RGLSYMGLMAKENAMEDBUFFERNONRESIDENTNVPROC __rglgen_glMakeNamedBufferNonResidentNV;
extern RGLSYMGLISNAMEDBUFFERRESIDENTNVPROC __rglgen_glIsNamedBufferResidentNV;
extern RGLSYMGLGETBUFFERPARAMETERUI64VNVPROC __rglgen_glGetBufferParameterui64vNV;
extern RGLSYMGLGETNAMEDBUFFERPARAMETERUI64VNVPROC __rglgen_glGetNamedBufferParameterui64vNV;
extern RGLSYMGLGETINTEGERUI64VNVPROC __rglgen_glGetIntegerui64vNV;
extern RGLSYMGLUNIFORMUI64NVPROC __rglgen_glUniformui64NV;
extern RGLSYMGLUNIFORMUI64VNVPROC __rglgen_glUniformui64vNV;
extern RGLSYMGLGETUNIFORMUI64VNVPROC __rglgen_glGetUniformui64vNV;
extern RGLSYMGLPROGRAMUNIFORMUI64NVPROC __rglgen_glProgramUniformui64NV;
extern RGLSYMGLPROGRAMUNIFORMUI64VNVPROC __rglgen_glProgramUniformui64vNV;
extern RGLSYMGLBINDSHADINGRATEIMAGENVPROC __rglgen_glBindShadingRateImageNV;
extern RGLSYMGLGETSHADINGRATEIMAGEPALETTENVPROC __rglgen_glGetShadingRateImagePaletteNV;
extern RGLSYMGLGETSHADINGRATESAMPLELOCATIONIVNVPROC __rglgen_glGetShadingRateSampleLocationivNV;
extern RGLSYMGLSHADINGRATEIMAGEBARRIERNVPROC __rglgen_glShadingRateImageBarrierNV;
extern RGLSYMGLSHADINGRATEIMAGEPALETTENVPROC __rglgen_glShadingRateImagePaletteNV;
extern RGLSYMGLSHADINGRATESAMPLEORDERNVPROC __rglgen_glShadingRateSampleOrderNV;
extern RGLSYMGLSHADINGRATESAMPLEORDERCUSTOMNVPROC __rglgen_glShadingRateSampleOrderCustomNV;
extern RGLSYMGLTEXTUREBARRIERNVPROC __rglgen_glTextureBarrierNV;
extern RGLSYMGLVERTEXATTRIBL1I64NVPROC __rglgen_glVertexAttribL1i64NV;
extern RGLSYMGLVERTEXATTRIBL2I64NVPROC __rglgen_glVertexAttribL2i64NV;
extern RGLSYMGLVERTEXATTRIBL3I64NVPROC __rglgen_glVertexAttribL3i64NV;
extern RGLSYMGLVERTEXATTRIBL4I64NVPROC __rglgen_glVertexAttribL4i64NV;
extern RGLSYMGLVERTEXATTRIBL1I64VNVPROC __rglgen_glVertexAttribL1i64vNV;
extern RGLSYMGLVERTEXATTRIBL2I64VNVPROC __rglgen_glVertexAttribL2i64vNV;
extern RGLSYMGLVERTEXATTRIBL3I64VNVPROC __rglgen_glVertexAttribL3i64vNV;
extern RGLSYMGLVERTEXATTRIBL4I64VNVPROC __rglgen_glVertexAttribL4i64vNV;
extern RGLSYMGLVERTEXATTRIBL1UI64NVPROC __rglgen_glVertexAttribL1ui64NV;
extern RGLSYMGLVERTEXATTRIBL2UI64NVPROC __rglgen_glVertexAttribL2ui64NV;
extern RGLSYMGLVERTEXATTRIBL3UI64NVPROC __rglgen_glVertexAttribL3ui64NV;
extern RGLSYMGLVERTEXATTRIBL4UI64NVPROC __rglgen_glVertexAttribL4ui64NV;
extern RGLSYMGLVERTEXATTRIBL1UI64VNVPROC __rglgen_glVertexAttribL1ui64vNV;
extern RGLSYMGLVERTEXATTRIBL2UI64VNVPROC __rglgen_glVertexAttribL2ui64vNV;
extern RGLSYMGLVERTEXATTRIBL3UI64VNVPROC __rglgen_glVertexAttribL3ui64vNV;
extern RGLSYMGLVERTEXATTRIBL4UI64VNVPROC __rglgen_glVertexAttribL4ui64vNV;
extern RGLSYMGLGETVERTEXATTRIBLI64VNVPROC __rglgen_glGetVertexAttribLi64vNV;
extern RGLSYMGLGETVERTEXATTRIBLUI64VNVPROC __rglgen_glGetVertexAttribLui64vNV;
extern RGLSYMGLVERTEXATTRIBLFORMATNVPROC __rglgen_glVertexAttribLFormatNV;
extern RGLSYMGLBUFFERADDRESSRANGENVPROC __rglgen_glBufferAddressRangeNV;
extern RGLSYMGLVERTEXFORMATNVPROC __rglgen_glVertexFormatNV;
extern RGLSYMGLNORMALFORMATNVPROC __rglgen_glNormalFormatNV;
extern RGLSYMGLCOLORFORMATNVPROC __rglgen_glColorFormatNV;
extern RGLSYMGLINDEXFORMATNVPROC __rglgen_glIndexFormatNV;
extern RGLSYMGLTEXCOORDFORMATNVPROC __rglgen_glTexCoordFormatNV;
extern RGLSYMGLEDGEFLAGFORMATNVPROC __rglgen_glEdgeFlagFormatNV;
extern RGLSYMGLSECONDARYCOLORFORMATNVPROC __rglgen_glSecondaryColorFormatNV;
extern RGLSYMGLFOGCOORDFORMATNVPROC __rglgen_glFogCoordFormatNV;
extern RGLSYMGLVERTEXATTRIBFORMATNVPROC __rglgen_glVertexAttribFormatNV;
extern RGLSYMGLVERTEXATTRIBIFORMATNVPROC __rglgen_glVertexAttribIFormatNV;
extern RGLSYMGLGETINTEGERUI64I_VNVPROC __rglgen_glGetIntegerui64i_vNV;
extern RGLSYMGLVIEWPORTSWIZZLENVPROC __rglgen_glViewportSwizzleNV;
extern RGLSYMGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC __rglgen_glFramebufferTextureMultiviewOVR;

#ifndef __APPLE__
typedef GLsync (APIENTRYP RGLSYMGLCREATESYNCFROMCLEVENTARBPROC) (struct _cl_context *context, struct _cl_event *event, GLbitfield flags);
typedef GLVULKANPROCNV (APIENTRYP RGLSYMGLGETVKPROCADDRNVPROC) (const GLchar *name);
extern RGLSYMGLCREATESYNCFROMCLEVENTARBPROC __rglgen_glCreateSyncFromCLeventARB;
extern RGLSYMGLGETVKPROCADDRNVPROC __rglgen_glGetVkProcAddrNV;
#endif

struct rglgen_sym_map { const char *sym; void *ptr; };
extern const struct rglgen_sym_map rglgen_symbol_map[];
#ifdef __cplusplus
}
#endif
#endif
