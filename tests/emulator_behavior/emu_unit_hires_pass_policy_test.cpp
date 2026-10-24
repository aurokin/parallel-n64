#include <cstdlib>
#include <iostream>

#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_pass_policy.hpp"

using RDP::detail::needs_presented_pass;
using RDP::detail::pass_samples_replacements;

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

// Whether the pass that writes RDRAM samples replacements for one configuration.
bool native_pass_replaces(unsigned upscaling, bool attached, bool replace_mode)
{
	return pass_samples_replacements(false, needs_presented_pass(upscaling, attached, replace_mode), replace_mode);
}
}

int main()
{
	// ADR-0020: with a pack attached, RDRAM matches feature-off at every factor,
	// the default 1x included, and the presented pass still replaces.
	for (unsigned upscaling : { 1u, 2u, 4u, 8u })
	{
		check(needs_presented_pass(upscaling, true, false), "an attached pack must get a separate presented pass");
		check(!native_pass_replaces(upscaling, true, false), "the RDRAM pass must render the game's own textures");
		check(pass_samples_replacements(true, true, false), "the presented pass must replace");
	}

	// Feature-off at 1x keeps the single upstream pass.
	check(!needs_presented_pass(1, false, false), "1x without a pack must not add a pass");
	check(needs_presented_pass(4, false, false), "upscaling always has a presented pass");

	// PARALLEL_RDP_HIRES_NATIVE_PASS=replace restores the earlier behavior: one pass at 1x
	// that replaces, and replacement in the native pass when upscaling.
	check(!needs_presented_pass(1, true, true), "replace mode keeps a single pass at 1x");
	check(native_pass_replaces(1, true, true), "replace mode replaces in the only 1x pass");
	check(native_pass_replaces(4, true, true), "replace mode replaces in the native pass when upscaling");
	return 0;
}
