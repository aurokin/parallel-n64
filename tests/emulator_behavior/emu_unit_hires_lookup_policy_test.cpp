#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_lookup_policy.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

using namespace RDP;
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

static void test_hires_rdram_view_valid_matrix()
{
	const uint8_t backing[8] = {};
	check(!hires_rdram_view_valid(nullptr, 8), "null rdram pointer should be invalid");
	check(!hires_rdram_view_valid(backing, 0), "zero rdram size should be invalid");
	check(!hires_rdram_view_valid(backing, 3), "non-power-of-two rdram size should be invalid");
	check(hires_rdram_view_valid(backing, 8), "power-of-two rdram view should be valid");
}

static void test_tlut_shadow_gate()
{
	check(should_update_tlut_shadow(true, true), "tlut shadow should run when rdram view is valid and mode is TLUT");
	check(!should_update_tlut_shadow(false, true), "tlut shadow should not run with invalid rdram view");
	check(!should_update_tlut_shadow(true, false), "tlut shadow should not run outside TLUT mode");
}

static void test_hires_lookup_fast_path_gate()
{
	check(!should_run_hires_lookup(false, true, false, 64, 64), "lookup should not run with invalid rdram view");
	check(!should_run_hires_lookup(true, false, false, 64, 64), "lookup should not run when provider is unavailable");
	check(!should_run_hires_lookup(true, true, true, 64, 64), "lookup should not run in TLUT mode");
	check(!should_run_hires_lookup(true, true, false, 0, 64), "lookup should not run with zero width");
	check(!should_run_hires_lookup(true, true, false, 64, 0), "lookup should not run with zero height");
	check(should_run_hires_lookup(true, true, false, 64, 64), "lookup should run when all prerequisites are met");
}

static void test_ci_palette_candidate_gate_contract()
{
	check(should_try_hires_ci_palette_candidates(TextureFormat::CI, TextureSize::Bpp4, true),
	      "CI4 with TLUT shadow should use palette candidates");
	check(should_try_hires_ci_palette_candidates(TextureFormat::CI, TextureSize::Bpp8, true),
	      "CI8 with TLUT shadow should use palette candidates");
	check(!should_try_hires_ci_palette_candidates(TextureFormat::CI, TextureSize::Bpp16, true),
	      "CI16 should skip palette candidate path");
	check(!should_try_hires_ci_palette_candidates(TextureFormat::RGBA, TextureSize::Bpp8, true),
	      "non-CI formats should skip palette candidate path");
	check(!should_try_hires_ci_palette_candidates(TextureFormat::CI, TextureSize::Bpp8, false),
	      "invalid TLUT shadow should skip palette candidate path");
}

static void test_ci_ambiguous_fallback_gate_contract()
{
	check(should_accept_hires_ci_ambiguous_fallback(true, 0x12345678u, false),
	      "explicitly allowed ambiguous CI fallback should be accepted");
	check(should_accept_hires_ci_ambiguous_fallback(false, 0u, false),
	      "missing preferred palette hint should allow ambiguous CI fallback");
	check(should_accept_hires_ci_ambiguous_fallback(false, 0x12345678u, true),
	      "matching preferred palette should allow ambiguous CI fallback");
	check(!should_accept_hires_ci_ambiguous_fallback(false, 0x12345678u, false),
	      "mismatched preferred palette should reject ambiguous CI fallback");
}

static void test_strict_lookup_gate_contract()
{
	const auto permissive_policy = resolve_hires_lookup_mode_policy(0);
	const auto strict_policy = resolve_hires_lookup_mode_policy(1);
	const auto owner_policy = resolve_hires_lookup_mode_policy(2);
	const auto no_reinterp_policy = resolve_hires_lookup_mode_policy(3);
	const auto owner_reinterp_policy = resolve_hires_lookup_mode_policy(4);

	check(!hires_lookup_strict_enabled(0), "lookup mode 0 should be permissive");
	check(hires_lookup_strict_enabled(1), "lookup mode 1 should be strict");
	check(!hires_lookup_strict_enabled(2), "lookup mode 2 should not force strict provider matching");
	check(!hires_lookup_strict_enabled(3), "lookup mode 3 should not force strict provider matching");
	check(!hires_lookup_strict_enabled(4), "lookup mode 4 should not force strict provider matching");
	check(!hires_lookup_owner_only_enabled(0), "lookup mode 0 should not be owner-only");
	check(!hires_lookup_owner_only_enabled(1), "lookup mode 1 should not be owner-only");
	check(hires_lookup_owner_only_enabled(2), "lookup mode 2 should be owner-only");
	check(!hires_lookup_owner_only_enabled(3), "lookup mode 3 should not be owner-only");
	check(!hires_lookup_owner_only_enabled(4), "lookup mode 4 should not be owner-only");
	check(!hires_lookup_no_reinterpretation_enabled(0), "lookup mode 0 should allow reinterpretation");
	check(!hires_lookup_no_reinterpretation_enabled(1), "lookup mode 1 should not be the no-reinterp mode");
	check(!hires_lookup_no_reinterpretation_enabled(2), "lookup mode 2 should not be the no-reinterp mode");
	check(hires_lookup_no_reinterpretation_enabled(3), "lookup mode 3 should be no-reinterp");
	check(!hires_lookup_no_reinterpretation_enabled(4), "lookup mode 4 should not be the no-reinterp mode");
	check(!hires_lookup_owner_reinterpretation_enabled(0), "lookup mode 0 should not be owner-reinterp");
	check(!hires_lookup_owner_reinterpretation_enabled(1), "lookup mode 1 should not be owner-reinterp");
	check(!hires_lookup_owner_reinterpretation_enabled(2), "lookup mode 2 should not be owner-reinterp");
	check(!hires_lookup_owner_reinterpretation_enabled(3), "lookup mode 3 should not be owner-reinterp");
	check(hires_lookup_owner_reinterpretation_enabled(4), "lookup mode 4 should be owner-reinterp");
	check(hires_lookup_fallbacks_enabled(0), "lookup mode 0 should keep fallbacks enabled");
	check(!hires_lookup_fallbacks_enabled(1), "lookup mode 1 should disable fallbacks");
	check(!hires_lookup_fallbacks_enabled(2), "lookup mode 2 should disable fallbacks");
	check(hires_lookup_fallbacks_enabled(3), "lookup mode 3 should keep non-reinterpretation fallbacks enabled");
	check(hires_lookup_fallbacks_enabled(4), "lookup mode 4 should keep reinterpretation fallbacks enabled");
	check(hires_lookup_block_reinterpretation_enabled(0), "lookup mode 0 should allow block reinterpretation");
	check(!hires_lookup_block_reinterpretation_enabled(1), "lookup mode 1 should disable block reinterpretation");
	check(!hires_lookup_block_reinterpretation_enabled(2), "lookup mode 2 should disable block reinterpretation");
	check(!hires_lookup_block_reinterpretation_enabled(3), "lookup mode 3 should disable block reinterpretation");
	check(hires_lookup_block_reinterpretation_enabled(4), "lookup mode 4 should allow block reinterpretation");
	check(hires_lookup_pending_block_retry_enabled(0), "lookup mode 0 should allow pending block retry");
	check(!hires_lookup_pending_block_retry_enabled(1), "lookup mode 1 should disable pending block retry");
	check(!hires_lookup_pending_block_retry_enabled(2), "lookup mode 2 should disable pending block retry");
	check(!hires_lookup_pending_block_retry_enabled(3), "lookup mode 3 should disable pending block retry");
	check(hires_lookup_pending_block_retry_enabled(4), "lookup mode 4 should allow pending block retry");
	check(should_try_hires_ci_low32_fallback(permissive_policy), "permissive lookup should allow CI low32 fallback");
	check(!should_try_hires_ci_low32_fallback(strict_policy), "strict lookup should reject CI low32 fallback");
	check(!should_try_hires_ci_low32_fallback(owner_policy), "owner lookup should reject CI low32 fallback");
	check(!should_try_hires_ci_low32_fallback(no_reinterp_policy), "no-reinterp lookup should reject CI low32 fallback");
	check(!should_try_hires_ci_low32_fallback(owner_reinterp_policy), "owner-reinterp lookup should reject CI low32 fallback");
	check(should_try_hires_tile_mask_fallback(permissive_policy, true),
	      "permissive lookup should allow tile-mask fallback");
	check(!should_try_hires_tile_mask_fallback(strict_policy, true),
	      "strict lookup should reject tile-mask fallback");
	check(!should_try_hires_tile_mask_fallback(owner_policy, true),
	      "owner lookup should reject tile-mask fallback");
	check(!should_try_hires_tile_mask_fallback(no_reinterp_policy, true),
	      "no-reinterp lookup should reject tile-mask fallback");
	check(!should_try_hires_tile_mask_fallback(owner_reinterp_policy, true),
	      "owner-reinterp lookup should reject tile-mask fallback");
	check(should_try_hires_tile_stride_fallback(permissive_policy, true),
	      "permissive lookup should allow tile-stride fallback");
	check(!should_try_hires_tile_stride_fallback(strict_policy, true),
	      "strict lookup should reject tile-stride fallback");
	check(!should_try_hires_tile_stride_fallback(owner_policy, true),
	      "owner lookup should reject tile-stride fallback");
	check(!should_try_hires_tile_stride_fallback(no_reinterp_policy, true),
	      "no-reinterp lookup should reject tile-stride fallback");
	check(!should_try_hires_tile_stride_fallback(owner_reinterp_policy, true),
	      "owner-reinterp lookup should reject tile-stride fallback");
	check(should_try_hires_block_tile_fallback(permissive_policy, true),
	      "permissive lookup should allow block-tile fallback");
	check(!should_try_hires_block_tile_fallback(strict_policy, true),
	      "strict lookup should reject block-tile fallback");
	check(!should_try_hires_block_tile_fallback(owner_policy, true),
	      "owner lookup should reject block-tile fallback");
	check(!should_try_hires_block_tile_fallback(no_reinterp_policy, true),
	      "no-reinterp lookup should reject block-tile fallback");
	check(should_try_hires_block_tile_fallback(owner_reinterp_policy, true),
	      "owner-reinterp lookup should allow block-tile fallback");
	check(should_try_hires_block_shape_fallback(permissive_policy, true),
	      "permissive lookup should allow block-shape fallback");
	check(!should_try_hires_block_shape_fallback(strict_policy, true),
	      "strict lookup should reject block-shape fallback");
	check(!should_try_hires_block_shape_fallback(owner_policy, true),
	      "owner lookup should reject block-shape fallback");
	check(!should_try_hires_block_shape_fallback(no_reinterp_policy, true),
	      "no-reinterp lookup should reject block-shape fallback");
	check(should_try_hires_block_shape_fallback(owner_reinterp_policy, true),
	      "owner-reinterp lookup should allow block-shape fallback");
}

static void test_lookup_birth_family_contract()
{
	const auto same_owner = make_hires_lookup_birth_signature(7, 0x300, 0, 0x300, 32, 32);
	check(!is_hires_lookup_birth_cross_formatsize(same_owner),
	      "same formatsize signature should not be cross-formatsize");
	check(is_hires_lookup_birth_owner_tile(same_owner),
	      "lookup tile zero should be owner-tile family");
	check(classify_hires_lookup_birth_family(same_owner) == HiresLookupBirthFamily::SameFormatsizeOwnerTile,
	      "same-format owner signature family mismatch");

	const auto same_alias = make_hires_lookup_birth_signature(7, 0x204, 7, 0x204, 4, 32);
	check(!is_hires_lookup_birth_cross_formatsize(same_alias),
	      "same formatsize alias signature should not be cross-formatsize");
	check(!is_hires_lookup_birth_owner_tile(same_alias),
	      "non-zero lookup tile should be alias-tile family");
	check(classify_hires_lookup_birth_family(same_alias) == HiresLookupBirthFamily::SameFormatsizeAliasTile,
	      "same-format alias signature family mismatch");

	const auto cross_owner = make_hires_lookup_birth_signature(7, 0x202, 0, 0x02, 16, 16);
	check(is_hires_lookup_birth_cross_formatsize(cross_owner),
	      "cross formatsize owner signature should be cross-formatsize");
	check(is_hires_lookup_birth_owner_tile(cross_owner),
	      "cross owner signature should still be owner-tile");
	check(classify_hires_lookup_birth_family(cross_owner) == HiresLookupBirthFamily::CrossFormatsizeOwnerTile,
	      "cross-format owner signature family mismatch");

	const auto cross_alias = make_hires_lookup_birth_signature(7, 0x202, 7, 0x02, 32, 16);
	check(is_hires_lookup_birth_cross_formatsize(cross_alias),
	      "cross formatsize alias signature should be cross-formatsize");
	check(!is_hires_lookup_birth_owner_tile(cross_alias),
	      "cross alias signature should not be owner-tile");
	check(classify_hires_lookup_birth_family(cross_alias) == HiresLookupBirthFamily::CrossFormatsizeAliasTile,
	      "cross-format alias signature family mismatch");

	const auto permissive_policy = resolve_hires_lookup_mode_policy(0);
	const auto owner_reinterp_policy = resolve_hires_lookup_mode_policy(4);
	check(should_accept_hires_reinterpretation_birth_family(permissive_policy, same_owner),
	      "permissive policy should allow same-format owner reinterpretation families");
	check(should_accept_hires_reinterpretation_birth_family(permissive_policy, same_alias),
	      "permissive policy should allow same-format alias reinterpretation families");
	check(should_accept_hires_reinterpretation_birth_family(permissive_policy, cross_owner),
	      "permissive policy should allow cross-format owner reinterpretation families");
	check(should_accept_hires_reinterpretation_birth_family(permissive_policy, cross_alias),
	      "permissive policy should allow cross-format alias reinterpretation families");
	check(should_accept_hires_reinterpretation_birth_family(owner_reinterp_policy, same_owner),
	      "owner-reinterp policy should allow same-format owner reinterpretation families");
	check(!should_accept_hires_reinterpretation_birth_family(owner_reinterp_policy, same_alias),
	      "owner-reinterp policy should reject same-format alias reinterpretation families");
	check(should_accept_hires_reinterpretation_birth_family(owner_reinterp_policy, cross_owner),
	      "owner-reinterp policy should allow cross-format owner reinterpretation families");
	check(!should_accept_hires_reinterpretation_birth_family(owner_reinterp_policy, cross_alias),
	      "owner-reinterp policy should reject cross-format alias reinterpretation families");
}

static void test_hires_key_base_addr_contract()
{
	const uint32_t tex_addr = 0x1000u;
	const uint32_t tex_width = 64u;
	const uint32_t key_start_x = 8u;
	const uint32_t key_start_y = 2u;

	check(compute_hires_key_base_addr(
				tex_addr,
				tex_width,
				key_start_x,
				key_start_y,
				TextureSize::Bpp16,
				true) == tex_addr,
	      "load-block key base address should ignore key start offsets");

	check(compute_hires_key_base_addr(
				tex_addr,
				tex_width,
				key_start_x,
				key_start_y,
				TextureSize::Bpp8,
				false) == 0x1088u,
	      "8bpp non-block key base address mismatch");

	check(compute_hires_key_base_addr(
				tex_addr,
				tex_width,
				key_start_x,
				key_start_y,
				TextureSize::Bpp16,
				false) == 0x1110u,
	      "16bpp non-block key base address mismatch");

	check(compute_hires_key_base_addr(
				tex_addr,
				tex_width,
				key_start_x,
				key_start_y,
				TextureSize::Bpp4,
				false) == 0x1044u,
	      "4bpp non-block key base address mismatch");

	check(compute_hires_block_probe_base_addr(
				tex_addr,
				16u,
				key_start_x,
				key_start_y,
				TextureSize::Bpp4) == 0x1014u,
	      "block probe base address should rebase against the sampled sub-rectangle");
}

static void test_hires_texture_byte_layout_contract()
{
	check(compute_hires_texture_row_bytes(7, TextureSize::Bpp4) == 4,
	      "4bpp row byte computation should round up odd widths");
	check(compute_hires_texture_row_bytes(8, TextureSize::Bpp8) == 8,
	      "8bpp row byte computation mismatch");
	check(compute_hires_texture_row_bytes(4, TextureSize::Bpp16) == 8,
	      "16bpp row byte computation mismatch");
	check(compute_hires_texture_row_bytes(4, TextureSize::Bpp32) == 16,
	      "32bpp row byte computation mismatch");

	check(compute_hires_texture_total_bytes(64, 1, TextureSize::Bpp16) == 128,
	      "texture byte size mismatch for 64x1 16bpp block");
	check(compute_hires_block_reinterpret_height(128, 32, TextureSize::Bpp16) == 2,
	      "block reinterpret height mismatch for 32x2 16bpp");
	check(compute_hires_block_reinterpret_height(128, 4, TextureSize::Bpp16) == 16,
	      "block reinterpret height mismatch for 4x16 16bpp");
	check(compute_hires_block_reinterpret_height(128, 7, TextureSize::Bpp16) == 0,
	      "block reinterpret height should reject incompatible row sizes");
}

static void test_hires_tile_lookup_dim_contract()
{
	check(derive_hires_tile_lookup_dim(64, 0, 0) == 64,
	      "raw tile lookup dim should pass through without mask or max");
	check(derive_hires_tile_lookup_dim(64, 3, 0) == 8,
	      "tile mask should clamp lookup dim");
	check(derive_hires_tile_lookup_dim(64, 6, 32) == 32,
	      "max dimension should clamp lookup dim after mask");
	check(derive_hires_tile_lookup_dim(16, 3, 64) == 8,
	      "mask clamp should apply when max dimension is larger");
}

static void test_hires_block_dxt_policy_contract()
{
	check(hires_calculate_dxt(0) == 1,
	      "calculate_dxt should map zero txl2words to 1");
	check(hires_calculate_dxt(1) == 2048,
	      "calculate_dxt mismatch for one txl2words");

	check(hires_txl2words(16, TextureSize::Bpp4) == 1,
	      "txl2words mismatch for 4bpp");
	check(hires_txl2words(16, TextureSize::Bpp8) == 2,
	      "txl2words mismatch for 8bpp");
	check(hires_txl2words(16, TextureSize::Bpp16) == 4,
	      "txl2words mismatch for 16bpp");
	check(hires_txl2words(16, TextureSize::Bpp32) == 8,
	      "txl2words mismatch for 32bpp");

	check(hires_reverse_dxt(0x800, 64, 16, TextureSize::Bpp4) == 1,
	      "reverse_dxt should normalize 0x800 to 1");
	check(hires_reverse_dxt(1024, 64, 16, TextureSize::Bpp4) == 2,
	      "reverse_dxt mismatch for dxt=1024");
	check(hires_reverse_dxt(128, 64, 64, TextureSize::Bpp32) == 16,
	      "reverse_dxt mismatch for dxt=128");

	check(compute_hires_block_row_stride_bytes(0, 64, 16, TextureSize::Bpp4, 32) == 32,
	      "block row stride should use tile stride when dxt is zero");
	check(compute_hires_block_row_stride_bytes(0x800, 64, 16, TextureSize::Bpp4, 32) == 8,
	      "block row stride mismatch for dxt=0x800");
	check(compute_hires_block_row_stride_bytes(1024, 64, 16, TextureSize::Bpp4, 32) == 16,
	      "block row stride mismatch for dxt=1024");

	check(compute_hires_width_from_row_stride(8, TextureSize::Bpp4) == 16,
	      "row-stride to width mismatch for 4bpp");
	check(compute_hires_width_from_row_stride(8, TextureSize::Bpp8) == 8,
	      "row-stride to width mismatch for 8bpp");
	check(compute_hires_width_from_row_stride(8, TextureSize::Bpp16) == 4,
	      "row-stride to width mismatch for 16bpp");
	check(compute_hires_width_from_row_stride(8, TextureSize::Bpp32) == 2,
	      "row-stride to width mismatch for 32bpp");
}

static void test_lookup_counter_updates()
{
	uint64_t total = 10;
	uint64_t hits = 4;
	uint64_t misses = 6;

	record_hires_lookup_result(true, total, hits, misses);
	check(total == 11 && hits == 5 && misses == 6, "hit counter update mismatch");

	record_hires_lookup_result(false, total, hits, misses);
	check(total == 12 && hits == 5 && misses == 7, "miss counter update mismatch");
}

static void test_descriptor_binding_result_updates()
{
	uint64_t total = 0;
	uint64_t provider_hits = 0;
	uint64_t provider_misses = 0;
	uint64_t descriptor_bound_hits = 0;
	uint64_t descriptor_unbound_hits = 0;

	check(did_hires_lookup_bind_descriptor(true, 7), "valid descriptor should count as bound on provider hit");
	check(!did_hires_lookup_bind_descriptor(true, 0xffffffffu), "invalid descriptor should not count as bound");
	check(!did_hires_lookup_bind_descriptor(false, 7), "provider miss should never count as descriptor bound");

	record_hires_lookup_binding_result(
			true,
			did_hires_lookup_bind_descriptor(true, 5),
			total,
			provider_hits,
			provider_misses,
			descriptor_bound_hits,
			descriptor_unbound_hits);
	check(total == 1 && provider_hits == 1 && provider_misses == 0,
	      "provider hit accounting mismatch");
	check(descriptor_bound_hits == 1 && descriptor_unbound_hits == 0,
	      "descriptor bound accounting mismatch");

	record_hires_lookup_binding_result(
			true,
			did_hires_lookup_bind_descriptor(true, 0xffffffffu),
			total,
			provider_hits,
			provider_misses,
			descriptor_bound_hits,
			descriptor_unbound_hits);
	check(total == 2 && provider_hits == 2 && provider_misses == 0,
	      "provider hit accounting mismatch on unbound descriptor");
	check(descriptor_bound_hits == 1 && descriptor_unbound_hits == 1,
	      "descriptor unbound accounting mismatch");

	record_hires_lookup_binding_result(
			false,
			did_hires_lookup_bind_descriptor(false, 3),
			total,
			provider_hits,
			provider_misses,
			descriptor_bound_hits,
			descriptor_unbound_hits);
	check(total == 3 && provider_hits == 2 && provider_misses == 1,
	      "provider miss accounting mismatch");
	check(descriptor_bound_hits == 1 && descriptor_unbound_hits == 1,
	      "descriptor counters should not change on provider miss");
}
}

int main()
{
	test_hires_rdram_view_valid_matrix();
	test_tlut_shadow_gate();
	test_hires_lookup_fast_path_gate();
	test_ci_palette_candidate_gate_contract();
	test_ci_ambiguous_fallback_gate_contract();
	test_strict_lookup_gate_contract();
	test_lookup_birth_family_contract();
	test_hires_key_base_addr_contract();
	test_hires_texture_byte_layout_contract();
	test_hires_tile_lookup_dim_contract();
	test_hires_block_dxt_policy_contract();
	test_lookup_counter_updates();
	test_descriptor_binding_result_updates();

	std::cout << "emu_unit_hires_lookup_policy_test: PASS" << std::endl;
	return 0;
}
