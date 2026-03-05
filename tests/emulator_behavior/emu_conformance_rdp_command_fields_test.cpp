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
	std::vector<char> events;
	uint64_t timeline_counter = 0;
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

static uint64_t signal_cb(void *userdata)
{
	auto *state = static_cast<HookState *>(userdata);
	state->events.push_back('S');
	return ++state->timeline_counter;
}

static void wait_cb(void *userdata, uint64_t timeline)
{
	auto *state = static_cast<HookState *>(userdata);
	check(timeline == state->timeline_counter, "wait timeline mismatch");
	state->events.push_back('W');
}

static void interrupt_cb(void *userdata)
{
	auto *state = static_cast<HookState *>(userdata);
	state->events.push_back('I');
}

static void write_command_pair(std::array<uint8_t, 128> &memory, size_t pair_index, uint32_t w0, uint32_t w1)
{
	uint32_t words[2] = { w0, w1 };
	std::memcpy(memory.data() + pair_index * sizeof(uint64_t), words, sizeof(words));
}
}

int main()
{
	std::array<uint8_t, 128> dram = {};
	std::array<uint8_t, 128> sp_dmem = {};
	std::array<uint32_t, 64> cmd_data = {};

	write_command_pair(dram, 0, 0x36000000u, 0x11111111u); // FillRectangle, 1 command word pair.
	write_command_pair(dram, 1, 0x24000000u, 0x22222222u); // TextureRectangle, 2 command word pairs.
	write_command_pair(dram, 2, 0x33333333u, 0x44444444u); // TextureRectangle continuation payload.
	write_command_pair(dram, 3, 0x29000000u, 0x55555555u); // SyncFull, 1 command word pair.

	CommandIngestState ingest = {};
	ingest.cmd_data = cmd_data.data();

	uint32_t dpc_start = 0;
	uint32_t dpc_current = 0;
	uint32_t dpc_end = 4u * sizeof(uint64_t);
	uint32_t dpc_status = 0;

	HookState async_hooks_state;
	CommandIngestHooks hooks = {};
	hooks.frontend_available = true;
	hooks.synchronous = false;
	hooks.userdata = &async_hooks_state;
	hooks.enqueue_command = enqueue_cb;
	hooks.signal_timeline = signal_cb;
	hooks.wait_for_timeline = wait_cb;
	hooks.raise_dp_interrupt = interrupt_cb;

	process_command_ingest(ingest,
	                       dram.data(),
	                       sp_dmem.data(),
	                       dpc_start,
	                       dpc_end,
	                       dpc_current,
	                       dpc_status,
	                       hooks);

	check(async_hooks_state.enqueued_words.size() == 3u, "enqueue count mismatch");
	check(async_hooks_state.enqueued_words[0] == 2u, "FillRectangle command length mismatch");
	check(async_hooks_state.enqueued_words[1] == 4u, "TextureRectangle command length mismatch");
	check(async_hooks_state.enqueued_words[2] == 2u, "SyncFull command length mismatch");
	check(async_hooks_state.enqueued_opcodes[0] == 0x36u, "FillRectangle opcode mismatch");
	check(async_hooks_state.enqueued_opcodes[1] == 0x24u, "TextureRectangle opcode mismatch");
	check(async_hooks_state.enqueued_opcodes[2] == 0x29u, "SyncFull opcode mismatch");
	check(async_hooks_state.events.size() == 1u && async_hooks_state.events[0] == 'I',
	      "async SyncFull must only raise interrupt");
	check(dpc_start == dpc_end && dpc_current == dpc_end, "DPC registers must reset to END");

	write_command_pair(dram, 0, 0x29000000u, 0x99999999u);
	ingest.cmd_cur = 0;
	ingest.cmd_ptr = 0;
	dpc_start = 0;
	dpc_current = 0;
	dpc_end = sizeof(uint64_t);
	dpc_status = 0;

	HookState sync_hooks_state;
	hooks.synchronous = true;
	hooks.userdata = &sync_hooks_state;

	process_command_ingest(ingest,
	                       dram.data(),
	                       sp_dmem.data(),
	                       dpc_start,
	                       dpc_end,
	                       dpc_current,
	                       dpc_status,
	                       hooks);

	check(sync_hooks_state.enqueued_words.size() == 1u && sync_hooks_state.enqueued_words[0] == 2u,
	      "synchronous SyncFull enqueue mismatch");
	check(sync_hooks_state.events.size() == 3u, "synchronous SyncFull event count mismatch");
	check(sync_hooks_state.events[0] == 'S' && sync_hooks_state.events[1] == 'W' && sync_hooks_state.events[2] == 'I',
	      "synchronous SyncFull ordering mismatch");

	std::cout << "emu_conformance_rdp_command_fields_test: PASS" << std::endl;
	return 0;
}
