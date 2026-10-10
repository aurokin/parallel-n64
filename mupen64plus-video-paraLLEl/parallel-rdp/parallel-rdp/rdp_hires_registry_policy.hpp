#pragma once

#include <cstddef>
#include <cstdint>

namespace RDP
{
namespace detail
{
// Keep admitted descriptors stable: queued draws may still reference every slot.
// Budget pressure falls back instead of evicting and overwriting those bindings.
inline uint32_t allocate_hires_registry_descriptor(size_t &resident_bytes,
                                                 size_t incoming_bytes, size_t budget_bytes,
                                                 uint32_t &next_descriptor, uint32_t capacity)
{
	if (budget_bytes != 0 &&
	    (incoming_bytes > budget_bytes || resident_bytes > budget_bytes - incoming_bytes))
		return 0xffffffffu;
	if (next_descriptor >= capacity)
		return 0xffffffffu;

	resident_bytes += incoming_bytes;
	return next_descriptor++;
}
}
}
