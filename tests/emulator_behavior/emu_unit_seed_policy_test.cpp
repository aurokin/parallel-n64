#include "support/emu_seed.hpp"

#include <cstdlib>
#include <iostream>

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

static void unset_seed_env()
{
	unsetenv("EMU_FUZZ_SEED");
}

static void test_default_seed_used_when_env_missing_or_invalid()
{
	unset_seed_env();
	check(EmuTest::parse_seed_env("EMU_FUZZ_SEED", 0x1234u) == 0x1234u,
	      "missing env should use default seed");

	setenv("EMU_FUZZ_SEED", "not-a-number", 1);
	check(EmuTest::parse_seed_env("EMU_FUZZ_SEED", 0x1234u) == 0x1234u,
	      "invalid env value should use default seed");
	unset_seed_env();
}

static void test_env_seed_override_parses_hex_and_decimal()
{
	setenv("EMU_FUZZ_SEED", "0x2a", 1);
	check(EmuTest::parse_seed_env("EMU_FUZZ_SEED", 0x1234u) == 0x2au,
	      "hex env seed parse mismatch");

	setenv("EMU_FUZZ_SEED", "1337", 1);
	check(EmuTest::parse_seed_env("EMU_FUZZ_SEED", 0x1234u) == 1337u,
	      "decimal env seed parse mismatch");
	unset_seed_env();
}

static void test_zero_seed_resolve_falls_back_to_default()
{
	setenv("EMU_FUZZ_SEED", "0", 1);
	const uint32_t seed = EmuTest::resolve_seed(
			"EMU_FUZZ_SEED", 0x5a5a5a5au,
			"emu.unit.seed_policy", "seed");
	check(seed == 0x5a5a5a5au, "zero seed should fall back to default");
	unset_seed_env();
}

static void test_xorshift_sequence_is_stable()
{
	uint32_t state = 0x1a2b3c4du;
	check(EmuTest::next_xorshift32(state) == 0xc9f6f11cu, "xorshift step1 mismatch");
	check(EmuTest::next_xorshift32(state) == 0xed7a2436u, "xorshift step2 mismatch");
	check(EmuTest::next_xorshift32(state) == 0x966aa9c8u, "xorshift step3 mismatch");
}
}

int main()
{
	test_default_seed_used_when_env_missing_or_invalid();
	test_env_seed_override_parses_hex_and_decimal();
	test_zero_seed_resolve_falls_back_to_default();
	test_xorshift_sequence_is_stable();
	std::cout << "emu_unit_seed_policy_test: PASS" << std::endl;
	return 0;
}
