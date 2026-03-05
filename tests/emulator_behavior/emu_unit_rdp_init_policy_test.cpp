#include "mupen64plus-video-paraLLEl/rdp_init_policy.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
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

static void test_init_prerequisite_checks()
{
	int dummy = 1;
	check(!init_prerequisites_met(nullptr, nullptr), "null context + null vulkan should fail");
	check(!init_prerequisites_met(&dummy, nullptr), "null vulkan should fail");
	check(!init_prerequisites_met(nullptr, &dummy), "null context should fail");
	check(init_prerequisites_met(&dummy, &dummy), "non-null context + vulkan should pass");
}

static void test_sync_frame_counts()
{
	{
		const SyncFrameCounts c = compute_sync_frame_counts(0x0u);
		check(c.num_frames == 0u, "mask=0 num_frames mismatch");
		check(c.num_sync_frames == 0u, "mask=0 num_sync_frames mismatch");
	}
	{
		const SyncFrameCounts c = compute_sync_frame_counts(0x5u);
		check(c.num_frames == 3u, "mask=0b101 num_frames mismatch");
		check(c.num_sync_frames == 2u, "mask=0b101 num_sync_frames mismatch");
	}
	{
		const SyncFrameCounts c = compute_sync_frame_counts(0x80000000u);
		check(c.num_frames == 32u, "highest-bit mask num_frames mismatch");
		check(c.num_sync_frames == 1u, "highest-bit mask num_sync_frames mismatch");
	}
	{
		const SyncFrameCounts c = compute_sync_frame_counts(0xffffffffu);
		check(c.num_frames == 32u, "full mask num_frames mismatch");
		check(c.num_sync_frames == 32u, "full mask num_sync_frames mismatch");
	}
}

static void test_host_memory_import_plan()
{
	{
		const HostMemoryImportPlan p = plan_host_memory_import(0x100123u, false, 65536u);
		check(p.aligned_rdram == 0x100123u, "unsupported-host path should preserve pointer");
		check(p.offset == 0u, "unsupported-host path should keep zero offset");
	}
	{
		const HostMemoryImportPlan p = plan_host_memory_import(0x100123u, true, 65536u);
		check(p.aligned_rdram == 0x100000u, "power-of-two alignment failed");
		check(p.offset == 0x123u, "power-of-two offset failed");
	}
	{
		const HostMemoryImportPlan p = plan_host_memory_import(12345u, true, 1000u);
		check(p.aligned_rdram == 12000u, "non-power-of-two alignment failed");
		check(p.offset == 345u, "non-power-of-two offset failed");
	}
	{
		const HostMemoryImportPlan p = plan_host_memory_import(0x100123u, true, 0u);
		check(p.aligned_rdram == 0x100123u, "zero-alignment should preserve pointer");
		check(p.offset == 0u, "zero-alignment should keep zero offset");
	}
}

struct FakeFrontend
{
	explicit FakeFrontend(bool supported_)
	    : supported(supported_)
	{
	}

	bool device_is_supported() const
	{
		return supported;
	}

	bool supported = false;
};

static void test_ensure_frontend_device_supported()
{
	{
		std::unique_ptr<FakeFrontend> frontend;
		check(!ensure_frontend_device_supported(frontend), "null frontend should fail");
	}
	{
		std::unique_ptr<FakeFrontend> frontend(new FakeFrontend(true));
		check(ensure_frontend_device_supported(frontend), "supported frontend should pass");
		check(frontend != nullptr, "supported frontend should remain allocated");
	}
	{
		std::unique_ptr<FakeFrontend> frontend(new FakeFrontend(false));
		check(!ensure_frontend_device_supported(frontend), "unsupported frontend should fail");
		check(frontend == nullptr, "unsupported frontend should be reset");
	}
}

struct FakeHandle
{
	void reset()
	{
		reset_calls++;
	}

	unsigned reset_calls = 0;
};

static void test_clear_deinit_state_idempotency()
{
	FakeHandle begin = {};
	FakeHandle end = {};
	std::vector<int> image_handles = {1, 2, 3};
	std::vector<int> images = {4, 5};
	std::unique_ptr<int> frontend(new int(1));
	std::unique_ptr<int> device(new int(2));
	std::unique_ptr<int> context(new int(3));

	clear_deinit_state(begin, end, image_handles, images, frontend, device, context);
	check(begin.reset_calls == 1u, "begin reset count mismatch after first clear");
	check(end.reset_calls == 1u, "end reset count mismatch after first clear");
	check(image_handles.empty(), "image_handles should be cleared");
	check(images.empty(), "images should be cleared");
	check(frontend == nullptr, "frontend should be reset");
	check(device == nullptr, "device should be reset");
	check(context == nullptr, "context should be reset");

	clear_deinit_state(begin, end, image_handles, images, frontend, device, context);
	check(begin.reset_calls == 2u, "begin reset count mismatch after second clear");
	check(end.reset_calls == 2u, "end reset count mismatch after second clear");
	check(image_handles.empty(), "image_handles should remain cleared");
	check(images.empty(), "images should remain cleared");
	check(frontend == nullptr, "frontend should remain reset");
	check(device == nullptr, "device should remain reset");
	check(context == nullptr, "context should remain reset");
}
}

int main()
{
	test_init_prerequisite_checks();
	test_sync_frame_counts();
	test_host_memory_import_plan();
	test_ensure_frontend_device_supported();
	test_clear_deinit_state_idempotency();
	std::cout << "emu_unit_rdp_init_policy_test: PASS" << std::endl;
	return 0;
}
