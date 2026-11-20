// SPDX-License-Identifier: MIT

#pragma once

#include "rdp_data_structures.hpp"

namespace RDP
{
namespace detail
{
struct HiresTexRectLift
{
	// The queued setup carried the native-resolution snap.
	bool was_snapped = false;
	// The presented pass rounds the rectangle's fractional bottom edge up.
	bool rounds_up_bottom = false;
};

// Patches the queued setup of a texture rectangle whose TEXEL0 replacement is
// bound and sampled (or copied).
//
// native_resolution_tex_rect exists to keep copy-mode strips stable at
// upscale, but it also snaps rasterization to the native pixel grid, capping
// replacement detail for the texrects hi-res packs care about most (UI, text).
// Clearing the snap lets the replacement rasterize at full scale. Copy-mode
// rects qualify only when replaced: the replacement branch bypasses TMEM, so
// the fragile raw-transport strips the option protects are exactly the
// unreplaced ones, which keep the snap.
//
// Once unsnapped, a fractional bottom edge (yl & 3) clips the sub-rows of the
// rect's last native line that lie past it, which the snapped draw covered,
// leaving a gap at abutting strip seams. The setup asks the presented pass to
// round yl up to the next whole native line: that restores exactly those
// sub-rows and never adds a native line. yl itself is left alone, so the CPU
// span job range and the binning bounds stay what they were (a native line's
// upscaled rows never straddle an 8-row bin at 2x, 4x or 8x).
//
// The setup is shared by both passes, so only state the native pass ignores
// may change here (ADR-0020): the snap bit is read only where SCALING_FACTOR
// > 1, and span setup honors the round-up bit only there.
inline HiresTexRectLift lift_hires_tex_rect_snap(TriangleSetup &queued_setup, unsigned upscaling)
{
	HiresTexRectLift lift;
	lift.was_snapped = (queued_setup.flags & TRIANGLE_SETUP_DISABLE_UPSCALING_BIT) != 0;
	queued_setup.flags &= ~TRIANGLE_SETUP_DISABLE_UPSCALING_BIT;
	if (upscaling > 1 && (queued_setup.yl & 3) != 0)
	{
		queued_setup.tile |= TRIANGLE_SETUP_TILE_HIRES_ROUND_UP_YL_BIT;
		lift.rounds_up_bottom = true;
	}
	return lift;
}
}
}
