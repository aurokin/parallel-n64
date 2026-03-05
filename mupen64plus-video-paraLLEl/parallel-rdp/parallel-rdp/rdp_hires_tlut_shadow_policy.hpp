#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "texture_keying.hpp"

namespace RDP
{
namespace detail
{
struct HiresTlutShadowUpdateResult
{
	bool updated = false;
	uint32_t shadow_offset = 0;
	uint32_t copied_bytes = 0;
};

inline constexpr uint32_t hires_tlut_shadow_base_offset_bytes()
{
	return 0x800u;
}

inline HiresTlutShadowUpdateResult update_hires_tlut_shadow(
		uint8_t *tlut_shadow,
		size_t tlut_shadow_size,
		bool &tlut_shadow_valid,
		const uint8_t *cpu_rdram,
		size_t rdram_size,
		uint32_t src_base_addr,
		uint32_t bytes,
		uint32_t tmem_offset_bytes)
{
	HiresTlutShadowUpdateResult result = {};
	if (!tlut_shadow || tlut_shadow_size == 0 || !cpu_rdram || rdram_size == 0 || bytes == 0)
		return result;

	const uint64_t shadow_base = uint64_t(hires_tlut_shadow_base_offset_bytes());
	const uint64_t shadow_end = shadow_base + uint64_t(tlut_shadow_size);
	const uint64_t upload_begin = uint64_t(tmem_offset_bytes);
	const uint64_t upload_end = upload_begin + uint64_t(bytes);

	const uint64_t clipped_begin = std::max(upload_begin, shadow_base);
	const uint64_t clipped_end = std::min(upload_end, shadow_end);
	if (clipped_end <= clipped_begin)
		return result;

	if (!tlut_shadow_valid)
		std::memset(tlut_shadow, 0, tlut_shadow_size);

	const uint32_t dst_offset = uint32_t(clipped_begin - shadow_base);
	const uint32_t src_offset = uint32_t(clipped_begin - upload_begin);
	const uint32_t copied_bytes = uint32_t(clipped_end - clipped_begin);

	for (uint32_t i = 0; i < copied_bytes; i++)
		tlut_shadow[dst_offset + i] = wrapped_read_u8(cpu_rdram, rdram_size, src_base_addr + src_offset + i);

	tlut_shadow_valid = true;
	result.updated = true;
	result.shadow_offset = dst_offset;
	result.copied_bytes = copied_bytes;
	return result;
}
}
}
