#include "hw/pvr/ta.h"
#include "hw/pvr/ta_ctx.h"
#include "hw/pvr/Renderer_if.h"

struct norend : Renderer
{
	bool Init() override {
		return true;
	}
	void Term() override { }

	void Process(TA_context* ctx) override {
		ta_parse(ctx, true);
	}

	bool Render() override {
		return !pvrrc.isRTT;
	}
	void RenderFramebuffer(const FramebufferInfo& info) override { }
};

Renderer *rend_norend() {
	return new norend();
}
