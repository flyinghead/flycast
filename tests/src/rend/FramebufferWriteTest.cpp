#include "gtest/gtest.h"
#include "types.h"
#include "emulator.h"
#include "hw/mem/addrspace.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/pvr/ta_ctx.h"
#include "rend/TexCache.h"

#include <algorithm>
#include <cstring>
#include <random>
#include <vector>

// WriteFramebuffer() packs a rendered RGBA frame into VRAM in the format
// selected by FB_W_CTRL. On arm64 the 16-bit and 32-bit modes go through
// NEON writers; everywhere else through the scalar per-pixel writer. The
// NEON writers must produce the same VRAM bytes as the scalar one.
//
// The reference below is the scalar algorithm for the 32-bit modes (0888
// KRGB and 8888 ARGB), written independently of TexCache.cpp: the same
// per-pixel packing and the same walk over padding and clip, one
// pvr_write32p() per pixel. Each case runs the reference and
// WriteFramebuffer() from the same VRAM fill and compares all of VRAM, so a
// write outside the frame (a wrong stride, or a vector store running past
// the clip, the bank boundary or the end of VRAM) fails too.
class FramebufferWriteTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!addrspace::reserve())
			die("addrspace::reserve failed");
		// emu.init() maps VRAM; the test only needs that. dc_reset() would
		// reset every module, including TMU, which TimerTest leaves
		// terminated (its scheduler ids are -1) and which then indexes the
		// scheduler list out of bounds.
		emu.init();
	}

	template<int Red, int Green, int Blue, int Alpha>
	static void referenceWrite32(u32 width, u32 height, const u8 *data, u32 dstAddr,
			FB_W_CTRL_type fb_w_ctrl, u32 linestride, const Rect& clip)
	{
		const u32 bpp = 4;
		u32 padding = linestride;
		if (padding > width * bpp)
			padding -= width * bpp;
		else
			padding = 0;

		const u8 *p = data + 4 * clip.origin.y * width;
		u32 addr = dstAddr + bpp * clip.origin.y * (width + padding / bpp);
		const u32 clipWidth = std::min<u32>(width, clip.origin.x + clip.size.x);
		height = std::min<u32>(height, clip.origin.y + clip.size.y);

		for (u32 l = clip.origin.y; l < height; l++)
		{
			p += 4 * clip.origin.x;
			addr += bpp * clip.origin.x;
			for (u32 c = clip.origin.x; c < clipWidth; c++)
			{
				u32 pixel = (p[Red] << 16) | (p[Green] << 8) | p[Blue];
				if (fb_w_ctrl.fb_packmode == 5)
					pixel |= (u32)fb_w_ctrl.fb_kval << 24;
				else
					pixel |= (u32)p[Alpha] << 24;
				pvr_write32p<u32, true>(addr, pixel);
				addr += 4;
				p += 4;
			}
			addr += padding + (width - clipWidth) * bpp;
			p += (width - clipWidth) * 4;
		}
	}

	template<int Red, int Green, int Blue, int Alpha>
	static void compareWithReference(u32 width, u32 height, const u8 *data, u32 dstAddr,
			FB_W_CTRL_type fb_w_ctrl, u32 linestride, const Rect& clip)
	{
		const size_t vramSize = VRAM_SIZE;
		std::vector<u8> reference(vramSize);
		std::memset(&vram[0], 0xA5, vramSize);
		referenceWrite32<Red, Green, Blue, Alpha>(width, height, data, dstAddr, fb_w_ctrl, linestride, clip);
		std::memcpy(reference.data(), &vram[0], vramSize);

		std::memset(&vram[0], 0xA5, vramSize);
		WriteFramebuffer<Red, Green, Blue, Alpha>(width, height, data, dstAddr, fb_w_ctrl, linestride, clip);

		const u8 *actual = &vram[0];
		size_t firstDiff = vramSize;
		if (std::memcmp(actual, reference.data(), vramSize) != 0)
			firstDiff = std::mismatch(actual, actual + vramSize, reference.data()).first - actual;
		EXPECT_EQ(firstDiff, vramSize)
			<< "mode " << fb_w_ctrl.fb_packmode << " order " << Red << Green << Blue << Alpha
			<< " " << width << "x" << height << " dst 0x" << std::hex << dstAddr
			<< " stride " << std::dec << linestride
			<< " clip (" << clip.origin.x << "," << clip.origin.y << " " << clip.size.x << "x" << clip.size.y << ")"
			<< ": VRAM differs at offset 0x" << std::hex << firstDiff;
	}

	// Every combination of the parameters that change the address walk, for
	// both pixel orders, for one packing mode.
	void sweep(u32 packmode)
	{
		std::mt19937 rng(0x12345678u + packmode);
		FB_W_CTRL_type ctrl{};
		ctrl.fb_packmode = packmode;
		ctrl.fb_kval = 0x5C;

		// Widths around and away from the 8-pixel vector width.
		const std::pair<u32, u32> sizes[] = {
			{ 1, 2 }, { 7, 3 }, { 8, 2 }, { 9, 2 }, { 13, 5 }, { 16, 4 }, { 33, 3 }, { 320, 8 }, { 640, 4 },
		};
		// Including non-multiples of 4 (padding / bpp truncates them).
		const u32 extraStrides[] = { 0, 4, 12, 13, 100 };
		for (const auto& [width, height] : sizes)
		{
			std::vector<u8> frame(width * height * 4);
			for (u8& b : frame)
				b = (u8)rng();

			const u32 bankBit = VRAM_32_BANK_BIT;
			const Rect clips[] = {
				Rect(glm::ivec2(0, 0), glm::ivec2(width, height)),
				Rect(glm::ivec2(0, 0), glm::ivec2(width > 3 ? width - 3 : 1, height > 1 ? height - 1 : 1)),
				Rect(glm::ivec2(width > 3 ? 3 : 0, height > 1 ? 1 : 0), glm::ivec2(width, height)),
				Rect(glm::ivec2(width > 6 ? 5 : 0, 0), glm::ivec2(2, height)),
			};
			for (u32 extra : extraStrides)
			{
				const u32 linestride = extra == 0 ? 0 : width * 4 + extra;
				// Every write must land inside VRAM: the writers don't mask the
				// address, the PVR registers that feed them do. The last start
				// puts the padded frame's final word at the end of VRAM.
				const u32 lineBytes = width * 4 + extra;
				// Frame start, mid VRAM, straddling the 32-bit bank boundary, and
				// ending at the last word of VRAM.
				const u32 addrs[] = {
					0, 0x1000, bankBit - 4 * 3, bankBit, (u32)VRAM_SIZE - height * lineBytes,
				};
				for (u32 addr : addrs)
					for (const Rect& clip : clips)
					{
						compareWithReference<0, 1, 2, 3>(width, height, frame.data(), addr, ctrl, linestride, clip);
						compareWithReference<2, 1, 0, 3>(width, height, frame.data(), addr, ctrl, linestride, clip);
					}
			}
		}
	}
};

TEST_F(FramebufferWriteTest, Mode0888MatchesReference)
{
	sweep(5);
}

TEST_F(FramebufferWriteTest, Mode8888MatchesReference)
{
	sweep(6);
}
