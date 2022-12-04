
/*
	TA-VTX handling

	Parsing of the TA stream and generation of vertex data !
*/
#include "ta.h"
#include "ta_ctx.h"
#include "pvr_mem.h"
#include "Renderer_if.h"
#include "cfg/option.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define TACALL DYNACALL
#ifdef NDEBUG
#undef verify
#define verify(x)
#endif

static u8 f32_su8_tbl[65536];

static u8 float_to_satu8(float val) {
	return f32_su8_tbl[(u32&)val >> 16];
}

static TA_context *vd_ctx;
#define vd_rc (vd_ctx->rend)

constexpr u32 ListType_None = -1;

static f32 f16(u16 v)
{
	u32 z=v<<16;
	return *(f32*)&z;
}

class BaseTAParser
{
	static Ta_Dma *DYNACALL NullVertexData(Ta_Dma *data, Ta_Dma *data_end)
	{
		INFO_LOG(PVR, "TA: Invalid state, ignoring VTX data");
		return data + SZ32;
	}

public:
	static void startList(u32 listType)
	{
		if (CurrentList != ListType_None)
			return;
		CurrentList = listType;
		if (listType == ListType_Opaque)
			CurrentPPlist = &vd_rc.global_param_op;
		else if (listType == ListType_Punch_Through)
			CurrentPPlist = &vd_rc.global_param_pt;
		else if (listType == ListType_Translucent)
			CurrentPPlist = &vd_rc.global_param_tr;

		CurrentPP = nullptr;
	}

	static void endList()
	{
		if (CurrentList == ListType_None)
			return;
		if (CurrentPP != nullptr && CurrentPP->count == 0 && CurrentPP == CurrentPPlist->LastPtr())
			CurrentPPlist->PopLast();
		CurrentPP = nullptr;
		CurrentPPlist = nullptr;

		if (CurrentList == ListType_Opaque_Modifier_Volume
				|| CurrentList == ListType_Translucent_Modifier_Volume)
			endModVol();
		CurrentList = ListType_None;
		VertexDataFP = NullVertexData;
	}

	static int getCurrentList() {
		return CurrentList;
	}

	static u32 getTileClip() {
		return tileclip_val;
	}

	static void setTileClip(u32 tileclip) {
		tileclip_val = tileclip;
	}

protected:
	typedef Ta_Dma* DYNACALL TaListFP(Ta_Dma* data, Ta_Dma* data_end);
	typedef void TACALL TaPolyParamFP(void* ptr);

	static void endModVol()
	{
		List<ModifierVolumeParam> *list = nullptr;
		if (CurrentList == ListType_Opaque_Modifier_Volume)
			list = &vd_rc.global_param_mvo;
		else if (CurrentList == ListType_Translucent_Modifier_Volume)
			list = &vd_rc.global_param_mvo_tr;
		else
			return;
		if (list->used() > 0)
		{
			ModifierVolumeParam *p = list->LastPtr();
			p->count = vd_rc.modtrig.used() - p->first;
			if (p->count == 0)
				list->PopLast();
		}
	}

	static void reset()
	{
		memset(FaceBaseColor, 0xff, sizeof(FaceBaseColor));
		memset(FaceOffsColor, 0xff, sizeof(FaceOffsColor));
		memset(FaceBaseColor1, 0xff, sizeof(FaceBaseColor1));
		memset(FaceOffsColor1, 0xff, sizeof(FaceOffsColor1));
		SFaceBaseColor = 0;
		SFaceOffsColor = 0;
		lmr = nullptr;
		CurrentList = ListType_None;
		CurrentPP = nullptr;
		CurrentPPlist = nullptr;
		VertexDataFP = NullVertexData;
	}

	static const u32 *ta_type_lut;

	//cache state vars
	static u32 tileclip_val;

	//TA state vars
	static u8 FaceBaseColor[4];
	static u8 FaceOffsColor[4];
	static u8 FaceBaseColor1[4];
	static u8 FaceOffsColor1[4];
	static u32 SFaceBaseColor;
	static u32 SFaceOffsColor;
	//vdec state variables
	static ModTriangle* lmr;

	static u32 CurrentList;
	static PolyParam* CurrentPP;
	static TaListFP *VertexDataFP;
public:
	static List<PolyParam>* CurrentPPlist;
	static TaListFP* TaCmd;
	static bool fetchTextures;
};

const u32 *BaseTAParser::ta_type_lut = TaTypeLut::instance().table;
u32 BaseTAParser::tileclip_val;
alignas(4) u8 BaseTAParser::FaceBaseColor[4];
alignas(4) u8 BaseTAParser::FaceOffsColor[4];
alignas(4) u8 BaseTAParser::FaceBaseColor1[4];
alignas(4) u8 BaseTAParser::FaceOffsColor1[4];
u32 BaseTAParser::SFaceBaseColor;
u32 BaseTAParser::SFaceOffsColor;
ModTriangle* BaseTAParser::lmr;
u32 BaseTAParser::CurrentList;
PolyParam* BaseTAParser::CurrentPP;
List<PolyParam>* BaseTAParser::CurrentPPlist;
BaseTAParser::TaListFP *BaseTAParser::TaCmd;
BaseTAParser::TaListFP *BaseTAParser::VertexDataFP;
bool BaseTAParser::fetchTextures = true;

template<int Red = 0, int Green = 1, int Blue = 2, int Alpha = 3>
class TAParserTempl : public BaseTAParser
{
	//part : 0 fill all data , 1 fill upper 32B , 2 fill lower 32B
	//Poly decoder , will be moved to pvr code
	template <u32 poly_type,u32 part>
	static Ta_Dma* TACALL ta_handle_poly(Ta_Dma* data,Ta_Dma* data_end)
	{
		TA_VertexParam* vp=(TA_VertexParam*)data;
		u32 rv=0;

		if (part==2)
		{
			TaCmd=ta_main;
		}

		switch (poly_type)
		{
#define ver_32B_def(num) \
case num : {\
AppendPolyVertex##num(&vp->vtx##num);\
rv=SZ32; }\
break;

			//32b , always in one pass :)
			ver_32B_def(0);//(Non-Textured, Packed Color)
			ver_32B_def(1);//(Non-Textured, Floating Color)
			ver_32B_def(2);//(Non-Textured, Intensity)
			ver_32B_def(3);//(Textured, Packed Color)
			ver_32B_def(4);//(Textured, Packed Color, 16bit UV)
			ver_32B_def(7);//(Textured, Intensity)
			ver_32B_def(8);//(Textured, Intensity, 16bit UV)
			ver_32B_def(9);//(Non-Textured, Packed Color, with Two Volumes)
			ver_32B_def(10);//(Non-Textured, Intensity,	with Two Volumes)

#undef ver_32B_def

#define ver_64B_def(num) \
case num : {\
/*process first half*/\
	if (part!=2)\
	{\
	rv+=SZ32;\
	AppendPolyVertex##num##A(&vp->vtx##num##A);\
	}\
	/*process second half*/\
	if (part==0)\
	{\
	AppendPolyVertex##num##B(&vp->vtx##num##B);\
	rv+=SZ32;\
	}\
	else if (part==2)\
	{\
	AppendPolyVertex##num##B((TA_Vertex##num##B*)data);\
	rv+=SZ32;\
	}\
	}\
	break;


			//64b , may be on 2 pass
			ver_64B_def(5);//(Textured, Floating Color)
			ver_64B_def(6);//(Textured, Floating Color, 16bit UV)
			ver_64B_def(11);//(Textured, Packed Color,	with Two Volumes)	
			ver_64B_def(12);//(Textured, Packed Color, 16bit UV, with Two Volumes)
			ver_64B_def(13);//(Textured, Intensity,	with Two Volumes)
			ver_64B_def(14);//(Textured, Intensity, 16bit UV, with Two Volumes)
#undef ver_64B_def
		}

		return data+rv;
	};

	//Code Splitter/routers

	static Ta_Dma* TACALL ta_modvolB_32(Ta_Dma* data,Ta_Dma* data_end)
	{
		AppendModVolVertexB((TA_ModVolB*)data);
		TaCmd=ta_main;
		return data+SZ32;
	}
		
	static Ta_Dma* TACALL ta_mod_vol_data(Ta_Dma* data,Ta_Dma* data_end)
	{
		TA_VertexParam* vp=(TA_VertexParam*)data;
		if (data == data_end - SZ32)
		{
			AppendModVolVertexA(&vp->mvolA);
			//32B more needed , 32B done :)
			TaCmd=ta_modvolB_32;
			return data+SZ32;
		}
		else
		{
			//all 64B done
			AppendModVolVertexA(&vp->mvolA);
			AppendModVolVertexB(&vp->mvolB);
			return data+SZ64;
		}
	}
	static Ta_Dma* TACALL ta_spriteB_data(Ta_Dma* data,Ta_Dma* data_end)
	{
		//32B more needed , 32B done :)
		TaCmd=ta_main;
			
		AppendSpriteVertexB((TA_Sprite1B*)data);

		return data+SZ32;
	}
	static Ta_Dma* TACALL ta_sprite_data(Ta_Dma* data,Ta_Dma* data_end)
	{
		verify(data->pcw.ParaType==ParamType_Vertex_Parameter);
		if (data == data_end - SZ32)
		{
			//32B more needed , 32B done :)
			TaCmd=ta_spriteB_data;

			TA_VertexParam* vp=(TA_VertexParam*)data;

			AppendSpriteVertexA(&vp->spr1A);
			return data+SZ32;
		}
		else
		{
			TA_VertexParam* vp=(TA_VertexParam*)data;

			AppendSpriteVertexA(&vp->spr1A);
			AppendSpriteVertexB(&vp->spr1B);

			return data+SZ64;
		}
	}

	template <u32 poly_type,u32 poly_size>
	static Ta_Dma* TACALL ta_poly_data(Ta_Dma* data,Ta_Dma* data_end)
	{
		verify(data < data_end);

					//If SZ64  && 32 bytes
#define IS_FIST_HALF (poly_size != SZ32 && data == data_end - SZ32)

		if (IS_FIST_HALF)
			goto fist_half;

		do
		{
			verify(data->pcw.ParaType == ParamType_Vertex_Parameter);
			ta_handle_poly<poly_type,0>(data, 0);
			if (data->pcw.EndOfStrip)
				goto strip_end;
			data += poly_size;
		} while (data <= data_end - poly_size);
			
		if (IS_FIST_HALF)
		{
		fist_half:
			ta_handle_poly<poly_type,1>(data,0);
			if (data->pcw.EndOfStrip) EndPolyStrip();
			TaCmd=ta_handle_poly<poly_type,2>;
					
			data+=SZ32;
		}
			
		return data;

strip_end:
		TaCmd=ta_main;
		if (data->pcw.EndOfStrip)
			EndPolyStrip();
		return data+poly_size;
	}

	static void TACALL AppendPolyParam2Full(void* vpp)
	{
		Ta_Dma* pp=(Ta_Dma*)vpp;

		AppendPolyParam2A((TA_PolyParam2A*)&pp[0]);
		AppendPolyParam2B((TA_PolyParam2B*)&pp[1]);
	}

	static void TACALL AppendPolyParam4Full(void* vpp)
	{
		Ta_Dma* pp=(Ta_Dma*)vpp;

		AppendPolyParam4A((TA_PolyParam4A*)&pp[0]);
		AppendPolyParam4B((TA_PolyParam4B*)&pp[1]);
	}
	//Second part of poly data
	template <int t>
	static Ta_Dma* TACALL ta_poly_B_32(Ta_Dma* data,Ta_Dma* data_end)
	{
		if (t==2)
			AppendPolyParam2B((TA_PolyParam2B*)data);
		else
			AppendPolyParam4B((TA_PolyParam4B*)data);
	
		TaCmd=ta_main;
		return data+SZ32;
	}

	static Ta_Dma* TACALL ta_main(Ta_Dma* data, Ta_Dma* data_end)
	{
		while (data < data_end)
		{
			if (settings.platform.isNaomi2() && (data->pcw.full & 0x08000000) != 0)
			{
				DEBUG_LOG(PVR, "Naomi 2 command detected");
				break;
			}
			switch (data->pcw.ParaType)
			{
				//Control parameter
				//32Bw3
			case ParamType_End_Of_List:
				endList();
				data += SZ32;
				break;

				//32B
			case ParamType_User_Tile_Clip:
				SetTileClip(data->data_32[3] & 63, data->data_32[4] & 31, data->data_32[5] & 63, data->data_32[6] & 31);
				data += SZ32;
				break;

				//32B
			case ParamType_Object_List_Set:
				INFO_LOG(PVR, "Unsupported list type: ParamType_Object_List_Set");	// NAOMI Virtual on Oratorio Tangram
				// *cough* ignore it :p
				data += SZ32;
				break;

				//Global Parameter
				//ModVolue :32B
				//PolyType :32B/64B
			case ParamType_Polygon_or_Modifier_Volume:
				{
					TileClipMode(data->pcw.User_Clip);
					//Yep , C++ IS lame & limited
					#include "ta_const_df.h"
					if (CurrentList==ListType_None)
						startList(data->pcw.ListType);

					if (IsModVolList(CurrentList))
					{
						//accept mod data
						StartModVol((TA_ModVolParam*)data);
						VertexDataFP = ta_mod_vol_data;
						data+=SZ32;
					}
					else
					{

						u32 uid = ta_type_lut[data->pcw.obj_ctrl];
						u32 psz=uid>>30;
						u32 pdid=(u8)(uid);
						u32 ppid=(u8)(uid>>8);

						VertexDataFP = ta_poly_data_lut[pdid];
							

						if (data <= data_end - psz)
						{

							//poly , 32B/64B
							ta_poly_param_lut[ppid](data);
							data+=psz;
						}
						else
						{

							//AppendPolyParam64A((TA_PolyParamA*)data);
							//64b , first part
							ta_poly_param_a_lut[ppid](data);
							//Handle next 32B ;)
							TaCmd=ta_poly_param_b_lut[ppid];
							data+=SZ32;
						}
					}
				}
				break;
				//32B
				//Sets Sprite info , and switches to ta_sprite_data function
			case ParamType_Sprite:
				{

					TileClipMode(data->pcw.User_Clip);
					if (CurrentList==ListType_None)
						startList(data->pcw.ListType);

					VertexDataFP = ta_sprite_data;
					AppendSpriteParam((TA_SpriteParam*)data);
					data+=SZ32;
				}
				break;

				//Variable size
			case ParamType_Vertex_Parameter:
				data = VertexDataFP(data, data_end);
				break;

				//not handled
				//Assumed to be 32B
			case 3:
			case 6:
				WARN_LOG(PVR, "Unhandled param type pcw %08x", data->pcw.full);
				throw TAParserException();
				//die("Unhandled parameter");
				//data += SZ32;
				break;
			}
		}

		return data;
	}

	TAParserTempl();

public:
	static void reset()
	{
		TaCmd = ta_main;
		BaseTAParser::reset();
	}

private:
	static void SetTileClip(u32 xmin,u32 ymin,u32 xmax,u32 ymax)
	{
		u32 rv=tileclip_val & 0xF0000000;
		rv|=xmin; //6 bits
		rv|=xmax<<6; //6 bits
		rv|=ymin<<12; //5 bits
		rv|=ymax<<17; //5 bits
		tileclip_val=rv;
	}

	static void TileClipMode(u32 mode)
	{
		//Group_En bit seems ignored, thanks p1pkin
		tileclip_val=(tileclip_val&(~0xF0000000)) | (mode<<28);
	}

	//Polys  -- update code on sprites if that gets updated too --
	template<class T>
	static void glob_param_bdc_(T* pp)
	{
		PolyParam* d_pp = CurrentPP;
		if (d_pp == NULL || d_pp->count != 0)
		{
			d_pp = CurrentPPlist->Append();
			CurrentPP = d_pp;
		}
		d_pp->init();
		d_pp->first = vd_rc.verts.used();

		d_pp->isp = pp->isp;
		d_pp->tsp = pp->tsp;
		d_pp->tcw = pp->tcw;
		d_pp->pcw = pp->pcw;
		d_pp->tileclip = tileclip_val;

		if (d_pp->pcw.Texture && fetchTextures)
			d_pp->texture = renderer->GetTexture(d_pp->tsp, d_pp->tcw);
	}

	#define glob_param_bdc(pp) glob_param_bdc_( (TA_PolyParam0*)pp)

	#define poly_float_color_(to,a,r,g,b) \
		to[Red] = float_to_satu8(r);	\
		to[Green] = float_to_satu8(g);	\
		to[Blue] = float_to_satu8(b);	\
		to[Alpha] = float_to_satu8(a);


	#define poly_float_color(to,src) \
		poly_float_color_(to,pp->src##A,pp->src##R,pp->src##G,pp->src##B)

	// Poly param handling

	// Packed/Floating Color
	static void TACALL AppendPolyParam0(void* vpp)
	{
		TA_PolyParam0* pp=(TA_PolyParam0*)vpp;

		glob_param_bdc(pp);
	}

	// Intensity, no Offset Color
	static void TACALL AppendPolyParam1(void* vpp)
	{
		TA_PolyParam1* pp=(TA_PolyParam1*)vpp;

		glob_param_bdc(pp);
		poly_float_color(FaceBaseColor,FaceColor);
	}

	// Intensity, use Offset Color
	static void TACALL AppendPolyParam2A(void* vpp)
	{
		TA_PolyParam2A* pp=(TA_PolyParam2A*)vpp;

		glob_param_bdc(pp);
	}

	static void TACALL AppendPolyParam2B(void* vpp)
	{
		TA_PolyParam2B* pp=(TA_PolyParam2B*)vpp;

		poly_float_color(FaceBaseColor,FaceColor);
		poly_float_color(FaceOffsColor,FaceOffset);
	}

	// Packed Color, with Two Volumes
	static void TACALL AppendPolyParam3(void* vpp)
	{
		TA_PolyParam3* pp=(TA_PolyParam3*)vpp;

		glob_param_bdc(pp);

		CurrentPP->tsp1.full = pp->tsp1.full;
		CurrentPP->tcw1.full = pp->tcw1.full;
		if (pp->pcw.Texture && fetchTextures)
			CurrentPP->texture1 = renderer->GetTexture(pp->tsp1, pp->tcw1);
	}

	// Intensity, with Two Volumes
	static void TACALL AppendPolyParam4A(void* vpp)
	{
		TA_PolyParam4A* pp=(TA_PolyParam4A*)vpp;

		glob_param_bdc(pp);

		CurrentPP->tsp1.full = pp->tsp1.full;
		CurrentPP->tcw1.full = pp->tcw1.full;
		if (pp->pcw.Texture && fetchTextures)
			CurrentPP->texture1 = renderer->GetTexture(pp->tsp1, pp->tcw1);
	}

	static void TACALL AppendPolyParam4B(void* vpp)
	{
		TA_PolyParam4B* pp=(TA_PolyParam4B*)vpp;

		poly_float_color(FaceBaseColor, FaceColor0);
		poly_float_color(FaceBaseColor1, FaceColor1);
	}

	//Poly Strip handling
	static void EndPolyStrip()
	{
		CurrentPP->count = vd_rc.verts.used() - CurrentPP->first;

		if (CurrentPP->count > 0)
		{
			PolyParam* d_pp = CurrentPPlist->Append();
			*d_pp = *CurrentPP;
			CurrentPP = d_pp;
			d_pp->first = vd_rc.verts.used();
			d_pp->count = 0;
		}
	}
	
	static inline void update_fz(float z)
	{
		if ((s32&)vd_rc.fZ_max<(s32&)z && (s32&)z<0x49800000)
			vd_rc.fZ_max=z;
	}

		//Poly Vertex handlers
		//Append vertex base
	template<class T>
	static Vertex* vert_cvt_base_(T* vtx)
	{
		f32 invW = vtx->xyz[2];
		Vertex* cv = vd_rc.verts.Append();
		cv->x = vtx->xyz[0];
		cv->y = vtx->xyz[1];
		cv->z = invW;
		update_fz(invW);
		return cv;
	}

	#define vert_cvt_base Vertex* cv=vert_cvt_base_((TA_Vertex0*)vtx)

		//Resume vertex base (for B part)
	#define vert_res_base \
		Vertex* cv=vd_rc.verts.LastPtr();

		//uv 16/32
	#define vert_uv_32(u_name,v_name) \
		cv->u = (vtx->u_name);\
		cv->v = (vtx->v_name);

	#define vert_uv_16(u_name,v_name) \
		cv->u = f16(vtx->u_name);\
		cv->v = f16(vtx->v_name);

	#define vert_uv1_32(u_name,v_name) \
		cv->u1 = (vtx->u_name);\
		cv->v1 = (vtx->v_name);

	#define vert_uv1_16(u_name,v_name) \
		cv->u1 = f16(vtx->u_name);\
		cv->v1 = f16(vtx->v_name);

		//Color conversions
	#define vert_packed_color_(to,src) \
		{ \
		u32 t=src; \
		to[Blue] = (u8)(t);t>>=8;\
		to[Green] = (u8)(t);t>>=8;\
		to[Red] = (u8)(t);t>>=8;\
		to[Alpha] = (u8)(t);      \
		}

	#define vert_float_color_(to,a,r,g,b) \
		to[Red] = float_to_satu8(r); \
		to[Green] = float_to_satu8(g); \
		to[Blue] = float_to_satu8(b); \
		to[Alpha] = float_to_satu8(a);

		//Macros to make thins easier ;)
	#define vert_packed_color(to,src) \
		vert_packed_color_(cv->to,vtx->src);

	#define vert_float_color(to,src) \
		vert_float_color_(cv->to,vtx->src##A,vtx->src##R,vtx->src##G,vtx->src##B)

		//Intensity handling

		//Notes:
		//Alpha doesn't get intensity
		//Intensity is clamped before the mul, as well as on face color to work the same as the hardware. [Fixes red dog]

	#define vert_face_base_color(baseint) \
		{ u32 satint = float_to_satu8(vtx->baseint); \
		cv->col[Red] = FaceBaseColor[Red] * satint / 256;  \
		cv->col[Green] = FaceBaseColor[Green] * satint / 256;  \
		cv->col[Blue] = FaceBaseColor[Blue] * satint / 256;  \
		cv->col[Alpha] = FaceBaseColor[Alpha]; }

	#define vert_face_offs_color(offsint) \
		{ u32 satint = float_to_satu8(vtx->offsint); \
		cv->spc[Red] = FaceOffsColor[Red] * satint / 256;  \
		cv->spc[Green] = FaceOffsColor[Green] * satint / 256;  \
		cv->spc[Blue] = FaceOffsColor[Blue] * satint / 256;  \
		cv->spc[Alpha] = FaceOffsColor[Alpha]; }

	#define vert_face_base_color1(baseint) \
		{ u32 satint = float_to_satu8(vtx->baseint); \
		cv->col1[Red] = FaceBaseColor1[Red] * satint / 256;  \
		cv->col1[Green] = FaceBaseColor1[Green] * satint / 256;  \
		cv->col1[Blue] = FaceBaseColor1[Blue] * satint / 256;  \
		cv->col1[Alpha] = FaceBaseColor1[Alpha]; }

	#define vert_face_offs_color1(offsint) \
		{ u32 satint = float_to_satu8(vtx->offsint); \
		cv->spc1[Red] = FaceOffsColor1[Red] * satint / 256;  \
		cv->spc1[Green] = FaceOffsColor1[Green] * satint / 256;  \
		cv->spc1[Blue] = FaceOffsColor1[Blue] * satint / 256;  \
		cv->spc1[Alpha] = FaceOffsColor1[Alpha]; }


	//(Non-Textured, Packed Color)
	static void AppendPolyVertex0(TA_Vertex0* vtx)
	{
		vert_cvt_base;

		vert_packed_color(col,BaseCol);
	}

	//(Non-Textured, Floating Color)
	static void AppendPolyVertex1(TA_Vertex1* vtx)
	{
		vert_cvt_base;

		vert_float_color(col,Base);
	}

	//(Non-Textured, Intensity)
	static void AppendPolyVertex2(TA_Vertex2* vtx)
	{
		vert_cvt_base;

		vert_face_base_color(BaseInt);
	}

	//(Textured, Packed Color)
	static void AppendPolyVertex3(TA_Vertex3* vtx)
	{
		vert_cvt_base;

		vert_packed_color(col,BaseCol);
		vert_packed_color(spc,OffsCol);

		vert_uv_32(u,v);
	}

	//(Textured, Packed Color, 16bit UV)
	static void AppendPolyVertex4(TA_Vertex4* vtx)
	{
		vert_cvt_base;

		vert_packed_color(col,BaseCol);
		vert_packed_color(spc,OffsCol);

		vert_uv_16(u,v);
	}

	//(Textured, Floating Color)
	static void AppendPolyVertex5A(TA_Vertex5A* vtx)
	{
		vert_cvt_base;

		//Colors are on B

		vert_uv_32(u,v);
	}

	static void AppendPolyVertex5B(TA_Vertex5B* vtx)
	{
		vert_res_base;

		vert_float_color(col,Base);
		vert_float_color(spc,Offs);
	}

	//(Textured, Floating Color, 16bit UV)
	static void AppendPolyVertex6A(TA_Vertex6A* vtx)
	{
		vert_cvt_base;

		//Colors are on B

		vert_uv_16(u,v);
	}

	static void AppendPolyVertex6B(TA_Vertex6B* vtx)
	{
		vert_res_base;

		vert_float_color(col,Base);
		vert_float_color(spc,Offs);
	}

	//(Textured, Intensity)
	static void AppendPolyVertex7(TA_Vertex7* vtx)
	{
		vert_cvt_base;

		vert_face_base_color(BaseInt);
		vert_face_offs_color(OffsInt);

		vert_uv_32(u,v);
	}

	//(Textured, Intensity, 16bit UV)
	static void AppendPolyVertex8(TA_Vertex8* vtx)
	{
		vert_cvt_base;

		vert_face_base_color(BaseInt);
		vert_face_offs_color(OffsInt);

		vert_uv_16(u,v);

	}

	//(Non-Textured, Packed Color, with Two Volumes)
	static void AppendPolyVertex9(TA_Vertex9* vtx)
	{
		vert_cvt_base;

		vert_packed_color(col,BaseCol0);
		vert_packed_color(col1, BaseCol1);
	}

	//(Non-Textured, Intensity,	with Two Volumes)
	static void AppendPolyVertex10(TA_Vertex10* vtx)
	{
		vert_cvt_base;

		vert_face_base_color(BaseInt0);
		vert_face_base_color1(BaseInt1);
	}

	//(Textured, Packed Color,	with Two Volumes)	
	static void AppendPolyVertex11A(TA_Vertex11A* vtx)
	{
		vert_cvt_base;

		vert_packed_color(col,BaseCol0);
		vert_packed_color(spc,OffsCol0);

		vert_uv_32(u0,v0);
	}

	static void AppendPolyVertex11B(TA_Vertex11B* vtx)
	{
		vert_res_base;

		vert_packed_color(col1, BaseCol1);
		vert_packed_color(spc1, OffsCol1);

		vert_uv1_32(u1, v1);
	}

	//(Textured, Packed Color, 16bit UV, with Two Volumes)
	static void AppendPolyVertex12A(TA_Vertex12A* vtx)
	{
		vert_cvt_base;

		vert_packed_color(col,BaseCol0);
		vert_packed_color(spc,OffsCol0);

		vert_uv_16(u0,v0);
	}

	static void AppendPolyVertex12B(TA_Vertex12B* vtx)
	{
		vert_res_base;

		vert_packed_color(col1, BaseCol1);
		vert_packed_color(spc1, OffsCol1);

		vert_uv1_16(u1, v1);
	}

	//(Textured, Intensity,	with Two Volumes)
	static void AppendPolyVertex13A(TA_Vertex13A* vtx)
	{
		vert_cvt_base;

		vert_face_base_color(BaseInt0);
		vert_face_offs_color(OffsInt0);

		vert_uv_32(u0,v0);
	}

	static void AppendPolyVertex13B(TA_Vertex13B* vtx)
	{
		vert_res_base;

		vert_face_base_color1(BaseInt1);
		vert_face_offs_color1(OffsInt1);

		vert_uv1_32(u1,v1);
	}

	//(Textured, Intensity, 16bit UV, with Two Volumes)
	static void AppendPolyVertex14A(TA_Vertex14A* vtx)
	{
		vert_cvt_base;

		vert_face_base_color(BaseInt0);
		vert_face_offs_color(OffsInt0);

		vert_uv_16(u0,v0);
	}

	static void AppendPolyVertex14B(TA_Vertex14B* vtx)
	{
		vert_res_base;

		vert_face_base_color1(BaseInt1);
		vert_face_offs_color1(OffsInt1);

		vert_uv1_16(u1, v1);
	}

	//Sprites
	static void AppendSpriteParam(TA_SpriteParam* spr)
	{
		//printf("Sprite\n");
		PolyParam* d_pp = CurrentPP;
		if (CurrentPP == NULL || CurrentPP->count != 0)
		{
			if (CurrentPPlist == nullptr)	// wldkickspw
				return;
			d_pp = CurrentPPlist->Append();
			CurrentPP = d_pp;
		}
		d_pp->init();
		d_pp->first = vd_rc.verts.used();
		d_pp->isp = spr->isp;
		d_pp->tsp = spr->tsp;
		d_pp->tcw = spr->tcw;
		d_pp->pcw = spr->pcw;
		d_pp->tileclip = tileclip_val;

		if (d_pp->pcw.Texture && fetchTextures)
			d_pp->texture = renderer->GetTexture(d_pp->tsp, d_pp->tcw);

		SFaceBaseColor = spr->BaseCol;
		SFaceOffsColor = spr->OffsCol;
        
        d_pp->isp.CullMode ^= 1;
	}

	#define append_sprite(indx) \
		vert_packed_color_(cv[indx].col,SFaceBaseColor)\
		vert_packed_color_(cv[indx].spc,SFaceOffsColor)

	#define sprite_uv(indx,u_name,v_name) \
		cv[indx].u = f16(sv->u_name);\
		cv[indx].v = f16(sv->v_name);

	//Sprite Vertex Handlers
	static void AppendSpriteVertexA(TA_Sprite1A* sv)
	{
        CurrentPP->count = 4;

		Vertex* cv = vd_rc.verts.Append(4);

		//Fill static stuff
		append_sprite(0);
		append_sprite(1);
		append_sprite(2);
		append_sprite(3);

		cv[2].x=sv->x0;
		cv[2].y=sv->y0;
		cv[2].z=sv->z0;
		update_fz(sv->z0);

		cv[3].x=sv->x1;
		cv[3].y=sv->y1;
		cv[3].z=sv->z1;
		update_fz(sv->z1);

		cv[1].x=sv->x2;
	}

	static void CaclulateSpritePlane(Vertex* base)
	{
		const Vertex& A=base[2];
		const Vertex& B=base[3];
		const Vertex& C=base[1];
		Vertex& P=base[0];
		//Vector AB = B-A;
		//Vector AC = C-A;
		//Vector AP = P-A;
		float AC_x=C.x-A.x,AC_y=C.y-A.y,AC_z=C.z-A.z,
			AB_x=B.x-A.x,AB_y=B.y-A.y,AB_z=B.z-A.z,
			AP_x=P.x-A.x,AP_y=P.y-A.y;

		float P_y = P.y, P_x = P.x, A_x = A.x, A_y = A.y, A_z = A.z;

		float AB_v=B.v-A.v,AB_u=B.u-A.u,
			AC_v=C.v-A.v,AC_u=C.u-A.u;

		float /*P_v,P_u,*/A_v=A.v,A_u=A.u;

		float k3 = (AC_x * AB_y - AC_y * AB_x);

		if (k3 == 0)
		{
			//throw new Exception("WTF?!");
		}

		float k2 = (AP_x * AB_y - AP_y * AB_x) / k3;

		float k1 = 0;

		if (AB_x == 0)
		{
			//if (AB_y == 0)
			//	;
			//    //throw new Exception("WTF?!");

			k1 = (P_y - A_y - k2 * AC_y) / AB_y;
		}
		else
		{
			k1 = (P_x - A_x - k2 * AC_x) / AB_x;
		}

		P.z = A_z + k1 * AB_z + k2 * AC_z;
		P.u = A_u + k1 * AB_u + k2 * AC_u;
		P.v = A_v + k1 * AB_v + k2 * AC_v;
	}

	static void AppendSpriteVertexB(TA_Sprite1B* sv)
	{
		vert_res_base;
		cv-=3;

		cv[1].y=sv->y2;
		cv[1].z=sv->z2;
		update_fz(sv->z2);

		cv[0].x=sv->x3;
		cv[0].y=sv->y3;


		sprite_uv(2, u0,v0);
		sprite_uv(3, u1,v1);
		sprite_uv(1, u2,v2);
		//sprite_uv(0, u0,v2);//or sprite_uv(u2,v0); ?

		CaclulateSpritePlane(cv);

		update_fz(cv[0].z);

		PolyParam* d_pp = CurrentPPlist->Append();
		*d_pp = *CurrentPP;
		CurrentPP = d_pp;
		d_pp->first = vd_rc.verts.used();
		d_pp->count = 0;
	}

	// Modifier Volumes Vertex handlers
	
	static void StartModVol(TA_ModVolParam* param)
	{
		endModVol();

		ModifierVolumeParam *p = NULL;
		if (CurrentList == ListType_Opaque_Modifier_Volume)
			p = vd_rc.global_param_mvo.Append();
		else if (CurrentList == ListType_Translucent_Modifier_Volume)
			p = vd_rc.global_param_mvo_tr.Append();
		else
			return;
		p->init();
		p->isp.full = param->isp.full;
		p->isp.VolumeLast = param->pcw.Volume != 0;
		p->first = vd_rc.modtrig.used();
	}

	static void AppendModVolVertexA(TA_ModVolA* mvv)
	{
		if (CurrentList != ListType_Opaque_Modifier_Volume && CurrentList != ListType_Translucent_Modifier_Volume)
			return;
		lmr=vd_rc.modtrig.Append();

		lmr->x0=mvv->x0;
		lmr->y0=mvv->y0;
		lmr->z0=mvv->z0;
		//update_fz(mvv->z0);

		lmr->x1=mvv->x1;
		lmr->y1=mvv->y1;
		lmr->z1=mvv->z1;
		//update_fz(mvv->z1);

		lmr->x2=mvv->x2;
	}

	static void AppendModVolVertexB(TA_ModVolB* mvv)
	{
		if (CurrentList != ListType_Opaque_Modifier_Volume && CurrentList != ListType_Translucent_Modifier_Volume)
			return;
		lmr->y2=mvv->y2;
		lmr->z2=mvv->z2;
		//update_fz(mvv->z2);
	}
};

static void getRegionTileClipping(u32& xmin, u32& xmax, u32& ymin, u32& ymax);
static void getRegionSettings(int passNumber, RenderPass& pass);

//
// Check if a vertex has huge x,y,z values or negative z
//
static bool is_vertex_inf(const Vertex& vtx)
{
	return std::isnan(vtx.x) || fabsf(vtx.x) > 3.4e37f
			|| std::isnan(vtx.y) || fabsf(vtx.y) > 3.4e37f
			|| std::isnan(vtx.z) || vtx.z < 0.f || vtx.z > 3.4e37f;
}

//
// Create the vertex index, eliminating invalid vertices and merging strips when possible.
//
static void make_index(const List<PolyParam> *polys, int first, int end, bool merge, rend_context* ctx)
{
	const u32 *indices = ctx->idx.head();
	const Vertex *vertices = ctx->verts.head();

	PolyParam *last_poly = nullptr;
	const PolyParam *end_poly = &polys->head()[end];
	bool cullingReversed = false;
	for (PolyParam *poly = &polys->head()[first]; poly != end_poly; poly++)
	{
		int first_index;
		bool dupe_next_vtx = false;
		if (merge
				&& last_poly != nullptr
				&& last_poly->count != 0
				&& poly->equivalentIgnoreCullingDirection(*last_poly))
		{
			const u32 last_vtx = indices[last_poly->first + last_poly->count - 1];
			*ctx->idx.Append() = last_vtx;
			if (poly->isp.CullMode < 2 || poly->isp.CullMode == last_poly->isp.CullMode)
			{
				if (cullingReversed)
					*ctx->idx.Append() = last_vtx;
				cullingReversed = false;
			}
			else
			{
				if (!cullingReversed)
					*ctx->idx.Append() = last_vtx;
				cullingReversed = true;
			}
			dupe_next_vtx = true;
			first_index = last_poly->first;
		}
		else
		{
			last_poly = poly;
			first_index = ctx->idx.used();
			cullingReversed = false;
		}
		int last_good_vtx = -1;
		for (u32 i = 0; i < poly->count; i++)
		{
			const Vertex& vtx = vertices[poly->first + i];
			if (!poly->isNaomi2() && is_vertex_inf(vtx))
			{
				while (i < poly->count - 1)
				{
					const Vertex& next_vtx = vertices[poly->first + i + 1];
					if (!is_vertex_inf(next_vtx))
					{
						// repeat last and next vertices to link strips
						if (last_good_vtx >= 0)
						{
							verify(!dupe_next_vtx);
							*ctx->idx.Append() = last_good_vtx;
							dupe_next_vtx = true;
						}
						break;
					}
					i++;
				}
			}
			else
			{
				last_good_vtx = poly->first + i;
				if (dupe_next_vtx)
				{
					*ctx->idx.Append() = last_good_vtx;
					dupe_next_vtx = false;
				}
				const u32 count = ctx->idx.used() - first_index;
				if (((i ^ count) & 1) ^ cullingReversed)
					*ctx->idx.Append() = last_good_vtx;
				*ctx->idx.Append() = last_good_vtx;
			}
		}
		if (last_poly == poly)
		{
			poly->first = first_index;
			poly->count = ctx->idx.used() - first_index;
		}
		else
		{
			last_poly->count = ctx->idx.used() - last_poly->first;
			poly->count = 0;
		}
	}
}

static void fix_texture_bleeding(const List<PolyParam> *list)
{
	const PolyParam *pp_end = list->LastPtr(0);
	const u32 *idx_base = vd_rc.idx.head();
	Vertex *vtx_base = vd_rc.verts.head();
	for (const PolyParam *pp = list->head(); pp != pp_end; pp++)
	{
		if (!pp->pcw.Texture || pp->count < 3)
			continue;
		// Find polygons that are facing the camera (constant z)
		// and only use 0 and 1 for U and V (some tolerance around 1 for SA2)
		// then apply a half-pixel correction on U and V.
		const u32 first = idx_base[pp->first];
		const u32 last = idx_base[pp->first + pp->count - 1];
		bool need_fixing = true;
		float z = 0.f;
		for (u32 idx = first; idx <= last && need_fixing; idx++)
		{
			Vertex& vtx = vtx_base[idx];

			if (vtx.u != 0.f && (vtx.u <= 0.995f || vtx.u > 1.f))
				need_fixing = false;
			else if (vtx.v != 0.f && (vtx.v <= 0.995f || vtx.v > 1.f))
				need_fixing = false;
			else if (idx == first)
				z = vtx.z;
			else if (z != vtx.z)
				need_fixing = false;
		}
		if (!need_fixing)
			continue;
		u32 tex_width = 8 << pp->tsp.TexU;
		u32 tex_height = 8 << pp->tsp.TexV;
		for (u32 idx = first; idx <= last; idx++)
		{
			Vertex& vtx = vtx_base[idx];
			if (vtx.u > 0.995f)
				vtx.u = 1.f;
			vtx.u = (0.5f + vtx.u * (tex_width - 1)) / tex_width;
			if (vtx.v > 0.995f)
				vtx.v = 1.f;
			vtx.v = (0.5f + vtx.v * (tex_height - 1)) / tex_height;
		}
	}
}

static bool operator<(const PolyParam& left, const PolyParam& right)
{
	return left.zvZ < right.zvZ;
}

static void sortPolyParams(List<PolyParam> *polys, int first, int end, rend_context* ctx)
{
	if (end - first <= 1)
		return;

	Vertex *vtx_base = ctx->verts.head();

	PolyParam *pp = &polys->head()[first];
	PolyParam *pp_end = &polys->head()[end];

	while (pp != pp_end)
	{
		if (pp->count < 3)
		{
			pp->zvZ = 0;
		}
		else
		{
			Vertex *vtx = &vtx_base[pp->first];
			Vertex *vtx_end = &vtx_base[pp->first + pp->count];

			if (pp->isNaomi2())
			{
				glm::mat4 mvMat = pp->mvMatrix != nullptr ? glm::make_mat4(pp->mvMatrix) : glm::mat4(1);
				glm::vec3 min{ 1e38f, 1e38f, 1e38f };
				glm::vec3 max{ -1e38f, -1e38f, -1e38f };
				while (vtx != vtx_end)
				{
					glm::vec3 pos{ vtx->x, vtx->y, vtx->z };
					min = glm::min(min, pos);
					max = glm::max(max, pos);
					vtx++;
				}
				glm::vec4 center((min + max) / 2.f, 1);
				glm::vec4 extents(max - glm::vec3(center), 0);
				// transform
				center = mvMat * center;
				glm::vec3 extentX = mvMat * glm::vec4(extents.x, 0, 0, 0);
				glm::vec3 extentY = mvMat * glm::vec4(0, extents.y, 0, 0);
				glm::vec3 extentZ = mvMat * glm::vec4(0, 0, extents.z, 0);
				// new AA extents
				glm::vec3 newExtent = glm::abs(extentX) + glm::abs(extentY) + glm::abs(extentZ);

				min = glm::vec3(center) - newExtent;
				max = glm::vec3(center) + newExtent;

				// project
				pp->zvZ = -1 / std::min(min.z, max.z);
			}
			else
			{
				u32 zv = 0xFFFFFFFF;
				while (vtx != vtx_end)
				{
					zv = std::min(zv, (u32&)vtx->z);
					vtx++;
				}

				pp->zvZ = (f32&)zv;
			}
		}
		pp++;
	}

	std::stable_sort(&polys->head()[first], pp_end);
}

static bool ta_parse_vdrc(TA_context* ctx)
{
	ctx->rend_inuse.lock();
	bool rv=false;
	verify(vd_ctx == nullptr);
	vd_ctx = ctx;

	ta_parse_reset();

	bool empty_context = true;
	int op_poly_count = 0;
	int pt_poly_count = 0;
	int tr_poly_count = 0;

	PolyParam *bgpp = vd_rc.global_param_op.head();
	if (bgpp->pcw.Texture)
	{
		bgpp->texture = renderer->GetTexture(bgpp->tsp, bgpp->tcw);
		empty_context = false;
	}

	const bool perPixel = config::RendererType == RenderType::OpenGL_OIT
			|| config::RendererType == RenderType::DirectX11_OIT
			|| config::RendererType == RenderType::Vulkan_OIT;
	const bool mergeTranslucent = config::PerStripSorting || perPixel;

	TA_context *childCtx = ctx;
	int pass = 0;
	while (childCtx != nullptr)
	{
		childCtx->MarkRend();
		vd_rc.proc_start = childCtx->rend.proc_start;
		vd_rc.proc_end = childCtx->rend.proc_end;

		Ta_Dma* ta_data = (Ta_Dma *)vd_rc.proc_start;
		Ta_Dma* ta_data_end = (Ta_Dma *)vd_rc.proc_end;

		while (ta_data < ta_data_end)
			try {
				ta_data = BaseTAParser::TaCmd(ta_data, ta_data_end);
			} catch (const TAParserException& e) {
				break;
			}

		if (vd_ctx->rend.Overrun)
			break;

		bool empty_pass = vd_rc.global_param_op.used() == (pass == 0 ? 0 : (int)vd_rc.render_passes.LastPtr()->op_count)
				&& vd_rc.global_param_pt.used() == (pass == 0 ? 0 : (int)vd_rc.render_passes.LastPtr()->pt_count)
				&& vd_rc.global_param_tr.used() == (pass == 0 ? 0 : (int)vd_rc.render_passes.LastPtr()->tr_count);
		empty_context = empty_context && empty_pass;

		if (pass == 0 || !empty_pass)
		{
			RenderPass *render_pass = vd_rc.render_passes.Append();
			getRegionSettings(pass, *render_pass);
			render_pass->op_count = vd_rc.global_param_op.used();
			make_index(&vd_rc.global_param_op, op_poly_count,
					render_pass->op_count, true, &vd_rc);
			op_poly_count = render_pass->op_count;
			render_pass->mvo_count = vd_rc.global_param_mvo.used();
			render_pass->pt_count = vd_rc.global_param_pt.used();
			make_index(&vd_rc.global_param_pt, pt_poly_count,
					render_pass->pt_count, true, &vd_rc);
			pt_poly_count = render_pass->pt_count;
			render_pass->tr_count = vd_rc.global_param_tr.used();
			if (render_pass->autosort && config::PerStripSorting && !perPixel)
				sortPolyParams(&vd_rc.global_param_tr, tr_poly_count,
					render_pass->tr_count, &vd_rc);
			make_index(&vd_rc.global_param_tr, tr_poly_count,
					render_pass->tr_count, mergeTranslucent, &vd_rc);
			tr_poly_count = render_pass->tr_count;
			render_pass->mvo_tr_count = vd_rc.global_param_mvo_tr.used();
		}
		childCtx = childCtx->nextContext;
		pass++;
	}
	rv = !empty_context;

	bool overrun = vd_ctx->rend.Overrun;
	if (overrun)
		WARN_LOG(PVR, "ERROR: TA context overrun");
	else if (config::RenderResolution > 480)
	{
		fix_texture_bleeding(&vd_rc.global_param_op);
		fix_texture_bleeding(&vd_rc.global_param_pt);
		fix_texture_bleeding(&vd_rc.global_param_tr);
	}
	if (rv && !overrun)
	{
		u32 xmin, xmax, ymin, ymax;
		getRegionTileClipping(xmin, xmax, ymin, ymax);
		vd_rc.fb_X_CLIP.min = std::max(vd_rc.fb_X_CLIP.min, xmin);
		vd_rc.fb_X_CLIP.max = std::min(vd_rc.fb_X_CLIP.max, xmax + 31);
		vd_rc.fb_Y_CLIP.min = std::max(vd_rc.fb_Y_CLIP.min, ymin);
		vd_rc.fb_Y_CLIP.max = std::min(vd_rc.fb_Y_CLIP.max, ymax + 31);
	}

	vd_ctx = nullptr;
	ctx->rend_inuse.unlock();

	ctx->rend.Overrun = overrun;

	return rv && !overrun;
}

static bool ta_parse_naomi2(TA_context* ctx)
{
	ctx->rend_inuse.lock();

	for (PolyParam& pp : ctx->rend.global_param_op)
	{
		if (pp.pcw.Texture)
			pp.texture = renderer->GetTexture(pp.tsp, pp.tcw);
		if (pp.tsp1.full != (u32)-1)
			pp.texture1 = renderer->GetTexture(pp.tsp1, pp.tcw1);
	}
	for (PolyParam& pp : ctx->rend.global_param_pt)
	{
		if (pp.pcw.Texture)
			pp.texture = renderer->GetTexture(pp.tsp, pp.tcw);
		if (pp.tsp1.full != (u32)-1)
			pp.texture1 = renderer->GetTexture(pp.tsp1, pp.tcw1);
	}
	for (PolyParam& pp : ctx->rend.global_param_tr)
	{
		if (pp.pcw.Texture)
			pp.texture = renderer->GetTexture(pp.tsp, pp.tcw);
		if (pp.tsp1.full != (u32)-1)
			pp.texture1 = renderer->GetTexture(pp.tsp1, pp.tcw1);
	}

	bool overrun = ctx->rend.Overrun;
	if (overrun)
	{
		WARN_LOG(PVR, "ERROR: TA context overrun");
	}
	else
	{
		ctx->rend.newRenderPass();
		int op_count = 0;
		int pt_count = 0;
		int tr_count = 0;
		const bool perPixel = config::RendererType == RenderType::OpenGL_OIT
				|| config::RendererType == RenderType::DirectX11_OIT
				|| config::RendererType == RenderType::Vulkan_OIT;
		const bool mergeTranslucent = config::PerStripSorting || perPixel;
		for (const RenderPass& pass : ctx->rend.render_passes)
		{
			make_index(&ctx->rend.global_param_op, op_count, pass.op_count, true, &ctx->rend);
			make_index(&ctx->rend.global_param_pt, pt_count, pass.pt_count, true, &ctx->rend);
			if (pass.autosort && config::PerStripSorting && !perPixel)
				sortPolyParams(&ctx->rend.global_param_tr, tr_count, pass.tr_count, &ctx->rend);
			make_index(&ctx->rend.global_param_tr, tr_count, pass.tr_count, mergeTranslucent, &ctx->rend);
			op_count = pass.op_count;
			pt_count = pass.pt_count;
			tr_count = pass.tr_count;
		}

		u32 xmin, xmax, ymin, ymax;
		getRegionTileClipping(xmin, xmax, ymin, ymax);
		ctx->rend.fb_X_CLIP.min = std::max(ctx->rend.fb_X_CLIP.min, xmin);
		ctx->rend.fb_X_CLIP.max = std::min(ctx->rend.fb_X_CLIP.max, xmax + 31);
		ctx->rend.fb_Y_CLIP.min = std::max(ctx->rend.fb_Y_CLIP.min, ymin);
		ctx->rend.fb_Y_CLIP.max = std::min(ctx->rend.fb_Y_CLIP.max, ymax + 31);
	}
	ctx->rend_inuse.unlock();

	return !overrun;
}

bool ta_parse(TA_context *ctx)
{
	if (settings.platform.isNaomi2())
		return ta_parse_naomi2(ctx);
	else
		return ta_parse_vdrc(ctx);
}

static PolyParam *n2CurrentPP;
static ModifierVolumeParam *n2CurrentMVP;

const float identityMat[] {
	1.f, 0.f, 0.f, 0.f,
	0.f, 1.f, 0.f, 0.f,
	0.f, 0.f, 1.f, 0.f,
	0.f, 0.f, 0.f, 1.f
};

const float defaultProjMat[] {
	579.411194f,   0.f,       0.f,  0.f,
	  0.f,      -579.411194f, 0.f,  0.f,
   -320.f,      -240.f,      -1.f, -1.f,
	  0.f,         0.f,       0.f,  0.f
};

void ta_add_poly(const PolyParam& pp)
{
	verify(ta_ctx != nullptr);
	verify(vd_ctx == nullptr);
	vd_ctx = ta_ctx;
	BaseTAParser::startList(pp.pcw.ListType);

	*BaseTAParser::CurrentPPlist->Append() = pp;
	n2CurrentPP = BaseTAParser::CurrentPPlist->LastPtr();
	n2CurrentPP->first = ta_ctx->rend.verts.used();
	n2CurrentPP->count = 0;
	n2CurrentPP->tileclip = BaseTAParser::getTileClip();
	if (n2CurrentPP->mvMatrix == nullptr)
		n2CurrentPP->mvMatrix = identityMat;
	if (n2CurrentPP->normalMatrix == nullptr)
		n2CurrentPP->normalMatrix = identityMat;
	if (n2CurrentPP->projMatrix == nullptr)
		n2CurrentPP->projMatrix = defaultProjMat;
	vd_ctx = nullptr;
}

void ta_add_poly(int listType, const ModifierVolumeParam& mvp)
{
	verify(ta_ctx != nullptr);
	verify(vd_ctx == nullptr);
	vd_ctx = ta_ctx;
	BaseTAParser::startList(listType);

	switch (BaseTAParser::getCurrentList())
	{
	case ListType_Opaque_Modifier_Volume:
		*ta_ctx->rend.global_param_mvo.Append() = mvp;
		n2CurrentMVP = ta_ctx->rend.global_param_mvo.LastPtr();
		break;
	case ListType_Translucent_Modifier_Volume:
		*ta_ctx->rend.global_param_mvo_tr.Append() = mvp;
		n2CurrentMVP = ta_ctx->rend.global_param_mvo_tr.LastPtr();
		break;
	default:
		die("wrong list type");
		break;
	}
	n2CurrentMVP->first = ta_ctx->rend.modtrig.used();
	n2CurrentMVP->count = 0;
	if (n2CurrentMVP->mvMatrix == nullptr)
		n2CurrentMVP->mvMatrix = identityMat;
	if (n2CurrentMVP->projMatrix == nullptr)
		n2CurrentMVP->projMatrix = defaultProjMat;
	vd_ctx = nullptr;
}

void ta_add_vertex(const Vertex& vtx)
{
	*ta_ctx->rend.verts.Append() = vtx;
	n2CurrentPP->count++;
}

void ta_add_triangle(const ModTriangle& tri)
{
	*ta_ctx->rend.modtrig.Append() = tri;
	n2CurrentMVP->count++;
}

float *ta_add_matrix(const float *matrix)
{
	N2Matrix *n2mat = ta_ctx->rend.matrices.Append();
	memcpy(n2mat->mat, matrix, sizeof(N2Matrix::mat));
	return n2mat->mat;
}

N2LightModel *ta_add_light(const N2LightModel& light)
{
	*ta_ctx->rend.lightModels.Append() = light;
	return ta_ctx->rend.lightModels.LastPtr();
}

u32 ta_add_ta_data(u32 *data, u32 size)
{
	verify(vd_ctx == nullptr);
	vd_ctx = ta_ctx;
	BaseTAParser::fetchTextures = false;

	Ta_Dma *ta_data = (Ta_Dma *)data;
	Ta_Dma *ta_data_end = (Ta_Dma *)(data + size / 4);
	try {
		ta_data = BaseTAParser::TaCmd(ta_data, ta_data_end);
	} catch (const FlycastException& e) {
		vd_ctx = nullptr;
		BaseTAParser::fetchTextures = true;
		throw;
	}

	vd_ctx = nullptr;
	BaseTAParser::fetchTextures = true;

	return (u8 *)ta_data - (u8 *)data;
}

u32 ta_get_tileclip() {
	return BaseTAParser::getTileClip();
}

void ta_set_tileclip(u32 tileclip) {
	BaseTAParser::setTileClip(tileclip);
}

u32 ta_get_list_type() {
	return BaseTAParser::getCurrentList();
}

void ta_set_list_type(u32 listType)
{
	verify(vd_ctx == nullptr);
	vd_ctx = ta_ctx;
	BaseTAParser::endList();
	if (listType != ListType_None)
		BaseTAParser::startList(listType);
	vd_ctx = nullptr;
}

void ta_parse_reset()
{
	using TAParser = TAParserTempl<>;
	using TAParserDX = TAParserTempl<2, 1, 0, 3>;

	if (isDirectX(config::RendererType))
		TAParserDX::reset();
	else
		TAParser::reset();
}

//decode a vertex in the native pvr format
//used for bg poly
template<int Red, int Green, int Blue, int Alpha>
void decode_pvr_vertex(u32 base, u32 ptr, Vertex* cv)
{
	//ISP
	//TSP
	//TCW
	ISP_TSP isp;
	isp.full = pvr_read32p<u32>(base);

	//XYZ
	//UV
	//Base Col
	//Offset Col

	//XYZ are _always_ there :)
	cv->x = pvr_read32p<float>(ptr);
	ptr += 4;
	cv->y = pvr_read32p<float>(ptr);
	ptr += 4;
	cv->z = pvr_read32p<float>(ptr);
	ptr += 4;

	if (isp.Texture)
	{	//Do texture , if any
		if (isp.UV_16b)
		{
			u32 uv = pvr_read32p<u32>(ptr);
			cv->u = f16((u16)uv);
			cv->v = f16((u16)(uv >> 16));
			ptr+=4;
		}
		else
		{
			cv->u = pvr_read32p<float>(ptr);
			ptr += 4;
			cv->v = pvr_read32p<float>(ptr);
			ptr += 4;
		}
	}

	//Color
	u32 col = pvr_read32p<u32>(ptr);
	ptr += 4;
	vert_packed_color_(cv->col, col);
	if (isp.Offset)
	{
		//Intensity color (can be missing too ;p)
		u32 col = pvr_read32p<u32>(ptr);
		ptr += 4;
		vert_packed_color_(cv->spc, col);
	}
}

static u8 float_to_satu8_math(float val)
{
	return (u8)(std::min(1.f, std::max(0.f, val)) * 255.f);
}

static void vtxdec_init()
{
	/*
		0x3b80 ~ 0x3f80 -> actual useful range. Rest is clamping to 0 or 255 ~
	*/

	for (u32 i = 0; i < ARRAY_SIZE(f32_su8_tbl); i++)
	{
		u32 fr = i << 16;
		
		f32_su8_tbl[i] = float_to_satu8_math((f32&)fr);
	}
}
static OnLoad ol_vtxdec(&vtxdec_init);

void FillBGP(TA_context* ctx)
{
	//Render pre-code
	//--BG poly
	u32 param_base=PARAM_BASE & 0xF00000;


	PolyParam* bgpp=ctx->rend.global_param_op.head();
	Vertex* cv=ctx->rend.verts.head();

	bool PSVM=FPU_SHAD_SCALE.intensity_shadow!=0; //double parameters for volumes

	//Get the strip base
	u32 strip_base=(param_base + ISP_BACKGND_T.tag_address*4) & 0x7FFFFF;	//this is *not* VRAM_MASK on purpose.It fixes naomi bios and quite a few naomi games
	//i have *no* idea why that happens, they manage to set the render target over there as well
	//and that area is *not* written by the games (they instead write the params on 000000 instead of 800000)
	//could be a h/w bug ? param_base is 400000 and tag is 100000*4
	//Calculate the vertex size
	//Update: Looks like I was handling the bank interleave wrong for 16 megs ram, could that be it?

	u32 strip_vs=3 + ISP_BACKGND_T.skip;
	u32 strip_vert_num=ISP_BACKGND_T.tag_offset;

	if (PSVM && ISP_BACKGND_T.shadow)
	{
		strip_vs+=ISP_BACKGND_T.skip;//2x the size needed :p
	}
	strip_vs*=4;
	//Get vertex ptr
	u32 vertex_ptr=strip_vert_num*strip_vs+strip_base +3*4;
	//now , all the info is ready :p

	bgpp->isp.full = pvr_read32p<u32>(strip_base);
	bgpp->tsp.full = pvr_read32p<u32>(strip_base + 4);
	bgpp->tcw.full = pvr_read32p<u32>(strip_base + 8);
	bgpp->count = 4;
	bgpp->first = 0;
	bgpp->tileclip = 0;//disabled ! HA ~

	bgpp->isp.DepthMode=7;// -> this makes things AWFULLY slow .. sometimes
	bgpp->isp.CullMode=0;// -> so that its not culled, or somehow else hidden !
	//Set some pcw bits .. I should really get rid of pcw ..
	bgpp->pcw.UV_16bit=bgpp->isp.UV_16b;
	bgpp->pcw.Gouraud=bgpp->isp.Gouraud;
	bgpp->pcw.Offset=bgpp->isp.Offset;
	bgpp->pcw.Texture = bgpp->isp.Texture;
	bgpp->pcw.Shadow = ISP_BACKGND_T.shadow;

	float scale_x= (SCALER_CTL.hscale) ? 2.f:1.f;	//if AA hack the hacked pos value hacks
	for (int i=0;i<3;i++)
	{
		if (isDirectX(config::RendererType))
			decode_pvr_vertex<2, 1, 0, 3>(strip_base,vertex_ptr,&cv[i]);
		else
			decode_pvr_vertex<0, 1, 2, 3>(strip_base,vertex_ptr,&cv[i]);
		vertex_ptr+=strip_vs;
	}

	f32 bg_depth = ISP_BACKGND_D.f;
	reinterpret_cast<u32&>(bg_depth) &= 0xFFFFFFF0;	// ISP_BACKGND_D has only 28 bits

	f32 min_u = std::min(cv[0].u, std::min(cv[1].u, cv[2].u));
	f32 max_u = std::max(cv[0].u, std::max(cv[1].u, cv[2].u));
	if (max_u == 0.f)
		max_u = 1.f;
	const f32 diff_u = (max_u - min_u) * 0.4f;
	max_u += diff_u;
	min_u -= diff_u;
	const f32 min_v = std::min(cv[0].v, std::min(cv[1].v, cv[2].v));
	f32 max_v = std::max(cv[0].v, std::max(cv[1].v, cv[2].v));
	if (max_v == 0.f)
		max_v = 1.f;
	cv[0].x = -256.f * scale_x;
	cv[0].y = 0.f;
	cv[0].z = bg_depth;
	cv[0].u = min_u;
	cv[0].v = min_v;

	cv[1].x = 896.f * scale_x;
	cv[1].y = 0.f;
	cv[1].z = bg_depth;
	cv[1].u = max_u;
	cv[1].v = min_v;

	cv[2].x = -256.f * scale_x;
	cv[2].y = 480.f;
	cv[2].z = bg_depth;
	cv[2].u = min_u;
	cv[2].v = max_v;

	cv[3] = cv[2];
	cv[3].x = 896.f * scale_x;
	cv[3].y = 480.f;
	cv[3].u = max_u;
	cv[3].v = max_v;
}

static void getRegionTileAddrAndSize(u32& address, u32& size)
{
	address = REGION_BASE;
	const bool type1_tile = ((FPU_PARAM_CFG >> 21) & 1) == 0;
	size = (type1_tile ? 5 : 6) * 4;
	bool empty_first_region = true;
	for (int i = type1_tile ? 4 : 5; i > 0; i--)
		if ((pvr_read32p<u32>(address + i * 4) & 0x80000000) == 0)
		{
			empty_first_region = false;
			break;
		}
	if (empty_first_region)
		address += size;
	RegionArrayTile tile;
	tile.full = pvr_read32p<u32>(address);
	if (tile.PreSort)
		// Windows CE weirdness
		size = 6 * 4;
}

static void getRegionTileClipping(u32& xmin, u32& xmax, u32& ymin, u32& ymax)
{
	xmin = 20;
	xmax = 0;
	ymin = 15;
	ymax = 0;

	u32 addr;
	u32 tile_size;
	getRegionTileAddrAndSize(addr, tile_size);

	RegionArrayTile tile;
	do {
		tile.full = pvr_read32p<u32>(addr);
		xmin = std::min(xmin, tile.X);
		xmax = std::max(xmax, tile.X);
		ymin = std::min(ymin, tile.Y);
		ymax = std::max(ymax, tile.Y);
		addr += tile_size;
	} while (!tile.LastRegion);

	xmin *= 32;
	xmax *= 32;
	ymin *= 32;
	ymax *= 32;
}

int getTAContextAddresses(u32 *addresses)
{
	u32 addr;
	u32 tile_size;
	getRegionTileAddrAndSize(addr, tile_size);

	RegionArrayTile tile;
	tile.full = pvr_read32p<u32>(addr);
	u32 x = tile.X;
	u32 y = tile.Y;
	u32 count = 0;
	do {
		tile.full = pvr_read32p<u32>(addr);
		if (tile.X != x || tile.Y != y)
			break;
		// Try the opaque pointer
		u32 opbAddr = pvr_read32p<u32>(addr + 4);
		if (opbAddr & 0x80000000)
		{
			// Try the translucent pointer
			opbAddr = pvr_read32p<u32>(addr + 12);
			if (opbAddr & 0x80000000)
			{
				// Try the punch-through pointer
				if (tile_size >= 24)
					opbAddr = pvr_read32p<u32>(addr + 20);
				if (opbAddr & 0x80000000)
				{
					INFO_LOG(PVR, "Can't find any non-null OPB for pass %d", count);
					break;
				}
			}
		}
		addresses[count++] = pvr_read32p<u32>(opbAddr);
		addr += tile_size;
	} while (!tile.LastRegion && count < MAX_PASSES);

	return count;
}

static void getRegionSettings(int passNumber, RenderPass& pass)
{
	u32 addr;
	u32 tileSize;
	getRegionTileAddrAndSize(addr, tileSize);
	addr += passNumber * tileSize;
	RegionArrayTile tile;
	tile.full = pvr_read32p<u32>(addr);

	if (((FPU_PARAM_CFG >> 21) & 1) == 0)
		// Type 1 region header type
		pass.autosort = (ISP_FEED_CFG & 1) == 0;
	else
		// Type 2
		pass.autosort = !tile.PreSort;
	pass.z_clear = !tile.NoZClear;
	pass.mv_op_tr_shared = pvr_read32p<u32>(addr + 8) == pvr_read32p<u32>(addr + 16);
}

void rend_context::newRenderPass()
{
	RenderPass pass;
	pass.op_count = global_param_op.used();
	pass.tr_count = global_param_tr.used();
	pass.pt_count = global_param_pt.used();
	pass.mvo_count = global_param_mvo.used();
	pass.mvo_tr_count = global_param_mvo_tr.used();
	getRegionSettings(render_passes.used(), pass);
	*render_passes.Append() = pass;
}
