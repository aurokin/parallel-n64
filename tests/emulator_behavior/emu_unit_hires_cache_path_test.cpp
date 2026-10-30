// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_runtime_policy.hpp"

namespace
{
void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}
}

int main(int argc, char **argv)
{
	using namespace RDP::detail;
	if (argc == 2)
	{
		const std::string hex = argv[1];
		check(hex.size() == 128, "ident probe takes a 64-byte header in hex");
		uint8_t header[64];
		for (unsigned i = 0; i < 64; i++)
			header[i] = uint8_t(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
		std::cout << hires_game_ident(header) << '\n';
		return 0;
	}
	using Paths = std::vector<std::string>;
	using Location = HiresListingLocation;
	const auto hires = Location::Hires;
	const std::vector<HiresListingEntry> listing = {
		{ "/hires/super mario 64", hires, true, { "z.phrb", ".hidden.phrb", "A.PHRB", "loose.png" } },
		{ "/hires/SUPER MARIO 64", hires, true, { "second.phrb", "first.phrb" } },
		{ "/hires/super mario 64.phrb", hires, false, {} },
		{ "/hires/SUPER MARIO 64.PHRB", hires, false, {} },
		{ "/hires/.SUPER MARIO 64.phrb", hires, false, {} },
		{ "/hires/OTHER GAME", hires, true, { "wrong.phrb" } },
	};
	const auto selected = select_hires_packages("SUPER MARIO 64", "SUPER MARIO 64", listing);
	check(selected.paths == Paths{ "/hires/SUPER MARIO 64.PHRB", "/hires/super mario 64.phrb",
	                               "/hires/SUPER MARIO 64/first.phrb", "/hires/SUPER MARIO 64/second.phrb",
	                               "/hires/super mario 64/A.PHRB", "/hires/super mario 64/z.phrb" },
	      "case-insensitive matches load files first, then directories, in byte order; dotfiles are skipped");
	check(selected.near_misses.empty() && selected.unconverted.empty(), "exact packages are not hints");
	const auto mario = select_hires_packages("MARIOKART64", "MARIOKART64", {
		{ "/hires/Mario Kart 64", hires, true, { "wrong.phrb" } },
	});
	check(mario.paths.empty() && mario.near_misses == Paths{ "/hires/Mario Kart 64" },
	      "a punctuation/space near miss is reported, never loaded");
	const auto fzero = select_hires_packages("F-ZERO X", "F-ZERO X", {
		{ "/hires/FZEROX.phrb", hires, false, {} },
	});
	check(fzero.paths.empty() && fzero.near_misses == Paths{ "/hires/FZEROX.phrb" },
	      "the FZEROX patched header must never alias to retail");
	const auto stadium = select_hires_packages("CP2J", "POKEMON STADIUM 2", {
		{ "/hires/POKEMON STADIUM 2", hires, true, { "western.phrb" } },
	});
	check(stadium.paths.empty() && stadium.near_misses == Paths{ "/hires/POKEMON STADIUM 2" },
	      "the Western Stadium name is diagnostic-only for CP2J");
	check(select_hires_packages("", "", listing).paths.empty(), "no identity selects nothing");

	// Each legacy location is a diagnostic source only. Use the GLideN64 name
	// even when the runtime identity is the exception cart's code.
	for (const auto &hint : std::vector<HiresListingEntry>{
		{ "/hires/POKEMON STADIUM 2_HIRESTEXTURES.HTS", hires, false, {} },
		{ "/hires/POKEMON STADIUM 2_HIRESTEXTURES.htc", hires, false, {} },
		{ "/hires/pokemon stadium 2", hires, true, { "pack.HTC" } },
		{ "/hires/pokemon stadium 2", hires, true, { "pack.hts" } },
		{ "/hires/pokemon stadium 2", hires, true, { "texture.PNG" } },
		{ "/hires/pokemon stadium 2", hires, true, { "texture.dds" } },
		{ "/Mupen64plus/cache/POKEMON STADIUM 2_HIRESTEXTURES.hts", Location::LegacyCache, false, {} },
		{ "/Mupen64plus/cache/POKEMON STADIUM 2_HIRESTEXTURES.htc", Location::LegacyCache, false, {} },
		{ "/Mupen64plus/hires_texture/POKEMON STADIUM 2", Location::LegacyTextures, true, {} },
	})
	{
		const auto result = select_hires_packages("CP2J", "POKEMON STADIUM 2", { hint });
		check(result.paths.empty() && result.unconverted == Paths{ hint.path }, "each unconverted source location is reported");
	}
	const auto converted = select_hires_packages("MARIOKART64", "MARIOKART64", {
		{ "/hires/MARIOKART64", hires, true, { "pack.phrb", "texture.png" } },
		{ "/hires/OTHER_HIRESTEXTURES.hts", hires, false, {} },
		{ "/hires/MARIOKART64_MEMORYCACHE.htc", hires, false, {} },
	});
	check(converted.unconverted.empty(), "converted directories, other games and enhancement caches are not hints");
	return 0;
}
