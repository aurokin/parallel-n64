// SPDX-License-Identifier: MIT

#pragma once

// Which rasterization passes sample hi-res replacements (ADR-0020).
//
// The native-resolution pass writes color and depth to RDRAM, where the CPU can
// read them, so it must render with the game's own textures whenever a separate
// presented pass exists. Upscaling always has one. At 1x the renderer adds a
// presented pass at native resolution while a replacement provider is attached;
// otherwise the native pass is the presented image and replacement would reach
// RDRAM.

namespace RDP
{
namespace detail
{
// Whether framebuffers keep a presented copy (the upscaled domain) that a second
// pass renders into and VI scans out. `native_pass_replaces` is the
// PARALLEL_RDP_HIRES_NATIVE_PASS=replace comparison mode, which keeps the earlier
// single-pass behavior at 1x.
inline bool needs_presented_pass(unsigned upscaling, bool replacement_attached, bool native_pass_replaces)
{
	return upscaling > 1 || (replacement_attached && !native_pass_replaces);
}

// Whether one rasterization pass samples replacements. `presented_pass` is true
// for the pass that renders into the presented copy.
inline bool pass_samples_replacements(bool presented_pass, bool has_presented_pass, bool native_pass_replaces)
{
	return presented_pass || !has_presented_pass || native_pass_replaces;
}
}
}
