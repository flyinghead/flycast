#pragma once
#include <map>
#include "gles.h"

#define TEXTURE_ID_CACHE_SIZE 32

class GLCache {
public:
	GLCache() { Reset(); }

	void BindTexture(GLenum target,  GLuint texture) {
		if (target == GL_TEXTURE_2D && texture != _texture) {
			glBindTexture(target, texture);
			_texture = texture;
		}
		else
			glBindTexture(target, texture);
	}

	void BlendFunc(GLenum sfactor, GLenum dfactor) {
		if (sfactor != _src_blend_factor || dfactor != _dst_blend_factor) {
			_src_blend_factor = sfactor;
			_dst_blend_factor = dfactor;
			glBlendFunc(sfactor, dfactor);
		}
	}

	void ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
		if (red != _clear_r || green != _clear_g || blue != _clear_b || alpha != _clear_a) {
			_clear_r = red;
			_clear_g = green;
			_clear_b = blue;
			_clear_a = alpha;
			glClearColor(red, green, blue, alpha);
		}
	}

	void CullFace(GLenum mode) {
		if (mode != _cull_face) {
			_cull_face = mode;
			glCullFace(mode);
		}
	}

	void DeleteTextures(GLsizei n, const GLuint *textures) {
		for (int i = 0; i < n; i++) {
			_texture_params.erase(textures[i]);
			if (textures[i] == _texture)
				_texture = 0;
		}
		glDeleteTextures(n, textures);
	}

	void DepthFunc(GLenum func) {
		if (func != _depth_func) {
			_depth_func = func;
			glDepthFunc(func);
		}
	}

	void DepthMask(GLboolean flag) {
		if (flag != _depth_mask) {
			_depth_mask = flag;
			glDepthMask(flag);
		}
	}

	void Enable(GLenum cap) {
		setCapability(cap, true);
	}

	void Disable(GLenum cap) {
		setCapability(cap, false);
	}

	void UseProgram(GLuint program) {
		if (program != _program) {
			_program = program;
			glUseProgram(program);
		}
	}

	void StencilFunc(GLenum func, GLint ref, GLuint mask) {
		if (_stencil_func != func || _stencil_ref != ref || _stencil_fmask != mask) {
			_stencil_func = func;
			_stencil_ref = ref;
			_stencil_fmask = mask;
			glStencilFunc(func, ref, mask);
		}
	}

	void StencilOp(GLenum sfail, GLenum dpfail, GLenum dppass) {
		if (_stencil_sfail != sfail ||_stencil_dpfail != dpfail || _stencil_dppass != dppass) {
			_stencil_sfail = sfail;
			_stencil_dpfail = dpfail;
			_stencil_dppass = dppass;
			glStencilOp(sfail, dpfail, dppass);
		}
	}

	void StencilMask(GLuint mask) {
		if (_stencil_mask != mask) {
			_stencil_mask = mask;
			glStencilMask(mask);
		}
	}

	void TexParameteri(GLenum target,  GLenum pname,  GLint param) {
		if (target == GL_TEXTURE_2D)
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

	void Reset() {
		_texture = 0;
		_src_blend_factor = GL_ONE;
		_dst_blend_factor = GL_ZERO;
		_clear_r = 0.f;
		_clear_g = 0.f;
		_clear_b = 0.f;
		_clear_a = 0.f;
		_en_blend = false;
		_en_cull_face = false;
		_en_depth_test = false;
		_en_scissor_test = false;
		_en_stencil_test = false;
		_cull_face = GL_BACK;
		_depth_func = GL_LESS;
		_depth_mask = true;
		_program = 0;
		_texture_cache_size = 0;
		_stencil_func = GL_ALWAYS;
		_stencil_ref = 0;
		_stencil_fmask = ~0;
		_stencil_sfail = GL_KEEP;
		_stencil_dpfail = GL_KEEP;
		_stencil_dppass = GL_KEEP;
		_stencil_mask = ~0;
	}

private:
	class TextureParameters {
	public:
		TextureParameters() : _min_filter(GL_NEAREST_MIPMAP_LINEAR), _mag_filter(GL_LINEAR), _wrap_s(GL_REPEAT), _wrap_t(GL_REPEAT) {}

		GLenum _min_filter;
		GLenum _mag_filter;
		GLenum _wrap_s;
		GLenum _wrap_t;
	};

	void setCapability(GLenum cap, bool value) {
		bool *pCap = NULL;
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
			if (*pCap == value)
				return;
			*pCap = value;
		}
		if (value)
			glEnable(cap);
		else
			glDisable(cap);
	}

	GLuint _texture;
	GLenum _src_blend_factor;
	GLenum _dst_blend_factor;
	GLclampf _clear_r;
	GLclampf _clear_g;
	GLclampf _clear_b;
	GLclampf _clear_a;
	bool _en_blend;
	bool _en_cull_face;
	bool _en_depth_test;
	bool _en_scissor_test;
	bool _en_stencil_test;
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
};

extern GLCache glcache;
