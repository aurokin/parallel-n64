#pragma once

#include <cstdint>
#include <string>

namespace RDP
{
namespace detail
{
enum class HiresConfigureOutcome
{
	Disabled,
	MissingPath,
	LoadFailed,
	LoadSucceeded
};

enum class HiresLookupSource : uint8_t
{
	None = 0,
	Primary,
	CiLow32,
	TileMask,
	TileStride,
	BlockTile,
	BlockShape,
	PendingBlockRetry,
	AliasPropagated
};

enum class HiresReinterpretationBirthPatternMode : uint8_t
{
	AllowAll = 0,
	NarrowPaperMarioProbe,
	NarrowSameFormatsize32x32Probe,
	NarrowCrossFormatsize16x16Probe,
	NarrowCrossFormatsize32x16Probe,
	NarrowSame32x32Cross16x16Probe,
	NarrowSame32x32Cross32x16Probe
};

enum class HiresCrossFormatsize32x16SourceFilter : uint8_t
{
	AllowAll = 0,
	PendingOnly,
	AliasOnly
};

struct HiresLookupModePolicy
{
	bool allow_ci_low32 = true;
	bool allow_tile_mask = true;
	bool allow_tile_stride = true;
	bool allow_block_tile = true;
	bool allow_block_shape = true;
	bool allow_pending_block_retry = true;
	bool allow_alias_group_binding = true;
	uint8_t reinterpretation_birth_family_mask = 0x0fu;
	HiresReinterpretationBirthPatternMode reinterpretation_birth_pattern_mode =
		HiresReinterpretationBirthPatternMode::AllowAll;
	bool restrict_cross_formatsize_16x16_to_primary_phase = false;
	HiresCrossFormatsize32x16SourceFilter cross_formatsize_32x16_source_filter =
		HiresCrossFormatsize32x16SourceFilter::AllowAll;
	uint8_t cross_formatsize_32x16_draw_ownership_mask = 0xffu;
};

inline std::string resolve_hires_cache_path(const std::string &configured_path, const char *env_path)
{
	if (!configured_path.empty())
		return configured_path;
	return env_path ? env_path : "";
}

inline bool should_attempt_hires_cache_load(bool enable, const char *cache_path)
{
	return enable && cache_path && *cache_path;
}

inline HiresConfigureOutcome classify_hires_configure_outcome(bool enable, const char *cache_path, bool load_ok)
{
	if (!enable)
		return HiresConfigureOutcome::Disabled;
	if (!cache_path || !*cache_path)
		return HiresConfigureOutcome::MissingPath;
	return load_ok ? HiresConfigureOutcome::LoadSucceeded : HiresConfigureOutcome::LoadFailed;
}

inline bool should_attach_hires_provider(HiresConfigureOutcome outcome)
{
	return outcome == HiresConfigureOutcome::LoadSucceeded;
}

inline constexpr uint32_t hires_invalid_descriptor_index()
{
	return 0xffffffffu;
}

inline bool hires_descriptor_index_valid(uint32_t index)
{
	return index != hires_invalid_descriptor_index();
}

inline constexpr uint32_t hires_shader_descriptor_mipmap_bit()
{
	return 1u << 30;
}

inline constexpr uint32_t hires_shader_descriptor_index_mask()
{
	return hires_shader_descriptor_mipmap_bit() - 1u;
}

inline uint32_t pack_hires_shader_descriptor_index(uint32_t descriptor_index, bool has_mips)
{
	if (!hires_descriptor_index_valid(descriptor_index))
		return hires_invalid_descriptor_index();

	const uint32_t packed_index = descriptor_index & hires_shader_descriptor_index_mask();
	return has_mips ? (packed_index | hires_shader_descriptor_mipmap_bit()) : packed_index;
}

inline uint32_t unpack_hires_shader_descriptor_index(uint32_t packed_index)
{
	return packed_index & hires_shader_descriptor_index_mask();
}

inline bool hires_shader_descriptor_has_mips(uint32_t packed_index)
{
	return (packed_index & hires_shader_descriptor_mipmap_bit()) != 0;
}

inline bool hires_lookup_strict_enabled(unsigned mode)
{
	return mode == 1;
}

inline bool hires_lookup_owner_only_enabled(unsigned mode)
{
	return mode == 2;
}

inline bool hires_lookup_no_reinterpretation_enabled(unsigned mode)
{
	return mode == 3;
}

inline bool hires_lookup_owner_reinterpretation_enabled(unsigned mode)
{
	return mode == 4;
}

inline bool hires_lookup_narrow_reinterpretation_enabled(unsigned mode)
{
	return mode == 5;
}

inline bool hires_lookup_narrow_reinterpretation_same_32x32_enabled(unsigned mode)
{
	return mode == 6;
}

inline bool hires_lookup_narrow_reinterpretation_cross_16x16_enabled(unsigned mode)
{
	return mode == 7;
}

inline bool hires_lookup_narrow_reinterpretation_cross_32x16_enabled(unsigned mode)
{
	return mode == 8;
}

inline bool hires_lookup_narrow_reinterpretation_pending_32x16_enabled(unsigned mode)
{
	return mode == 12;
}

inline bool hires_lookup_narrow_reinterpretation_alias_32x16_enabled(unsigned mode)
{
	return mode == 13;
}

inline bool hires_lookup_narrow_reinterpretation_32x32_pending_32x16_enabled(unsigned mode)
{
	return mode == 14;
}

inline bool hires_lookup_narrow_reinterpretation_32x32_alias_32x16_enabled(unsigned mode)
{
	return mode == 15;
}

inline bool hires_lookup_narrow_reinterpretation_phase_16x16_pending_32x16_enabled(unsigned mode)
{
	return mode == 16;
}

inline bool hires_lookup_narrow_reinterpretation_phase_16x16_alias_32x16_enabled(unsigned mode)
{
	return mode == 17;
}

inline bool hires_lookup_narrow_reinterpretation_32x32_16x16_enabled(unsigned mode)
{
	return mode == 9;
}

inline bool hires_lookup_narrow_reinterpretation_32x32_32x16_enabled(unsigned mode)
{
	return mode == 10;
}

inline bool hires_lookup_narrow_reinterpretation_phase_16x16_enabled(unsigned mode)
{
	return mode == 11;
}

inline bool hires_lookup_fallbacks_enabled(unsigned mode)
{
	return mode == 0 || mode == 3 || mode == 4 || mode == 5 || mode == 6 || mode == 7 || mode == 8 || mode == 9 || mode == 10 || mode == 11 || mode == 12 || mode == 13 || mode == 14 || mode == 15 || mode == 16 || mode == 17;
}

inline bool hires_lookup_block_reinterpretation_enabled(unsigned mode)
{
	return mode == 0 || mode == 4 || mode == 5 || mode == 6 || mode == 7 || mode == 8 || mode == 9 || mode == 10 || mode == 11 || mode == 12 || mode == 13 || mode == 14 || mode == 15 || mode == 16 || mode == 17;
}

inline bool hires_lookup_pending_block_retry_enabled(unsigned mode)
{
	return mode == 0 || mode == 4 || mode == 5 || mode == 6 || mode == 7 || mode == 8 || mode == 9 || mode == 10 || mode == 11 || mode == 12 || mode == 13 || mode == 14 || mode == 15 || mode == 16 || mode == 17;
}

inline HiresLookupModePolicy resolve_hires_lookup_mode_policy(unsigned mode)
{
	HiresLookupModePolicy policy = {};

	switch (mode)
	{
	case 1:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = false;
		policy.allow_block_shape = false;
		policy.allow_pending_block_retry = false;
		policy.allow_alias_group_binding = false;
		break;

	case 2:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = false;
		policy.allow_block_shape = false;
		policy.allow_pending_block_retry = false;
		policy.allow_alias_group_binding = false;
		break;

	case 3:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = false;
		policy.allow_block_shape = false;
		policy.allow_pending_block_retry = false;
		policy.allow_alias_group_binding = true;
		break;

	case 4:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = true;
		policy.allow_block_shape = true;
		policy.allow_pending_block_retry = true;
		policy.allow_alias_group_binding = true;
		policy.reinterpretation_birth_family_mask = 0x5u;
		break;

	case 5:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = true;
		policy.allow_block_shape = true;
		policy.allow_pending_block_retry = true;
		policy.allow_alias_group_binding = true;
		policy.reinterpretation_birth_family_mask = 0x0fu;
		policy.reinterpretation_birth_pattern_mode = HiresReinterpretationBirthPatternMode::NarrowPaperMarioProbe;
		break;

	case 6:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = true;
		policy.allow_block_shape = true;
		policy.allow_pending_block_retry = true;
		policy.allow_alias_group_binding = true;
		policy.reinterpretation_birth_family_mask = 0x0fu;
		policy.reinterpretation_birth_pattern_mode =
			HiresReinterpretationBirthPatternMode::NarrowSameFormatsize32x32Probe;
		break;

	case 7:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = true;
		policy.allow_block_shape = true;
		policy.allow_pending_block_retry = true;
		policy.allow_alias_group_binding = true;
		policy.reinterpretation_birth_family_mask = 0x0fu;
		policy.reinterpretation_birth_pattern_mode =
			HiresReinterpretationBirthPatternMode::NarrowCrossFormatsize16x16Probe;
		break;

	case 8:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = true;
		policy.allow_block_shape = true;
		policy.allow_pending_block_retry = true;
		policy.allow_alias_group_binding = true;
		policy.reinterpretation_birth_family_mask = 0x0fu;
		policy.reinterpretation_birth_pattern_mode =
			HiresReinterpretationBirthPatternMode::NarrowCrossFormatsize32x16Probe;
		break;

	case 9:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = true;
		policy.allow_block_shape = true;
		policy.allow_pending_block_retry = true;
		policy.allow_alias_group_binding = true;
		policy.reinterpretation_birth_family_mask = 0x0fu;
		policy.reinterpretation_birth_pattern_mode =
			HiresReinterpretationBirthPatternMode::NarrowSame32x32Cross16x16Probe;
		break;

	case 10:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = true;
		policy.allow_block_shape = true;
		policy.allow_pending_block_retry = true;
		policy.allow_alias_group_binding = true;
		policy.reinterpretation_birth_family_mask = 0x0fu;
		policy.reinterpretation_birth_pattern_mode =
			HiresReinterpretationBirthPatternMode::NarrowSame32x32Cross32x16Probe;
		break;

	case 11:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = true;
		policy.allow_block_shape = true;
		policy.allow_pending_block_retry = true;
		policy.allow_alias_group_binding = true;
		policy.reinterpretation_birth_family_mask = 0x0fu;
		policy.reinterpretation_birth_pattern_mode = HiresReinterpretationBirthPatternMode::NarrowPaperMarioProbe;
		policy.restrict_cross_formatsize_16x16_to_primary_phase = true;
		break;

	case 12:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = true;
		policy.allow_block_shape = true;
		policy.allow_pending_block_retry = true;
		policy.allow_alias_group_binding = true;
		policy.reinterpretation_birth_family_mask = 0x0fu;
		policy.reinterpretation_birth_pattern_mode =
			HiresReinterpretationBirthPatternMode::NarrowCrossFormatsize32x16Probe;
		policy.cross_formatsize_32x16_source_filter = HiresCrossFormatsize32x16SourceFilter::PendingOnly;
		break;

	case 13:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = true;
		policy.allow_block_shape = true;
		policy.allow_pending_block_retry = true;
		policy.allow_alias_group_binding = true;
		policy.reinterpretation_birth_family_mask = 0x0fu;
		policy.reinterpretation_birth_pattern_mode =
			HiresReinterpretationBirthPatternMode::NarrowCrossFormatsize32x16Probe;
		policy.cross_formatsize_32x16_source_filter = HiresCrossFormatsize32x16SourceFilter::AliasOnly;
		break;

	case 14:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = true;
		policy.allow_block_shape = true;
		policy.allow_pending_block_retry = true;
		policy.allow_alias_group_binding = true;
		policy.reinterpretation_birth_family_mask = 0x0fu;
		policy.reinterpretation_birth_pattern_mode =
			HiresReinterpretationBirthPatternMode::NarrowSame32x32Cross32x16Probe;
		policy.cross_formatsize_32x16_source_filter = HiresCrossFormatsize32x16SourceFilter::PendingOnly;
		break;

	case 15:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = true;
		policy.allow_block_shape = true;
		policy.allow_pending_block_retry = true;
		policy.allow_alias_group_binding = true;
		policy.reinterpretation_birth_family_mask = 0x0fu;
		policy.reinterpretation_birth_pattern_mode =
			HiresReinterpretationBirthPatternMode::NarrowSame32x32Cross32x16Probe;
		policy.cross_formatsize_32x16_source_filter = HiresCrossFormatsize32x16SourceFilter::AliasOnly;
		break;

	case 16:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = true;
		policy.allow_block_shape = true;
		policy.allow_pending_block_retry = true;
		policy.allow_alias_group_binding = true;
		policy.reinterpretation_birth_family_mask = 0x0fu;
		policy.reinterpretation_birth_pattern_mode = HiresReinterpretationBirthPatternMode::NarrowPaperMarioProbe;
		policy.restrict_cross_formatsize_16x16_to_primary_phase = true;
		policy.cross_formatsize_32x16_source_filter = HiresCrossFormatsize32x16SourceFilter::PendingOnly;
		break;

	case 17:
		policy.allow_ci_low32 = false;
		policy.allow_tile_mask = false;
		policy.allow_tile_stride = false;
		policy.allow_block_tile = true;
		policy.allow_block_shape = true;
		policy.allow_pending_block_retry = true;
		policy.allow_alias_group_binding = true;
		policy.reinterpretation_birth_family_mask = 0x0fu;
		policy.reinterpretation_birth_pattern_mode = HiresReinterpretationBirthPatternMode::NarrowPaperMarioProbe;
		policy.restrict_cross_formatsize_16x16_to_primary_phase = true;
		policy.cross_formatsize_32x16_source_filter = HiresCrossFormatsize32x16SourceFilter::AliasOnly;
		break;

	default:
		break;
	}

	return policy;
}
}
}
