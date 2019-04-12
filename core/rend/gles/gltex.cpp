#include <algorithm>
#include "glcache.h"
#include "rend/TexCache.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/mem/_vmem.h"
#include "deps/libpng/png.h"
#include "deps/xxhash/xxhash.h"
#include "CustomTexture.h"

/*
Textures

Textures are converted to native OpenGL textures
The mapping is done with tcw:tsp -> GL texture. That includes stuff like
filtering/ texture repeat

To save space native formats are used for 1555/565/4444 (only bit shuffling is done)
YUV is converted to 8888
PALs are decoded to their unpaletted format (5551/565/4444/8888 depending on palette type)

Mipmaps
	not supported for now

Compression
	look into it, but afaik PVRC is not realtime doable
*/

#if FEAT_HAS_SOFTREND
	#include <xmmintrin.h>
#endif

extern u32 decoded_colors[3][65536];

struct PvrTexInfo
{
	const char* name;
	int bpp;        //4/8 for pal. 16 for yuv, rgb, argb
	GLuint type;
	// Conversion to 16 bpp
	TexConvFP *PL;
	TexConvFP *TW;
	TexConvFP *VQ;
	// Conversion to 32 bpp
	TexConvFP32 *PL32;
	TexConvFP32 *TW32;
	TexConvFP32 *VQ32;
};

PvrTexInfo format[8]=
{	// name     bpp GL format				   Planar		Twiddled	 VQ				Planar(32b)    Twiddled(32b)  VQ (32b)
	{"1555", 	16,	GL_UNSIGNED_SHORT_5_5_5_1, tex1555_PL,	tex1555_TW,  tex1555_VQ,	tex1555_PL32,  tex1555_TW32,  tex1555_VQ32 },	//1555
	{"565", 	16, GL_UNSIGNED_SHORT_5_6_5,   tex565_PL,	tex565_TW,   tex565_VQ, 	tex565_PL32,   tex565_TW32,   tex565_VQ32 },	//565
	{"4444", 	16, GL_UNSIGNED_SHORT_4_4_4_4, tex4444_PL,	tex4444_TW,  tex4444_VQ, 	tex4444_PL32,  tex4444_TW32,  tex4444_VQ32 },	//4444
	{"yuv", 	16, GL_UNSIGNED_BYTE,          NULL, 		NULL, 		 NULL,			texYUV422_PL,  texYUV422_TW,  texYUV422_VQ },	//yuv
	{"bumpmap", 16, GL_UNSIGNED_SHORT_4_4_4_4, texBMP_PL,	texBMP_TW,	 texBMP_VQ, 	NULL},											//bump map
	{"pal4", 	4,	0,						   0,			texPAL4_TW,  0, 			NULL, 		   texPAL4_TW32,  NULL },			//pal4
	{"pal8", 	8,	0,						   0,			texPAL8_TW,  0, 			NULL, 		   texPAL8_TW32,  NULL },			//pal8
	{"ns/1555", 0},																														// Not supported (1555)
};

const u32 MipPoint[8] =
{
	0x00006,//8
	0x00016,//16
	0x00056,//32
	0x00156,//64
	0x00556,//128
	0x01556,//256
	0x05556,//512
	0x15556//1024
};

const GLuint PAL_TYPE[4]=
{GL_UNSIGNED_SHORT_5_5_5_1,GL_UNSIGNED_SHORT_5_6_5,GL_UNSIGNED_SHORT_4_4_4_4, GL_UNSIGNED_BYTE};

CustomTexture custom_texture;

static void dumpRtTexture(u32 name, u32 w, u32 h) {
	char sname[256];
	sprintf(sname, "texdump/%x-%d.png", name, FrameCount);
	FILE *fp = fopen(sname, "wb");
    if (fp == NULL)
    	return;

	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	png_bytepp rows = (png_bytepp)malloc(h * sizeof(png_bytep));
	for (int y = 0; y < h; y++) {
		rows[y] = (png_bytep)malloc(w * 4);	// 32-bit per pixel
		glReadPixels(0, y, w, 1, GL_RGBA, GL_UNSIGNED_BYTE, rows[y]);
	}

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    png_init_io(png_ptr, fp);


    /* write header */
    png_set_IHDR(png_ptr, info_ptr, w, h,
                         8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                         PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);


            /* write bytes */
    png_write_image(png_ptr, rows);

    /* end write */
    png_write_end(png_ptr, NULL);
    fclose(fp);

	for (int y = 0; y < h; y++)
		free(rows[y]);
	free(rows);
}

//Texture Cache :)
void TextureCacheData::PrintTextureName()
{
	printf("Texture: %s ",tex?tex->name:"?format?");

	if (tcw.VQ_Comp)
		printf(" VQ");

	if (tcw.ScanOrder==0)
		printf(" TW");

	if (tcw.MipMapped)
		printf(" MM");

	if (tcw.StrideSel)
		printf(" Stride");

	printf(" %dx%d @ 0x%X",8<<tsp.TexU,8<<tsp.TexV,tcw.TexAddr<<3);
	printf(" id=%d\n", texID);
}

//Create GL texture from tsp/tcw
void TextureCacheData::Create(bool isGL)
{
	//ask GL for texture ID
	if (isGL) {
		texID = glcache.GenTexture();
	}
	else {
		texID = 0;
	}

	pData = 0;
	tex_type = 0;

	//Reset state info ..
	Lookups=0;
	Updates=0;
	dirty=FrameCount;
	lock_block=0;

	//decode info from tsp/tcw into the texture struct
	tex=&format[tcw.PixelFmt == PixelReserved ? Pixel1555 : tcw.PixelFmt];	//texture format table entry

	sa_tex = (tcw.TexAddr<<3) & VRAM_MASK;	//texture start address
	sa = sa_tex;							//data texture start address (modified for MIPs, as needed)
	w=8<<tsp.TexU;                   //tex width
	h=8<<tsp.TexV;                   //tex height

	//PAL texture
	if (tex->bpp==4)
		indirect_color_ptr=tcw.PalSelect<<4;
	else if (tex->bpp==8)
		indirect_color_ptr=(tcw.PalSelect>>4)<<8;

	//VQ table (if VQ tex)
	if (tcw.VQ_Comp)
		indirect_color_ptr=sa;

	//Convert a pvr texture into OpenGL
	switch (tcw.PixelFmt)
	{

	case Pixel1555: 	//0     1555 value: 1 bit; RGB values: 5 bits each
	case PixelReserved: //7     Reserved        Regarded as 1555
	case Pixel565: 		//1     565      R value: 5 bits; G value: 6 bits; B value: 5 bits
	case Pixel4444: 	//2     4444 value: 4 bits; RGB values: 4 bits each
	case PixelYUV:		//3     YUV422 32 bits per 2 pixels; YUYV values: 8 bits each
	case PixelBumpMap:	//4		Bump Map 	16 bits/pixel; S value: 8 bits; R value: 8 bits
	case PixelPal4:		//5     4 BPP Palette   Palette texture with 4 bits/pixel
	case PixelPal8:		//6     8 BPP Palette   Palette texture with 8 bits/pixel
		if (tcw.ScanOrder && (tex->PL || tex->PL32))
		{
			//Texture is stored 'planar' in memory, no deswizzle is needed
			//verify(tcw.VQ_Comp==0);
			if (tcw.VQ_Comp != 0)
				printf("Warning: planar texture with VQ set (invalid)\n");

			//Planar textures support stride selection, mostly used for non power of 2 textures (videos)
			int stride=w;
			if (tcw.StrideSel)
				stride=(TEXT_CONTROL&31)*32;
			//Call the format specific conversion code
			texconv = tex->PL;
			texconv32 = tex->PL32;
			//calculate the size, in bytes, for the locking
			size=stride*h*tex->bpp/8;
		}
		else
		{
			// Quake 3 Arena uses one. Not sure if valid but no need to crash
			//verify(w==h || !tcw.MipMapped); // are non square mipmaps supported ? i can't recall right now *WARN*

			if (tcw.VQ_Comp)
			{
				verify(tex->VQ != NULL || tex->VQ32 != NULL);
				indirect_color_ptr=sa;
				if (tcw.MipMapped)
					sa+=MipPoint[tsp.TexU];
				texconv = tex->VQ;
				texconv32 = tex->VQ32;
				size=w*h/8;
			}
			else
			{
				verify(tex->TW != NULL || tex->TW32 != NULL);
				if (tcw.MipMapped)
					sa+=MipPoint[tsp.TexU]*tex->bpp/2;
				texconv = tex->TW;
				texconv32 = tex->TW32;
				size=w*h*tex->bpp/8;
			}
		}
		break;
	default:
		printf("Unhandled texture %d\n",tcw.PixelFmt);
		size=w*h*2;
		texconv = NULL;
		texconv32 = NULL;
	}
}

void TextureCacheData::ComputeHash()
{
	texture_hash = XXH32(&vram[sa], size, 7);
	if (IsPaletted())
		texture_hash ^= palette_hash;
	old_texture_hash = texture_hash;
	texture_hash ^= tcw.full;
}
	
void TextureCacheData::Update()
{
	//texture state tracking stuff
	Updates++;
	dirty=0;

	GLuint textype=tex->type;

	bool has_alpha = false;
	if (IsPaletted())
	{
		textype=PAL_TYPE[PAL_RAM_CTRL&3];
		if (textype == GL_UNSIGNED_BYTE)
			has_alpha = true;

		// Get the palette hash to check for future updates
		if (tcw.PixelFmt == PixelPal4)
			palette_hash = pal_hash_16[tcw.PalSelect];
		else
			palette_hash = pal_hash_256[tcw.PalSelect >> 4];
	}

	palette_index=indirect_color_ptr; //might be used if pal. tex
	vq_codebook=(u8*)&vram[indirect_color_ptr];  //might be used if VQ tex

	//texture conversion work
	u32 stride=w;

	if (tcw.StrideSel && tcw.ScanOrder && (tex->PL || tex->PL32))
		stride=(TEXT_CONTROL&31)*32; //I think this needs +1 ?

	//PrintTextureName();
	u32 original_h = h;
	if (sa_tex > VRAM_SIZE || size == 0 || sa + size > VRAM_SIZE)
	{
		if (sa + size > VRAM_SIZE)
		{
			// Shenmue Space Harrier mini-arcade loads a texture that goes beyond the end of VRAM
			// but only uses the top portion of it
			h = (VRAM_SIZE - sa) * 8 / stride / tex->bpp;
			size = stride * h * tex->bpp/8;
		}
		else
		{
			printf("Warning: invalid texture. Address %08X %08X size %d\n", sa_tex, sa, size);
			return;
		}
	}
	if (settings.rend.CustomTextures)
		custom_texture.LoadCustomTextureAsync(this);

	void *temp_tex_buffer = NULL;
	u32 upscaled_w = w;
	u32 upscaled_h = h;

	PixelBuffer<u16> pb16;
	PixelBuffer<u32> pb32;

	// Figure out if we really need to use a 32-bit pixel buffer
	bool need_32bit_buffer = true;
	if ((settings.rend.TextureUpscale <= 1
			|| w * h > settings.rend.MaxFilteredTextureSize
				* settings.rend.MaxFilteredTextureSize		// Don't process textures that are too big
			|| tcw.PixelFmt == PixelYUV)					// Don't process YUV textures
		&& (!IsPaletted() || textype != GL_UNSIGNED_BYTE)
		&& texconv != NULL)
		need_32bit_buffer = false;
	// TODO avoid upscaling/depost. textures that change too often

	if (texconv32 != NULL && need_32bit_buffer)
	{
		// Force the texture type since that's the only 32-bit one we know
		textype = GL_UNSIGNED_BYTE;

		pb32.init(w, h);

		texconv32(&pb32, (u8*)&vram[sa], stride, h);

#ifdef DEPOSTERIZE
		{
			// Deposterization
			PixelBuffer<u32> tmp_buf;
			tmp_buf.init(w, h);

			DePosterize(pb32.data(), tmp_buf.data(), w, h);
			pb32.steal_data(tmp_buf);
		}
#endif

		// xBRZ scaling
		if (settings.rend.TextureUpscale > 1)
		{
			PixelBuffer<u32> tmp_buf;
			tmp_buf.init(w * settings.rend.TextureUpscale, h * settings.rend.TextureUpscale);

			if (tcw.PixelFmt == Pixel1555 || tcw.PixelFmt == Pixel4444)
				// Alpha channel formats. Palettes with alpha are already handled
				has_alpha = true;
			UpscalexBRZ(settings.rend.TextureUpscale, pb32.data(), tmp_buf.data(), w, h, has_alpha);
			pb32.steal_data(tmp_buf);
			upscaled_w *= settings.rend.TextureUpscale;
			upscaled_h *= settings.rend.TextureUpscale;
		}
		temp_tex_buffer = pb32.data();
	}
	else if (texconv != NULL)
	{
		pb16.init(w, h);

		texconv(&pb16,(u8*)&vram[sa],stride,h);
		temp_tex_buffer = pb16.data();
	}
	else
	{
		//fill it in with a temp color
		printf("UNHANDLED TEXTURE\n");
		pb16.init(w, h);
		memset(pb16.data(), 0x80, w * h * 2);
		temp_tex_buffer = pb16.data();
	}
	// Restore the original texture height if it was constrained to VRAM limits above
	h = original_h;

	//lock the texture to detect changes in it
	lock_block = libCore_vramlock_Lock(sa_tex,sa+size-1,this);

	if (texID) {
		//upload to OpenGL !
		UploadToGPU(textype, upscaled_w, upscaled_h, (u8*)temp_tex_buffer);
		if (settings.rend.DumpTextures)
		{
			ComputeHash();
			custom_texture.DumpTexture(texture_hash, upscaled_w, upscaled_h, textype, temp_tex_buffer);
		}
	}
	else {
		#if FEAT_HAS_SOFTREND
			if (textype == GL_UNSIGNED_SHORT_5_6_5)
				tex_type = 0;
			else if (textype == GL_UNSIGNED_SHORT_5_5_5_1)
				tex_type = 1;
			else if (textype == GL_UNSIGNED_SHORT_4_4_4_4)
				tex_type = 2;

			u16 *tex_data = (u16 *)temp_tex_buffer;
			if (pData) {
				_mm_free(pData);
			}

			pData = (u16*)_mm_malloc(w * h * 16, 16);
			for (int y = 0; y < h; y++) {
				for (int x = 0; x < w; x++) {
					u32* data = (u32*)&pData[(x + y*w) * 8];

					data[0] = decoded_colors[tex_type][tex_data[(x + 1) % w + (y + 1) % h * w]];
					data[1] = decoded_colors[tex_type][tex_data[(x + 0) % w + (y + 1) % h * w]];
					data[2] = decoded_colors[tex_type][tex_data[(x + 1) % w + (y + 0) % h * w]];
					data[3] = decoded_colors[tex_type][tex_data[(x + 0) % w + (y + 0) % h * w]];
				}
			}
		#else
			die("Soft rend disabled, invalid code path");
		#endif
	}
}

void TextureCacheData::UploadToGPU(GLuint textype, int width, int height, u8 *temp_tex_buffer)
{
	//upload to OpenGL !
	glcache.BindTexture(GL_TEXTURE_2D, texID);
	GLuint comps=textype == GL_UNSIGNED_SHORT_5_6_5 ? GL_RGB : GL_RGBA;
	glTexImage2D(GL_TEXTURE_2D, 0,comps, width, height, 0, comps, textype, temp_tex_buffer);
	if (tcw.MipMapped && settings.rend.UseMipmaps)
		glGenerateMipmap(GL_TEXTURE_2D);
}

void TextureCacheData::CheckCustomTexture()
{
	if (custom_load_in_progress == 0 && custom_image_data != NULL)
	{
		UploadToGPU(GL_UNSIGNED_BYTE, custom_width, custom_height, custom_image_data);
		delete [] custom_image_data;
		custom_image_data = NULL;
	}
}

//true if : dirty or paletted texture and hashes don't match
bool TextureCacheData::NeedsUpdate() {
	bool rc = dirty
			|| (tcw.PixelFmt == PixelPal4 && palette_hash != pal_hash_16[tcw.PalSelect])
			|| (tcw.PixelFmt == PixelPal8 && palette_hash != pal_hash_256[tcw.PalSelect >> 4]);
	return rc;
}
	
bool TextureCacheData::Delete()
{
	if (custom_load_in_progress > 0)
		return false;
	
	if (pData) {
		#if FEAT_HAS_SOFTREND
			_mm_free(pData);
			pData = 0;
		#else
			die("softrend disabled, invalid codepath");
		#endif
	}

	if (texID) {
		glcache.DeleteTextures(1, &texID);
	}
	if (lock_block)
		libCore_vramlock_Unlock_block(lock_block);
	lock_block=0;
	if (custom_image_data != NULL)
		delete [] custom_image_data;
	
	return true;
}


#include <map>
map<u64,TextureCacheData> TexCache;
typedef map<u64,TextureCacheData>::iterator TexCacheIter;

TextureCacheData *getTextureCacheData(TSP tsp, TCW tcw);

void BindRTT(u32 addy, u32 fbw, u32 fbh, u32 channels, u32 fmt)
{
	if (gl.rtt.fbo) glDeleteFramebuffers(1,&gl.rtt.fbo);
	if (gl.rtt.tex) glcache.DeleteTextures(1,&gl.rtt.tex);
	if (gl.rtt.depthb) glDeleteRenderbuffers(1,&gl.rtt.depthb);

	gl.rtt.TexAddr=addy>>3;

	// Find the smallest power of two texture that fits into the viewport
	int fbh2 = 2;
	while (fbh2 < fbh)
		fbh2 *= 2;
	int fbw2 = 2;
	while (fbw2 < fbw)
		fbw2 *= 2;

	if (settings.rend.RenderToTextureUpscale > 1 && !settings.rend.RenderToTextureBuffer)
	{
		fbw *= settings.rend.RenderToTextureUpscale;
		fbh *= settings.rend.RenderToTextureUpscale;
		fbw2 *= settings.rend.RenderToTextureUpscale;
		fbh2 *= settings.rend.RenderToTextureUpscale;
	}
	// Get the currently bound frame buffer object. On most platforms this just gives 0.
	//glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_i32OriginalFbo);

	// Generate and bind a render buffer which will become a depth buffer shared between our two FBOs
	glGenRenderbuffers(1, &gl.rtt.depthb);
	glBindRenderbuffer(GL_RENDERBUFFER, gl.rtt.depthb);

	/*
		Currently it is unknown to GL that we want our new render buffer to be a depth buffer.
		glRenderbufferStorage will fix this and in this case will allocate a depth buffer
		m_i32TexSize by m_i32TexSize.
	*/

	if (gl.is_gles)
	{
#if defined(GL_DEPTH24_STENCIL8_OES) && defined(GL_DEPTH_COMPONENT24_OES)
		if (gl.GL_OES_packed_depth_stencil_supported)
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, fbw2, fbh2);
		else if (gl.GL_OES_depth24_supported)
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24_OES, fbw2, fbh2);
		else
#endif
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, fbw2, fbh2);
	}
	else
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, fbw2, fbh2);

	// Create a texture for rendering to
	gl.rtt.tex = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, gl.rtt.tex);

	glTexImage2D(GL_TEXTURE_2D, 0, channels, fbw2, fbh2, 0, channels, fmt, 0);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	// Create the object that will allow us to render to the aforementioned texture
	glGenFramebuffers(1, &gl.rtt.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, gl.rtt.fbo);

	// Attach the texture to the FBO
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl.rtt.tex, 0);

	// Attach the depth buffer we created earlier to our FBO.
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gl.rtt.depthb);

	if (!gl.is_gles || gl.GL_OES_packed_depth_stencil_supported)
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gl.rtt.depthb);

	// Check that our FBO creation was successful
	GLuint uStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	verify(uStatus == GL_FRAMEBUFFER_COMPLETE);

	glViewport(0, 0, fbw, fbh);		// TODO CLIP_X/Y min?
}

void ReadRTTBuffer() {
	u32 w = pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1;
	u32 h = pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1;

	u32 stride = FB_W_LINESTRIDE.stride * 8;
	if (stride == 0)
		stride = w * 2;
	else if (w * 2 > stride) {
    	// Happens for Virtua Tennis
    	w = stride / 2;
    }
	u32 size = w * h * 2;

	const u8 fb_packmode = FB_W_CTRL.fb_packmode;

	if (settings.rend.RenderToTextureBuffer)
	{
		u32 tex_addr = gl.rtt.TexAddr << 3;

		// Manually mark textures as dirty and remove all vram locks before calling glReadPixels
		// (deadlock on rpi)
		for (TexCacheIter i = TexCache.begin(); i != TexCache.end(); i++)
		{
			if (i->second.sa_tex <= tex_addr + size - 1 && i->second.sa + i->second.size - 1 >= tex_addr) {
				i->second.dirty = FrameCount;
				if (i->second.lock_block != NULL) {
					libCore_vramlock_Unlock_block(i->second.lock_block);
					i->second.lock_block = NULL;
				}
			}
		}
		vram.UnLockRegion(0, 2 * vram.size);

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		u16 *dst = (u16 *)&vram[tex_addr];

		GLint color_fmt, color_type;
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &color_fmt);
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &color_type);

		if (fb_packmode == 1 && stride == w * 2 && color_fmt == GL_RGB && color_type == GL_UNSIGNED_SHORT_5_6_5) {
			// Can be read directly into vram
			glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, dst);
		}
		else
		{
			PixelBuffer<u32> tmp_buf;
			tmp_buf.init(w, h);

			const u16 kval_bit = (FB_W_CTRL.fb_kval & 0x80) << 8;
			const u8 fb_alpha_threshold = FB_W_CTRL.fb_alpha_threshold;

			u8 *p = (u8 *)tmp_buf.data();
			glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, p);

			for (u32 l = 0; l < h; l++) {
				switch(fb_packmode)
				{
				case 0: //0x0   0555 KRGB 16 bit  (default)	Bit 15 is the value of fb_kval[7].
					for (u32 c = 0; c < w; c++) {
						*dst++ = (((p[0] >> 3) & 0x1F) << 10) | (((p[1] >> 3) & 0x1F) << 5) | ((p[2] >> 3) & 0x1F) | kval_bit;
						p += 4;
					}
					break;
				case 1: //0x1   565 RGB 16 bit
					for (u32 c = 0; c < w; c++) {
						*dst++ = (((p[0] >> 3) & 0x1F) << 11) | (((p[1] >> 2) & 0x3F) << 5) | ((p[2] >> 3) & 0x1F);
						p += 4;
					}
					break;
				case 2: //0x2   4444 ARGB 16 bit
					for (u32 c = 0; c < w; c++) {
						*dst++ = (((p[0] >> 4) & 0xF) << 8) | (((p[1] >> 4) & 0xF) << 4) | ((p[2] >> 4) & 0xF) | (((p[3] >> 4) & 0xF) << 12);
						p += 4;
					}
					break;
				case 3://0x3    1555 ARGB 16 bit    The alpha value is determined by comparison with the value of fb_alpha_threshold.
					for (u32 c = 0; c < w; c++) {
						*dst++ = (((p[0] >> 3) & 0x1F) << 10) | (((p[1] >> 3) & 0x1F) << 5) | ((p[2] >> 3) & 0x1F) | (p[3] > fb_alpha_threshold ? 0x8000 : 0);
						p += 4;
					}
					break;
				}
				dst += (stride - w * 2) / 2;
			}
		}

		// Restore VRAM locks
		for (TexCacheIter i = TexCache.begin(); i != TexCache.end(); i++)
		{
				if (i->second.lock_block != NULL) {
						vram.LockRegion(i->second.sa_tex, i->second.sa + i->second.size - i->second.sa_tex);

						//TODO: Fix this for 32M wrap as well
						if (_nvmem_enabled() && VRAM_SIZE == 0x800000) {
								vram.LockRegion(i->second.sa_tex + VRAM_SIZE, i->second.sa + i->second.size - i->second.sa_tex);
						}
				}
		}
	}
	else
	{
		//memset(&vram[fb_rtt.TexAddr << 3], '\0', size);
	}

    //dumpRtTexture(fb_rtt.TexAddr, w, h);

    if (w > 1024 || h > 1024 || settings.rend.RenderToTextureBuffer) {
    	glcache.DeleteTextures(1, &gl.rtt.tex);
    }
    else
    {
    	// TexAddr : fb_rtt.TexAddr, Reserved : 0, StrideSel : 0, ScanOrder : 1
    	TCW tcw = { { gl.rtt.TexAddr, 0, 0, 1 } };
    	switch (fb_packmode) {
    	case 0:
    	case 3:
    		tcw.PixelFmt = Pixel1555;
    		break;
    	case 1:
    		tcw.PixelFmt = Pixel565;
    		break;
    	case 2:
    		tcw.PixelFmt = Pixel4444;
    		break;
    	}
    	TSP tsp = { 0 };
    	for (tsp.TexU = 0; tsp.TexU <= 7 && (8 << tsp.TexU) < w; tsp.TexU++);
    	for (tsp.TexV = 0; tsp.TexV <= 7 && (8 << tsp.TexV) < h; tsp.TexV++);

    	TextureCacheData *texture_data = getTextureCacheData(tsp, tcw);
    	if (texture_data->texID != 0)
    		glcache.DeleteTextures(1, &texture_data->texID);
    	else
    		texture_data->Create(false);
    	texture_data->texID = gl.rtt.tex;
    	texture_data->dirty = 0;
    	if (texture_data->lock_block == NULL)
    		texture_data->lock_block = libCore_vramlock_Lock(texture_data->sa_tex, texture_data->sa + texture_data->size - 1, texture_data);
    }
    gl.rtt.tex = 0;

	if (gl.rtt.fbo) { glDeleteFramebuffers(1,&gl.rtt.fbo); gl.rtt.fbo = 0; }
	if (gl.rtt.depthb) { glDeleteRenderbuffers(1,&gl.rtt.depthb); gl.rtt.depthb = 0; }

}

static int TexCacheLookups;
static int TexCacheHits;
static float LastTexCacheStats;

// Only use TexU and TexV from TSP in the cache key
//     TexV : 7, TexU : 7
const TSP TSPTextureCacheMask = { { 7, 7 } };
//     TexAddr : 0x1FFFFF, Reserved : 0, StrideSel : 0, ScanOrder : 1, PixelFmt : 7, VQ_Comp : 1, MipMapped : 1
const TCW TCWTextureCacheMask = { { 0x1FFFFF, 0, 0, 1, 7, 1, 1 } };

TextureCacheData *getTextureCacheData(TSP tsp, TCW tcw) {
	u64 key = tsp.full & TSPTextureCacheMask.full;
	if (tcw.PixelFmt == PixelPal4 || tcw.PixelFmt == PixelPal8)
		// Paletted textures have a palette selection that must be part of the key
		// We also add the palette type to the key to avoid thrashing the cache
		// when the palette type is changed. If the palette type is changed back in the future,
		// this texture will stil be available.
		key |= ((u64)tcw.full << 32) | ((PAL_RAM_CTRL & 3) << 6);
	else
		key |= (u64)(tcw.full & TCWTextureCacheMask.full) << 32;

	TexCacheIter tx = TexCache.find(key);

	TextureCacheData* tf;
	if (tx != TexCache.end())
	{
		tf = &tx->second;
		// Needed if the texture is updated
		tf->tcw.StrideSel = tcw.StrideSel;
	}
	else //create if not existing
	{
		tf=&TexCache[key];

		tf->tsp = tsp;
		tf->tcw = tcw;
	}

	return tf;
}

GLuint gl_GetTexture(TSP tsp, TCW tcw)
{
	TexCacheLookups++;

	//lookup texture
	TextureCacheData* tf = getTextureCacheData(tsp, tcw);

	if (tf->texID == 0)
		tf->Create(true);

	//update if needed
	if (tf->NeedsUpdate())
		tf->Update();
	else
	{
		tf->CheckCustomTexture();
		TexCacheHits++;
	}

//	if (os_GetSeconds() - LastTexCacheStats >= 2.0)
//	{
//		LastTexCacheStats = os_GetSeconds();
//		printf("Texture cache efficiency: %.2f%% cache size %ld\n", (float)TexCacheHits / TexCacheLookups * 100, TexCache.size());
//		TexCacheLookups = 0;
//		TexCacheHits = 0;
//	}

	//update state for opts/stuff
	tf->Lookups++;

	//return gl texture
	return tf->texID;
}


text_info raw_GetTexture(TSP tsp, TCW tcw)
{
	text_info rv = { 0 };

	//lookup texture
	TextureCacheData* tf;
	u64 key = ((u64)(tcw.full & TCWTextureCacheMask.full) << 32) | (tsp.full & TSPTextureCacheMask.full);

	TexCacheIter tx = TexCache.find(key);

	if (tx != TexCache.end())
	{
		tf = &tx->second;
	}
	else //create if not existing
	{
		tf = &TexCache[key];

		tf->tsp = tsp;
		tf->tcw = tcw;
		tf->Create(false);
	}

	//update if needed
	if (tf->NeedsUpdate())
		tf->Update();

	//update state for opts/stuff
	tf->Lookups++;

	//return gl texture
	rv.height = tf->h;
	rv.width = tf->w;
	rv.pdata = tf->pData;
	rv.textype = tf->tex_type;
	
	
	return rv;
}

void CollectCleanup() {
	vector<u64> list;

	u32 TargetFrame = max((u32)120,FrameCount) - 120;

	for (TexCacheIter i=TexCache.begin();i!=TexCache.end();i++)
	{
		if ( i->second.dirty &&  i->second.dirty < TargetFrame) {
			list.push_back(i->first);
		}

		if (list.size() > 5)
			break;
	}

	for (size_t i=0; i<list.size(); i++) {
		if (TexCache[list[i]].Delete())
		{
			//printf("Deleting %d\n", TexCache[list[i]].texID);
			TexCache.erase(list[i]);
		}
	}
}

void DoCleanup() {

}
void killtex()
{
	for (TexCacheIter i=TexCache.begin();i!=TexCache.end();i++)
	{
		i->second.Delete();
	}

	TexCache.clear();
	printf("Texture cache cleared\n");
}

void rend_text_invl(vram_block* bl)
{
	TextureCacheData* tcd = (TextureCacheData*)bl->userdata;
	tcd->dirty=FrameCount;
	tcd->lock_block=0;

	libCore_vramlock_Unlock_block_wb(bl);
}

GLuint fbTextureId;

void RenderFramebuffer()
{
	if (FB_R_SIZE.fb_x_size == 0 || FB_R_SIZE.fb_y_size == 0)
		return;

	int width = (FB_R_SIZE.fb_x_size + 1) << 1;     // in 16-bit words
	int height = FB_R_SIZE.fb_y_size + 1;
	int modulus = (FB_R_SIZE.fb_modulus - 1) << 1;
	
	int bpp;
	switch (FB_R_CTRL.fb_depth)
	{
		case fbde_0555:
		case fbde_565:
			bpp = 2;
			break;
		case fbde_888:
			bpp = 3;
			width = (width * 2) / 3;		// in pixels
			modulus = (modulus * 2) / 3;	// in pixels
			break;
		case fbde_C888:
			bpp = 4;
			width /= 2;             // in pixels
			modulus /= 2;           // in pixels
			break;
		default:
			die("Invalid framebuffer format\n");
			bpp = 4;
			break;
	}
	
	if (fbTextureId == 0)
		fbTextureId = glcache.GenTexture();
	
	glcache.BindTexture(GL_TEXTURE_2D, fbTextureId);
	
	//set texture repeat mode
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	u32 addr = SPG_CONTROL.interlace && !SPG_STATUS.fieldnum ? FB_R_SOF2 : FB_R_SOF1;
	
	PixelBuffer<u32> pb;
	pb.init(width, height);
	u8 *dst = (u8*)pb.data();
	
	switch (FB_R_CTRL.fb_depth)
	{
		case fbde_0555:    // 555 RGB
			for (int y = 0; y < height; y++)
			{
				for (int i = 0; i < width; i++)
				{
					u16 src = pvr_read_area1_16(addr);
					*dst++ = (((src >> 10) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
					*dst++ = (((src >> 5) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
					*dst++ = (((src >> 0) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
					*dst++ = 0xFF;
					addr += bpp;
				}
				addr += modulus * bpp;
			}
			break;
			
		case fbde_565:    // 565 RGB
			for (int y = 0; y < height; y++)
			{
				for (int i = 0; i < width; i++)
				{
					u16 src = pvr_read_area1_16(addr);
					*dst++ = (((src >> 11) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
					*dst++ = (((src >> 5) & 0x3F) << 2) + (FB_R_CTRL.fb_concat >> 1);
					*dst++ = (((src >> 0) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
					*dst++ = 0xFF;
					addr += bpp;
				}
				addr += modulus * bpp;
			}
			break;
		case fbde_888:		// 888 RGB
			for (int y = 0; y < height; y++)
			{
				for (int i = 0; i < width; i++)
				{
					if (addr & 1)
					{
						u32 src = pvr_read_area1_32(addr - 1);
						*dst++ = src >> 16;
						*dst++ = src >> 8;
						*dst++ = src;
					}
					else
					{
						u32 src = pvr_read_area1_32(addr);
						*dst++ = src >> 24;
						*dst++ = src >> 16;
						*dst++ = src >> 8;
					}
					*dst++ = 0xFF;
					addr += bpp;
				}
				addr += modulus * bpp;
			}
			break;
		case fbde_C888:     // 0888 RGB
			for (int y = 0; y < height; y++)
			{
				for (int i = 0; i < width; i++)
				{
					u32 src = pvr_read_area1_32(addr);
					*dst++ = src >> 16;
					*dst++ = src >> 8;
					*dst++ = src;
					*dst++ = 0xFF;
					addr += bpp;
				}
				addr += modulus * bpp;
			}
			break;
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pb.data());
}

GLuint init_output_framebuffer(int width, int height)
{
	if (width != gl.ofbo.width || height != gl.ofbo.height)
	{
		free_output_framebuffer();
		gl.ofbo.width = width;
		gl.ofbo.height = height;
	}
	if (gl.ofbo.fbo == 0)
	{
		// Create the depth+stencil renderbuffer
		glGenRenderbuffers(1, &gl.ofbo.depthb);
		glBindRenderbuffer(GL_RENDERBUFFER, gl.ofbo.depthb);

		if (gl.is_gles)
		{
#if defined(GL_DEPTH24_STENCIL8_OES) && defined(GL_DEPTH_COMPONENT24_OES)
			if (gl.GL_OES_packed_depth_stencil_supported)
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, width, height);
			else if (gl.GL_OES_depth24_supported)
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24_OES, width, height);
			else
#endif
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
		}
		else
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

		if (gl.gl_major < 3)
		{
			// Create a texture for rendering to
			gl.ofbo.tex = glcache.GenTexture();
			glcache.BindTexture(GL_TEXTURE_2D, gl.ofbo.tex);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		else
		{
			// Use a renderbuffer and glBlitFramebuffer
			glGenRenderbuffers(1, &gl.ofbo.colorb);
			glBindRenderbuffer(GL_RENDERBUFFER, gl.ofbo.colorb);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
		}

		// Create the framebuffer
		glGenFramebuffers(1, &gl.ofbo.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, gl.ofbo.fbo);

		// Attach the depth buffer to our FBO.
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gl.ofbo.depthb);

		if (!gl.is_gles || gl.GL_OES_packed_depth_stencil_supported)
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gl.ofbo.depthb);

		// Attach the texture/renderbuffer to the FBO
		if (gl.gl_major < 3)
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl.ofbo.tex, 0);
		else
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, gl.ofbo.colorb);

		// Check that our FBO creation was successful
		GLuint uStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		verify(uStatus == GL_FRAMEBUFFER_COMPLETE);

		glcache.Disable(GL_SCISSOR_TEST);
		glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	else
		glBindFramebuffer(GL_FRAMEBUFFER, gl.ofbo.fbo);

	glViewport(0, 0, width, height);
	glCheck();

	return gl.ofbo.fbo;
}

void free_output_framebuffer()
{
	if (gl.ofbo.fbo != 0)
	{
		glDeleteFramebuffers(1, &gl.ofbo.fbo);
		gl.ofbo.fbo = 0;
		glDeleteRenderbuffers(1, &gl.ofbo.depthb);
		gl.ofbo.depthb = 0;
		if (gl.ofbo.tex != 0)
		{
			glcache.DeleteTextures(1, &gl.ofbo.tex);
			gl.ofbo.tex = 0;
		}
		if (gl.ofbo.colorb != 0)
		{
			glDeleteRenderbuffers(1, &gl.ofbo.colorb);
			gl.ofbo.colorb = 0;
		}
	}
}
