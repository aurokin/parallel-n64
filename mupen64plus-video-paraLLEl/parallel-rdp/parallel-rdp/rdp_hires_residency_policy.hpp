// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

namespace RDP
{
namespace detail
{
constexpr uint32_t HiresInvalidSlot = 0xffffffffu;
constexpr uint64_t HiresRetirementFrames = 8;

// Keep lookup identity/selector provenance while removing GPU availability.
template <typename TileState, typename TileBinding>
inline void clear_hires_evicted_binding(uint32_t slot, TileState &state, TileBinding &binding)
{
	if (state.vk_image_index == slot || binding.repl_desc_index == slot)
	{
		state.hit = false;
		state.vk_image_index = HiresInvalidSlot;
		binding = {};
	}
}

inline uint32_t hires_slot_capacity(uint64_t entry_count, uint32_t device_limit)
{
	return uint32_t(std::min(std::min(entry_count, uint64_t(65535)) + 1, uint64_t(device_limit)));
}

enum class HiresDeviceClass { Discrete, Shared, Mobile };

inline uint64_t hires_default_budget(HiresDeviceClass device_class, uint64_t heap_size,
                                    bool has_memory_budget, uint64_t heap_budget)
{
	constexpr uint64_t MiB = 1024 * 1024;
	const uint64_t available = has_memory_budget ? heap_budget : heap_size;
	if (device_class == HiresDeviceClass::Discrete)
		return std::max(512 * MiB, std::min(available / 2, 4096 * MiB));
	return std::max(256 * MiB, std::min(available / 4,
	                                  (device_class == HiresDeviceClass::Mobile ? 1024 : 2048) * MiB));
}

enum class HiresAdmission { Admitted, TooLarge, Saturated, NoSlot, LoadFailed };

struct HiresAdmissionResult
{
	HiresAdmissionResult(uint32_t slot_, HiresAdmission reason_) : slot(slot_), reason(reason_) {}
	uint32_t slot;
	HiresAdmission reason;
};

// Slot zero is the fallback. All other slots are either free or in the LRU.
// Preflight the whole eviction set so a refusal never disturbs resident images.
class HiresResidencyPolicy
{
public:
	void reset(uint32_t capacity, uint64_t budget)
	{
		slots.assign(capacity, {});
		budget_bytes = budget;
		resident_bytes = 0;
		resident_count = 0;
		head = tail = 0;
		free_head = capacity > 1 ? 1 : 0;
		for (uint32_t i = 1; i < capacity; i++)
			slots[i].next_free = i + 1 < capacity ? i + 1 : 0;
	}

	template <typename Evict>
	HiresAdmissionResult admit(uint64_t bytes, uint64_t frame, Evict evict)
	{
		if (bytes == 0 || (budget_bytes && bytes > budget_bytes))
			return { HiresInvalidSlot, HiresAdmission::TooLarge };
		uint64_t remaining = resident_bytes;
		uint32_t candidate = head;
		uint32_t evict_count = 0;
		const uint64_t limit = budget_bytes ? budget_bytes : std::numeric_limits<uint64_t>::max();
		while (remaining > limit - bytes || (!free_head && evict_count == 0))
		{
			if (!candidate || frame - slots[candidate].last_used_frame < HiresRetirementFrames)
				return { HiresInvalidSlot, remaining > limit - bytes ? HiresAdmission::Saturated : HiresAdmission::NoSlot };
			remaining -= slots[candidate].bytes;
			candidate = slots[candidate].next;
			evict_count++;
		}
		while (evict_count)
		{
			evict_count--;
			const uint32_t slot = head;
			evict(slot);
			release(slot);
		}
		const uint32_t slot = free_head;
		if (!slot)
			return { HiresInvalidSlot, HiresAdmission::NoSlot };
		free_head = slots[slot].next_free;
		slots[slot].bytes = bytes;
		slots[slot].last_used_frame = frame;
		append(slot);
		resident_bytes += bytes;
		resident_count++;
		return { slot, HiresAdmission::Admitted };
	}

	// Decode/upload runs only after admission. A failed load owns no reservation.
	template <typename Evict, typename Load>
	HiresAdmissionResult admit_and_load(uint64_t bytes, uint64_t frame, Evict evict, Load load)
	{
		auto result = admit(bytes, frame, evict);
		if (result.reason == HiresAdmission::Admitted && !load(result.slot))
		{
			release(result.slot);
			return { HiresInvalidSlot, HiresAdmission::LoadFailed };
		}
		return result;
	}

	void touch(uint32_t slot, uint64_t frame)
	{
		assert(slot > 0 && slot < slots.size() && slots[slot].bytes);
		if (slots[slot].last_used_frame == frame)
			return;
		slots[slot].last_used_frame = frame;
		unlink(slot);
		append(slot);
	}

	void release(uint32_t slot)
	{
		assert(slot > 0 && slot < slots.size() && slots[slot].bytes);
		resident_bytes -= slots[slot].bytes;
		resident_count--;
		unlink(slot);
		slots[slot] = {};
		slots[slot].next_free = free_head;
		free_head = slot;
	}

	uint64_t resident_bytes = 0;
	uint64_t budget_bytes = 0;
	uint32_t resident_count = 0;

private:
	struct Slot
	{
		uint64_t bytes = 0, last_used_frame = 0;
		uint32_t prev = 0, next = 0, next_free = 0;
	};
	std::vector<Slot> slots;
	uint32_t head = 0, tail = 0, free_head = 0;

	void unlink(uint32_t slot)
	{
		if (slots[slot].prev) slots[slots[slot].prev].next = slots[slot].next;
		else head = slots[slot].next;
		if (slots[slot].next) slots[slots[slot].next].prev = slots[slot].prev;
		else tail = slots[slot].prev;
	}

	void append(uint32_t slot)
	{
		slots[slot].prev = tail;
		slots[slot].next = 0;
		if (tail) slots[tail].next = slot;
		else head = slot;
		tail = slot;
	}
};
}

// Read by frontend glue only after the command ring has drained.
struct HiresResidencyStatus
{
	uint64_t budget = 0, high_water_bytes = 0, uploads = 0, upload_bytes = 0;
	uint64_t evictions = 0, refused_saturated = 0, refused_too_large = 0, refused_no_slot = 0;
	uint64_t decode_failures = 0, upload_failures = 0;
	double load_ms_total = 0, load_ms_frame = 0, load_ms_max_frame = 0;
};
}
