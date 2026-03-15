#pragma once

#include <cstddef>
#include <cstdint>
#include "texture_replacement.hpp"

namespace RDP
{
namespace detail
{
enum class HiresRegistryResidencyState
{
	Missing,
	Queued,
	Ready,
	Failed,
};

enum class HiresRegistryTransition
{
	QueueUpload,
	UploadSucceeded,
	UploadFailed,
	DisableOrReset,
};

inline HiresRegistryResidencyState advance_hires_registry_state(
		HiresRegistryResidencyState current,
		HiresRegistryTransition transition)
{
	if (transition == HiresRegistryTransition::DisableOrReset)
		return HiresRegistryResidencyState::Missing;

	switch (current)
	{
	case HiresRegistryResidencyState::Missing:
		return transition == HiresRegistryTransition::QueueUpload ?
		       HiresRegistryResidencyState::Queued :
		       current;

	case HiresRegistryResidencyState::Queued:
		if (transition == HiresRegistryTransition::UploadSucceeded)
			return HiresRegistryResidencyState::Ready;
		if (transition == HiresRegistryTransition::UploadFailed)
			return HiresRegistryResidencyState::Failed;
		return current;

	case HiresRegistryResidencyState::Ready:
		return current;

	case HiresRegistryResidencyState::Failed:
		return transition == HiresRegistryTransition::QueueUpload ?
		       HiresRegistryResidencyState::Queued :
		       current;
	}

	return current;
}

inline constexpr uint32_t hires_registry_invalid_handle()
{
	return 0xffffffffu;
}

inline bool hires_registry_handle_valid(uint32_t handle, uint32_t capacity)
{
	return capacity > 0 &&
	       handle != hires_registry_invalid_handle() &&
	       handle < capacity;
}

enum class HiresRegistryHandleAllocationResult
{
	Allocated,
	Exhausted,
};

inline HiresRegistryHandleAllocationResult check_hires_registry_handle_allocation(
		uint32_t next_handle,
		uint32_t capacity)
{
	return next_handle < capacity ?
	       HiresRegistryHandleAllocationResult::Allocated :
	       HiresRegistryHandleAllocationResult::Exhausted;
}

inline bool should_queue_hires_upload(HiresRegistryResidencyState state,
                                      bool lookup_matched,
                                      bool descriptor_valid)
{
	if (!lookup_matched)
		return false;

	if (state == HiresRegistryResidencyState::Queued)
		return false;

	if (state == HiresRegistryResidencyState::Ready && descriptor_valid)
		return false;

	return true;
}

template <typename Entry>
inline Entry *find_hires_registry_formatsize_match(Entry *entries, size_t count, uint16_t formatsize)
{
	if (!entries || count == 0)
		return nullptr;

	Entry *wildcard_match = nullptr;
	for (size_t i = 0; i < count; i++)
	{
		if (entries[i].formatsize == formatsize)
			return &entries[i];
		if (entries[i].formatsize == 0 || formatsize == 0)
			wildcard_match = &entries[i];
	}

	return wildcard_match;
}

template <typename Entry>
inline Entry *find_hires_registry_formatsize_exact(Entry *entries, size_t count, uint16_t formatsize)
{
	if (!entries || count == 0)
		return nullptr;

	for (size_t i = 0; i < count; i++)
	{
		if (entries[i].formatsize == formatsize)
			return &entries[i];
	}

	return nullptr;
}

template <typename Entry>
inline const Entry *find_hires_registry_formatsize_match(const Entry *entries, size_t count, uint16_t formatsize)
{
	if (!entries || count == 0)
		return nullptr;

	const Entry *wildcard_match = nullptr;
	for (size_t i = 0; i < count; i++)
	{
		if (entries[i].formatsize == formatsize)
			return &entries[i];
		if (entries[i].formatsize == 0 || formatsize == 0)
			wildcard_match = &entries[i];
	}

	return wildcard_match;
}

template <typename Entry>
inline const Entry *find_hires_registry_formatsize_exact(const Entry *entries, size_t count, uint16_t formatsize)
{
	if (!entries || count == 0)
		return nullptr;

	for (size_t i = 0; i < count; i++)
	{
		if (entries[i].formatsize == formatsize)
			return &entries[i];
	}

	return nullptr;
}

struct HiresRegistryEntryResidentMeta
{
	bool resident = false;
	bool pinned = false;
	uint64_t last_used_tick = 0;
};

inline int choose_hires_eviction_candidate(const HiresRegistryEntryResidentMeta *entries, size_t count)
{
	if (!entries || count == 0)
		return -1;

	int best = -1;
	for (size_t i = 0; i < count; i++)
	{
		if (!entries[i].resident || entries[i].pinned)
			continue;

		if (best < 0 || entries[i].last_used_tick < entries[best].last_used_tick)
			best = int(i);
	}

	return best;
}

enum class HiresRegistryBudgetDecision
{
	Admit,
	EvictOldestThenAdmit,
	RejectOverBudget,
};

inline HiresRegistryBudgetDecision decide_hires_registry_budget(
		size_t resident_bytes,
		size_t incoming_bytes,
		size_t budget_bytes,
		bool eviction_enabled,
		bool has_evictable_candidate)
{
	if (budget_bytes == 0)
		return HiresRegistryBudgetDecision::Admit;

	if (incoming_bytes > budget_bytes)
		return HiresRegistryBudgetDecision::RejectOverBudget;

	if (resident_bytes <= budget_bytes - incoming_bytes)
		return HiresRegistryBudgetDecision::Admit;

	if (eviction_enabled && has_evictable_candidate)
		return HiresRegistryBudgetDecision::EvictOldestThenAdmit;

	return HiresRegistryBudgetDecision::RejectOverBudget;
}

struct HiresReplacementAlphaStats
{
	size_t total_pixels = 0;
	size_t zero_alpha = 0;
	size_t full_alpha = 0;
	size_t partial_alpha = 0;
};

inline HiresReplacementAlphaStats analyze_hires_replacement_alpha(const uint8_t *rgba8, size_t size)
{
	HiresReplacementAlphaStats stats = {};
	if (!rgba8 || size < 4)
		return stats;

	stats.total_pixels = size / 4;
	for (size_t i = 0; i + 3 < size; i += 4)
	{
		const uint8_t alpha = rgba8[i + 3];
		if (alpha == 0)
			stats.zero_alpha++;
		else if (alpha == 255)
			stats.full_alpha++;
		else
			stats.partial_alpha++;
	}

	return stats;
}

inline HiresAlphaContentClass classify_hires_replacement_alpha_content(const HiresReplacementAlphaStats &stats)
{
	if (stats.total_pixels == 0)
		return HiresAlphaContentClass::Unknown;

	if (stats.zero_alpha == 0 && stats.partial_alpha == 0)
		return HiresAlphaContentClass::Opaque;

	if (stats.partial_alpha * 100u <= stats.total_pixels * 2u)
		return HiresAlphaContentClass::MostlyBinary;

	return HiresAlphaContentClass::Soft;
}
}
}
