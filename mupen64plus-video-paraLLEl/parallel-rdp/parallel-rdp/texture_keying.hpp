#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include "rdp_common.hpp"

namespace RDP
{
inline uint16_t formatsize_key(TextureFormat fmt, TextureSize siz)
{
	return uint16_t(uint8_t(fmt)) | (uint16_t(uint8_t(siz)) << 8);
}

inline uint8_t wrapped_read_u8(const uint8_t *rdram, size_t rdram_size, uint32_t addr)
{
	return rdram[addr & uint32_t(rdram_size - 1)];
}

inline uint32_t wrapped_read_u32(const uint8_t *rdram, size_t rdram_size, uint32_t addr)
{
	uint32_t v = 0;
	v |= uint32_t(wrapped_read_u8(rdram, rdram_size, addr + 0)) << 0;
	v |= uint32_t(wrapped_read_u8(rdram, rdram_size, addr + 1)) << 8;
	v |= uint32_t(wrapped_read_u8(rdram, rdram_size, addr + 2)) << 16;
	v |= uint32_t(wrapped_read_u8(rdram, rdram_size, addr + 3)) << 24;
	return v;
}

inline uint32_t rice_crc32_wrapped(const uint8_t *rdram, size_t rdram_size, uint32_t base_addr,
                                   uint32_t width, uint32_t height, uint32_t size, uint32_t row_stride)
{
	// Compatibility checksum specified by the pack format; this is not CRC-32.
	if (!rdram || !rdram_size || !width || !height)
		return 0;
	const uint32_t row_bytes = (width << size) / 2u;
	if (row_bytes < 4u)
		return 0;

	uint32_t checksum = 0;
	for (uint32_t row = 0; row < height; ++row)
	{
		const uint32_t address = base_addr + row * row_stride;
		uint32_t final_sample = 0;
		for (uint32_t remaining = row_bytes; remaining >= 4u; remaining -= 4u)
		{
			const uint32_t offset = remaining - 4u;
			final_sample = wrapped_read_u32(rdram, rdram_size, address + offset) ^ offset;
			checksum = ((checksum << 4u) | (checksum >> 28u)) + final_sample;
		}
		checksum += final_sample ^ (height - row - 1u);
	}
	return checksum;
}

inline uint32_t compute_ci8_max_index(const uint8_t *rdram, size_t rdram_size, uint32_t base_addr,
                                      uint32_t width, uint32_t height, uint32_t row_stride)
{
	if (!rdram || !rdram_size)
		return 0;
	uint32_t largest = 0;
	for (uint32_t row = 0; row < height; ++row)
		for (uint32_t column = 0; column < width; ++column)
			largest = std::max(largest, uint32_t(wrapped_read_u8(
				rdram, rdram_size, base_addr + row * row_stride + column)));
	return largest;
}

inline uint32_t compute_ci4_max_index(const uint8_t *rdram, size_t rdram_size, uint32_t base_addr,
                                      uint32_t width, uint32_t height, uint32_t row_stride)
{
	if (!rdram || !rdram_size)
		return 0;
	// Rice includes both nibbles of the last byte for an odd texel count.
	const uint32_t byte_count = (width + 1u) / 2u;
	uint32_t largest = 0;
	for (uint32_t row = 0; row < height; ++row)
		for (uint32_t column = 0; column < byte_count; ++column)
		{
			const uint32_t pair = wrapped_read_u8(
				rdram, rdram_size, base_addr + row * row_stride + column);
			largest = std::max(largest, std::max(pair >> 4u, pair & 15u));
		}
	return largest;
}
}
