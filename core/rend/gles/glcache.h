#pragma once
#include "gles.h"

#define TEXTURE_ID_CACHE_SIZE 32

class GLCache {
public:
	GLCache() { Reset(); }

	void BindBuffer(GLenum target,  GLuint buffer) {
		switch (target) {
		case GL_ARRAY_BUFFER:
			if (_array_buffer != buffer) {
				glBindBuffer(target, buffer);
				_array_buffer = buffer;
			}
		case GL_ELEMENT_ARRAY_BUFFER:
			if (_element_array_buffer != buffer) {
				glBindBuffer(target, buffer);
				_element_array_buffer = buffer;
			}
		}
	}
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
			glClearColor(red, green, blue, alpha);
			_clear_r = red;
			_clear_g = green;
			_clear_b = blue;
			_clear_a = alpha;
		}
	}

	void CullFace(GLenum mode) {
		if (mode != _cull_face) {
			_cull_face = mode;
			glCullFace(mode);
		}
	}

	void DeleteBuffers(GLsizei n, const GLuint *buffers) {
		for (int i = 0; i < n; i++) {
			if (buffers[i] == _array_buffer)
				_array_buffer = 0;
			if (buffers[i] == _element_array_buffer)
				_element_array_buffer = 0;
		}
		glDeleteBuffers(n, buffers);
	}

	void DeleteTextures(GLsizei n, const GLuint *textures) {
		for (int i = 0; i < n; i++) {
			if (textures[i] == _texture) {
				_texture = 0;
				break;
			}
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
		switch (cap) {
		case GL_BLEND:
			if (_en_blend)
				return;
			_en_blend = true;
			break;
		case GL_CULL_FACE:
			if (_en_cull_face)
				return;
			_en_cull_face = true;
			break;
		case GL_DEPTH_TEST:
			if (_en_depth_test)
				return;
			_en_depth_test = true;
			break;
		case GL_SCISSOR_TEST:
			if (_en_scissor_test)
				return;
			_en_scissor_test = true;
			break;
		case GL_STENCIL_TEST:
			if (_en_stencil_test)
				return;
			_en_stencil_test = true;
			break;
		}
		glEnable(cap);
	}

	void Disable(GLenum cap) {
		switch (cap) {
		case GL_BLEND:
			if (!_en_blend)
				return;
			_en_blend = false;
			break;
		case GL_CULL_FACE:
			if (!_en_cull_face)
				return;
			_en_cull_face = false;
			break;
		case GL_DEPTH_TEST:
			if (!_en_depth_test)
				return;
			_en_depth_test = false;
			break;
		case GL_SCISSOR_TEST:
			if (!_en_scissor_test)
				return;
			_en_scissor_test = false;
			break;
		case GL_STENCIL_TEST:
			if (!_en_stencil_test)
				return;
			_en_stencil_test = false;
			break;
		}
		glDisable(cap);
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

	GLuint GenTexture() {
		if (_texture_cache_size == 0) {
			_texture_cache_size = TEXTURE_ID_CACHE_SIZE;
			glGenTextures(_texture_cache_size,  _texture_ids);
		}
		return _texture_ids[--_texture_cache_size];
	}

	void Reset() {
		_array_buffer = 0;
		_element_array_buffer = 0;
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
	GLuint _array_buffer;
	GLuint _element_array_buffer;
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
};

extern GLCache glcache;
