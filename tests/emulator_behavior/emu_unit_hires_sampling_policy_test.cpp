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

static void test_copy_mode_pack_contract()
{
	const uint16_t packed_opaque = pack_hires_copy_rgba5551(0xff, 0x80, 0x00, 0xff);
	check((packed_opaque & 1u) == 1u,
	      "opaque alpha should keep alpha bit set in RGBA5551 pack");

	const uint16_t packed_low_alpha = pack_hires_copy_rgba5551(0x20, 0x40, 0x60, 1);
	check((packed_low_alpha & 1u) == 1u,
	      "non-zero alpha should keep alpha bit set in RGBA5551 pack");

	const uint16_t packed_zero_alpha = pack_hires_copy_rgba5551(0x20, 0x40, 0x60, 0);
	check((packed_zero_alpha & 1u) == 0u,
	      "zero alpha should clear alpha bit in RGBA5551 pack");

	check((packed_low_alpha >> 11) == ((0x20 & 0xf8u) >> 3),
	      "red channel packing should preserve top 5 bits");
}

}

int main()
{
	test_filter_mode_sanitization_contract();
	test_srgb_mode_resolution_contract();
	test_copy_mode_pack_contract();
	std::cout << "emu_unit_hires_sampling_policy_test: PASS" << std::endl;
	return 0;
}
