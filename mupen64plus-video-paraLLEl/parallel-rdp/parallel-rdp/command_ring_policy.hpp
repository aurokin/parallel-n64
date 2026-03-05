#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace RDP
{
namespace detail
{
inline bool can_enqueue_ring_words(uint64_t write_count, uint64_t read_count, size_t ring_size, unsigned num_words)
{
	return write_count + num_words + 1 <= read_count + ring_size;
}

inline void write_ring_command(uint32_t *ring,
                               size_t mask,
                               uint64_t &write_count,
                               unsigned num_words,
                               const uint32_t *words)
{
	ring[write_count++ & mask] = num_words;
	for (unsigned i = 0; i < num_words; i++)
		ring[write_count++ & mask] = words[i];
}

inline uint32_t read_ring_command(const uint32_t *ring, size_t mask, uint64_t &read_count, std::vector<uint32_t> &out_words)
{
	uint32_t num_words = ring[read_count++ & mask];
	out_words.resize(num_words);
	for (uint32_t i = 0; i < num_words; i++)
		out_words[i] = ring[read_count++ & mask];
	return num_words;
}
}
}
