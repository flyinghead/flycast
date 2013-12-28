
#include "hw/pvr/Renderer_if.h"
#include "oslib/oslib.h"

void rend_set_fb_scale(float x,float y) { }
void rend_text_invl(vram_block* bl) { }

struct norend : Renderer
{
	bool Init()
	{
		return true;
	}

	void Resize(int w, int h) { }
	void Term() { }

	bool Render()
	{
		return !pvrrc.isRTT;
	}

	void Present() { }
};


Renderer* rend_norend() { return new norend(); }
