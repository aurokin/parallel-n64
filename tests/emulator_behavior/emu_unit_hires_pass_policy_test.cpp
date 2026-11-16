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
bool native_pass_replaces(unsigned upscaling, bool attached)
{
	return pass_samples_replacements(false, needs_presented_pass(upscaling, attached));
}
}

int main()
{
	// ADR-0020: with a pack attached, RDRAM matches feature-off at every factor,
	// the default 1x included, and the presented pass still replaces.
	for (unsigned upscaling : { 1u, 2u, 4u, 8u })
	{
		check(needs_presented_pass(upscaling, true), "an attached pack must get a separate presented pass");
		check(!native_pass_replaces(upscaling, true), "the RDRAM pass must render the game's own textures");
		check(pass_samples_replacements(true, true), "the presented pass must replace");
	}

	// Feature-off at 1x keeps the single upstream pass.
	check(!needs_presented_pass(1, false), "1x without a pack must not add a pass");
	check(needs_presented_pass(4, false), "upscaling always has a presented pass");
	return 0;
}
