/*
	Copyright 2018 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "gl4.h"
#include "rend/gles/glcache.h"

GLuint gl4BindRTT(u32 addy, u32 fbw, u32 fbh, u32 channels, u32 fmt)
{
	if (gl.rtt.fbo) glDeleteFramebuffers(1,&gl.rtt.fbo);
	if (gl.rtt.tex) glcache.DeleteTextures(1,&gl.rtt.tex);

	gl.rtt.TexAddr=addy>>3;

	// Find the smallest power of two texture that fits the viewport
	u32 fbh2 = 8;
	while (fbh2 < fbh)
		fbh2 *= 2;
	u32 fbw2 = 8;
	while (fbw2 < fbw)
		fbw2 *= 2;

	if (config::RenderToTextureUpscale > 1 && !config::RenderToTextureBuffer)
	{
		fbw *= config::RenderToTextureUpscale;
		fbh *= config::RenderToTextureUpscale;
		fbw2 *= config::RenderToTextureUpscale;
		fbh2 *= config::RenderToTextureUpscale;
	}

	// Create a texture for rendering to
	gl.rtt.tex = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, gl.rtt.tex);

	glTexImage2D(GL_TEXTURE_2D, 0, channels, fbw2, fbh2, 0, channels, fmt, 0);

	// Create the object that will allow us to render to the aforementioned texture
	glGenFramebuffers(1, &gl.rtt.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, gl.rtt.fbo);

	// Attach the texture to the FBO
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl.rtt.tex, 0);

	// Check that our FBO creation was successful
	GLuint uStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	verify(uStatus == GL_FRAMEBUFFER_COMPLETE);

	glViewport(0, 0, fbw, fbh);		// TODO CLIP_X/Y min?

	return gl.rtt.fbo;
}
