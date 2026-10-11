#include <cmath>
#include <cstdlib>
#include <iostream>

// Supply only the GLSL value type; the included header owns all filtering math.
struct vec4
{
	float x, y, z, w;

	vec4(float r, float g, float b, float a) : x(r), y(g), z(b), w(a) {}
};

#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/shaders/hires_mip_filter.h"

namespace
{
void check_sample(vec4 a, vec4 b, float fraction, vec4 expected, const char *message)
{
	const vec4 actual = hires_alpha_weighted_mix(a, b, fraction);
	const float channels[] = { actual.x, actual.y, actual.z, actual.w };
	const float expected_channels[] = { expected.x, expected.y, expected.z, expected.w };
	for (unsigned i = 0; i < 4; i++)
	{
		if (!std::isfinite(channels[i]) || std::abs(channels[i] - expected_channels[i]) > 1e-6f)
		{
			std::cerr << "FAIL: " << message << " channel " << i
			          << " expected " << expected_channels[i] << " got " << channels[i] << '\n';
			std::exit(1);
		}
	}
}
}

int main()
{
	const vec4 transparent(0.0f, 0.0f, 0.0f, 0.0f);
	const vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
	const vec4 blue(0.0f, 0.0f, 1.0f, 1.0f);

	check_sample(transparent, red, 0.5f, vec4(1.0f, 0.0f, 0.0f, 0.5f),
	             "transparent lower mip must not darken opaque red");
	check_sample(red, transparent, 0.5f, vec4(1.0f, 0.0f, 0.0f, 0.5f),
	             "transparent upper mip must not darken opaque red");
	check_sample(red, vec4(0.0f, 0.0f, 1.0f, 0.25f), 0.5f,
	             vec4(0.8f, 0.0f, 0.2f, 0.625f), "unequal opacity must weight color contribution");
	check_sample(red, blue, 0.25f, vec4(0.75f, 0.0f, 0.25f, 1.0f),
	             "opaque interpolation must preserve ordinary linear filtering");
	check_sample(vec4(1.0f, 0.0f, 0.0f, 0.5f), vec4(0.0f, 0.0f, 1.0f, 0.5f), 0.25f,
	             vec4(0.75f, 0.0f, 0.25f, 0.5f), "equal opacity must preserve ordinary linear color");
	check_sample(red, blue, 0.0f, red, "zero fraction must preserve the lower mip");
	check_sample(red, blue, 1.0f, blue, "unit fraction must preserve the upper mip");
	check_sample(transparent, red, 0.0f, transparent, "transparent lower endpoint must stay finite");
	check_sample(red, transparent, 1.0f, transparent, "transparent upper endpoint must stay finite");
	check_sample(vec4(1.0f, 0.0f, 0.0f, 0.0f), vec4(0.0f, 0.0f, 1.0f, 0.0f), 0.5f,
	             transparent, "fully transparent mips must return finite zero RGBA");

	// A 4x4 cutout with one opaque red corner yields these per-level samples
	// at ST=(2,0): mip0 is empty; alpha-weighted mip1 bilerp is red with A=16.
	check_sample(transparent, vec4(1.0f, 0.0f, 0.0f, 16.0f / 255.0f), 0.5f,
	             vec4(1.0f, 0.0f, 0.0f, 8.0f / 255.0f), "fractional mip cutout must retain red RGB");

	std::cout << "emu_unit_hires_mip_filter_test: PASS\n";
	return 0;
}
