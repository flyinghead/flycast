#pragma once
#include <map>
#include "gles.h"

#define TEXTURE_ID_CACHE_SIZE 32

class GLCache {
public:
	GLCache() { Reset(); }

	void BindTexture(GLenum target,  GLuint texture) {
      if (target == GL_TEXTURE_2D && !_disable_cache) {
      	if (texture != _texture) {
			glBindTexture(target, texture);
			_texture = texture;
			}
		}
		else
			glBindTexture(target, texture);
	}

	void BlendFunc(GLenum sfactor, GLenum dfactor) {
      if (sfactor != _src_blend_factor || dfactor != _dst_blend_factor || _disable_cache) {
			_src_blend_factor = sfactor;
			_dst_blend_factor = dfactor;
			glBlendFunc(sfactor, dfactor);
		}
	}

	void ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
      if (red != _clear_r || green != _clear_g || blue != _clear_b || alpha != _clear_a || _disable_cache) {
			_clear_r = red;
			_clear_g = green;
			_clear_b = blue;
			_clear_a = alpha;
			glClearColor(red, green, blue, alpha);
		}
	}

	void CullFace(GLenum mode) {
      if (mode != _cull_face || _disable_cache) {
			_cull_face = mode;
			glCullFace(mode);
		}
	}

	void DeleteTextures(GLsizei n, const GLuint *textures) {
		for (int i = 0; i < n; i++)
      {
         _texture_params.erase(textures[i]);
			if (textures[i] == _texture)
				_texture = 0;
		}
		glDeleteTextures(n, textures);
	}

	void DepthFunc(GLenum func) {
		if (func != _depth_func || _disable_cache) {
			_depth_func = func;
			glDepthFunc(func);
		}
	}

	void DepthMask(GLboolean flag) {
      if (flag != _depth_mask || _disable_cache) {
			_depth_mask = flag;
			glDepthMask(flag);
		}
	}

	void Enable(GLenum cap) {
		setCapability(cap, GL_TRUE);
	}

	void Disable(GLenum cap) {
		setCapability(cap, GL_FALSE);
	}

	void UseProgram(GLuint program) {
      if (program != _program || _disable_cache) {
			_program = program;
			glUseProgram(program);
		}
	}

	void StencilFunc(GLenum func, GLint ref, GLuint mask) {
      if (_stencil_func != func || _stencil_ref != ref || _stencil_fmask != mask || _disable_cache) {
			_stencil_func = func;
			_stencil_ref = ref;
			_stencil_fmask = mask;

			glStencilFunc(func, ref, mask);
		}
	}

	void StencilOp(GLenum sfail, GLenum dpfail, GLenum dppass) {
      if (_stencil_sfail != sfail ||_stencil_dpfail != dpfail || _stencil_dppass != dppass || _disable_cache) {
			_stencil_sfail = sfail;
			_stencil_dpfail = dpfail;
			_stencil_dppass = dppass;

			glStencilOp(sfail, dpfail, dppass);
		}
	}

	void StencilMask(GLuint mask) {
      if (_stencil_mask != mask || _disable_cache) {
			_stencil_mask = mask;
			glStencilMask(mask);
		}
	}

	void TexParameteri(GLenum target,  GLenum pname,  GLint param)
	{
		if (target == GL_TEXTURE_2D && !_disable_cache)
		{
			TextureParameters &cur_params = _texture_params[_texture];
			switch (pname) {
			case GL_TEXTURE_MIN_FILTER:
				if (cur_params._min_filter == param)
					return;
				cur_params._min_filter = param;
				break;
			case GL_TEXTURE_MAG_FILTER:
				if (cur_params._mag_filter == param)
					return;
				cur_params._mag_filter = param;
				break;
			case GL_TEXTURE_WRAP_S:
				if (cur_params._wrap_s == param)
					return;
				cur_params._wrap_s = param;
				break;
			case GL_TEXTURE_WRAP_T:
				if (cur_params._wrap_t == param)
					return;
				cur_params._wrap_t = param;
				break;
			}
		}
		glTexParameteri(target, pname, param);
	}

	GLuint GenTexture() {
		if (_texture_cache_size == 0) {
			_texture_cache_size = TEXTURE_ID_CACHE_SIZE;
			glGenTextures(_texture_cache_size,  _texture_ids);
		}
		return _texture_ids[--_texture_cache_size];
	}

	void DeleteProgram(GLuint program)
	{
		GLsizei shader_count;
		GLuint shaders[2];
		glGetAttachedShaders(program, ARRAY_SIZE(shaders), &shader_count, shaders);
		for (int i = 0; i < shader_count; i++)
			glDeleteShader(shaders[i]);

		glDeleteProgram(program);
		if (_program == program)
			_program = 0;
	}

	void Reset() {
		_texture = 0xFFFFFFFFu;
		_src_blend_factor = 0xFFFFFFFFu;
		_dst_blend_factor = 0xFFFFFFFFu;
		_clear_r = -1.f;
		_clear_g = -1.f;
		_clear_b = -1.f;
		_clear_a = -1.f;
		_en_blend = 0xFF;
		_en_cull_face = 0xFF;
		_en_depth_test = 0xFF;
		_en_scissor_test = 0xFF;
		_en_stencil_test = 0xFF;
		_cull_face = 0xFFFFFFFFu;
		_depth_func = 0xFFFFFFFFu;
		_depth_mask = 0xFF;
		_program = 0xFFFFFFFFu;
		_texture_cache_size = 0;
		_stencil_func = 0xFFFFFFFFu;
		_stencil_ref = -1;
		_stencil_fmask = 0;
		_stencil_sfail = 0xFFFFFFFFu;
		_stencil_dpfail = 0xFFFFFFFFu;
		_stencil_dppass = 0xFFFFFFFFu;
		_stencil_mask = 0;
	}

	void DisableCache() { _disable_cache = true; }
	void EnableCache()
	{
	   _disable_cache = false;
	   Reset();
	}

private:
	class TextureParameters {
	public:
		TextureParameters() : _min_filter(0xFFFFFFFFu), _mag_filter(0xFFFFFFFFu), _wrap_s(0xFFFFFFFFu), _wrap_t(0xFFFFFFFFu) {}

		GLenum _min_filter;
		GLenum _mag_filter;
		GLenum _wrap_s;
		GLenum _wrap_t;
	};

	void setCapability(GLenum cap, GLboolean value) {
		GLboolean *pCap = NULL;
		switch (cap) {
		case GL_BLEND:
			pCap = &_en_blend;
			break;
		case GL_CULL_FACE:
			pCap = &_en_cull_face;
			break;
		case GL_DEPTH_TEST:
			pCap = &_en_depth_test;
			break;
		case GL_SCISSOR_TEST:
			pCap = &_en_scissor_test;
			break;
		case GL_STENCIL_TEST:
			pCap = &_en_stencil_test;
			break;
		}
		if (pCap != NULL) {
			if (*pCap == value && !_disable_cache)
				return;
			*pCap = value;
		}
		if (value)
			glEnable(cap);
		else
			glDisable(cap);
	}

	GLuint _array_buffer;
	GLuint _element_array_buffer;
	GLuint _texture;
	GLenum _src_blend_factor;
	GLenum _dst_blend_factor;
	GLclampf _clear_r;
	GLclampf _clear_g;
	GLclampf _clear_b;
	GLclampf _clear_a;
	GLboolean _en_blend;
	GLboolean _en_cull_face;
	GLboolean _en_depth_test;
	GLboolean _en_scissor_test;
	GLboolean _en_stencil_test;
	GLenum _cull_face;
	GLenum _depth_func;
	GLboolean _depth_mask;
	GLuint _program;
	GLenum _stencil_func;
	GLint _stencil_ref;
	GLuint _stencil_fmask;
	GLenum _stencil_sfail;
	GLenum _stencil_dpfail;
	GLenum _stencil_dppass;
	GLuint _stencil_mask;
	GLuint _texture_ids[TEXTURE_ID_CACHE_SIZE];
	GLuint _texture_cache_size;
	std::map<GLuint, TextureParameters> _texture_params;
	bool _disable_cache;
};

extern GLCache glcache;
