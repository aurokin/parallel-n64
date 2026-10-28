// SPDX-License-Identifier: MIT

#pragma once

#include "texture_keying.hpp"
#include "rdp_hires_compat_view.hpp"
#include <cstring>
#include <vector>

// xxHash v0.8.4, https://github.com/Cyan4973/xxHash/releases/tag/v0.8.4
// BSD-2-Clause; see xxhash.LICENSE and the notice in xxhash.h.
#define XXH_INLINE_ALL
#include "xxhash.h"

namespace RDP
{
namespace detail
{
// GLideN64 StrongCRC32 hashes host RDRAM bytes, without the N64 byte swizzle.
// Tiny rows include the preceding bytes, just like upstream's Rice-compatible
// pre-address read. Wrap every read within the supplied power-of-two memory.
inline bool pack_hires_strong_rows(const uint8_t *memory, size_t memory_size, uint32_t address,
                                  uint32_t width, uint32_t height, uint32_t size,
                                  uint32_t stride, std::vector<uint8_t> &bytes)
{
	if (!memory || !memory_size || (memory_size & (memory_size - 1)) ||
	    !width || !height || size > 3)
		return false;
	const uint64_t active = (uint64_t(width) << size) >> 1;
	const uint64_t row_bytes = std::max<uint64_t>(4, active);
	// A source view cannot require an unbounded staging allocation.
	if (row_bytes * height > memory_size)
		return false;
	const size_t start = bytes.size();
	bytes.resize(start + size_t(row_bytes * height));
	for (uint32_t y = 0; y < height; y++)
	{
		const uint32_t row = address + y * stride - uint32_t(row_bytes - active);
		for (uint32_t x = 0; x < row_bytes; x++)
			bytes[start + size_t(y) * row_bytes + x] = wrapped_read_u8(memory, memory_size, row + x);
	}
	return true;
}

inline uint32_t hires_strong_crc32(const uint8_t *bytes, size_t size)
{
	const uint64_t hash = XXH3_64bits(bytes, size);
	return uint32_t(hash) ^ uint32_t(hash >> 32);
}

// Stage the exact same source view as the Rice draw key. CI uses the live
// packed TLUT shadow, selecting the rendering tile's bank and cimax prefix.
inline bool pack_hires_strong_source(const uint8_t *rdram, size_t rdram_size,
                                    const HiresCompatView &view, TextureFormat format, TextureSize size,
                                    const uint8_t *palette, uint32_t bank,
                                    std::vector<uint8_t> &bytes, size_t &texture_bytes)
{
	bytes.clear();
	if (!pack_hires_strong_rows(rdram, rdram_size, view.address, view.width, view.height,
	                            unsigned(size), view.row_stride, bytes))
		return false;
	texture_bytes = bytes.size();
	if (format != TextureFormat::CI || size > TextureSize::Bpp8)
		return true;
	if (!palette || bank > 15)
		return false;
	// Upstream CalculateMaxCI4b scans floor(width / 2) bytes, including
	// both nibbles. Do not round an odd CI4 width up here.
	const uint32_t cimax = size == TextureSize::Bpp4 ?
	    compute_ci4_max_index(rdram, rdram_size, view.address, view.width & ~1u, view.height, view.row_stride) :
	    compute_ci8_max_index(rdram, rdram_size, view.address, view.width, view.height, view.row_stride);
	return pack_hires_strong_rows(palette, 512, size == TextureSize::Bpp4 ? bank * 32 : 0,
	                              cimax + 1, 1, 2, size == TextureSize::Bpp4 ? 32 : 512, bytes);
}

// Memoize with the existing Rice source identity, retaining the packed bytes
// too: Rice omits bytes and can collide while the strong-hash input changes.
// One memo per rendering tile bounds retention, including negative lookups.
struct HiresStrongHashMemo
{
	uint64_t rice_checksum64 = 0;
	uint64_t checksum64 = 0;
	size_t texture_bytes = 0;
	std::vector<uint8_t> source;
	uint64_t computations = 0;
	uint64_t hits = 0;

	uint64_t resolve(uint64_t rice, const std::vector<uint8_t> &bytes, size_t texture_size)
	{
		if (!source.empty() && rice_checksum64 == rice && texture_bytes == texture_size && source == bytes)
		{
			hits++;
			return checksum64;
		}
		rice_checksum64 = rice;
		texture_bytes = texture_size;
		source = bytes;
		checksum64 = hires_strong_crc32(bytes.data(), texture_size);
		if (bytes.size() > texture_size)
			checksum64 |= uint64_t(hires_strong_crc32(bytes.data() + texture_size, bytes.size() - texture_size)) << 32;
		computations++;
		return checksum64;
	}
};
}
}
