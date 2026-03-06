#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_bindless_view_policy.hpp"

#include <cstdlib>
#include <iostream>

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

static void test_bindless_view_fallback_contract()
{
	check(select_hires_bindless_view_mode(false, false, false) == HiresBindlessViewMode::DefaultView,
	      "unorm uploads should fall back to default image view when no unorm alt view exists");
	check(select_hires_bindless_view_mode(true, false, false) == HiresBindlessViewMode::DefaultView,
	      "srgb uploads should fall back to default image view when no srgb alt view exists");

	check(select_hires_bindless_view_mode(false, true, false) == HiresBindlessViewMode::UnormView,
	      "unorm uploads should use unorm alt view when available");
	check(select_hires_bindless_view_mode(true, false, true) == HiresBindlessViewMode::SrgbView,
	      "srgb uploads should use srgb alt view when available");

	check(select_hires_bindless_view_mode(true, true, false) == HiresBindlessViewMode::DefaultView,
	      "srgb uploads should not use unorm alt view as a substitute");
	check(select_hires_bindless_view_mode(false, false, true) == HiresBindlessViewMode::DefaultView,
	      "unorm uploads should not use srgb alt view as a substitute");
}
}

int main()
{
	test_bindless_view_fallback_contract();
	std::cout << "emu_unit_hires_bindless_view_policy_test: PASS" << std::endl;
	return 0;
}
