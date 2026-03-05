#include "command_ring_policy.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

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
}

int main()
{
	const size_t ring_size = 8;
	const size_t mask = ring_size - 1;
	std::vector<uint32_t> ring(ring_size, 0u);
	std::vector<uint32_t> out;
	uint64_t write_count = 0;
	uint64_t read_count = 0;

	check(can_enqueue_ring_words(write_count, read_count, ring_size, 7u), "max payload enqueue should fit");
	check(!can_enqueue_ring_words(write_count, read_count, ring_size, 8u), "payload beyond capacity should not fit");

	const uint32_t cmd_a[] = { 11u, 12u, 13u };
	write_ring_command(ring.data(), mask, write_count, 3u, cmd_a); // consumes 4 slots
	const uint32_t cmd_b[] = { 21u, 22u };
	write_ring_command(ring.data(), mask, write_count, 2u, cmd_b); // consumes 3 slots
	check(write_count == 7u, "write_count mismatch before wrap enqueue");

	uint32_t words = read_ring_command(ring.data(), mask, read_count, out);
	check(words == 3u, "first command word count mismatch");
	check(out.size() == 3u && out[0] == 11u && out[2] == 13u, "first command payload mismatch");
	check(read_count == 4u, "read_count mismatch after first read");

	const uint32_t cmd_c[] = { 31u, 32u, 33u };
	check(can_enqueue_ring_words(write_count, read_count, ring_size, 3u), "wrap enqueue should fit");
	write_ring_command(ring.data(), mask, write_count, 3u, cmd_c); // wraps around ring
	check(write_count == 11u, "write_count mismatch after wrap enqueue");

	words = read_ring_command(ring.data(), mask, read_count, out);
	check(words == 2u, "second command word count mismatch");
	check(out.size() == 2u && out[0] == 21u && out[1] == 22u, "second command payload mismatch");

	words = read_ring_command(ring.data(), mask, read_count, out);
	check(words == 3u, "third command word count mismatch");
	check(out.size() == 3u && out[0] == 31u && out[2] == 33u, "third command payload mismatch");

	write_ring_command(ring.data(), mask, write_count, 0u, nullptr); // sentinel-like empty payload
	words = read_ring_command(ring.data(), mask, read_count, out);
	check(words == 0u, "zero-word command mismatch");
	check(out.empty(), "zero-word command should produce empty payload");

	std::cout << "emu_unit_command_ring_policy_test: PASS" << std::endl;
	return 0;
}
