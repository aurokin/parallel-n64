#include <cstdlib>
#include <iostream>
#include <vector>

#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_cutout_policy.hpp"

using namespace RDP;

namespace
{
CombinerInputsRGB rgb(unsigned a, unsigned b, unsigned c, unsigned d)
{
	return {RGBMulAdd(a), RGBMulSub(b), RGBMul(c), RGBAdd(d)};
}

CombinerInputsAlpha alpha(unsigned a, unsigned b, unsigned c, unsigned d)
{
	return {AlphaAddSub(a), AlphaAddSub(b), AlphaMul(c), AlphaAddSub(d)};
}

struct Case
{
	const char *name;
	bool preserves;
	StaticRasterizationState state = {};
	ConstantCombinerInputs constants[2] = {};
};

Case single(const char *name, bool preserves, CombinerInputsRGB color, CombinerInputsAlpha opacity,
            unsigned sample = RASTERIZATION_USES_TEXEL0_BIT)
{
	Case test = {name, preserves};
	test.state.flags = sample | RASTERIZATION_SAMPLE_MODE_BIT;
	test.state.combiner[1] = {color, opacity};
	return test;
}
}

int main()
{
	const auto opaque = alpha(6, 6, 7, 6);
	const auto shade = alpha(7, 7, 7, 4);
	const auto constant = alpha(7, 7, 7, 7);
	const auto decal = rgb(1, 4, 8, 4);
	std::vector<Case> cases;
	cases.push_back(single("captured Boo decal / ENV alpha", true, decal, constant));
	cases.back().constants[1].add[3] = 255;
	cases.push_back(single("empty sprite repaint / SHADE alpha", false, rgb(8, 8, 16, 1), shade));
	cases.push_back(single("decal without surface bias", false, rgb(1, 4, 8, 7), constant));
	cases.push_back(single("decal with other texture alpha", false, rgb(1, 4, 9, 4), constant,
	                       RASTERIZATION_USES_TEXEL0_BIT | RASTERIZATION_USES_PIPELINED_TEXEL1_BIT));
	cases.push_back(single("one-cycle constant intensity sibling", true, rgb(8, 8, 1, 7), opaque));
	cases.back().constants[1].muladd[0] = 90;
	cases.back().constants[1].mulsub[0] = 40;
	cases.back().constants[1].add[0] = 40;
	Case intensity = cases.back();
	cases.push_back(single("TEXEL1-only intensity sibling", true, rgb(8, 8, 2, 7), opaque,
	                       RASTERIZATION_USES_PIPELINED_TEXEL1_BIT));
	cases.back().constants[1].add[0] = 40;
	cases.push_back(single("ONE bias sibling", true, rgb(8, 8, 1, 6), opaque));
	cases.push_back(single("black bias product-only effect", false, rgb(8, 8, 1, 7), opaque));
	cases.push_back(single("SHADE alpha material", true, rgb(8, 8, 1, 6), shade));
	cases.push_back(single("same SHADE selectors cancel", true, rgb(4, 4, 1, 6), opaque));
	cases.push_back(single("dead texture-alpha multiplier", true, decal, alpha(6, 6, 1, 6)));
	cases.push_back(single("equal resolved alpha constants cancel", true, decal, alpha(7, 7, 1, 6)));
	cases.back().constants[1].muladd[3] = cases.back().constants[1].mulsub[3] = 80;
	cases.push_back(cases.back());
	cases.back().name = "unequal normalized alpha constants do not cancel";
	cases.back().preserves = false;
	cases.back().constants[1].mulsub[3] = 40;

	// Every alpha operand, including RGB multipliers that read COMBINED alpha.
	for (unsigned texel : {1u, 2u})
	{
		for (unsigned operand = 0; operand < 4; operand++)
		{
			auto test = single("live texture alpha consumer", false, decal, alpha(7, 7, 4, 7),
			                   RASTERIZATION_USES_TEXEL0_BIT | RASTERIZATION_USES_PIPELINED_TEXEL1_BIT);
			auto &mux = test.state.combiner[1].alpha;
			if (operand == 0) mux.muladd = AlphaAddSub(texel);
			if (operand == 1) mux.mulsub = AlphaAddSub(texel);
			if (operand == 2)
			{
				mux.muladd = AlphaAddSub::One;
				mux.mul = AlphaMul(texel);
			}
			if (operand == 3) mux.add = AlphaAddSub(texel);
			cases.push_back(test);
		}
	}

	Case lit = {"captured PLAY-OOT-001 shell", true};
	lit.state.flags = RASTERIZATION_SAMPLE_MODE_BIT | RASTERIZATION_MULTI_CYCLE_BIT | RASTERIZATION_USES_TEXEL0_BIT;
	lit.state.combiner[0] = {rgb(8, 8, 1, 7), opaque};
	lit.state.combiner[1] = {rgb(0, 8, 4, 7), opaque};
	lit.constants[0].muladd[0] = 90;
	lit.constants[0].mulsub[0] = lit.constants[0].add[0] = 40;
	lit.constants[0].add[1] = 10;
	cases.push_back(lit);
	lit.name = "G_CC_PASS2 alpha follows independent COMBINED";
	lit.state.combiner[1].alpha = alpha(7, 7, 7, 0);
	cases.push_back(lit);
	lit.name = "G_CC_PASS2 follows texture-dependent COMBINED";
	lit.preserves = false;
	lit.state.combiner[0].alpha = alpha(7, 7, 7, 1);
	cases.push_back(lit);
	lit.name = "dead first-cycle alpha without alpha compare";
	lit.preserves = true;
	lit.state.combiner[1].alpha = opaque;
	cases.push_back(lit);
	lit.name = "alpha compare consumes first-cycle alpha";
	lit.preserves = false;
	lit.state.flags |= RASTERIZATION_ALPHA_TEST_BIT;
	cases.push_back(lit);
	lit.name = "alpha compare accepts first-cycle ONE";
	lit.preserves = true;
	lit.state.combiner[0].alpha = opaque;
	cases.push_back(lit);
	lit.name = "RGB COMBINED_ALPHA follows empty sample";
	lit.state.flags &= ~RASTERIZATION_ALPHA_TEST_BIT;
	lit.state.combiner[0].alpha = alpha(7, 7, 7, 1);
	lit.state.combiner[1].rgb = rgb(0, 8, 7, 4);
	cases.push_back(lit);

	// The previous implementation excluded these shapes. The general rule
	// accepts independent surfaces and still rejects live texture dependencies.
	Case material = lit;
	material.state.combiner[0].alpha = opaque;
	material.state.combiner[1] = {rgb(0, 8, 4, 7), shade};
	material.name = "lit material / variable but independent SHADE alpha";
	cases.push_back(material);
	material.state.combiner[1].alpha = opaque;
	material.name = "lit product without any RGB bias";
	material.preserves = false;
	material.constants[0].add[0] = material.constants[0].add[1] = 0;
	cases.push_back(material);
	material = lit;
	material.state.combiner[0].alpha = opaque;
	material.state.combiner[1] = {rgb(0, 8, 4, 7), opaque};
	material.name = "second-cycle independent subtraction";
	material.constants[1].mulsub[0] = 12;
	cases.push_back(material);
	material.name = "second-cycle independent addition";
	material.constants[1].mulsub[0] = 0;
	material.constants[1].add[0] = 12;
	cases.push_back(material);
	material.name = "one-cycle cannot consume absent first cycle";
	material.preserves = false;
	material.constants[1].add[0] = 0;
	material.state.flags &= ~RASTERIZATION_MULTI_CYCLE_BIT;
	cases.push_back(material);
	for (unsigned operand = 0; operand < 4; operand++)
	{
		auto test = intensity;
		test.name = "intensity input mutations retain only surviving bias";
		test.preserves = operand != 3;
		auto &mux = test.state.combiner[1].rgb;
		if (operand == 0) mux.muladd = RGBMulAdd::Texel0;
		if (operand == 1) mux.mulsub = RGBMulSub::Texel0;
		if (operand == 2) mux.mul = RGBMul::Zero;
		if (operand == 3) mux.add = RGBAdd::Texel0;
		cases.push_back(test);
	}

	material = lit;
	material.name = "first-cycle TEXEL1 intensity";
	material.state.flags = RASTERIZATION_MULTI_CYCLE_BIT | RASTERIZATION_SAMPLE_MODE_BIT | RASTERIZATION_USES_TEXEL1_BIT;
	material.state.combiner[0] = {rgb(8, 8, 2, 7), opaque};
	material.state.combiner[1] = {rgb(0, 8, 4, 7), opaque};
	cases.push_back(material);
	material.name = "second-cycle TEXEL1 aliases physical TEXEL0";
	material.state.flags = RASTERIZATION_MULTI_CYCLE_BIT | RASTERIZATION_SAMPLE_MODE_BIT | RASTERIZATION_USES_TEXEL0_BIT;
	material.state.combiner[0] = {rgb(8, 8, 16, 4), opaque};
	material.state.combiner[1] = {rgb(0, 8, 2, 6), opaque};
	cases.push_back(material);
	material.name = "second-cycle live peer remains texture dependent";
	material.preserves = false;
	material.state.flags |= RASTERIZATION_USES_TEXEL1_BIT;
	material.state.combiner[1].rgb.mul = RGBMul::Texel0;
	cases.push_back(material);

	for (const auto &change : std::vector<Case>{cases[0], intensity, lit})
	{
		auto test = change;
		test.name = "conversion remains conservative";
		test.preserves = false;
		test.state.flags |= RASTERIZATION_CONVERT_ONE_BIT;
		cases.push_back(test);
		test.state.flags &= ~(RASTERIZATION_CONVERT_ONE_BIT | RASTERIZATION_SAMPLE_MODE_BIT);
		test.name = "pre-combiner conversion remains conservative";
		cases.push_back(test);
	}
	cases.push_back(single("two-cycle decal with independent alpha", true, decal, opaque,
	                       RASTERIZATION_USES_TEXEL1_BIT | RASTERIZATION_MULTI_CYCLE_BIT));
	cases.back().state.combiner[0] = {rgb(8, 8, 16, 7), opaque};
	cases.push_back(single("two samples both cancel", true, rgb(2, 2, 1, 4), opaque,
	                       RASTERIZATION_USES_TEXEL0_BIT | RASTERIZATION_USES_PIPELINED_TEXEL1_BIT));
	cases.push_back(single("one empty must not erase other texture dependency", false, rgb(1, 4, 9, 4), opaque,
	                       RASTERIZATION_USES_TEXEL0_BIT | RASTERIZATION_USES_PIPELINED_TEXEL1_BIT));
	cases.push_back(single("TEXEL1-only empty sprite repaint", false, rgb(8, 8, 16, 2), shade,
	                       RASTERIZATION_USES_PIPELINED_TEXEL1_BIT));
	cases.push_back(single("LOD remains a texture-free material term", true, rgb(6, 8, 13, 7), opaque));
	cases.push_back(single("NOISE remains a texture-free material term", true, rgb(7, 8, 4, 7), opaque));
	cases.push_back(single("negative constant clamps to no surface", false, rgb(8, 8, 16, 7), opaque));
	cases.back().constants[1].mulsub[0] = 64;
	cases.back().constants[1].mul[0] = 255;
	cases.push_back(single("copy pipe exempt", false, decal, opaque, RASTERIZATION_COPY_BIT));
	cases.push_back(single("fill pipe exempt", false, decal, opaque, RASTERIZATION_FILL_BIT));

	cases.push_back(single("texture-free negative expression clamps to black", false, rgb(1, 8, 4, 7), opaque));
	for (unsigned channel = 0; channel < 3; channel++)
		cases.back().constants[1].mulsub[channel] = 1;
	Case wrapped = {"nine-bit COMBINED multiplier 512 is zero", true};
	wrapped.state.flags = RASTERIZATION_SAMPLE_MODE_BIT | RASTERIZATION_MULTI_CYCLE_BIT |
	                      RASTERIZATION_USES_TEXEL0_BIT | RASTERIZATION_USES_TEXEL1_BIT;
	wrapped.state.combiner[0] = {rgb(6, 7, 16, 6), opaque};
	wrapped.state.combiner[1] = {rgb(1, 2, 0, 6), opaque};
	wrapped.constants[0].mulsub[1] = wrapped.constants[0].mulsub[2] = 255;
	for (unsigned channel = 0; channel < 3; channel++)
		wrapped.constants[0].mul[channel] = 255;
	cases.push_back(wrapped);

	wrapped.name = "nine-bit COMBINED 512 cancels converted zero";
	wrapped.state.combiner[0].rgb = rgb(1, 7, 15, 6);
	wrapped.state.combiner[1].rgb = rgb(0, 8, 1, 6);
	wrapped.constants[0].mulsub[1] = wrapped.constants[0].mul[1] = 255;
	wrapped.constants[0].mulsub[2] = wrapped.constants[0].mul[2] = 0;
	cases.push_back(wrapped);

	cases.push_back(single("correlated SHADE polynomial always clamps black", false, rgb(4, 8, 4, 1), opaque));
	for (unsigned channel = 0; channel < 3; channel++)
		cases.back().constants[1].mulsub[channel] = 255;

	cases.push_back(single("independent RGB and alpha inputs need separate witnesses", true, rgb(7, 4, 11, 1), opaque));

	for (const auto &test : cases)
	{
		bool actual = detail::hires_cutout_preserves_surface(test.state, test.constants, true);
		if (detail::hires_cutout_preserves_surface(test.state, test.constants, false))
		{
			std::cerr << "FAIL: feature-off policy must be inert: " << test.name << '\n';
			return EXIT_FAILURE;
		}
		if (actual != test.preserves)
		{
			std::cerr << "FAIL: " << test.name << " expected " << test.preserves << " got " << actual << '\n';
			return EXIT_FAILURE;
		}
	}
	// noise.h supplies ((seed & 7) << 6) | 0x20, not an arbitrary byte.
	// Check a normalized NOISE * 0.5 mux across every encoded source value,
	// including its upper values' special nine-bit expansion.
	const int noise_half[] = {16, 48, 80, 112, 144, 176, -48, -16};
	ConstantCombinerInputs noise_constants = {};
	noise_constants.mul[0] = 128;
	for (unsigned code = 0; code < 8; code++)
	{
		detail::HiresCutout::Witness witness = {255, 255, 255, int(code)};
		auto result = detail::HiresCutout::rgb_cycle(rgb(7, 8, 16, 7), noise_constants,
		                                           {}, {}, 0, 1, 3, false, &witness);
		if (result.symbol != 0 || result.literal != noise_half[code])
		{
			std::cerr << "FAIL: encoded NOISE row " << code << " expected " << noise_half[code]
			          << " got " << result.literal << '\n';
			return EXIT_FAILURE;
		}
	}

	std::cout << "emu_unit_hires_cutout_policy_test: PASS (" << cases.size() << " cutout cases; 8 NOISE rows)\n";
}
