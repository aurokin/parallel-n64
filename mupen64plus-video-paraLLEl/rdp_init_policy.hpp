#ifndef PARALLEL_RDP_INIT_POLICY_HPP
#define PARALLEL_RDP_INIT_POLICY_HPP

#include <cstddef>
#include <cstdint>

namespace RDP
{
namespace detail
{
struct SyncFrameCounts
{
	unsigned num_frames = 0;
	unsigned num_sync_frames = 0;
};

inline bool init_prerequisites_met(const void *context_ptr, const void *vulkan_ptr)
{
	return context_ptr != nullptr && vulkan_ptr != nullptr;
}

inline SyncFrameCounts compute_sync_frame_counts(unsigned mask)
{
	SyncFrameCounts counts = {};
	for (unsigned i = 0; i < 32; i++)
	{
		if (mask & (1u << i))
		{
			counts.num_frames = i + 1;
			counts.num_sync_frames++;
		}
	}
	return counts;
}

struct HostMemoryImportPlan
{
	uintptr_t aligned_rdram = 0;
	uintptr_t offset = 0;
};

inline HostMemoryImportPlan plan_host_memory_import(uintptr_t rdram_ptr,
                                                    bool supports_external_memory_host,
                                                    size_t min_alignment)
{
	HostMemoryImportPlan plan = {};
	plan.aligned_rdram = rdram_ptr;

	if (!supports_external_memory_host)
		return plan;

	if (min_alignment == 0)
		return plan;

	// Vulkan implementations should expose power-of-two alignments, but keep this robust.
	if ((min_alignment & (min_alignment - 1)) == 0)
		plan.offset = rdram_ptr & (uintptr_t(min_alignment) - 1);
	else
		plan.offset = rdram_ptr % uintptr_t(min_alignment);

	plan.aligned_rdram -= plan.offset;
	return plan;
}

template <typename FrontendPtr>
inline bool ensure_frontend_device_supported(FrontendPtr &frontend)
{
	if (!frontend)
		return false;
	if (!frontend->device_is_supported())
	{
		frontend.reset();
		return false;
	}
	return true;
}

template <typename BeginTs, typename EndTs, typename RetroImageHandles, typename RetroImages,
          typename FrontendPtr, typename DevicePtr, typename ContextPtr>
inline void clear_deinit_state(BeginTs &begin_ts,
                               EndTs &end_ts,
                               RetroImageHandles &retro_image_handles,
                               RetroImages &retro_images,
                               FrontendPtr &frontend,
                               DevicePtr &device,
                               ContextPtr &context)
{
	begin_ts.reset();
	end_ts.reset();
	retro_image_handles.clear();
	retro_images.clear();
	frontend.reset();
	device.reset();
	context.reset();
}
}
}

#endif
