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
	test_hires_key_base_addr_contract();
	test_hires_texture_byte_layout_contract();
	test_hires_tile_lookup_dim_contract();
	test_hires_block_dxt_policy_contract();
	test_lookup_counter_updates();
	test_descriptor_binding_result_updates();

	std::cout << "emu_unit_hires_lookup_policy_test: PASS" << std::endl;
	return 0;
}
