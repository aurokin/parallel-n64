#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_ci_binding_policy.hpp"
#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_load_origin_policy.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
using namespace RDP;

void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

constexpr uint16_t ci4 = uint16_t(TextureFormat::CI) | (uint16_t(TextureSize::Bpp4) << 8);
constexpr uint16_t ci8 = uint16_t(TextureFormat::CI) | (uint16_t(TextureSize::Bpp8) << 8);
}

int main()
{
	using namespace RDP;
	using namespace RDP::detail;

	const auto bank_a = make_hires_ci4_palette_binding(ci4, 0);
	check(hires_ci4_binding_matches(bank_a, TextureFormat::CI, TextureSize::Bpp4, 0, true),
	      "unchanged bank retains its replacement");
	// Texture bytes and TMEM offset remain unchanged: SetTile alone changes
	// the palette. Neither TEXEL0 nor TEXEL1 may retain bank A at that draw.
	for (uint32_t bank_b = 1; bank_b < 16; bank_b++)
	{
		check(!hires_ci4_binding_matches(bank_a, TextureFormat::CI, TextureSize::Bpp4, bank_b, true),
		      "live CI4 bank switch rejects the prior replacement");
		const auto replacement_b = make_hires_ci4_palette_binding(ci4, bank_b);
		check(hires_ci4_binding_matches(replacement_b, TextureFormat::CI, TextureSize::Bpp4, bank_b, true),
		      "replacement keyed for the new bank can serve");
		check(hires_ci4_binding_matches(bank_a, TextureFormat::CI, TextureSize::Bpp4, bank_b, false),
		      "an unsampled peer with a pending descriptor retains its binding");
	}

	// A load can propagate before the render tile's SetTile arrives. Its
	// stale intermediate palette is harmless until it is actually sampled.
	const auto pending_load = make_hires_ci4_palette_binding(ci4, 3);
	check(hires_ci4_binding_matches(pending_load, TextureFormat::CI, TextureSize::Bpp4, 7, false),
	      "load-before-SetTile preserves pending replacement provenance");
	check(hires_ci4_binding_matches(pending_load, TextureFormat::CI, TextureSize::Bpp4, 3, true),
	      "final matching SetTile allows the propagated replacement");
	check(!hires_ci4_binding_matches(pending_load, TextureFormat::CI, TextureSize::Bpp4, 7, true),
	      "final mismatching SetTile cannot serve the pending replacement");

	for (uint32_t stale_palette = 0; stale_palette < 16; stale_palette++)
	{
		const auto reinterpreted = make_hires_ci4_palette_binding(
			ci4, hires_ci4_lookup_palette(TextureSize::Bpp8, stale_palette));
		check(reinterpreted.valid && reinterpreted.palette == 0,
		      "CI8-to-CI4 lookup ignores the CI8 descriptor's stale palette field");
		check(hires_ci4_lookup_palette(TextureSize::Bpp4, stale_palette) == stale_palette,
		      "direct CI4 lookup preserves its actual bank");
	}

	const auto not_ci4 = make_hires_ci4_palette_binding(ci8, 4);
	check(!not_ci4.valid, "a new non-CI4 lookup resets prior CI4 provenance");
	check(hires_ci4_binding_matches(not_ci4, TextureFormat::CI, TextureSize::Bpp8, 7, true),
	      "CI8 ignores bank selection");
	check(hires_ci4_binding_matches(bank_a, TextureFormat::RGBA, TextureSize::Bpp16, 7, true),
	      "non-CI draws are unaffected by the CI4 palette guard");
	check(!hires_ci4_binding_matches(not_ci4, TextureFormat::CI, TextureSize::Bpp4, 0, true),
	      "unqualified CI4 provenance cannot serve even bank zero");

	HiresLoadOrigin target;
	target.format = TextureFormat::CI;
	target.size = TextureSize::Bpp4;
	target.stride = 24;
	target.width = 48;
	target.height = 72;
	HiresLoadOrigin peers[2] = { target, target };
	peers[0].address = 0x123400;
	check(find_hires_load_origin(target, peers, 2) == peers[0].address,
	      "missing live tile recovers a matching peer's index origin without copying its palette binding");
	peers[1].address = peers[0].address;
	check(find_hires_load_origin(target, peers, 2) == peers[0].address,
	      "multiple agreeing peers establish one load origin");
	peers[1].address += 0x1000;
	check(find_hires_load_origin(target, peers, 2) == 0,
	      "ambiguous load origins preserve native fallback");
	auto already_tracked = target;
	already_tracked.address = 0x987600;
	check(find_hires_load_origin(already_tracked, peers, 2) == already_tracked.address,
	      "known local origin is not overwritten by conflicting peer inference");
	auto unknown_extent = target;
	unknown_extent.width = 0;
	check(find_hires_load_origin(unknown_extent, peers, 2) == 0,
	      "unknown native extent cannot establish an origin");
	peers[1].width++;
	check(find_hires_load_origin(target, peers, 2) == peers[0].address,
	      "a different native extent cannot conflict with the matching peer");
	for (unsigned mismatch = 0; mismatch < 6; mismatch++)
	{
		auto unrelated = peers[0];
		switch (mismatch)
		{
		case 0: unrelated.offset += 8; break;
		case 1: unrelated.stride += 8; break;
		case 2: unrelated.format = TextureFormat::RGBA; break;
		case 3: unrelated.size = TextureSize::Bpp8; break;
		case 4: unrelated.width++; break;
		case 5: unrelated.height++; break;
		}
		check(find_hires_load_origin(target, &unrelated, 1) == 0,
		      "a different index view cannot establish the missing load origin");
	}
	const uint32_t tmem_bytes = 0x1000;
	const uint32_t origin_span = target.stride * target.height;
	// Origins remain relevant after a replacement was rejected. A later
	// write overlapping their byte window must retire those addresses too.
	check(hires_tmem_spans_overlap(0x100, 32, target.offset, origin_span, tmem_bytes),
	      "interior write overlaps an address-only tile's index window");
	peers[0].address = 0;
	peers[1].address = 0;
	check(find_hires_load_origin(target, peers, 2) == 0,
	      "retired address-only origins cannot be adopted by another live tile");
	check(!hires_tmem_spans_overlap(0x900, 32, target.offset, origin_span, tmem_bytes),
	      "nonoverlapping palette write preserves the index origin");
	check(hires_tmem_spans_overlap(0xff0, 32, 0, 8, tmem_bytes),
	      "wrapped load invalidates the beginning of TMEM");
	std::cout << "emu_unit_hires_ci_binding_test: PASS\n";
}
