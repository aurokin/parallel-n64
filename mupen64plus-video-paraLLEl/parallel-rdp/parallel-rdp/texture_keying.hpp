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
	if (!rdram || rdram_size == 0 || width == 0 || height == 0)
		return 0;

	const uint32_t bytes_per_line = (width << size) >> 1;
	if (bytes_per_line < 4)
		return 0;

	uint32_t crc = 0;
	uint32_t line = 0;
	for (int y = int(height) - 1; y >= 0; y--, line++)
	{
		uint32_t esi = 0;
		uint32_t row_addr = (base_addr + line * row_stride) & uint32_t(rdram_size - 1);
		for (int x = int(bytes_per_line) - 4; x >= 0; x -= 4)
		{
			esi = wrapped_read_u32(rdram, rdram_size, row_addr + uint32_t(x));
			esi ^= uint32_t(x);
			crc = (crc << 4) + ((crc >> 28) & 15);
			crc += esi;
		}

		esi ^= uint32_t(y);
		crc += esi;
	}

	return crc;
}

inline uint32_t compute_ci8_max_index(const uint8_t *rdram, size_t rdram_size, uint32_t base_addr,
                                      uint32_t width, uint32_t height, uint32_t row_stride)
{
	uint32_t cimax = 0;
	for (uint32_t y = 0; y < height; y++)
	{
		const uint32_t row_addr = (base_addr + y * row_stride) & uint32_t(rdram_size - 1);
		for (uint32_t x = 0; x < width; x++)
		{
			const uint32_t idx = wrapped_read_u8(rdram, rdram_size, row_addr + x);
			cimax = std::max(cimax, idx);
			if (cimax == 0xff)
				return cimax;
		}
	}
	return cimax;
}

inline uint32_t compute_ci4_max_index(const uint8_t *rdram, size_t rdram_size, uint32_t base_addr,
                                      uint32_t width, uint32_t height, uint32_t row_stride)
{
	uint32_t cimax = 0;
	const uint32_t row_bytes = (width + 1) >> 1;
	for (uint32_t y = 0; y < height; y++)
	{
		const uint32_t row_addr = (base_addr + y * row_stride) & uint32_t(rdram_size - 1);
		for (uint32_t x = 0; x < row_bytes; x++)
		{
			const uint8_t v = wrapped_read_u8(rdram, rdram_size, row_addr + x);
			cimax = std::max<uint32_t>(cimax, (v >> 4) & 0xf);
			cimax = std::max<uint32_t>(cimax, v & 0xf);
			if (cimax == 0xf)
				return cimax;
		}
	}
	return cimax;
}
}
