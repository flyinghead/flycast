#include "hw\pvr\Renderer_if.h"
#include "hw\pvr\pvr_mem.h"
#include "oslib\oslib.h"

/*
	SSE/MMX based softrend

	Initial code by skmp and gigaherz

	This is a rather weird very basic pvr softrend. 
	Renders	in some kind of tile format (that I forget now),
	and does depth and color, but no alpha, texture, or pixel
	processing. All of the pipeline is based on quads.
*/

#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>

BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), 0, 0, 1, 32, BI_RGB };

struct softrend : Renderer
{
	virtual bool Process(TA_context* ctx) {
		//disable RTTs for now ..
		if (ctx->rend.isRTT)
			return false;

		ctx->rend_inuse.Lock();
		ctx->MarkRend();

		if (!ta_parse_vdrc(ctx))
			return false;

		return true;
	}

	DECL_ALIGN(32) u32 render_buffer[640 * 480 * 2 * 4]; //Color + depth
	DECL_ALIGN(32) u32 pixels[640 * 480 * 4];

	static __m128i _mm_load_scaled(int v, int s)
	{
		return _mm_setr_epi32(v, v + s, v + s + s, v + s + s + s);
	}
	static __m128i _mm_broadcast(int v)
	{
		__m128i rv = _mm_cvtsi32_si128(v);
		return _mm_shuffle_epi32(rv, 0);
	}
	static __m128 _mm_load_ps_r(float a, float b, float c, float d)
	{
		static __declspec(align(128)) float v[4];
		v[0] = a;
		v[1] = b;
		v[2] = c;
		v[3] = d;

		return _mm_load_ps(v);
	}

	__forceinline int iround(float x)
	{
		return _mm_cvtt_ss2si(_mm_load_ss(&x));
	}

	int mmin(int a, int b, int c, int d)
	{
		int rv = min(a, b);
		rv = min(c, rv);
		return max(d, rv);
	}

	int mmax(int a, int b, int c, int d)
	{
		int rv = max(a, b);
		rv = max(c, rv);
		return min(d, rv);
	}

	//i think this gives false positives ...
	//yup, if ANY of the 3 tests fail the ANY tests fails.
	__forceinline void EvalHalfSpace(bool& all, bool& any, int cp, int sv, int lv)
	{
		//bool a00 = C1 + DX12 * y0 - DY12 * x0 > 0;
		//bool a10 = C1 + DX12 * y0 - DY12 * x0 > qDY12;
		//bool a01 = C1 + DX12 * y0 - DY12 * x0 > -qDX12;
		//bool a11 = C1 + DX12 * y0 - DY12 * x0 > (qDY12-qDX12);

		//C1 + DX12 * y0 - DY12 * x0 > 0
		// + DX12 * y0 - DY12 * x0 > 0 - C1
		//int pd=DX * y0 - DY * x0;

		bool a = cp > sv;	//needed for ANY
		bool b = cp > lv;	//needed for ALL

		any &= a;
		all &= b;
	}

	//return true if any is positive
	__forceinline bool EvalHalfSpaceFAny(int cp12, int cp23, int cp31)
	{
		int svt = cp12; //needed for ANY
		svt |= cp23;
		svt |= cp31;

		return svt>0;
	}

	__forceinline bool EvalHalfSpaceFAll(int cp12, int cp23, int cp31, int lv12, int lv23, int lv31)
	{
		int lvt = cp12 - lv12;
		lvt |= cp23 - lv23;
		lvt |= cp31 - lv31;	//needed for all

		return lvt>0;
	}

	__forceinline void PlaneMinMax(int& MIN, int& MAX, int DX, int DY, int q)
	{
		int q_fp = (q - 1) << 4;
		int v1 = 0;
		int v2 = q_fp*DY;
		int v3 = -q_fp*DX;
		int v4 = q_fp*(DY - DX);

		MIN = min(v1, min(v2, min(v3, v4)));
		MAX = max(v1, max(v2, max(v3, v4)));
	}

	struct PlaneStepper
	{
		__m128 ddx, ddy;
		__m128 c;

		void Setup(const Vertex &v1, const Vertex &v2, const Vertex &v3, int minx, int miny, int q
			, float v1_a, float v2_a, float v3_a
			, float v1_b, float v2_b, float v3_b
			, float v1_c, float v2_c, float v3_c
			, float v1_d, float v2_d, float v3_d)
		{
			//			float v1_z=v1.z,v2_z=v2.z,v3_z=v3.z;
			float Aa = ((v3_a - v1_a) * (v2.y - v1.y) - (v2_a - v1_a) * (v3.y - v1.y));
			float Ba = ((v3.x - v1.x) * (v2_a - v1_a) - (v2.x - v1.x) * (v3_a - v1_a));

			float Ab = ((v3_b - v1_b) * (v2.y - v1.y) - (v2_b - v1_b) * (v3.y - v1.y));
			float Bb = ((v3.x - v1.x) * (v2_b - v1_b) - (v2.x - v1.x) * (v3_b - v1_b));

			float Ac = ((v3_c - v1_c) * (v2.y - v1.y) - (v2_c - v1_c) * (v3.y - v1.y));
			float Bc = ((v3.x - v1.x) * (v2_c - v1_c) - (v2.x - v1.x) * (v3_c - v1_c));

			float Ad = ((v3_d - v1_d) * (v2.y - v1.y) - (v2_d - v1_d) * (v3.y - v1.y));
			float Bd = ((v3.x - v1.x) * (v2_d - v1_d) - (v2.x - v1.x) * (v3_d - v1_d));

			float C = ((v2.x - v1.x) * (v3.y - v1.y) - (v3.x - v1.x) * (v2.y - v1.y));
			float ddx_s_a = -Aa / C;
			float ddy_s_a = -Ba / C;

			float ddx_s_b = -Ab / C;
			float ddy_s_b = -Bb / C;

			float ddx_s_c = -Ac / C;
			float ddy_s_c = -Bc / C;

			float ddx_s_d = -Ad / C;
			float ddy_s_d = -Bd / C;

			ddx = _mm_load_ps_r(ddx_s_a, ddx_s_b, ddx_s_c, ddx_s_d);
			ddy = _mm_load_ps_r(ddy_s_a, ddy_s_b, ddy_s_c, ddy_s_d);

			float c_s_a = (v1_a - ddx_s_a *v1.x - ddy_s_a*v1.y);
			float c_s_b = (v1_b - ddx_s_b *v1.x - ddy_s_b*v1.y);
			float c_s_c = (v1_c - ddx_s_c *v1.x - ddy_s_c*v1.y);
			float c_s_d = (v1_d - ddx_s_d *v1.x - ddy_s_d*v1.y);

			c = _mm_load_ps_r(c_s_a, c_s_b, c_s_c, c_s_d);

			//z = z1 + dzdx * (minx - v1.x) + dzdy * (minx - v1.y);
			//z = (z1 - dzdx * v1.x - v1.y*dzdy) +  dzdx*inx + dzdy *iny;	
		}

		__forceinline __m128 Ip(__m128 x, __m128 y) const
		{
			__m128 p1 = _mm_mul_ps(x, ddx);
			__m128 p2 = _mm_mul_ps(y, ddy);

			__m128 s1 = _mm_add_ps(p1, p2);
			return _mm_add_ps(s1, c);
		}

		__forceinline __m128 InStep(__m128 bas) const
		{
			return _mm_add_ps(bas, ddx);
		}
	};

	struct IPs
	{
		PlaneStepper ZUV;
		PlaneStepper Col;

		void Setup(const Vertex &v1, const Vertex &v2, const Vertex &v3, int minx, int miny, int q)
		{
			ZUV.Setup(v1, v2, v3, minx, miny, q,
				v1.z, v2.z, v3.z,
				v1.u, v2.u, v3.u,
				v1.v, v2.v, v3.v,
				0, -1, 1);

			Col.Setup(v1, v2, v3, minx, miny, q,
				v1.col[2], v2.col[2], v3.col[2],
				v1.col[1], v2.col[1], v3.col[1],
				v1.col[0], v2.col[0], v3.col[0],
				v1.col[3], v2.col[3], v3.col[3]
				);
		}
	};


	IPs __declspec(align(64)) ip;



	template<bool useoldmsk, bool alpha_blend>
	__forceinline void PixelFlush(__m128 x, __m128 y, u8* cb, __m128 oldmask)
	{
		x = _mm_shuffle_ps(x, x, 0);
		__m128 invW = ip.ZUV.Ip(x, y);
		__m128 u = ip.ZUV.InStep(invW);
		__m128 v = ip.ZUV.InStep(u);
		__m128 ws = ip.ZUV.InStep(v);

		_MM_TRANSPOSE4_PS(invW, u, v, ws);

		//invW : {z1,z2,z3,z4}
		//u    : {u1,u2,u3,u4}
		//v    : {v1,v2,v3,v4}
		//wx   : {?,?,?,?}

		__m128* zb = (__m128*)&cb[640 * 480 * 4];

		__m128 ZMask = _mm_cmpge_ps(invW, *zb);
		if (useoldmsk)
			ZMask = _mm_and_ps(oldmask, ZMask);
		u32 msk = _mm_movemask_ps(ZMask);//0xF

		if (msk == 0)
			return;

		__m128i rv;

		{
			__m128 a = ip.Col.Ip(x, y);
			__m128 b = ip.Col.InStep(a);
			__m128 c = ip.Col.InStep(b);
			__m128 d = ip.Col.InStep(c);

			__m128i ui = _mm_cvttps_epi32(u);
			__m128i vi = _mm_cvttps_epi32(v);

			//(int)v<<x+(int)u
			__m128i textadr = _mm_add_epi32(_mm_slli_epi32(vi, 8), ui);//texture addresses ! 4x of em !

			//we need : 

			__m128i ab = _mm_packs_epi32(_mm_cvttps_epi32(a), _mm_cvttps_epi32(b));
			__m128i cd = _mm_packs_epi32(_mm_cvttps_epi32(c), _mm_cvttps_epi32(d));

			rv = _mm_packus_epi16(ab, cd);
			//rv = _mm_xor_si128(rv,textadr);
		}

		//__m128i rv=ip.col;//_mm_xor_si128(_mm_cvtps_epi32(_mm_mul_ps(x,Z.c)),_mm_cvtps_epi32(y));

		if (alpha_blend) {
			__m128i fb = *(__m128i*)cb;
#if 0
			for (int i = 0; i < 16; i+=4) {
				u8 src_blend[4] = { rv.m128i_u8[i + 3], rv.m128i_u8[i + 3], rv.m128i_u8[i + 3], rv.m128i_u8[i + 3] };
				u8 dst_blend[4] = { 255 - rv.m128i_u8[i + 3], 255 - rv.m128i_u8[i + 3], 255 - rv.m128i_u8[i + 3], 255 - rv.m128i_u8[i + 3] };
				for (int j = 0; j < 4; j++) {
					rv.m128i_u8[i + j] = (rv.m128i_u8[i + j] * src_blend[j])/256 + (fb.m128i_u8[i + j] * dst_blend[j])/256;
				}
			}
#else
			static __m128i shuffle_alpha = {
				0x0E, 0x80, 0x0E, 0x80, 0x0E, 0x80, 0x0E, 0x80,
				0x06, 0x80, 0x06, 0x80, 0x06, 0x80, 0x06, 0x80
			};
			
			
			__m128i lo_rv = _mm_cvtepu8_epi16(rv);
			__m128i hi_rv = _mm_cvtepu8_epi16(_mm_shuffle_epi32(rv, _MM_SHUFFLE(1, 0, 3, 2)));


			__m128i lo_fb = _mm_cvtepu8_epi16(fb);
			__m128i hi_fb = _mm_cvtepu8_epi16(_mm_shuffle_epi32(fb, _MM_SHUFFLE(1, 0, 3, 2)));

			__m128i lo_rv_alpha = _mm_shuffle_epi8(lo_rv, shuffle_alpha);
			__m128i hi_rv_alpha = _mm_shuffle_epi8(hi_rv, shuffle_alpha);

			__m128i lo_fb_alpha = _mm_sub_epi16(_mm_set1_epi16(255), lo_rv_alpha);
			__m128i hi_fb_alpha = _mm_sub_epi16(_mm_set1_epi16(255), hi_rv_alpha);

			
			lo_rv = _mm_mullo_epi16(lo_rv, lo_rv_alpha);
			hi_rv = _mm_mullo_epi16(hi_rv, hi_rv_alpha);

			lo_fb = _mm_mullo_epi16(lo_fb, lo_fb_alpha);
			hi_fb = _mm_mullo_epi16(hi_fb, hi_fb_alpha);

			rv = _mm_packus_epi16(_mm_srli_epi16(_mm_adds_epu16(lo_rv, lo_fb), 8), _mm_srli_epi16(_mm_adds_epu16(hi_rv, hi_fb), 8));
#endif
		}
		
		if (msk != 0xF)
		{
			rv = _mm_and_si128(rv, *(__m128i*)&ZMask);
			rv = _mm_or_si128(_mm_andnot_si128(*(__m128i*)&ZMask, *(__m128i*)cb), rv);

			invW = _mm_and_ps(invW, ZMask);
			invW = _mm_or_ps(_mm_andnot_ps(ZMask, *zb), invW);

		}
		*zb = invW;
		*(__m128i*)cb = rv;
	}
	//u32 nok,fok;
	template <bool alpha_blend>
	void Rendtriangle(const Vertex &v1, const Vertex &v2, const Vertex &v3, u32* colorBuffer)
	{
		const int stride = 640 * 4;
		//Plane equation


		// 28.4 fixed-point coordinates
		const int Y1 = iround(16.0f * v1.y);
		const int Y2 = iround(16.0f * v2.y);
		const int Y3 = iround(16.0f * v3.y);

		const int X1 = iround(16.0f * v1.x);
		const int X2 = iround(16.0f * v2.x);
		const int X3 = iround(16.0f * v3.x);

		int sgn = 1;

		// Deltas
		{
			//area: (X1-X3)*(Y2-Y3)-(Y1-Y3)*(X2-X3)

			if (((X1 - X3)*(Y2 - Y3) - (Y1 - Y3)*(X2 - X3))>0)
				sgn = -1;
		}

		const int DX12 = sgn*(X1 - X2);
		const int DX23 = sgn*(X2 - X3);
		const int DX31 = sgn*(X3 - X1);

		const int DY12 = sgn*(Y1 - Y2);
		const int DY23 = sgn*(Y2 - Y3);
		const int DY31 = sgn*(Y3 - Y1);

		// Fixed-point deltas
		const int FDX12 = DX12 << 4;
		const int FDX23 = DX23 << 4;
		const int FDX31 = DX31 << 4;

		const int FDY12 = DY12 << 4;
		const int FDY23 = DY23 << 4;
		const int FDY31 = DY31 << 4;

		// Block size, standard 4x4 (must be power of two)
		const int q = 4;

		// Bounding rectangle
		int minx = (mmin(X1, X2, X3, 0) + 0xF) >> 4;
		int miny = (mmin(Y1, Y2, Y3, 0) + 0xF) >> 4;

		// Start in corner of block
		minx &= ~(q - 1);
		miny &= ~(q - 1);

		int spanx = ((mmax(X1, X2, X3, 640 << 4) + 0xF) >> 4) - minx;
		int spany = ((mmax(Y1, Y2, Y3, 480 << 4) + 0xF) >> 4) - miny;

		// Half-edge constants
		int C1 = DY12 * X1 - DX12 * Y1;
		int C2 = DY23 * X2 - DX23 * Y2;
		int C3 = DY31 * X3 - DX31 * Y3;

		// Correct for fill convention
		if (DY12 < 0 || (DY12 == 0 && DX12 > 0)) C1++;
		if (DY23 < 0 || (DY23 == 0 && DX23 > 0)) C2++;
		if (DY31 < 0 || (DY31 == 0 && DX31 > 0)) C3++;

		int MAX_12, MAX_23, MAX_31, MIN_12, MIN_23, MIN_31;

		PlaneMinMax(MIN_12, MAX_12, DX12, DY12, q);
		PlaneMinMax(MIN_23, MAX_23, DX23, DY23, q);
		PlaneMinMax(MIN_31, MAX_31, DX31, DY31, q);

		const int FDqX12 = FDX12 * q;
		const int FDqX23 = FDX23 * q;
		const int FDqX31 = FDX31 * q;

		const int FDqY12 = FDY12 * q;
		const int FDqY23 = FDY23 * q;
		const int FDqY31 = FDY31 * q;

		const int FDX12mq = FDX12 + FDY12*q;
		const int FDX23mq = FDX23 + FDY23*q;
		const int FDX31mq = FDX31 + FDY31*q;

		int hs12 = C1 + FDX12 * miny - FDY12 * minx + FDqY12 - MIN_12;
		int hs23 = C2 + FDX23 * miny - FDY23 * minx + FDqY23 - MIN_23;
		int hs31 = C3 + FDX31 * miny - FDY31 * minx + FDqY31 - MIN_31;

		MAX_12 -= MIN_12;
		MAX_23 -= MIN_23;
		MAX_31 -= MIN_31;

		int C1_pm = MIN_12;
		int C2_pm = MIN_23;
		int C3_pm = MIN_31;


		u8* cb_y = (u8*)colorBuffer;
		cb_y += miny*stride + minx*(q * 4);

		ip.Setup(v1, v2, v3, minx, miny, q);
		__m128 y_ps = _mm_cvtepi32_ps(_mm_broadcast(miny));
		__m128 minx_ps = _mm_cvtepi32_ps(_mm_load_scaled(minx - q, 1));
		static __declspec(align(16)) float ones_ps[4] = { 1, 1, 1, 1 };
		static __declspec(align(16)) float q_ps[4] = { q, q, q, q };

		// Loop through blocks
		for (int y = spany; y > 0; y -= q)
		{
			int Xhs12 = hs12;
			int Xhs23 = hs23;
			int Xhs31 = hs31;
			u8* cb_x = cb_y;
			__m128 x_ps = minx_ps;
			for (int x = spanx; x > 0; x -= q)
			{
				Xhs12 -= FDqY12;
				Xhs23 -= FDqY23;
				Xhs31 -= FDqY31;
				x_ps = _mm_add_ps(x_ps, *(__m128*)q_ps);

				// Corners of block
				bool any = EvalHalfSpaceFAny(Xhs12, Xhs23, Xhs31);

				// Skip block when outside an edge
				if (!any)
				{
					cb_x += q*q * 4;
					continue;
				}

				bool all = EvalHalfSpaceFAll(Xhs12, Xhs23, Xhs31, MAX_12, MAX_23, MAX_31);

				// Accept whole block when totally covered
				if (all)
				{
					__m128 yl_ps = y_ps;
					for (int iy = q; iy > 0; iy--)
					{
						PixelFlush<false, alpha_blend>(x_ps, yl_ps, cb_x, x_ps);
						yl_ps = _mm_add_ps(yl_ps, *(__m128*)ones_ps);
						cb_x += sizeof(__m128);
					}
				}
				else // Partially covered block
				{
					int CY1 = C1_pm + Xhs12;
					int CY2 = C2_pm + Xhs23;
					int CY3 = C3_pm + Xhs31;

					__m128i pfdx12 = _mm_broadcast(FDX12);
					__m128i pfdx23 = _mm_broadcast(FDX23);
					__m128i pfdx31 = _mm_broadcast(FDX31);

					__m128i pcy1 = _mm_load_scaled(CY1, -FDY12);
					__m128i pcy2 = _mm_load_scaled(CY2, -FDY23);
					__m128i pcy3 = _mm_load_scaled(CY3, -FDY31);

					__m128i pzero = _mm_setzero_si128();

					//bool ok=false;
					__m128 yl_ps = y_ps;

					for (int iy = q; iy > 0; iy--)
					{
						__m128i a = _mm_cmpgt_epi32(_mm_or_si128(_mm_or_si128(pcy1, pcy2), pcy3), pzero);
						int msk = _mm_movemask_ps(*(__m128*)&a);
						if (msk != 0)
						{
							PixelFlush<true, alpha_blend>(x_ps, yl_ps, cb_x, *(__m128*)&a);
						}

						yl_ps = _mm_add_ps(yl_ps, *(__m128*)ones_ps);
						cb_x += sizeof(__m128);

						//CY1 += FDX12mq;
						//CY2 += FDX23mq;
						//CY3 += FDX31mq;
						pcy1 = _mm_add_epi32(pcy1, pfdx12);
						pcy2 = _mm_add_epi32(pcy2, pfdx23);
						pcy3 = _mm_add_epi32(pcy3, pfdx31);
					}
					/*
					if (!ok)
					{
					nok++;
					}
					else
					{
					fok++;
					}*/
				}
			}
		next_y:
			hs12 += FDqX12;
			hs23 += FDqX23;
			hs31 += FDqX31;
			cb_y += stride*q;
			y_ps = _mm_add_ps(y_ps, *(__m128*)q_ps);
		}
	}

	template <bool alpha_blend>
	void RenderParamList(List<PolyParam>* param_list) {
		
		Vertex* verts = pvrrc.verts.head();
		u16* idx = pvrrc.idx.head();

		PolyParam* params = param_list->head();
		int param_count = param_list->used();

		for (int i = 0; i < param_count; i++)
		{
			int vertex_count = params[i].count - 2;

			u16* poly_idx = &idx[params[i].first];

			for (int v = 0; v < vertex_count; v++) {

				Rendtriangle<alpha_blend>(verts[poly_idx[v]], verts[poly_idx[v + 1]], verts[poly_idx[v + 2]], render_buffer);
			}
		}
	}
	virtual bool Render() {
		bool is_rtt = pvrrc.isRTT;

		memset(render_buffer, 0, sizeof(render_buffer));

		if (pvrrc.verts.used()<3)
			return false;
	

		RenderParamList<false>(&pvrrc.global_param_op);
		RenderParamList<false>(&pvrrc.global_param_pt);
		RenderParamList<true>(&pvrrc.global_param_tr);
		

		/*
		for (int y = 0; y < 480; y++) {
			for (int x = 0; x < 640; x++) {
				color_buffer[x + y * 640] = rand();
			}
		} */

		return !is_rtt;
	}

	HWND hWnd;
	HBITMAP hBMP = 0, holdBMP;
	HDC hmem;
	


	virtual bool Init() {
		hWnd = (HWND)libPvr_GetRenderTarget();

		bi.biWidth = 640;
		bi.biHeight = 480;

		RECT rect;

		GetClientRect(hWnd, &rect);

		HDC hdc = GetDC(hWnd);

		FillRect(hdc, &rect, (HBRUSH)(COLOR_BACKGROUND));

		bi.biSizeImage = bi.biWidth * bi.biHeight * 4;

		hBMP = CreateCompatibleBitmap(hdc, bi.biWidth, bi.biHeight);
		hmem = CreateCompatibleDC(hdc);
		holdBMP = (HBITMAP)SelectObject(hmem, hBMP);
		ReleaseDC(hWnd, hdc);

		return true;
	}

	virtual void Resize(int w, int h) {

	}

	virtual void Term() {
		if (hBMP) {
			DeleteObject(SelectObject(hmem, holdBMP));
			DeleteDC(hmem);
		}
	}

	virtual void Present() {

		__m128* psrc = (__m128*)render_buffer;
		__m128* pdst = (__m128*)pixels;

		const int stride = 640 / 4;
		for (int y = 0; y<480; y += 4)
		{
			for (int x = 0; x<640; x += 4)
			{
				pdst[(480 - (y + 0))*stride + x / 4] = *psrc++;
				pdst[(480 - (y + 1))*stride + x / 4] = *psrc++;
				pdst[(480 - (y + 2))*stride + x / 4] = *psrc++;
				pdst[(480 - (y + 3))*stride + x / 4] = *psrc++;
			}
		}
		
		SetDIBits(hmem, hBMP, 0, 480, pixels, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

		RECT clientRect;

		GetClientRect(hWnd, &clientRect);

		HDC hdc = GetDC(hWnd);
		int w = clientRect.right - clientRect.left;
		int h = clientRect.bottom - clientRect.top;
		int x = (w - 640) / 2;
		int y = (h - 480) / 2;

		BitBlt(hdc, x, y, 640 , 480 , hmem, 0, 0, SRCCOPY);
		ReleaseDC(hWnd, hdc);
	}
};

Renderer* rend_softrend() {
	return new(_mm_malloc(sizeof(softrend), 32)) softrend();
}