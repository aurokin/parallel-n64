#pragma once

#include <cstddef>
#include <cstdint>
#include "rdp_hires_runtime_policy.hpp"

namespace RDP
{
namespace detail
{
enum class HiresBindingOwnershipClass : uint8_t
{
	None = 0,
	UnboundProviderHit,
	UploadOwner,
	FallbackOwner,
	AliasConsumer
};

enum class HiresDrawOwnershipClass : uint8_t
{
	None = 0,
	CopyConsumer,
	DescriptorlessConsumer,
	UploadOwner,
	FallbackOwner,
	AliasConsumer,
	Mixed
};

inline const char *hires_binding_ownership_class_name(HiresBindingOwnershipClass klass)
{
	switch (klass)
	{
	case HiresBindingOwnershipClass::None:
		return "none";
	case HiresBindingOwnershipClass::UnboundProviderHit:
		return "unbound";
	case HiresBindingOwnershipClass::UploadOwner:
		return "upload_owner";
	case HiresBindingOwnershipClass::FallbackOwner:
		return "fallback_owner";
	case HiresBindingOwnershipClass::AliasConsumer:
		return "alias_consumer";
	default:
		return "unknown";
	}
}

inline const char *hires_draw_ownership_class_name(HiresDrawOwnershipClass klass)
{
	switch (klass)
	{
	case HiresDrawOwnershipClass::None:
		return "none";
	case HiresDrawOwnershipClass::CopyConsumer:
		return "copy_consumer";
	case HiresDrawOwnershipClass::DescriptorlessConsumer:
		return "descriptorless_consumer";
	case HiresDrawOwnershipClass::UploadOwner:
		return "upload_owner";
	case HiresDrawOwnershipClass::FallbackOwner:
		return "fallback_owner";
	case HiresDrawOwnershipClass::AliasConsumer:
		return "alias_consumer";
	case HiresDrawOwnershipClass::Mixed:
		return "mixed";
	default:
		return "unknown";
	}
}

inline uint8_t hires_draw_ownership_class_mask_bit(HiresDrawOwnershipClass klass)
{
	switch (klass)
	{
	case HiresDrawOwnershipClass::CopyConsumer:
		return 1u << 0;
	case HiresDrawOwnershipClass::DescriptorlessConsumer:
		return 1u << 1;
	case HiresDrawOwnershipClass::UploadOwner:
		return 1u << 2;
	case HiresDrawOwnershipClass::FallbackOwner:
		return 1u << 3;
	case HiresDrawOwnershipClass::AliasConsumer:
		return 1u << 4;
	case HiresDrawOwnershipClass::Mixed:
		return 1u << 5;
	default:
		return 0;
	}
}

template <typename ReplacementTileStateType>
inline HiresBindingOwnershipClass classify_hires_binding_ownership_class(const ReplacementTileStateType &state)
{
	if (!state.valid || !state.hit)
		return HiresBindingOwnershipClass::None;

	if (!hires_descriptor_index_valid(state.vk_image_index))
		return HiresBindingOwnershipClass::UnboundProviderHit;

	if (state.lookup_source == HiresLookupSource::AliasPropagated || state.source_lookup_tile_index != 0)
		return HiresBindingOwnershipClass::AliasConsumer;

	if (state.lookup_source == HiresLookupSource::Primary)
		return HiresBindingOwnershipClass::UploadOwner;

	return HiresBindingOwnershipClass::FallbackOwner;
}

inline uint8_t hires_binding_ownership_class_mask_bit(HiresBindingOwnershipClass klass)
{
	switch (klass)
	{
	case HiresBindingOwnershipClass::UploadOwner:
		return 1u << 0;
	case HiresBindingOwnershipClass::FallbackOwner:
		return 1u << 1;
	case HiresBindingOwnershipClass::AliasConsumer:
		return 1u << 2;
	default:
		return 0;
	}
}

template <typename TileInfoType, typename ReplacementTileStateType, size_t NumTiles>
inline HiresDrawOwnershipClass classify_hires_draw_ownership_class(
		bool copy_mode,
		unsigned draw_replacement_desc_count,
		const ReplacementTileStateType (&replacement_states)[NumTiles],
		const TileInfoType (&draw_tiles)[NumTiles])
{
	bool any_hires_hit = false;
	bool any_bound_descriptor = false;
	uint8_t binding_mask = 0;

	for (size_t i = 0; i < NumTiles; i++)
	{
		const auto klass = classify_hires_binding_ownership_class(replacement_states[i]);
		if (klass == HiresBindingOwnershipClass::None)
			continue;

		any_hires_hit = true;
		if (hires_descriptor_index_valid(draw_tiles[i].replacement.repl_desc_index))
			any_bound_descriptor = true;
		binding_mask |= hires_binding_ownership_class_mask_bit(klass);
	}

	if (!any_hires_hit)
		return HiresDrawOwnershipClass::None;

	if (copy_mode)
		return HiresDrawOwnershipClass::CopyConsumer;

	if (draw_replacement_desc_count == 0 || !any_bound_descriptor)
		return HiresDrawOwnershipClass::DescriptorlessConsumer;

	if ((binding_mask & (binding_mask - 1u)) != 0)
		return HiresDrawOwnershipClass::Mixed;

	if (binding_mask & hires_binding_ownership_class_mask_bit(HiresBindingOwnershipClass::UploadOwner))
		return HiresDrawOwnershipClass::UploadOwner;

	if (binding_mask & hires_binding_ownership_class_mask_bit(HiresBindingOwnershipClass::FallbackOwner))
		return HiresDrawOwnershipClass::FallbackOwner;

	if (binding_mask & hires_binding_ownership_class_mask_bit(HiresBindingOwnershipClass::AliasConsumer))
		return HiresDrawOwnershipClass::AliasConsumer;

	return HiresDrawOwnershipClass::DescriptorlessConsumer;
}
}
}
