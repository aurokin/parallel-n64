// SPDX-License-Identifier: MIT

#pragma once

#include "rdp_data_structures.hpp"

namespace RDP
{
namespace detail
{
namespace HiresCutout
{
// A literal or a symbolic channel, with the physical texture inputs it needs.
// Distinct resolved constants must stay distinct even though normalization gives
// them the same selector. Symbols let (SHADE - SHADE) cancel without knowing its
// per-pixel value; arbitrary arithmetic retains its dependencies conservatively.
struct Witness
{
	int shade_rgb, shade_alpha, lod, noise_code;
};

struct Value
{
	int literal;
	unsigned symbol;
	unsigned textures;

	Value(int literal_ = 0, unsigned symbol_ = 0, unsigned textures_ = 0)
	    : literal(literal_), symbol(symbol_), textures(textures_)
	{
	}
};

inline int sign_extend_9(int value)
{
	return ((value & 511) ^ 256) - 256;
}

inline int expand(int value)
{
	return sign_extend_9(value - 128) + 128;
}

inline Value equation(Value a, Value b, Value c, Value d, unsigned symbol)
{
	const bool equal = a.symbol == b.symbol && (a.symbol != 0 || expand(a.literal) == expand(b.literal));
	if (equal || (c.symbol == 0 && sign_extend_9(c.literal) == 0))
	{
		if (d.symbol == 0)
			d.literal = expand(d.literal);
		return d;
	}
	if ((a.symbol | b.symbol | c.symbol | d.symbol) == 0)
	{
		int product = (expand(a.literal) - expand(b.literal)) * sign_extend_9(c.literal) + 128;
		int rounded = product >= 0 ? product / 256 : -((-product + 255) / 256);
		return {rounded + expand(d.literal), 0, 0};
	}
	return {0, symbol, a.textures | b.textures | c.textures | d.textures};
}

inline Value texture(unsigned slot, unsigned channel, unsigned empty, bool swap)
{
	slot ^= unsigned(swap);
	return (empty & (1u << slot)) != 0 ? Value{} : Value{0, 8 + slot * 4 + channel, 1u << slot};
}

inline Value alpha(unsigned selector, bool multiplier, uint8_t constant, Value combined,
                   unsigned empty, bool swap, const Witness *witness = nullptr)
{
	if (selector == 1 || selector == 2)
		return texture(selector - 1, 3, empty, swap);
	if (selector == 4)
		return witness ? Value{witness->shade_alpha} : Value{0, 4, 0}; // SHADE alpha.
	if (!multiplier && selector == 0)
		return combined;
	if (!multiplier && selector == 6)
		return {256, 0, 0}; // ONE is nine-bit in the combiner.
	if (multiplier && selector == 0)
		return witness ? Value{witness->lod} : Value{0, 6, 0}; // LOD fraction.
	return {constant, 0, 0};
}

inline Value rgb(unsigned selector, unsigned operand, const uint8_t *constant,
                 Value combined, Value combined_alpha, unsigned channel, unsigned empty, bool swap, const Witness *witness = nullptr)
{
	if (selector == 0)
		return combined;
	if (selector == 1 || selector == 2)
		return texture(selector - 1, channel, empty, swap);
	if (selector == 4)
		return witness ? Value{witness->shade_rgb} : Value{0, 1 + channel, 0}; // SHADE RGB.
	if ((operand == 0 || operand == 3) && selector == 6)
		return {256, 0, 0};
	if (operand == 0 && selector == 7)
		return witness ? Value{((witness->noise_code & 7) << 6) | 0x20} : Value{0, 5, 0}; // noise.h encoding.
	if ((operand == 1 && selector == 7) || (operand == 2 && selector == 15))
		return {(int(constant[1]) << 8) | constant[2], 0, 0}; // K4 / K5.
	if (operand == 2)
	{
		if (selector == 7)
			return combined_alpha;
		if (selector == 8 || selector == 9)
			return texture(selector - 8, 3, empty, swap);
		if (selector == 11)
			return witness ? Value{witness->shade_alpha} : Value{0, 4, 0};
		if (selector == 13)
			return witness ? Value{witness->lod} : Value{0, 6, 0};
	}
	return {constant[channel], 0, 0};
}

inline Value alpha_cycle(const CombinerInputsAlpha &mux, const ConstantCombinerInputs &constants,
                         Value combined, unsigned cycle, bool swap, unsigned empty = 0, const Witness *witness = nullptr)
{
	return equation(alpha(unsigned(mux.muladd), false, constants.muladd[3], combined, empty, swap, witness),
	                alpha(unsigned(mux.mulsub), false, constants.mulsub[3], combined, empty, swap, witness),
	                alpha(unsigned(mux.mul), true, constants.mul[3], combined, empty, swap, witness),
	                alpha(unsigned(mux.add), false, constants.add[3], combined, empty, swap, witness), 19 + cycle * 4);
}

inline Value rgb_cycle(const CombinerInputsRGB &mux, const ConstantCombinerInputs &constants,
                       Value combined, Value combined_alpha, unsigned channel,
                       unsigned cycle, unsigned empty, bool swap, const Witness *witness = nullptr)
{
	return equation(rgb(unsigned(mux.muladd), 0, constants.muladd, combined, combined_alpha, channel, empty, swap, witness),
	                rgb(unsigned(mux.mulsub), 1, constants.mulsub, combined, combined_alpha, channel, empty, swap, witness),
	                rgb(unsigned(mux.mul), 2, constants.mul, combined, combined_alpha, channel, empty, swap, witness),
	                rgb(unsigned(mux.add), 3, constants.add, combined, combined_alpha, channel, empty, swap, witness),
	                16 + cycle * 4 + channel);
}

// Dependency reduction proves texture freedom, but a symbolic expression need
// not survive the RDP's output clamp. Prove that it can by evaluating exact
// fixed-point arithmetic at valid combiner inputs. These generic witnesses are
// conservative: if none proves a visible term, keep the authored cutout. Never
// infer a surface from an overestimated range or from output alpha alone.
inline bool visible_rgb_term(const StaticRasterizationState &state,
                             const ConstantCombinerInputs (&constants)[2], unsigned channel)
{
	const bool multi = (state.flags & RASTERIZATION_MULTI_CYCLE_BIT) != 0;
	// Enumerate noise.h's eight legal encodings, not arbitrary byte values.
	const bool uses_noise = state.combiner[1].rgb.muladd == RGBMulAdd::Noise ||
	                        (multi && state.combiner[0].rgb.muladd == RGBMulAdd::Noise);
	const int levels[] = {255, 128, 64, 192};
	for (int noise_code = 0; noise_code < (uses_noise ? 8 : 1); noise_code++)
	{
		for (unsigned index = 0; index < 12; index++)
		{
			Witness witness;
			if (index < 4)
				witness = {levels[index], levels[index], levels[index], noise_code};
			else
			{
				unsigned corner = index - 4;
				witness = {(corner & 1) != 0 ? 255 : 0, (corner & 2) != 0 ? 255 : 0,
				           (corner & 4) != 0 ? 255 : 0, noise_code};
			}
			// Once texture independence is proven for an empty input, the other
			// texture may also be zero without changing that reduced expression.
			Value first_alpha = multi ? alpha_cycle(state.combiner[0].alpha, constants[0], {}, 0, false, 3, &witness) : Value{};
			Value first = multi ? rgb_cycle(state.combiner[0].rgb, constants[0], {}, {}, channel, 0, 3, false, &witness) : Value{};
			Value final = rgb_cycle(state.combiner[1].rgb, constants[1], first, first_alpha, channel, 1, 3, multi, &witness);
			if (final.symbol == 0 && expand(final.literal) > 0)
				return true;
		}
	}
	return false;
}

}

// Evaluate normalized muxes at draw enqueue, using the constants uploaded for
// this draw. Alpha is evaluated WITHOUT substituting texels: all consumers must
// be texture-independent, including cycle 0's alpha-compare reference. RGB is
// then reduced with each potentially empty physical sample set to RGBA zero.
// Require a surviving texture-free term: a zero sprite product is still absent,
// even if its output alpha comes from SHADE. Check samples separately because
// the shader can see one empty sample while the other remains nonempty.
inline bool hires_cutout_preserves_surface(const StaticRasterizationState &state,
                                           const ConstantCombinerInputs (&constants)[2], bool replacements_enabled)
{
	using namespace HiresCutout;
	if (!replacements_enabled)
		return false;
	const auto flags = state.flags;
	if ((flags & (RASTERIZATION_FILL_BIT | RASTERIZATION_COPY_BIT | RASTERIZATION_CONVERT_ONE_BIT)) != 0)
		return false;
	const bool multi = (flags & RASTERIZATION_MULTI_CYCLE_BIT) != 0;
	unsigned samples = ((flags & RASTERIZATION_USES_TEXEL0_BIT) != 0 ? 1u : 0u) |
	                   ((flags & (RASTERIZATION_USES_TEXEL1_BIT | RASTERIZATION_USES_PIPELINED_TEXEL1_BIT)) != 0 ? 2u : 0u);
	if (samples == 0)
		return false;
	// Conversion can turn an empty RGBA sample into a nonzero combiner input.
	if ((flags & RASTERIZATION_SAMPLE_MODE_BIT) == 0 &&
	    (((samples & 1) != 0 && (flags & RASTERIZATION_BILERP_0_BIT) == 0) ||
	     ((samples & 2) != 0 && (flags & (RASTERIZATION_TLUT_BIT | RASTERIZATION_BILERP_1_BIT)) == 0)))
		return false;

	Value first_alpha = multi ? alpha_cycle(state.combiner[0].alpha, constants[0], {}, 0, false) : Value{};
	Value final_alpha = alpha_cycle(state.combiner[1].alpha, constants[1], first_alpha, 1, multi);
	if (final_alpha.textures != 0 ||
	    (multi && (flags & RASTERIZATION_ALPHA_TEST_BIT) != 0 && first_alpha.textures != 0))
		return false;

	const unsigned empty_samples[] = {1u, 2u};
	for (unsigned empty : empty_samples)
	{
		if ((samples & empty) == 0)
			continue;
		Value empty_alpha = multi ? alpha_cycle(state.combiner[0].alpha, constants[0], {}, 0, false, empty) : Value{};
		bool surface = false;
		for (unsigned channel = 0; channel < 3; channel++)
		{
			Value first = multi ? rgb_cycle(state.combiner[0].rgb, constants[0], {}, {}, channel, 0, empty, false) : Value{};
			Value final = rgb_cycle(state.combiner[1].rgb, constants[1], first, empty_alpha, channel, 1, empty, multi);
			if (final.textures != 0)
				return false;
			if (!surface)
				surface = final.symbol == 0 ? expand(final.literal) > 0 : visible_rgb_term(state, constants, channel);
		}
		if (!surface)
			return false;
	}
	return true;
}
}
}
