#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_tex_rect_policy.hpp"
#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_tex_rect_policy.hpp"

#include <cstdlib>
#include <iostream>

using namespace RDP;
using namespace RDP::detail;

namespace
{
void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

// The queued setup of one texture rectangle as the command processor builds it.
TriangleSetup queued_tex_rect(uint32_t yl, bool copy, bool native_resolution_tex_rect)
{
	TextureRectanglePolicyInput in = {};
	in.rect.xh = 40 << 2;
	in.rect.xl = 8 << 2;
	in.rect.yh = 16 << 2;
	in.rect.yl = yl;
	in.raster_flags = copy ? RASTERIZATION_COPY_BIT : 0;
	in.rect = apply_copy_fill_y_adjust(in.rect, in.raster_flags);
	in.word1 = 3u << 24;
	in.native_resolution_tex_rect = native_resolution_tex_rect;
	return build_texture_rectangle_policy(in).setup;
}

// Everything the native pass reads from a queued setup. The snap bit is read
// only where SCALING_FACTOR > 1 (shading.h, span_setup.comp), the round-up bit
// only by span setup at SCALING_FACTOR > 1, and the texrect mark only inside
// replacement sampling, which the native pass never runs (ADR-0020).
bool same_for_native_pass(const TriangleSetup &a, const TriangleSetup &b)
{
	const uint8_t flag_mask = uint8_t(~TRIANGLE_SETUP_DISABLE_UPSCALING_BIT);
	const uint8_t tile_mask = uint8_t(~(TRIANGLE_SETUP_TILE_HIRES_TEX_RECT_BIT |
	                                    TRIANGLE_SETUP_TILE_HIRES_ROUND_UP_YL_BIT));
	return a.xh == b.xh && a.xm == b.xm && a.xl == b.xl &&
	       a.yh == b.yh && a.ym == b.ym && a.yl == b.yl &&
	       a.dxhdy == b.dxhdy && a.dxmdy == b.dxmdy && a.dxldy == b.dxldy &&
	       (a.flags & flag_mask) == (b.flags & flag_mask) &&
	       (a.tile & tile_mask) == (b.tile & tile_mask);
}
}

int main()
{
	// A replacement bound to a texrect changes nothing the RDRAM pass reads, for
	// every bottom-edge phase, mode, snap option and upscale factor.
	for (unsigned upscaling : { 1u, 2u, 4u, 8u })
		for (bool copy : { false, true })
			for (bool snap_option : { false, true })
				for (uint32_t phase = 0; phase < 4; phase++)
				{
					const TriangleSetup without_pack = queued_tex_rect((40 << 2) | phase, copy, snap_option);
					TriangleSetup with_pack = without_pack;
					with_pack.tile |= TRIANGLE_SETUP_TILE_HIRES_TEX_RECT_BIT;
					const auto lift = lift_hires_tex_rect_snap(with_pack, upscaling);

					check(same_for_native_pass(without_pack, with_pack),
					      "lifting a texrect snap must leave the native pass's setup unchanged");
					check(with_pack.yl == without_pack.yl, "the queued bottom edge must stay the game's");
					check((with_pack.flags & TRIANGLE_SETUP_DISABLE_UPSCALING_BIT) == 0,
					      "a lifted texrect must not snap in the presented pass");
					check(lift.was_snapped == snap_option, "the lift must report the snap it cleared");

					const bool round_up = (with_pack.tile & TRIANGLE_SETUP_TILE_HIRES_ROUND_UP_YL_BIT) != 0;
					check(round_up == lift.rounds_up_bottom, "the round-up report must match the setup");
					// Copy rects always have a fractional edge (yl |= 3); an unsnapped
					// 1- or 2-cycle rect presents its own partial last line.
					check(round_up == (upscaling > 1 && (without_pack.yl & 3) != 0 && (snap_option || copy)),
					      "only an upscaled fractional bottom edge of a snapped or copy rect rounds up");
				}

	return 0;
}
