#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_runtime_policy.hpp"
#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_consumer_policy.hpp"
#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_ownership_policy.hpp"
#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/texture_replacement.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace RDP::detail;

namespace
{
static void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}

static void test_cache_path_resolution_precedence()
{
	check(resolve_hires_cache_path("/explicit/path", "/env/path") == "/explicit/path",
	      "explicit cache path should take precedence over env");
	check(resolve_hires_cache_path("", "/env/path") == "/env/path",
	      "env cache path should be used when explicit path is empty");
	check(resolve_hires_cache_path("", nullptr).empty(),
	      "cache path should resolve to empty string when explicit/env are absent");
}

static void test_should_attempt_hires_cache_load_matrix()
{
	check(!should_attempt_hires_cache_load(false, "/cache"),
	      "disabled path should never attempt cache load");
	check(!should_attempt_hires_cache_load(true, nullptr),
	      "null path should not attempt cache load");
	check(!should_attempt_hires_cache_load(true, ""),
	      "empty path should not attempt cache load");
	check(should_attempt_hires_cache_load(true, "/cache"),
	      "enabled + non-empty path should attempt cache load");
}

static void test_classify_hires_configure_outcome_matrix()
{
	check(classify_hires_configure_outcome(false, "/cache", true) == HiresConfigureOutcome::Disabled,
	      "disabled outcome mismatch");
	check(classify_hires_configure_outcome(true, nullptr, true) == HiresConfigureOutcome::MissingPath,
	      "missing-path outcome mismatch (null)");
	check(classify_hires_configure_outcome(true, "", true) == HiresConfigureOutcome::MissingPath,
	      "missing-path outcome mismatch (empty)");
	check(classify_hires_configure_outcome(true, "/cache", false) == HiresConfigureOutcome::LoadFailed,
	      "load-failed outcome mismatch");
	check(classify_hires_configure_outcome(true, "/cache", true) == HiresConfigureOutcome::LoadSucceeded,
	      "load-succeeded outcome mismatch");

	check(!should_attach_hires_provider(HiresConfigureOutcome::Disabled),
	      "disabled outcome should not attach provider");
	check(!should_attach_hires_provider(HiresConfigureOutcome::MissingPath),
	      "missing-path outcome should not attach provider");
	check(!should_attach_hires_provider(HiresConfigureOutcome::LoadFailed),
	      "load-failed outcome should not attach provider");
	check(should_attach_hires_provider(HiresConfigureOutcome::LoadSucceeded),
	      "load-succeeded outcome should attach provider");
}

static void test_descriptor_index_sentinel_contract()
{
	check(hires_invalid_descriptor_index() == 0xffffffffu,
	      "invalid descriptor sentinel mismatch");
	check(!hires_descriptor_index_valid(hires_invalid_descriptor_index()),
	      "invalid descriptor sentinel should be rejected");
	check(hires_descriptor_index_valid(0u), "descriptor index zero should be considered valid");
	check(hires_descriptor_index_valid(17u), "descriptor index 17 should be considered valid");

	check(hires_shader_descriptor_mipmap_bit() == (1u << 30),
	      "shader descriptor mipmap bit mismatch");
	check(hires_shader_descriptor_index_mask() == ((1u << 30) - 1u),
	      "shader descriptor index mask mismatch");

	const uint32_t packed_nomips = pack_hires_shader_descriptor_index(17u, false);
	check(unpack_hires_shader_descriptor_index(packed_nomips) == 17u,
	      "packed descriptor should preserve index when mips are disabled");
	check(!hires_shader_descriptor_has_mips(packed_nomips),
	      "packed descriptor should not report mip flag when disabled");

	const uint32_t packed_mips = pack_hires_shader_descriptor_index(17u, true);
	check(unpack_hires_shader_descriptor_index(packed_mips) == 17u,
	      "packed descriptor should preserve index when mips are enabled");
	check(hires_shader_descriptor_has_mips(packed_mips),
	      "packed descriptor should report mip flag when enabled");
	check(pack_hires_shader_descriptor_index(hires_invalid_descriptor_index(), true) == hires_invalid_descriptor_index(),
	      "invalid descriptor should remain invalid when packed");

	const RDP::ReplacementMeta meta = {};
	check(meta.vk_image_index == hires_invalid_descriptor_index(),
	      "ReplacementMeta default descriptor index should match invalid sentinel");
}

static void test_lookup_mode_policy_contract()
{
	const auto permissive = resolve_hires_lookup_mode_policy(0);
	check(permissive.allow_ci_low32, "permissive mode should allow CI low32 fallback");
	check(permissive.allow_tile_mask, "permissive mode should allow tile-mask fallback");
	check(permissive.allow_tile_stride, "permissive mode should allow tile-stride fallback");
	check(permissive.allow_block_tile, "permissive mode should allow block-tile fallback");
	check(permissive.allow_block_shape, "permissive mode should allow block-shape fallback");
	check(permissive.allow_pending_block_retry, "permissive mode should allow pending block retry");
	check(permissive.allow_alias_group_binding, "permissive mode should allow alias-group binding");

	const auto strict = resolve_hires_lookup_mode_policy(1);
	check(!strict.allow_ci_low32, "strict mode should reject CI low32 fallback");
	check(!strict.allow_tile_mask, "strict mode should reject tile-mask fallback");
	check(!strict.allow_tile_stride, "strict mode should reject tile-stride fallback");
	check(!strict.allow_block_tile, "strict mode should reject block-tile fallback");
	check(!strict.allow_block_shape, "strict mode should reject block-shape fallback");
	check(!strict.allow_pending_block_retry, "strict mode should reject pending block retry");
	check(!strict.allow_alias_group_binding, "strict mode should reject alias-group binding");

	const auto owner = resolve_hires_lookup_mode_policy(2);
	check(!owner.allow_ci_low32, "owner mode should reject CI low32 fallback");
	check(!owner.allow_tile_mask, "owner mode should reject tile-mask fallback");
	check(!owner.allow_tile_stride, "owner mode should reject tile-stride fallback");
	check(!owner.allow_block_tile, "owner mode should reject block-tile fallback");
	check(!owner.allow_block_shape, "owner mode should reject block-shape fallback");
	check(!owner.allow_pending_block_retry, "owner mode should reject pending block retry");
	check(!owner.allow_alias_group_binding, "owner mode should reject alias-group binding");

	const auto no_reinterp = resolve_hires_lookup_mode_policy(3);
	check(!no_reinterp.allow_ci_low32, "no-reinterp mode should reject CI low32 fallback");
	check(!no_reinterp.allow_tile_mask, "no-reinterp mode should reject tile-mask fallback");
	check(!no_reinterp.allow_tile_stride, "no-reinterp mode should reject tile-stride fallback");
	check(!no_reinterp.allow_block_tile, "no-reinterp mode should reject block-tile fallback");
	check(!no_reinterp.allow_block_shape, "no-reinterp mode should reject block-shape fallback");
	check(!no_reinterp.allow_pending_block_retry, "no-reinterp mode should reject pending block retry");
	check(no_reinterp.allow_alias_group_binding, "no-reinterp mode should keep alias-group binding");

	const auto owner_reinterp = resolve_hires_lookup_mode_policy(4);
	check(!owner_reinterp.allow_ci_low32, "owner-reinterp mode should reject CI low32 fallback");
	check(!owner_reinterp.allow_tile_mask, "owner-reinterp mode should reject tile-mask fallback");
	check(!owner_reinterp.allow_tile_stride, "owner-reinterp mode should reject tile-stride fallback");
	check(owner_reinterp.allow_block_tile, "owner-reinterp mode should allow block-tile fallback");
	check(owner_reinterp.allow_block_shape, "owner-reinterp mode should allow block-shape fallback");
	check(owner_reinterp.allow_pending_block_retry, "owner-reinterp mode should allow pending block retry");
	check(owner_reinterp.allow_alias_group_binding, "owner-reinterp mode should keep alias-group binding");
	check(owner_reinterp.reinterpretation_birth_family_mask == 0x5u,
	      "owner-reinterp mode should allow only owner reinterpretation families");

	const auto narrow_reinterp = resolve_hires_lookup_mode_policy(5);
	const auto narrow_32x32 = resolve_hires_lookup_mode_policy(6);
	const auto narrow_16x16 = resolve_hires_lookup_mode_policy(7);
	const auto narrow_32x16 = resolve_hires_lookup_mode_policy(8);
	const auto narrow_32x32_16x16 = resolve_hires_lookup_mode_policy(9);
	const auto narrow_32x32_32x16 = resolve_hires_lookup_mode_policy(10);
	const auto narrow_phase_16x16 = resolve_hires_lookup_mode_policy(11);
	const auto narrow_pending_32x16 = resolve_hires_lookup_mode_policy(12);
	const auto narrow_alias_32x16 = resolve_hires_lookup_mode_policy(13);
	const auto narrow_32x32_pending_32x16 = resolve_hires_lookup_mode_policy(14);
	const auto narrow_32x32_alias_32x16 = resolve_hires_lookup_mode_policy(15);
	const auto narrow_phase_16x16_pending_32x16 = resolve_hires_lookup_mode_policy(16);
	const auto narrow_phase_16x16_alias_32x16 = resolve_hires_lookup_mode_policy(17);
	check(!narrow_reinterp.allow_ci_low32, "narrow-reinterp mode should reject CI low32 fallback");
	check(!narrow_reinterp.allow_tile_mask, "narrow-reinterp mode should reject tile-mask fallback");
	check(!narrow_reinterp.allow_tile_stride, "narrow-reinterp mode should reject tile-stride fallback");
	check(narrow_reinterp.allow_block_tile, "narrow-reinterp mode should allow block-tile fallback");
	check(narrow_reinterp.allow_block_shape, "narrow-reinterp mode should allow block-shape fallback");
	check(narrow_reinterp.allow_pending_block_retry, "narrow-reinterp mode should allow pending block retry");
	check(narrow_reinterp.allow_alias_group_binding, "narrow-reinterp mode should keep alias-group binding");
	check(narrow_reinterp.reinterpretation_birth_family_mask == 0x0fu,
	      "narrow-reinterp mode should let the birth-pattern filter govern reinterpretation families");
	check(narrow_reinterp.reinterpretation_birth_pattern_mode == HiresReinterpretationBirthPatternMode::NarrowPaperMarioProbe,
	      "narrow-reinterp mode should enable the first narrow birth-pattern filter");
	check(narrow_32x32.reinterpretation_birth_pattern_mode ==
	              HiresReinterpretationBirthPatternMode::NarrowSameFormatsize32x32Probe,
	      "narrow-32x32 mode should isolate the same-format 32x32 birth-pattern filter");
	check(narrow_16x16.reinterpretation_birth_pattern_mode ==
	              HiresReinterpretationBirthPatternMode::NarrowCrossFormatsize16x16Probe,
	      "narrow-16x16 mode should isolate the cross-format 16x16 birth-pattern filter");
	check(narrow_32x16.reinterpretation_birth_pattern_mode ==
	              HiresReinterpretationBirthPatternMode::NarrowCrossFormatsize32x16Probe,
	      "narrow-32x16 mode should isolate the cross-format 32x16 birth-pattern filter");
	check(narrow_32x32_16x16.reinterpretation_birth_pattern_mode ==
	              HiresReinterpretationBirthPatternMode::NarrowSame32x32Cross16x16Probe,
	      "narrow-32x32-16x16 mode should isolate the 32x32 + 16x16 birth-pattern filter");
	check(narrow_32x32_32x16.reinterpretation_birth_pattern_mode ==
	              HiresReinterpretationBirthPatternMode::NarrowSame32x32Cross32x16Probe,
	      "narrow-32x32-32x16 mode should isolate the 32x32 + 32x16 birth-pattern filter");
	check(narrow_phase_16x16.reinterpretation_birth_pattern_mode ==
	              HiresReinterpretationBirthPatternMode::NarrowPaperMarioProbe,
	      "narrow-reinterp-phase-16x16 mode should keep the full Paper Mario birth-pattern filter");
	check(narrow_phase_16x16.restrict_cross_formatsize_16x16_to_primary_phase,
	      "narrow-reinterp-phase-16x16 mode should enable the 16x16 primary-phase consumer filter");
	check(narrow_pending_32x16.reinterpretation_birth_pattern_mode ==
	              HiresReinterpretationBirthPatternMode::NarrowCrossFormatsize32x16Probe,
	      "narrow-32x16-pending mode should keep the 32x16 birth-pattern filter");
	check(narrow_pending_32x16.cross_formatsize_32x16_source_filter ==
	              HiresCrossFormatsize32x16SourceFilter::PendingOnly,
	      "narrow-32x16-pending mode should enable the pending-only 32x16 consumer filter");
	check(narrow_alias_32x16.reinterpretation_birth_pattern_mode ==
	              HiresReinterpretationBirthPatternMode::NarrowCrossFormatsize32x16Probe,
	      "narrow-32x16-alias mode should keep the 32x16 birth-pattern filter");
	check(narrow_alias_32x16.cross_formatsize_32x16_source_filter ==
	              HiresCrossFormatsize32x16SourceFilter::AliasOnly,
	      "narrow-32x16-alias mode should enable the alias-only 32x16 consumer filter");
	check(narrow_32x32_pending_32x16.reinterpretation_birth_pattern_mode ==
	              HiresReinterpretationBirthPatternMode::NarrowSame32x32Cross32x16Probe,
	      "narrow-32x32-pending-32x16 mode should keep the 32x32 + 32x16 birth-pattern filter");
	check(narrow_32x32_pending_32x16.cross_formatsize_32x16_source_filter ==
	              HiresCrossFormatsize32x16SourceFilter::PendingOnly,
	      "narrow-32x32-pending-32x16 mode should enable the pending-only 32x16 consumer filter");
	check(narrow_32x32_alias_32x16.reinterpretation_birth_pattern_mode ==
	              HiresReinterpretationBirthPatternMode::NarrowSame32x32Cross32x16Probe,
	      "narrow-32x32-alias-32x16 mode should keep the 32x32 + 32x16 birth-pattern filter");
	check(narrow_32x32_alias_32x16.cross_formatsize_32x16_source_filter ==
	              HiresCrossFormatsize32x16SourceFilter::AliasOnly,
	      "narrow-32x32-alias-32x16 mode should enable the alias-only 32x16 consumer filter");
	check(narrow_phase_16x16_pending_32x16.reinterpretation_birth_pattern_mode ==
	              HiresReinterpretationBirthPatternMode::NarrowPaperMarioProbe,
	      "narrow-reinterp-phase-16x16-pending-32x16 mode should keep the full Paper Mario birth-pattern filter");
	check(narrow_phase_16x16_pending_32x16.restrict_cross_formatsize_16x16_to_primary_phase,
	      "narrow-reinterp-phase-16x16-pending-32x16 mode should enable the 16x16 primary-phase consumer filter");
	check(narrow_phase_16x16_pending_32x16.cross_formatsize_32x16_source_filter ==
	              HiresCrossFormatsize32x16SourceFilter::PendingOnly,
	      "narrow-reinterp-phase-16x16-pending-32x16 mode should enable the pending-only 32x16 consumer filter");
	check(narrow_phase_16x16_alias_32x16.reinterpretation_birth_pattern_mode ==
	              HiresReinterpretationBirthPatternMode::NarrowPaperMarioProbe,
	      "narrow-reinterp-phase-16x16-alias-32x16 mode should keep the full Paper Mario birth-pattern filter");
	check(narrow_phase_16x16_alias_32x16.restrict_cross_formatsize_16x16_to_primary_phase,
	      "narrow-reinterp-phase-16x16-alias-32x16 mode should enable the 16x16 primary-phase consumer filter");
	check(narrow_phase_16x16_alias_32x16.cross_formatsize_32x16_source_filter ==
	              HiresCrossFormatsize32x16SourceFilter::AliasOnly,
	      "narrow-reinterp-phase-16x16-alias-32x16 mode should enable the alias-only 32x16 consumer filter");
}

struct StubReplacementTileState
{
	uint32_t vk_image_index = hires_invalid_descriptor_index();
	HiresLookupSource lookup_source = HiresLookupSource::None;
	HiresLookupSource origin_lookup_source = HiresLookupSource::None;
	uint8_t source_load_tile_index = 0;
	uint16_t source_load_formatsize = 0;
	uint8_t source_lookup_tile_index = 0;
	uint16_t source_lookup_formatsize = 0;
	uint32_t source_key_width = 0;
	uint32_t source_key_height = 0;
	bool valid = false;
	bool hit = false;
};

struct StubTileInfo
{
	struct
	{
		uint32_t repl_desc_index = hires_invalid_descriptor_index();
		uint32_t repl_w = 0;
		uint32_t repl_h = 0;
	} replacement;
};

static void test_hires_ownership_class_contract()
{
	StubReplacementTileState state = {};
	check(classify_hires_binding_ownership_class(state) == HiresBindingOwnershipClass::None,
	      "empty replacement state should classify as none");

	state.valid = true;
	state.hit = true;
	check(classify_hires_binding_ownership_class(state) == HiresBindingOwnershipClass::UnboundProviderHit,
	      "provider hit without descriptor should classify as unbound");

	state.vk_image_index = 7;
	state.lookup_source = HiresLookupSource::Primary;
	check(classify_hires_binding_ownership_class(state) == HiresBindingOwnershipClass::UploadOwner,
	      "primary owner hit should classify as upload_owner");

	state.lookup_source = HiresLookupSource::PendingBlockRetry;
	check(classify_hires_binding_ownership_class(state) == HiresBindingOwnershipClass::FallbackOwner,
	      "fallback owner hit should classify as fallback_owner");

	state.lookup_source = HiresLookupSource::AliasPropagated;
	check(classify_hires_binding_ownership_class(state) == HiresBindingOwnershipClass::AliasConsumer,
	      "alias-propagated hit should classify as alias_consumer");

	state.lookup_source = HiresLookupSource::BlockTile;
	state.source_lookup_tile_index = 7;
	check(classify_hires_binding_ownership_class(state) == HiresBindingOwnershipClass::AliasConsumer,
	      "non-owner tile lookup should classify as alias_consumer");

	StubReplacementTileState draw_states[2] = {};
	StubTileInfo draw_tiles[2] = {};
	check(classify_hires_draw_ownership_class(false, 0, draw_states, draw_tiles) == HiresDrawOwnershipClass::None,
	      "draw without HIRES state should classify as none");

	draw_states[0] = state;
	draw_tiles[0].replacement.repl_desc_index = 7;
	check(classify_hires_draw_ownership_class(false, 1, draw_states, draw_tiles) == HiresDrawOwnershipClass::AliasConsumer,
	      "descriptor-backed alias hit should classify as alias_consumer draw");

	draw_states[0].lookup_source = HiresLookupSource::Primary;
	draw_states[0].source_lookup_tile_index = 0;
	check(classify_hires_draw_ownership_class(false, 1, draw_states, draw_tiles) == HiresDrawOwnershipClass::UploadOwner,
	      "descriptor-backed primary owner hit should classify as upload_owner draw");

	draw_states[0].lookup_source = HiresLookupSource::PendingBlockRetry;
	check(classify_hires_draw_ownership_class(false, 1, draw_states, draw_tiles) == HiresDrawOwnershipClass::FallbackOwner,
	      "descriptor-backed fallback owner hit should classify as fallback_owner draw");

	check(classify_hires_draw_ownership_class(true, 1, draw_states, draw_tiles) == HiresDrawOwnershipClass::CopyConsumer,
	      "copy-mode draw with HIRES state should classify as copy_consumer");

	draw_tiles[0].replacement.repl_desc_index = hires_invalid_descriptor_index();
	check(classify_hires_draw_ownership_class(false, 0, draw_states, draw_tiles) == HiresDrawOwnershipClass::DescriptorlessConsumer,
	      "descriptorless HIRES draw should classify as descriptorless_consumer");

	draw_tiles[0].replacement.repl_desc_index = 7;
	draw_states[1] = draw_states[0];
	draw_states[1].lookup_source = HiresLookupSource::AliasPropagated;
	draw_tiles[1].replacement.repl_desc_index = 9;
	check(classify_hires_draw_ownership_class(false, 2, draw_states, draw_tiles) == HiresDrawOwnershipClass::Mixed,
	      "mixed ownership draw should classify as mixed");
}

static void test_hires_consumer_archetype_contract()
{
	StubReplacementTileState state = {};
	StubTileInfo tile = {};
	state.valid = true;
	state.hit = true;
	state.vk_image_index = 7;
	state.source_load_tile_index = 7;
	state.source_lookup_tile_index = 0;

	tile.replacement.repl_desc_index = 7;

	state.lookup_source = HiresLookupSource::AliasPropagated;
	state.origin_lookup_source = HiresLookupSource::BlockTile;
	state.source_load_formatsize = 0x202u;
	state.source_lookup_formatsize = 0x02u;
	state.source_key_width = 16u;
	state.source_key_height = 16u;
	tile.replacement.repl_w = 100u;
	tile.replacement.repl_h = 100u;
	check(classify_hires_consumer_archetype(0x21864010u, HiresDrawOwnershipClass::AliasConsumer, state, tile) ==
	              HiresConsumerArchetype::CrossFormatsize16x16PrimaryAliasConsumer,
	      "primary 16x16 alias consumer should classify as cross16x16_primary_alias");
	check(classify_hires_consumer_archetype(0x218640d4u, HiresDrawOwnershipClass::AliasConsumer, state, tile) ==
	              HiresConsumerArchetype::CrossFormatsize16x16SecondaryAliasConsumer,
	      "secondary 16x16 alias consumer should classify as cross16x16_secondary_alias");
	check(classify_hires_consumer_archetype(0x21864010u, HiresDrawOwnershipClass::Mixed, state, tile) ==
	              HiresConsumerArchetype::CrossFormatsize16x16PrimaryOwnerLike,
	      "primary 16x16 mixed draw should classify as cross16x16_primary_owner");
	check(classify_hires_consumer_archetype(0x218640d4u, HiresDrawOwnershipClass::Mixed, state, tile) ==
	              HiresConsumerArchetype::CrossFormatsize16x16SecondaryOwnerLike,
	      "secondary 16x16 mixed draw should classify as cross16x16_secondary_owner");

	state.source_key_width = 32u;
	state.source_key_height = 16u;
	tile.replacement.repl_w = 512u;
	tile.replacement.repl_h = 256u;
	state.lookup_source = HiresLookupSource::PendingBlockRetry;
	state.origin_lookup_source = HiresLookupSource::PendingBlockRetry;
	check(classify_hires_consumer_archetype(0x21844118u, HiresDrawOwnershipClass::Mixed, state, tile) ==
	              HiresConsumerArchetype::CrossFormatsize32x16PendingProducer,
	      "pending 32x16 owner draw should classify as cross32x16_pending");
	state.lookup_source = HiresLookupSource::AliasPropagated;
	state.origin_lookup_source = HiresLookupSource::BlockTile;
	check(classify_hires_consumer_archetype(0x21844118u, HiresDrawOwnershipClass::DescriptorlessConsumer, state, tile) ==
	              HiresConsumerArchetype::CrossFormatsize32x16AliasConsumer,
	      "alias-backed 32x16 consumer should classify as cross32x16_alias");

	state.source_load_formatsize = 0x300u;
	state.source_lookup_formatsize = 0x300u;
	state.source_key_width = 32u;
	state.source_key_height = 32u;
	tile.replacement.repl_w = 320u;
	tile.replacement.repl_h = 320u;
	check(classify_hires_consumer_archetype(0x21844108u, HiresDrawOwnershipClass::AliasConsumer, state, tile) ==
	              HiresConsumerArchetype::SameFormatsize32x32AliasConsumer,
	      "same-format 32x32 alias draw should classify as same32x32_alias");
}
}

int main()
{
	test_cache_path_resolution_precedence();
	test_should_attempt_hires_cache_load_matrix();
	test_classify_hires_configure_outcome_matrix();
	test_descriptor_index_sentinel_contract();
	test_lookup_mode_policy_contract();
	test_hires_ownership_class_contract();
	test_hires_consumer_archetype_contract();
	std::cout << "emu_unit_hires_runtime_policy_test: PASS" << std::endl;
	return 0;
}
