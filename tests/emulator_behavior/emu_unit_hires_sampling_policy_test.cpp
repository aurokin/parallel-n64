#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_sampling_policy.hpp"

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

static void test_filter_mode_sanitization_contract()
{
	check(sanitize_hires_filter_mode(0) == HiresFilterMode::Nearest,
	      "filter mode 0 should map to nearest");
	check(sanitize_hires_filter_mode(1) == HiresFilterMode::Linear,
	      "filter mode 1 should map to linear");
	check(sanitize_hires_filter_mode(2) == HiresFilterMode::Trilinear,
	      "filter mode 2 should map to trilinear");
	check(sanitize_hires_filter_mode(999) == HiresFilterMode::Linear,
	      "unknown filter mode should default to linear");

	check(!hires_filter_uses_mipmaps(HiresFilterMode::Nearest),
	      "nearest should not request mipmaps");
	check(!hires_filter_uses_mipmaps(HiresFilterMode::Linear),
	      "linear should not request mipmaps");
	check(hires_filter_uses_mipmaps(HiresFilterMode::Trilinear),
	      "trilinear should request mipmaps");
}

static void test_srgb_mode_resolution_contract()
{
	check(sanitize_hires_srgb_mode(0) == HiresSrgbMode::Auto,
	      "srgb mode 0 should map to auto");
	check(sanitize_hires_srgb_mode(1) == HiresSrgbMode::On,
	      "srgb mode 1 should map to on");
	check(sanitize_hires_srgb_mode(2) == HiresSrgbMode::Off,
	      "srgb mode 2 should map to off");
	check(sanitize_hires_srgb_mode(999) == HiresSrgbMode::Auto,
	      "unknown srgb mode should default to auto");

	check(resolve_hires_upload_srgb(HiresSrgbMode::On, false),
	      "srgb on should force srgb uploads");
	check(!resolve_hires_upload_srgb(HiresSrgbMode::Off, true),
	      "srgb off should force unorm uploads");
	check(resolve_hires_upload_srgb(HiresSrgbMode::Auto, true),
	      "srgb auto should honor replacement srgb=true");
	check(!resolve_hires_upload_srgb(HiresSrgbMode::Auto, false),
	      "srgb auto should honor replacement srgb=false");
}
}

int main()
{
	test_filter_mode_sanitization_contract();
	test_srgb_mode_resolution_contract();
	std::cout << "emu_unit_hires_sampling_policy_test: PASS" << std::endl;
	return 0;
}
