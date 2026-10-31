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
	const std::vector<HiresListingEntry> listing = {
		{ "/hires/super mario 64", true, { "z.phrb", ".hidden.phrb", "A.PHRB", "loose.png" } },
		{ "/hires/SUPER MARIO 64", true, { "second.phrb", "first.phrb" } },
		{ "/hires/super mario 64.phrb", false, {} },
		{ "/hires/SUPER MARIO 64.PHRB", false, {} },
		{ "/hires/.SUPER MARIO 64.phrb", false, {} },
		{ "/hires/OTHER GAME", true, { "wrong.phrb" } },
	};
	const auto selected = select_hires_packages("SUPER MARIO 64", "SUPER MARIO 64", listing);
	check(selected.paths == Paths{ "/hires/SUPER MARIO 64.PHRB", "/hires/super mario 64.phrb",
	                               "/hires/SUPER MARIO 64/first.phrb", "/hires/SUPER MARIO 64/second.phrb",
	                               "/hires/super mario 64/A.PHRB", "/hires/super mario 64/z.phrb" },
	      "case-insensitive matches load files first, then directories, in byte order; dotfiles are skipped");
	check(selected.near_misses.empty() && selected.unconverted.empty(), "exact packages are not hints");
	const auto mario = select_hires_packages("MARIOKART64", "MARIOKART64", {
		{ "/hires/Mario Kart 64", true, { "wrong.phrb" } },
	});
	check(mario.paths.empty() && mario.near_misses == Paths{ "/hires/Mario Kart 64" },
	      "a punctuation/space near miss is reported, never loaded");
	const auto fzero = select_hires_packages("F-ZERO X", "F-ZERO X", {
		{ "/hires/FZEROX.phrb", false, {} },
	});
	check(fzero.paths.empty() && fzero.near_misses == Paths{ "/hires/FZEROX.phrb" },
	      "the FZEROX patched header must never alias to retail");
	for (const auto &code : { "CPSJ", "CP2J" })
	{
		const auto stadium = select_hires_packages(code, "POKEMON STADIUM 2", {
			{ "/hires/POKEMON STADIUM 2", true, { "western.phrb" } },
		});
		check(stadium.paths.empty() && stadium.near_misses.empty() &&
		      stadium.different_game == Paths{ "/hires/POKEMON STADIUM 2" },
		      "a shared Western Stadium name identifies a different game, never rename advice");
	}
	for (const auto &name : { std::string("INVALID*NAME"), std::string("\xb6\xc0\xb6\xc5") })
	{
		const auto path = "/hires/" + name;
		const auto fallback = select_hires_packages("ABCD", name, { { path, true, { "pack.phrb" } } });
		check(fallback.paths.empty() && fallback.different_game.empty() && fallback.near_misses == Paths{ path },
		      "unusable GLideN64 names retain rename advice for the code identity");
	}
	const auto blank = select_hires_packages("ABCD", "", { { "/hires/A B C D", true, { "pack.phrb" } } });
	check(blank.near_misses == Paths{ "/hires/A B C D" } && blank.different_game.empty(),
	      "blank names retain code near-miss advice");
	check(select_hires_packages("", "", listing).paths.empty(), "no identity selects nothing");

	// Sources in the hires root are diagnostic only, using the GLideN64 name.
	for (const auto &hint : std::vector<HiresListingEntry>{
		{ "/hires/POKEMON STADIUM 2_HIRESTEXTURES.HTS", false, {} },
		{ "/hires/POKEMON STADIUM 2_HIRESTEXTURES.htc", false, {} },
		{ "/hires/pokemon stadium 2", true, { "pack.HTC" } },
		{ "/hires/pokemon stadium 2", true, { "pack.hts" } },
		{ "/hires/pokemon stadium 2", true, { "texture.PNG" } },
		{ "/hires/pokemon stadium 2", true, { "texture.dds" } },
	})
	{
		const auto result = select_hires_packages("CP2J", "POKEMON STADIUM 2", { hint });
		check(result.paths.empty() && result.unconverted == Paths{ hint.path }, "each unconverted source location is reported");
	}
	const auto legacy = hires_legacy_sources("/system", "POKEMON STADIUM 2");
	check(legacy.size() == 3 &&
	      legacy[0].path == "/system/Mupen64plus/cache/POKEMON STADIUM 2_HIRESTEXTURES.hts" && !legacy[0].directory &&
	      legacy[1].path == "/system/Mupen64plus/cache/POKEMON STADIUM 2_HIRESTEXTURES.htc" && !legacy[1].directory &&
	      legacy[2].path == "/system/Mupen64plus/hires_texture/POKEMON STADIUM 2" && legacy[2].directory,
	      "legacy hints probe only the exact named files and texture directory");
	check(hires_legacy_sources("/system", "").empty(), "blank GLideN64 names do not probe legacy roots");
	const auto overridden = resolve_hires_cache_path(true, "/system", "SUPER MARIO 64", "SUPER MARIO 64",
	                                                  "/override", { { "/hires/SUPER MARIO 64.phrb", false, {} },
	                                                                 { "/hires/SuperMario64", true, {} },
	                                                                 { "/hires/SUPER MARIO 64_HIRESTEXTURES.hts", false, {} } });
	check(overridden.paths == Paths{ "/override" } && overridden.near_misses.empty() &&
	      overridden.different_game.empty() && overridden.unconverted.empty(),
	      "the explicit override supersedes discovery and all its hints");
	check(resolve_hires_cache_path(true, "", "", "", "/override", listing).paths == Paths{ "/override" },
	      "the override requires neither a system directory nor an identity");
	check(resolve_hires_cache_path(false, "/system", "SUPER MARIO 64", "SUPER MARIO 64",
	                               "/override", listing).paths.empty(), "hi-res off suppresses the override and discovery");
	for (const auto *override_path : { static_cast<const char *>(nullptr), "" })
		check(resolve_hires_cache_path(true, "/system", "SUPER MARIO 64", "SUPER MARIO 64",
		                               override_path, listing).paths == selected.paths,
		      "unset and empty overrides retain normal discovery");
	const auto converted = select_hires_packages("MARIOKART64", "MARIOKART64", {
		{ "/hires/MARIOKART64", true, { "pack.phrb", "texture.png" } },
		{ "/hires/OTHER_HIRESTEXTURES.hts", false, {} },
		{ "/hires/MARIOKART64_MEMORYCACHE.htc", false, {} },
	});
	check(converted.unconverted.empty(), "converted directories, other games and enhancement caches are not hints");
	return 0;
}
