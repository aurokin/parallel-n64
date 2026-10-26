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

int main()
{
	using RDP::detail::resolve_hires_cache_path;
	using Paths = std::vector<std::string>;
	const Paths mario = { "/system/parallel-n64/hires/SUPER MARIO 64.phrb",
	                      "/system/parallel-n64/hires/SUPER MARIO 64" };
	check(resolve_hires_cache_path("/system", "SUPER MARIO 64", nullptr) == mario,
	      "default candidates must be game-specific, file first, then directory");
	check(resolve_hires_cache_path("/system", "SUPER MARIO 64", "") == mario,
	      "empty override must use the scoped default");
	check(resolve_hires_cache_path("/system", std::string("SUPER MARIO 64   \0 \0\0", 20), nullptr) == mario,
	      "trailing spaces and NULs must be trimmed from the fixed-width name");
	check(resolve_hires_cache_path("/system", "  MiXeD Name\t ", nullptr) ==
	      Paths{ "/system/parallel-n64/hires/  MiXeD Name\t.phrb",
	             "/system/parallel-n64/hires/  MiXeD Name\t" },
	      "leading spaces, case, internal spaces and trailing tabs must be preserved");
	for (const auto &name : { std::string(), std::string("   \0\0", 5) })
		check(resolve_hires_cache_path("/system", name, nullptr).empty(),
		      "empty names must not select a shared directory");
	check(resolve_hires_cache_path("", "SUPER MARIO 64", nullptr).empty(),
	      "missing system directory must not select a relative shared directory");
	for (const char *override_path : { "/packs/exact.phrb", "/packs/collection", "relative.phrb" })
	{
		check(resolve_hires_cache_path("/system", "SUPER MARIO 64", override_path) == Paths{ override_path },
		      "an explicit file or directory must be the only path");
		check(resolve_hires_cache_path("", "", override_path) == Paths{ override_path },
		      "override must work without a ROM name or system directory");
	}
	check(resolve_hires_cache_path("/nonexistent", "NO PACK", nullptr) ==
	      Paths{ "/nonexistent/parallel-n64/hires/NO PACK.phrb",
	             "/nonexistent/parallel-n64/hires/NO PACK" },
	      "unmatched candidates must never fall back to loading the system directory");
	return 0;
}
