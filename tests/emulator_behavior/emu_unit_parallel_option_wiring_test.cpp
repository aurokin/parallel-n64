#include <libretro.h>
#include <libretro_vulkan.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#define PARALLEL_RDP_HPP
namespace RDP
{
extern const struct retro_hw_render_interface_vulkan *vulkan;
extern unsigned width;
extern unsigned height;
extern unsigned upscaling;
extern unsigned downscaling_steps;
extern unsigned overscan;
extern unsigned vi_scaling_mode;
extern unsigned experimental_vi;
extern unsigned experimental_texrect;
extern bool synchronous;
extern bool divot_filter;
extern bool gamma_dither;
extern bool vi_aa;
extern bool vi_scale;
extern bool dither_filter;
extern bool interlacing;
extern bool native_texture_lod;
extern bool native_tex_rect;
extern bool hires_textures;
extern unsigned hires_filter;
extern unsigned hires_srgb;
extern unsigned hires_lookup_mode;
extern unsigned hires_budget_mb;
extern std::string hires_cache_path;

bool init();
void deinit();
void begin_frame();
void process_commands();
void complete_frame();
void profile_refresh_begin();
void profile_refresh_end();
}

#include "mupen64plus-video-paraLLEl/parallel.cpp"

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

static void reset_rdp_state()
{
	RDP::synchronous = false;
	RDP::divot_filter = false;
	RDP::gamma_dither = false;
	RDP::vi_aa = false;
	RDP::vi_scale = false;
	RDP::dither_filter = false;
	RDP::interlacing = false;
	RDP::upscaling = 1;
	RDP::downscaling_steps = 0;
	RDP::vi_scaling_mode = 0u;
	RDP::experimental_vi = 0u;
	RDP::experimental_texrect = 0u;
	RDP::native_texture_lod = false;
	RDP::native_tex_rect = true;
	RDP::hires_textures = false;
	RDP::hires_filter = 1;
	RDP::hires_srgb = 0;
	RDP::hires_lookup_mode = 0;
	RDP::hires_budget_mb = 0;
	RDP::hires_cache_path.clear();
	RDP::overscan = 0;
}

static void test_setter_to_global_wiring()
{
	reset_rdp_state();

	parallel_set_synchronous_rdp(true);
	parallel_set_divot_filter(true);
	parallel_set_gamma_dither(true);
	parallel_set_vi_aa(true);
	parallel_set_vi_scale(true);
	parallel_set_dither_filter(true);
	parallel_set_interlacing(true);
	parallel_set_upscaling(8);
	parallel_set_downscaling_steps(3);
	parallel_set_vi_scaling_mode(1u);
	parallel_set_experimental_vi(2u);
	parallel_set_experimental_texrect(1u);
	parallel_set_native_texture_lod(true);
	parallel_set_native_tex_rect(false);
	parallel_set_hires_textures(true);
	parallel_set_hires_filter(2);
	parallel_set_hires_srgb(1);
	parallel_set_hires_lookup_mode(1);
	parallel_set_hires_budget_mb(256);
	parallel_set_hires_cache_path("/tmp/hires-cache");
	parallel_set_overscan_crop(24);

	check(RDP::synchronous, "parallel_set_synchronous_rdp wiring mismatch");
	check(RDP::divot_filter, "parallel_set_divot_filter wiring mismatch");
	check(RDP::gamma_dither, "parallel_set_gamma_dither wiring mismatch");
	check(RDP::vi_aa, "parallel_set_vi_aa wiring mismatch");
	check(RDP::vi_scale, "parallel_set_vi_scale wiring mismatch");
	check(RDP::dither_filter, "parallel_set_dither_filter wiring mismatch");
	check(RDP::interlacing, "parallel_set_interlacing wiring mismatch");
	check(RDP::upscaling == 8u, "parallel_set_upscaling wiring mismatch");
	check(RDP::downscaling_steps == 3u, "parallel_set_downscaling_steps wiring mismatch");
	check(RDP::vi_scaling_mode == 1u, "parallel_set_vi_scaling_mode wiring mismatch");
	check(RDP::experimental_vi == 2u, "parallel_set_experimental_vi wiring mismatch");
	check(RDP::experimental_texrect == 1u, "parallel_set_experimental_texrect wiring mismatch");
	check(RDP::native_texture_lod, "parallel_set_native_texture_lod wiring mismatch");
	check(!RDP::native_tex_rect, "parallel_set_native_tex_rect wiring mismatch");
	check(RDP::hires_textures, "parallel_set_hires_textures wiring mismatch");
	check(RDP::hires_filter == 2u, "parallel_set_hires_filter wiring mismatch");
	check(RDP::hires_srgb == 1u, "parallel_set_hires_srgb wiring mismatch");
	check(RDP::hires_lookup_mode == 1u, "parallel_set_hires_lookup_mode wiring mismatch");
	check(RDP::hires_budget_mb == 256u, "parallel_set_hires_budget_mb wiring mismatch");
	check(RDP::hires_cache_path == "/tmp/hires-cache", "parallel_set_hires_cache_path wiring mismatch");
	check(RDP::overscan == 24u, "parallel_set_overscan_crop wiring mismatch");

	parallel_set_hires_cache_path(nullptr);
	check(RDP::hires_cache_path.empty(), "parallel_set_hires_cache_path should clear path on nullptr");
}

}

namespace RDP
{
const struct retro_hw_render_interface_vulkan *vulkan = nullptr;
unsigned width = 0;
unsigned height = 0;
unsigned upscaling = 1;
unsigned downscaling_steps = 0;
unsigned overscan = 0;
unsigned vi_scaling_mode = 0u;
unsigned experimental_vi = 0u;
unsigned experimental_texrect = 0u;
bool synchronous = false;
bool divot_filter = false;
bool gamma_dither = false;
bool vi_aa = false;
bool vi_scale = false;
bool dither_filter = false;
bool interlacing = false;
bool native_texture_lod = false;
bool native_tex_rect = true;
bool hires_textures = false;
unsigned hires_filter = 1;
unsigned hires_srgb = 0;
unsigned hires_lookup_mode = 0;
unsigned hires_budget_mb = 0;
std::string hires_cache_path;

bool init() { return true; }
void deinit() {}
void begin_frame() {}
void process_commands() {}
void complete_frame() {}
void profile_refresh_begin() {}
void profile_refresh_end() {}
}

extern "C" int retro_return(bool)
{
	return 0;
}

int main()
{
	test_setter_to_global_wiring();
	std::cout << "emu_unit_parallel_option_wiring_test: PASS" << std::endl;
	return 0;
}
