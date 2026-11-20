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
// Unsnapped at upscale, a fractional bottom edge (yl & 3) clips the upscaled
// rows of the rect's last native line that lie past it. The setup asks the
// presented pass to round yl up to the next whole native line, restoring
// exactly those rows without adding a native line, where the rect's footprint
// is that whole line:
// - the snap was lifted: the snapped draw covered the line, so lifting it
//   alone would open a gap at seams with abutting snapped strips;
// - copy mode: its bottom line is inclusive (yl |= 3) and has no coverage, the
//   y analog of span setup's copy right-edge completion, and applies with the
//   native-resolution option off too (Paper Mario's title strips are copy).
// An unsnapped 1- or 2-cycle rect keeps its edge: its partial last line is
// real geometry, which the same rect without a pack also presents and which
// abutting unsnapped strips complete.
//
// yl itself is left alone, so the CPU span job range and the binning bounds
// stay what they were (a native line's upscaled rows never straddle an 8-row
// bin at 2x, 4x or 8x).
//
// The setup is shared by both passes, so only state the native pass ignores
// may change here (ADR-0020): the snap bit is read only where SCALING_FACTOR
// > 1, and span setup honors the round-up bit only there.
inline HiresTexRectLift lift_hires_tex_rect_snap(TriangleSetup &queued_setup, unsigned upscaling)
{
	HiresTexRectLift lift;
	lift.was_snapped = (queued_setup.flags & TRIANGLE_SETUP_DISABLE_UPSCALING_BIT) != 0;
	// Texrects carry SKIP_XFRAC exactly in copy mode (build_texture_rectangle_policy).
	const bool copy = (queued_setup.flags & TRIANGLE_SETUP_SKIP_XFRAC_BIT) != 0;
	queued_setup.flags &= ~TRIANGLE_SETUP_DISABLE_UPSCALING_BIT;
	if (upscaling > 1 && (queued_setup.yl & 3) != 0 && (lift.was_snapped || copy))
	{
		queued_setup.tile |= TRIANGLE_SETUP_TILE_HIRES_ROUND_UP_YL_BIT;
		lift.rounds_up_bottom = true;
	}
	return lift;
}
}
}
