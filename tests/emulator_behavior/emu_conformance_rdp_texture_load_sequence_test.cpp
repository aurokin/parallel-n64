#include "mupen64plus-video-paraLLEl/rdp_command_ingest.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

using namespace RDP::detail;

namespace
{
struct HookState
{
	std::vector<unsigned> enqueued_words;
	std::vector<uint32_t> enqueued_opcodes;
	unsigned interrupt_count = 0;
};

static void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}

static void enqueue_cb(void *userdata, unsigned num_words, const uint32_t *words)
{
	auto *state = static_cast<HookState *>(userdata);
	state->enqueued_words.push_back(num_words);
	state->enqueued_opcodes.push_back((words[0] >> 24) & 63u);
}

static void interrupt_cb(void *userdata)
{
	auto *state = static_cast<HookState *>(userdata);
	state->interrupt_count++;
}

static void write_command_pair(std::array<uint8_t, 256> &memory, size_t pair_index, uint32_t w0, uint32_t w1)
{
	uint32_t words[2] = { w0, w1 };
	std::memcpy(memory.data() + pair_index * sizeof(uint64_t), words, sizeof(words));
}
}

int main()
{
	std::array<uint8_t, 256> dram = {};
	std::array<uint8_t, 256> sp_dmem = {};
	std::array<uint32_t, 128> cmd_data = {};

	// Minimal texture-load sequence + sync commands (all single-pair commands in the RDP list).
	write_command_pair(dram, 0, 0x3d000000u, 0x11111111u); // SetTextureImage
	write_command_pair(dram, 1, 0x35000000u, 0x22222222u); // SetTile
	write_command_pair(dram, 2, 0x33000000u, 0x33333333u); // LoadBlock
	write_command_pair(dram, 3, 0x32000000u, 0x44444444u); // SetTileSize
	write_command_pair(dram, 4, 0x26000000u, 0x55555555u); // SyncLoad
	write_command_pair(dram, 5, 0x27000000u, 0x66666666u); // SyncPipe
	write_command_pair(dram, 6, 0x28000000u, 0x77777777u); // SyncTile

	CommandIngestState ingest = {};
	ingest.cmd_data = cmd_data.data();

	uint32_t dpc_start = 0;
	uint32_t dpc_current = 0;
	uint32_t dpc_end = 7u * sizeof(uint64_t);
	uint32_t dpc_status = 0;

	HookState hook_state;
	CommandIngestHooks hooks = {};
	hooks.frontend_available = true;
	hooks.synchronous = false;
	hooks.userdata = &hook_state;
	hooks.enqueue_command = enqueue_cb;
	hooks.raise_dp_interrupt = interrupt_cb;

	process_command_ingest(ingest,
	                       dram.data(),
	                       sp_dmem.data(),
	                       dpc_start,
	                       dpc_end,
	                       dpc_current,
	                       dpc_status,
	                       hooks);

	const std::array<uint32_t, 7> expected_opcodes = { 0x3du, 0x35u, 0x33u, 0x32u, 0x26u, 0x27u, 0x28u };
	check(hook_state.enqueued_opcodes.size() == expected_opcodes.size(), "texture sequence enqueue count mismatch");
	for (size_t i = 0; i < expected_opcodes.size(); i++)
	{
		check(hook_state.enqueued_opcodes[i] == expected_opcodes[i], "texture sequence opcode mismatch");
		check(hook_state.enqueued_words[i] == 2u, "texture sequence command length mismatch");
	}

	check(hook_state.interrupt_count == 0u, "non-Full sync commands must not raise DP interrupt");
	check(dpc_start == dpc_end && dpc_current == dpc_end, "DPC registers must reset to END");

	std::cout << "emu_conformance_rdp_texture_load_sequence_test: PASS" << std::endl;
	return 0;
}
