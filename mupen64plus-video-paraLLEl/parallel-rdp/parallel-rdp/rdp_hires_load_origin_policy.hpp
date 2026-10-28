// SPDX-License-Identifier: MIT

#pragma once

#include "rdp_common.hpp"
#include "rdp_hires_compat_view.hpp"
#include <algorithm>

namespace RDP
{
namespace detail
{
struct HiresLoadOrigin
{
	TextureFormat format = TextureFormat::RGBA;
	TextureSize size = TextureSize::Bpp4;
	uint32_t offset = 0;
	uint32_t stride = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t address = 0;
	HiresLoadTileSource source;
};

// Palette banks share index bytes, but a different TMEM window or native
// extent cannot establish a missing tile's texture-image origin.
inline HiresLoadOrigin find_hires_load_origin_info(const HiresLoadOrigin &target,
                                                 const HiresLoadOrigin *peers, unsigned count)
{
	if (target.address != 0 || target.width == 0 || target.height == 0)
		return target;
	HiresLoadOrigin resolved = target;
	for (unsigned i = 0; i < count; i++)
	{
		const auto &peer = peers[i];
		if (peer.address == 0 || peer.offset != target.offset || peer.stride != target.stride ||
		    peer.format != target.format || peer.size != target.size ||
		    peer.width != target.width || peer.height != target.height)
			continue;
		if (resolved.address != 0 &&
		    (resolved.address != peer.address || !(resolved.source == peer.source)))
			return target;
		resolved = peer;
	}
	return resolved;
}

inline uint32_t find_hires_load_origin(const HiresLoadOrigin &target,
                                      const HiresLoadOrigin *peers, unsigned count)
{
	return find_hires_load_origin_info(target, peers, count).address;
}

inline bool hires_tmem_spans_overlap(uint32_t a0, uint32_t an, uint32_t b0, uint32_t bn,
                                    uint32_t tmem_bytes)
{
	const uint32_t a1 = std::min<uint32_t>(a0 + an, tmem_bytes);
	const uint32_t b1 = std::min<uint32_t>(b0 + bn, tmem_bytes);
	const uint32_t a_wrap = a0 + an > tmem_bytes ? a0 + an - tmem_bytes : 0;
	const uint32_t b_wrap = b0 + bn > tmem_bytes ? b0 + bn - tmem_bytes : 0;
	return (a0 < b1 && b0 < a1) ||
	       (a_wrap != 0 && b0 < a_wrap) || (b_wrap != 0 && a0 < b_wrap) ||
	       (a_wrap != 0 && b_wrap != 0);
}
}
}
