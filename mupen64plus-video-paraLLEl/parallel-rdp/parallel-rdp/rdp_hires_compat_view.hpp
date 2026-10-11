#pragma once

#include "rdp_common.hpp"
#include "rdp_hires_texture_layout.hpp"
#include <algorithm>

namespace RDP
{
namespace detail
{
// LoadTile's RDRAM rows and crop are independent of padded TMEM rows.
// Keep the loading format so a later reinterpretation cannot reuse these units.
struct HiresLoadTileSource
{
	uint32_t address = 0;
	uint32_t image_width = 0;
	uint32_t row_stride = 0;
	uint32_t tmem_stride = 0;
	TextureFormat format = TextureFormat::RGBA;
	TextureSize size = TextureSize::Bpp4;
	bool valid = false;

	bool operator==(const HiresLoadTileSource &other) const
	{
		return valid == other.valid && address == other.address && image_width == other.image_width &&
		       row_stride == other.row_stride && tmem_stride == other.tmem_stride &&
		       format == other.format && size == other.size;
	}
};

// A split upload restores the full TileSize after its last chunk, whose crop
// cannot describe that full view. Preserve the legacy path for those loads.
inline bool hires_load_tile_source_supported(bool is_load_tile, TextureSize image_size,
                                            TextureSize tile_size, uint32_t tmem_offset)
{
	return is_load_tile && image_size == tile_size && tmem_offset == 0;
}

struct HiresCompatView
{
	uint32_t address = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t row_stride = 0;
	bool source_layout = false;
};

inline bool hires_source_view_needs_rekey(const HiresCompatView &view, uint32_t key_width,
                                         uint32_t key_height)
{
	return view.source_layout && (view.width != key_width || view.height != key_height);
}

// Only draw-time source keys depend on retaining this qualified layout.
// Pending upload/LoadBlock bindings keep their existing reshape policy.
inline bool hires_source_binding_must_retire(bool source_binding, bool view_valid,
                                            const HiresCompatView &view)
{
	return source_binding && (!view_valid || !view.source_layout);
}

inline bool compute_hires_compat_view(const TileMeta &meta, const TileSize &size, uint32_t address,
                                    const HiresLoadTileSource &source, HiresCompatView &view)
{
	view = {};
	const uint32_t sl = size.slo >> 2, sh = size.shi >> 2;
	const uint32_t tl = size.tlo >> 2, th = size.thi >> 2;
	if (sh < sl || th < tl)
		return false;
	view.width = ((sh - sl) + 1u) & 0xfffu;
	view.height = th - tl + 1u;
	if (meta.mask_s != 0)
		view.width = std::min(view.width, 1u << meta.mask_s);
	if (meta.mask_t != 0)
		view.height = std::min(view.height, 1u << meta.mask_t);
	view.address = address;
	view.row_stride = meta.stride;
	if (meta.size == TextureSize::Bpp32)
		view.row_stride <<= 1;
	if (source.valid && source.format == meta.fmt && source.size == meta.size &&
	    source.tmem_stride == meta.stride && source.image_width != 0 && source.row_stride != 0)
	{
		view.width = std::min(view.width, source.image_width);
		view.address = source.address;
		view.row_stride = source.row_stride;
		view.source_layout = true;
	}
	if (view.row_stride == 0)
		view.row_stride = compute_hires_texture_row_bytes(view.width, meta.size);
	return view.width != 0 && view.height != 0 && view.row_stride != 0;
}

// Alignment bytes are not additional sampled texels. Source-backed windows
// still reject real extra columns/rows; legacy LoadBlock views keep their guard.
inline bool hires_compat_view_fits_key(const HiresCompatView &view, uint32_t key_width,
                                     uint32_t key_height, TextureSize key_size, uint32_t tmem_stride)
{
	if (key_width == 0 || key_height == 0)
		return true;
	if (view.source_layout)
		return view.width <= key_width && view.height <= key_height;
	return uint64_t(view.height) * tmem_stride <=
	       ((uint64_t(key_width) << unsigned(key_size)) >> 1) * key_height;
}
}
}
