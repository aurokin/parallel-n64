#pragma once

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace EmuTest
{
inline uint32_t parse_seed_env(const char *env_var, uint32_t default_seed)
{
	const char *env = std::getenv(env_var);
	if (!env || !*env)
		return default_seed;

	errno = 0;
	char *end = nullptr;
	unsigned long value = std::strtoul(env, &end, 0);
	if (errno != 0 || end == env || *end != '\0')
		return default_seed;

	return static_cast<uint32_t>(value);
}

inline uint32_t resolve_seed(const char *env_var, uint32_t default_seed,
                             const char *test_name, const char *seed_label)
{
	uint32_t seed = parse_seed_env(env_var, default_seed);
	if (seed == 0)
		seed = default_seed;
	std::cout << test_name << " " << seed_label << "=0x" << std::hex << seed << std::dec << std::endl;
	return seed;
}

inline uint32_t next_xorshift32(uint32_t &state)
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}
}
