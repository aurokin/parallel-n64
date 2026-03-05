#pragma once

#include "rdp_data_structures.hpp"

namespace RDP::detail
{
inline void apply_set_scissor_words(ScissorState &scissor_state,
                                    StaticRasterizationState &static_state,
                                    uint32_t word0,
                                    uint32_t word1)
{
	scissor_state.xlo = (word0 >> 12) & 0xfffu;
	scissor_state.xhi = (word1 >> 12) & 0xfffu;
	scissor_state.ylo = (word0 >> 0) & 0xfffu;
	scissor_state.yhi = (word1 >> 0) & 0xfffu;

	static_state.flags &= ~(RASTERIZATION_INTERLACE_FIELD_BIT |
	                        RASTERIZATION_INTERLACE_KEEP_ODD_BIT);
	if (word1 & (1u << 25))
		static_state.flags |= RASTERIZATION_INTERLACE_FIELD_BIT;
	if (word1 & (1u << 24))
		static_state.flags |= RASTERIZATION_INTERLACE_KEEP_ODD_BIT;
}
}
