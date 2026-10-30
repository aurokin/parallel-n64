#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_residency_policy.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace RDP::detail;

static void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

int main()
{
	struct TileState
	{
		uint32_t vk_image_index = 1;
		bool hit = true;
		uint64_t checksum = 123, selector = 456;
	} tile;
	struct TileBinding { uint32_t repl_desc_index = HiresInvalidSlot; } binding;
	binding.repl_desc_index = 1;
	clear_hires_evicted_binding(2, tile, binding);
	check(tile.hit && binding.repl_desc_index == 1, "eviction preserves unrelated tile bindings");
	clear_hires_evicted_binding(1, tile, binding);
	check(!tile.hit && tile.vk_image_index == HiresInvalidSlot && binding.repl_desc_index == HiresInvalidSlot,
	      "an evicted slot cannot remain bound to a tile");
	check(tile.checksum == 123 && tile.selector == 456, "eviction preserves lookup identity and selector");

	HiresResidencyPolicy policy;
	std::vector<uint32_t> evicted;
	auto evict = [&](uint32_t slot) { evicted.push_back(slot); };
	auto admit = [&](uint64_t bytes, uint64_t frame) { return policy.admit(bytes, frame, evict); };

	// N64-14: one incoming texture requires multiple victims. Releasing both
	// the new reservation and the survivor must leave no stale byte accounting.
	policy.reset(10, 100);
	check(admit(30, 0).slot == 1 && admit(30, 0).slot == 2 && admit(30, 0).slot == 3,
	      "initial slot order");
	const auto large = admit(70, 8);
	check(large.reason == HiresAdmission::Admitted && large.slot == 2 && evicted == std::vector<uint32_t>({1, 2}),
	      "a single admission must evict more than one retired texture");
	check(policy.resident_bytes == 100 && policy.resident_count == 2, "multi-eviction respects budget");
	policy.release(large.slot);
	check(policy.resident_bytes == 30, "release subtracts the incoming reservation exactly once");
	policy.release(3);
	check(policy.resident_bytes == 0 && policy.resident_count == 0, "all releases return accounting to zero");

	policy.reset(3, 100);
	evicted.clear();
	check(admit(30, 0).slot == 1 && admit(30, 1).slot == 2, "fill two slots");
	check(admit(80, 8).reason == HiresAdmission::Saturated && evicted.empty(),
	      "preflight must not evict an old image when the remaining victims are too recent");
	check(policy.resident_bytes == 60 && policy.resident_count == 2, "refusal changes no accounting");
	policy.touch(1, 8);
	for (uint64_t frame = 9; frame < 16; frame++)
	{
		policy.touch(2, frame);
		check(admit(80, frame).reason == HiresAdmission::Saturated && evicted.empty(),
		      "a bound texture used within eight frames must never be rebound");
	}
	check(admit(30, 16).slot == 1 && evicted == std::vector<uint32_t>({1}),
	      "slot reuse starts exactly eight frames after its last draw-time touch");
	check(policy.resident_bytes == 60, "slot reuse preserves exact accounting");

	policy.reset(4, 90);
	evicted.clear();
	admit(30, 0); admit(30, 0); admit(30, 0);
	policy.touch(1, 1); policy.touch(1, 1);
	check(admit(30, 9).slot == 2 && evicted == std::vector<uint32_t>({2}),
	      "first touch per frame determines deterministic LRU order");
	check(admit(30, 9).slot == 3 && evicted == std::vector<uint32_t>({2, 3}),
	      "LRU order is stable after reuse");

	policy.reset(2, 0);
	evicted.clear();
	check(admit(1000, 0).slot == 1, "zero budget is unlimited");
	check(admit(1, 7).reason == HiresAdmission::NoSlot && evicted.empty(), "slot exhaustion obeys retirement");
	check(admit(1, 8).slot == 1 && policy.resident_bytes == 1, "unlimited bytes still reuse retired slots");
	policy.reset(0, 100);
	check(admit(1, 0).reason == HiresAdmission::NoSlot, "empty capacity refuses cleanly");

	policy.reset(3, 100);
	unsigned decodes = 0, uploads = 0;
	auto load = [&](uint32_t) { decodes++; uploads++; return true; };
	check(policy.admit_and_load(100, 0, evict, load).reason == HiresAdmission::Admitted, "initial load");
	check(policy.admit_and_load(1, 0, evict, load).reason == HiresAdmission::Saturated,
	      "budget refusal before decode");
	check(policy.admit_and_load(101, 0, evict, load).reason == HiresAdmission::TooLarge,
	      "oversized refusal before decode");
	check(decodes == 1 && uploads == 1, "refusals cost no decode or upload");
	policy.reset(3, 100);
	check(policy.admit_and_load(70, 0, evict, [&](uint32_t) {
		decodes++; uploads++; return false;
	}).reason == HiresAdmission::LoadFailed, "failed upload reports failure");
	check(policy.resident_bytes == 0 && policy.resident_count == 0, "failed upload releases reservation");
	check(policy.admit_and_load(100, 0, evict, load).slot == 1, "failed upload returns its slot immediately");
	policy.reset(3, 100);
	check(policy.resident_bytes == 0 && policy.resident_count == 0, "clear resets accounting");

	check(hires_slot_capacity(51211, 4096) == 4096 && hires_slot_capacity(UINT64_MAX, UINT32_MAX) == 65536,
	      "capacity fits both device and shader limits without entry-count overflow");
	check(hires_slot_capacity(10, 4096) == 11 && hires_slot_capacity(0, 4096) == 1,
	      "capacity reserves the white fallback");
	constexpr uint64_t MiB = 1024 * 1024, GiB = 1024 * MiB;
	for (bool extension : {false, true})
	{
		auto budget = [&](HiresDeviceClass kind, uint64_t available) {
			return hires_default_budget(kind, extension ? 32 * GiB : available, extension, available);
		};
		check(budget(HiresDeviceClass::Discrete, 7 * GiB) == 3584 * MiB, "discrete uses half the selected heap");
		check(budget(HiresDeviceClass::Discrete, 32 * GiB) == 4 * GiB, "discrete upper clamp");
		check(budget(HiresDeviceClass::Discrete, 128 * MiB) == 512 * MiB, "discrete lower clamp");
		check(budget(HiresDeviceClass::Shared, 5 * GiB) == 1280 * MiB, "shared uses a quarter of selected heap");
		check(budget(HiresDeviceClass::Shared, 32 * GiB) == 2 * GiB, "shared upper clamp");
		check(budget(HiresDeviceClass::Shared, 128 * MiB) == 256 * MiB, "shared lower clamp");
		check(budget(HiresDeviceClass::Mobile, 3 * GiB) == 768 * MiB, "mobile uses a quarter of selected heap");
		check(budget(HiresDeviceClass::Mobile, 32 * GiB) == GiB, "mobile upper clamp");
		check(budget(HiresDeviceClass::Mobile, 128 * MiB) == 256 * MiB, "mobile lower clamp");
	}
	std::cout << "emu_unit_hires_residency_test: PASS\n";
}
