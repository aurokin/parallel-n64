// SPDX-License-Identifier: MIT

#include "rdp_hires.hpp"
#include "rdp_renderer.hpp"
#include "rdp_hires_ci_palette_policy.hpp"
#include "rdp_hires_cutout_policy.hpp"
#include "rdp_hires_load_origin_policy.hpp"
#include "rdp_hires_runtime_policy.hpp"
#include "rdp_hires_tex_rect_policy.hpp"
#include "rdp_hires_texture_layout.hpp"
#include "texture_keying.hpp"
#include "texture_replacement.hpp"
#include "logging.hpp"
#include "timer.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace RDP
{
namespace detail
{
inline uint64_t compose_hires_checksum64(uint32_t texture_crc, uint32_t palette_crc)
{
	return (uint64_t(palette_crc) << 32) | uint64_t(texture_crc);
}

inline uint16_t clamp_hires_dimension_u16(uint32_t dim)
{
	return static_cast<uint16_t>(std::min<uint32_t>(dim, 0xffffu));
}

template <typename TileState>
inline void write_hires_lookup_tile_state(TileState &state,
                                          bool hit,
                                          uint64_t checksum64,
                                          uint64_t upload_checksum64,
                                          uint64_t selector_checksum64,
                                          uint16_t formatsize,
                                          uint32_t orig_w,
                                          uint32_t orig_h)
{
	state.valid = true;
	state.hit = hit;
	state.checksum64 = checksum64;
	state.upload_checksum64 = upload_checksum64;
	state.selector_checksum64 = selector_checksum64;
	state.formatsize = formatsize;
	state.orig_w = clamp_hires_dimension_u16(orig_w);
	state.orig_h = clamp_hires_dimension_u16(orig_h);
	state.key_w = state.orig_w;
	state.key_h = state.orig_h;
}

inline bool hires_rdram_view_valid(const void *cpu_rdram, size_t rdram_size)
{
	return cpu_rdram && rdram_size && ((rdram_size & (rdram_size - 1)) == 0);
}

inline bool should_update_tlut_shadow(bool rdram_view_ok, bool is_tlut_mode)
{
	return rdram_view_ok && is_tlut_mode;
}

inline bool should_run_hires_lookup(bool rdram_view_ok,
                                    bool has_replacement_provider,
                                    bool is_tlut_mode,
                                    uint32_t key_width_pixels,
                                    uint32_t key_height_pixels)
{
	return rdram_view_ok &&
	       has_replacement_provider &&
	       !is_tlut_mode &&
	       key_width_pixels > 0 &&
	       key_height_pixels > 0;
}

template <typename TileState, unsigned NumTiles>
inline void reset_hires_tracking_state(TileState (&tiles)[NumTiles],
                                       bool &tlut_shadow_valid,
                                       uint64_t &lookup_total,
                                       uint64_t &lookup_hits,
                                       uint64_t &lookup_misses)
{
	for (auto &tile : tiles)
		tile = {};
	tlut_shadow_valid = false;
	lookup_total = 0;
	lookup_hits = 0;
	lookup_misses = 0;
}

inline bool hires_provider_changed(const void *previous_provider, const void *next_provider)
{
	return previous_provider != next_provider;
}
}

namespace
{
static bool compute_hires_tile_size_pixels(const TileSize &size, uint32_t &width_pixels, uint32_t &height_pixels)
{
	const uint32_t sl = size.slo >> 2;
	const uint32_t sh = size.shi >> 2;
	const uint32_t tl = size.tlo >> 2;
	const uint32_t th = size.thi >> 2;
	if (sh < sl || th < tl)
		return false;

	width_pixels = ((sh - sl) + 1u) & 0xfffu;
	height_pixels = (th - tl) + 1u;
	return width_pixels != 0 && height_pixels != 0;
}

// GlideN64 sizes its texture-cache entries (the space hi-res replacements
// are authored in) from the rendering tile's SetTileSize extent clamped by
// the coordinate masks. This is also the window its Rice CRC hashes over.
static bool compute_hires_gliden64_display_dims(const TileMeta &meta, const TileSize &size,
                                                uint32_t &width_pixels, uint32_t &height_pixels)
{
	if (!compute_hires_tile_size_pixels(size, width_pixels, height_pixels))
		return false;
	if (meta.mask_s != 0)
		width_pixels = std::min(width_pixels, 1u << meta.mask_s);
	if (meta.mask_t != 0)
		height_pixels = std::min(height_pixels, 1u << meta.mask_t);
	return width_pixels != 0 && height_pixels != 0;
}

// Scan RDRAM CI texture data to find the maximum palette index used (GlideN64's "cimax").
// CI8: each byte is an index. CI4: each byte has two 4-bit indices.
static uint32_t scan_rdram_ci_max_index(
	const uint8_t *cpu_rdram, size_t rdram_size,
	uint32_t addr, uint32_t width, uint32_t height,
	uint32_t size_enum, uint32_t row_stride)
{
	uint32_t cimax = 0;
	const uint32_t limit = (size_enum == 0) ? 0x0fu : 0xffu;

	for (uint32_t y = 0; y < height && cimax < limit; y++)
	{
		const uint32_t row_addr = addr + y * row_stride;
		if (size_enum == 0)
		{
			// CI4: two indices per byte, width/2 bytes per row (round up)
			const uint32_t bytes_per_row = (width + 1) >> 1;
			for (uint32_t x = 0; x < bytes_per_row && cimax < limit; x++)
			{
				const uint8_t b = cpu_rdram[(row_addr + x) & (rdram_size - 1)];
				const uint32_t hi = (b >> 4) & 0x0f;
				const uint32_t lo = b & 0x0f;
				if (hi > cimax) cimax = hi;
				if (lo > cimax) cimax = lo;
			}
		}
		else
		{
			// CI8: one index per byte
			for (uint32_t x = 0; x < width && cimax < limit; x++)
			{
				const uint8_t idx = cpu_rdram[(row_addr + x) & (rdram_size - 1)];
				if (idx > cimax) cimax = idx;
			}
		}
	}
	return cimax;
}

// Compute the lazy draw key over the masked view and its tracked source layout.
// LoadTile uses source rows/crop, while LoadBlock preserves the line-based path.
// Optional bank-zero identity is diagnostic evidence for CI4 palette-role
// comparisons. It must not substitute for the current rendering tile's bank.
static uint64_t compute_gliden64_compat_checksum64(
	const uint8_t *cpu_rdram, size_t rdram_size,
	uint32_t rdram_load_addr,
	const TileMeta &meta,
	const TileSize &tile_size,
	const detail::HiresLoadTileSource &source,
	const uint8_t *tlut_shadow,
	bool tlut_shadow_valid,
	uint64_t *alt_checksum64)
{
	if (alt_checksum64)
		*alt_checksum64 = 0;
	if (!cpu_rdram || rdram_size == 0)
		return 0;

	detail::HiresCompatView view;
	if (!detail::compute_hires_compat_view(meta, tile_size, rdram_load_addr, source, view))
		return 0;
	const uint32_t tile_width = view.width, tile_height = view.height;
	const uint32_t bpl = view.row_stride;
	rdram_load_addr = view.address;

	// GlideN64 uses the tile descriptor's size (meta.size), not SetTextureImage's size.
	// For IA textures, SetTextureImage.size may be 16bpp while tile.size is 8bpp.
	const uint32_t tile_size_enum = uint32_t(meta.size);

	uint32_t texture_crc = rice_crc32_wrapped(
		cpu_rdram, rdram_size, rdram_load_addr,
		tile_width, tile_height,
		tile_size_enum, bpl);

	// For CI textures, compute palette CRC matching GlideN64's checksum64 convention:
	// checksum64 = (palette_crc << 32) | texture_crc
	if (meta.fmt == TextureFormat::CI && tlut_shadow && tlut_shadow_valid)
	{
		const uint32_t cimax = scan_rdram_ci_max_index(
			cpu_rdram, rdram_size, rdram_load_addr,
			tile_width, tile_height, tile_size_enum, bpl);

		uint32_t palette_crc = 0;
		if (meta.size == TextureSize::Bpp8)
		{
			// CI8: CRC over full palette, stride=512 (256 entries × 2 bytes)
			palette_crc = rice_crc32_wrapped(
				tlut_shadow, 512, 0,
				cimax + 1, 1, 2, 512);
		}
		else
		{
			// CI4: CRC over 16-entry palette bank, stride=32 (16 entries × 2 bytes)
			const uint32_t bank_offset = uint32_t(meta.palette) * 32u;
			if (bank_offset + 32u <= 512u)
			{
				palette_crc = rice_crc32_wrapped(
					tlut_shadow, 512, bank_offset,
					cimax + 1, 1, 2, 32);
			}

			if (alt_checksum64 && bank_offset != 0)
			{
				const uint32_t bank0_palette_crc = rice_crc32_wrapped(
					tlut_shadow, 512, 0,
					cimax + 1, 1, 2, 32);
				if (bank0_palette_crc != palette_crc)
					*alt_checksum64 = (uint64_t(bank0_palette_crc) << 32) | uint64_t(texture_crc);
			}
		}

		return (uint64_t(palette_crc) << 32) | uint64_t(texture_crc);
	}

	return uint64_t(texture_crc);
}

static uint16_t emulate_load_tlut_entry_word(const uint8_t *rdram, size_t rdram_size, uint32_t addr)
{
	const uint32_t mapped_addr = addr ^ 2u;
	const uint8_t b0 = wrapped_read_u8(rdram, rdram_size, mapped_addr + 0);
	const uint8_t b1 = wrapped_read_u8(rdram, rdram_size, mapped_addr + 1);
	return uint16_t(b1) | (uint16_t(b0) << 8);
}

// Returns whether any zero-alpha texel carried nonzero RGB.
static bool zero_transparent_replacement_rgb(std::vector<uint8_t> &rgba8)
{
	bool changed = false;
	for (size_t i = 0; i + 3 < rgba8.size(); i += 4)
	{
		if (rgba8[i + 3] == 0)
		{
			changed = changed || (rgba8[i + 0] | rgba8[i + 1] | rgba8[i + 2]) != 0;
			rgba8[i + 0] = 0;
			rgba8[i + 1] = 0;
			rgba8[i + 2] = 0;
		}
	}
	return changed;
}

static unsigned hires_replacement_mip_levels(unsigned w, unsigned h)
{
	unsigned levels = 1;
	while (w > 1 || h > 1)
	{
		w = std::max(w >> 1, 1u);
		h = std::max(h >> 1, 1u);
		levels++;
	}
	return levels;
}

static uint64_t hires_replacement_mip_chain_bytes(unsigned w, unsigned h)
{
	uint64_t total = 0;
	for (;;)
	{
		total += uint64_t(w) * uint64_t(h) * 4u;
		if (w == 1 && h == 1)
			break;
		w = std::max(w >> 1, 1u);
		h = std::max(h >> 1, 1u);
	}
	return total;
}

// Alpha-weighted 2x2 box reduce. Transparent texels carry zeroed RGB, so a
// plain average would pull cutout mip levels toward black; weight RGB by
// alpha and renormalize, while alpha itself takes the plain average.
static void downsample_replacement_rgba8_alpha_weighted(
		const std::vector<uint8_t> &src, unsigned sw, unsigned sh,
		std::vector<uint8_t> &dst, unsigned dw, unsigned dh)
{
	dst.resize(size_t(dw) * size_t(dh) * 4u);
	for (unsigned y = 0; y < dh; y++)
	{
		unsigned sy0 = std::min(2u * y, sh - 1u);
		unsigned sy1 = std::min(2u * y + 1u, sh - 1u);
		for (unsigned x = 0; x < dw; x++)
		{
			unsigned sx0 = std::min(2u * x, sw - 1u);
			unsigned sx1 = std::min(2u * x + 1u, sw - 1u);
			const uint8_t *taps[4] = {
				&src[4u * (size_t(sy0) * sw + sx0)],
				&src[4u * (size_t(sy0) * sw + sx1)],
				&src[4u * (size_t(sy1) * sw + sx0)],
				&src[4u * (size_t(sy1) * sw + sx1)],
			};
			uint32_t rsum = 0, gsum = 0, bsum = 0, asum = 0;
			for (auto *t : taps)
			{
				uint32_t a = t[3];
				rsum += t[0] * a;
				gsum += t[1] * a;
				bsum += t[2] * a;
				asum += a;
			}
			uint8_t *d = &dst[4u * (size_t(y) * dw + x)];
			if (asum != 0)
			{
				d[0] = uint8_t((rsum + asum / 2) / asum);
				d[1] = uint8_t((gsum + asum / 2) / asum);
				d[2] = uint8_t((bsum + asum / 2) / asum);
			}
			else
			{
				d[0] = 0;
				d[1] = 0;
				d[2] = 0;
			}
			d[3] = uint8_t((asum + 2) / 4);
		}
	}
}

static bool should_alias_hires_tile_binding(const TileMeta &source_meta, const TileMeta &target_meta)
{
	return source_meta.offset == target_meta.offset &&
	       source_meta.stride == target_meta.stride &&
	       source_meta.fmt == target_meta.fmt &&
	       source_meta.size == target_meta.size &&
	       source_meta.palette == target_meta.palette;
}

static bool should_alias_hires_load_binding(const TileMeta &source_meta, const TileMeta &target_meta)
{
	return source_meta.offset == target_meta.offset;
}

static bool should_apply_hires_propagated_binding(const TileMeta &source_meta, const TileMeta &target_meta)
{
	return should_alias_hires_tile_binding(source_meta, target_meta) ||
	       should_alias_hires_load_binding(source_meta, target_meta);
}
}

void HiresReplacement::init(Vulkan::Device *device_, bool descriptor_indexing, std::function<void()> flush_and_signal_)
{
	device = device_;
	flush_and_signal_callback = std::move(flush_and_signal_);
	shader_enabled = descriptor_indexing;
	if (const char *env = getenv("PARALLEL_RDP_DISABLE_HIRES_SHADER"))
	{
		if (strtol(env, nullptr, 0) != 0)
		{
			LOGW("Disabling hi-res replacement shader path due to PARALLEL_RDP_DISABLE_HIRES_SHADER.\n");
			shader_enabled = false;
		}
	}

	// Loads and evictions rewrite pool slots that no pending command buffer
	// reads while earlier frames still execute with the same set.
	if (shader_enabled && !device->get_device_features().descriptor_indexing_features.descriptorBindingUpdateUnusedWhilePending)
	{
		LOGW("Disabling hi-res replacement shader path: descriptorBindingUpdateUnusedWhilePending is unsupported.\n");
		shader_enabled = false;
	}

	if (shader_enabled && !init_hires_resources(1))
	{
		LOGW("Failed to initialize hi-res bindless resources. Hi-res shader path will stay disabled.\n");
		shader_enabled = false;
	}
}

void HiresReplacement::set_rdram(const uint8_t *host_rdram, size_t size)
{
	cpu_rdram = host_rdram;
	rdram_size = size;
}

bool HiresReplacement::attach(const ReplacementProvider *provider)
{
	if (detail::hires_provider_changed(replacement_provider, provider))
	{
		clear_hires_images();
		// Shaders already declare set 3. Retain its fallback binding even if
		// allocating the next pack's pool (or the one-slot pool) fails.
		auto previous = std::move(hires_resources);
		hires_resources = {};
		hires_resources.bindless_pool = std::move(previous.bindless_pool);
		hires_resources.fallback_image = std::move(previous.fallback_image);
	}

	replacement_provider = provider;
	detail::reset_hires_tracking_state(
			replacement_tiles, tlut_shadow_valid, hires_lookup_total, hires_lookup_hits, hires_lookup_misses);
	memset(tlut_shadow, 0, sizeof(tlut_shadow));
	memset(tlut_tmem_shadow, 0, sizeof(tlut_tmem_shadow));
	hires_descriptor_sampled_resolutions = 0;
	hires_descriptor_sampled_family_singleton_resolutions = 0;
	hires_descriptor_sampled_ordered_surface_singleton_resolutions = 0;
	hires_descriptor_sampled_exact_selector_resolutions = 0;
	hires_descriptor_native_checksum_resolutions = 0;
	hires_descriptor_native_checksum_exact_resolutions = 0;
	hires_descriptor_native_checksum_identity_assisted_resolutions = 0;
	hires_descriptor_native_checksum_generic_fallback_resolutions = 0;
	hires_descriptor_compat_resolutions = 0;
	for (auto &memo : hires_strong_memo)
		memo = {};
	hires_strong_draw_hits = 0;
	hires_compat_draw_time_hits = 0;
	hires_compat_draw_time_ci_attempts = 0;
	hires_compat_draw_time_ci_hits = 0;
	hires_jabo_lane_enabled = provider && provider->has_jabo_entries();
	hires_jabo_draw_hits = 0;
	hires_jabo_draw_misses = 0;
	hires_jabo_md5_computations = 0;
	hires_jabo_memo_hits = 0;
	hires_jabo_memo.clear();
	rule_counters = {};

	bool ready = true;
	if (provider)
	{
		// A constant guard keeps residency deterministic across hosts. Refuse
		// unsupported frontend configurations rather than shortening GPU retirement.
		ready = shader_enabled && device->get_num_frame_contexts() + 1 <= detail::HiresRetirementFrames;
		if (ready)
		{
			assert(device->get_num_frame_contexts() + 1 <= detail::HiresRetirementFrames);
			// The bindless layout holds at most VULKAN_NUM_BINDINGS_BINDLESS_VARYING
			// images (clamped to the device limit in DescriptorSetAllocator).
			const auto limit = std::min<uint32_t>(
					device->get_device_features().descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindSampledImages,
					Vulkan::VULKAN_NUM_BINDINGS_BINDLESS_VARYING);
			ready = init_hires_resources(detail::hires_slot_capacity(provider->entry_count(), limit));
		}
		if (!ready)
		{
			LOGW("Hi-res resources unavailable (descriptor pool or frame-context limit).\n");
			replacement_provider = nullptr;
			init_hires_resources(1);
		}
	}
	else if (shader_enabled)
		init_hires_resources(1);

	return ready;
}

void HiresReplacement::begin_frame()
{
	if (replacement_provider)
	{
		hires_frame++;
		hires_residency_status.load_ms_frame = 0;
	}
}

void HiresReplacement::bind(Vulkan::CommandBuffer &cmd) const
{
	if (shader_enabled && hires_resources.bindless_pool)
		cmd.set_bindless(3, hires_resources.bindless_pool->get_descriptor_set());
}

void HiresReplacement::set_gliden64_compat_crc(bool enable)
{
	hires_gliden64_compat_crc_enabled = enable;
	if (enable)
		LOGI("Hi-res GlideN64-compat RDRAM CRC fallback enabled.\n");
}

void HiresReplacement::set_filter(unsigned mode)
{
	if (mode > 2)
		mode = 1;
	hires_filter_mode = mode;
	LOGI("Hi-res replacement filter mode: %u (0=nearest, 1=bilinear, 2=trilinear).\n", mode);
}

Vulkan::ImageHandle HiresReplacement::create_hires_replacement_image_with_mips(const ReplacementImage &replacement)
{
	const unsigned width = replacement.meta.repl_w;
	const unsigned height = replacement.meta.repl_h;
	const unsigned levels = hires_replacement_mip_levels(width, height);

	// Level 0 reuses the decoded payload; lower levels are generated with
	// the alpha-weighted reduce so cutout mips stay halo-free.
	std::vector<std::vector<uint8_t>> mips(levels - 1);
	std::vector<Vulkan::ImageInitialData> initial(levels);
	initial[0].data = replacement.rgba8.data();
	initial[0].row_length = width;
	initial[0].image_height = height;

	const std::vector<uint8_t> *prev = &replacement.rgba8;
	unsigned prev_w = width, prev_h = height;
	for (unsigned level = 1; level < levels; level++)
	{
		unsigned mip_w = std::max(prev_w >> 1, 1u);
		unsigned mip_h = std::max(prev_h >> 1, 1u);
		downsample_replacement_rgba8_alpha_weighted(*prev, prev_w, prev_h, mips[level - 1], mip_w, mip_h);
		initial[level].data = mips[level - 1].data();
		initial[level].row_length = mip_w;
		initial[level].image_height = mip_h;
		prev = &mips[level - 1];
		prev_w = mip_w;
		prev_h = mip_h;
	}

	auto info = Vulkan::ImageCreateInfo::immutable_2d_image(width, height, VK_FORMAT_R8G8B8A8_UNORM, false);
	info.levels = levels;
	// The RDP samples on AsyncCompute; sharing also makes Granite wait for the upload and layout transition.
	info.misc |= Vulkan::IMAGE_MISC_CONCURRENT_QUEUE_GRAPHICS_BIT |
	             Vulkan::IMAGE_MISC_CONCURRENT_QUEUE_ASYNC_COMPUTE_BIT;
	return device->create_image(info, initial.data());
}

void HiresReplacement::configure_gpu_budget(unsigned option_mb)
{
	const auto gpu = device->get_physical_device();
	uint32_t count = 0;
	vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, nullptr);
	std::vector<VkExtensionProperties> extensions(count);
	vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, extensions.data());
	hires_memory_budget_supported = std::any_of(extensions.begin(), extensions.end(), [](const VkExtensionProperties &ext) {
		return strcmp(ext.extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) == 0;
	});
	const auto &memory = device->get_memory_properties();
	uint64_t heap_size = 0;
	for (unsigned i = 0; i < memory.memoryHeapCount; i++)
		if ((memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) && memory.memoryHeaps[i].size > heap_size)
		{
			hires_heap_index = i;
			heap_size = memory.memoryHeaps[i].size;
		}
	VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
	if (hires_memory_budget_supported)
	{
		VkPhysicalDeviceMemoryProperties2 props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
		props.pNext = &budget;
		vkGetPhysicalDeviceMemoryProperties2(gpu, &props);
	}
#if defined(__ANDROID__) || defined(ANDROID) || defined(IOS)
	const auto device_class = detail::HiresDeviceClass::Mobile;
#else
	const auto device_class = device->get_gpu_properties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ?
	                          detail::HiresDeviceClass::Discrete : detail::HiresDeviceClass::Shared;
#endif
	hires_residency_status = {};
	hires_residency_status.budget = detail::hires_default_budget(device_class, heap_size,
	                                                         hires_memory_budget_supported, budget.heapBudget[hires_heap_index]);
	const char *basis = hires_memory_budget_supported ? "heapBudget" : "heap size";
	if (option_mb)
	{
		hires_residency_status.budget = uint64_t(option_mb) * 1024 * 1024;
		basis = "core option";
	}
	if (const char *env = getenv("PARALLEL_RDP_HIRES_GPU_BUDGET_MB"))
	{
		char *end = nullptr;
		const auto mb = strtoull(env, &end, 0);
		if (*env && *env != '-' && end && !*end && mb <= UINT64_MAX / (1024 * 1024))
		{
			hires_residency_status.budget = mb * 1024 * 1024;
			basis = "environment";
		}
	}
	LOGI("Hi-res memory budget: bytes=%llu basis=%s device_class=%u heap_size=%llu heap_budget=%llu.\n",
	     (unsigned long long)hires_residency_status.budget, basis, unsigned(device_class),
	     (unsigned long long)heap_size, (unsigned long long)budget.heapBudget[hires_heap_index]);
}

void HiresReplacement::log_summary() const
{
	emit_hires_summary("", true);
}

void HiresReplacement::log_summary_periodic(uint64_t frame) const
{
	char label[48];
	std::snprintf(label, sizeof(label), " (periodic, frame=%llu)", static_cast<unsigned long long>(frame));
	emit_hires_summary(label, false);
}

// label is inserted between the line name and its colon. The final summary
// passes "" so its format stays byte-identical for existing log parsers.
// Periodic snapshots skip the sampled-duplicate diagnostics: those describe the
// loaded pack rather than runtime traffic, and parsers count them per line.
void HiresReplacement::emit_hires_summary(const char *label, bool include_duplicate_diagnostics) const
{
	if (!replacement_provider)
		return;

	VkPhysicalDeviceMemoryBudgetPropertiesEXT heap = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
	if (hires_memory_budget_supported)
	{
		VkPhysicalDeviceMemoryProperties2 props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
		props.pNext = &heap;
		vkGetPhysicalDeviceMemoryProperties2(device->get_physical_device(), &props);
	}
	const auto &r = hires_residency_status;
	LOGI("Hi-res residency summary%s: mode=sync budget=%llu resident=%u resident_bytes=%llu high_water_bytes=%llu uploads=%llu upload_bytes=%llu evictions=%llu refused_saturated=%llu refused_too_large=%llu refused_no_slot=%llu decode_failures=%llu upload_failures=%llu load_ms_total=%.3f load_ms_max_frame=%.3f heap_usage=%llu heap_budget=%llu.\n",
	     label, (unsigned long long)r.budget, hires_resources.residency.resident_count,
	     (unsigned long long)hires_resources.residency.resident_bytes, (unsigned long long)r.high_water_bytes,
	     (unsigned long long)r.uploads, (unsigned long long)r.upload_bytes, (unsigned long long)r.evictions,
	     (unsigned long long)r.refused_saturated, (unsigned long long)r.refused_too_large, (unsigned long long)r.refused_no_slot,
	     (unsigned long long)r.decode_failures, (unsigned long long)r.upload_failures,
	     r.load_ms_total, r.load_ms_max_frame,
	     (unsigned long long)heap.heapUsage[hires_heap_index], (unsigned long long)heap.heapBudget[hires_heap_index]);
	uint64_t strong_computations = 0, strong_memo_hits = 0;
	for (const auto &memo : hires_strong_memo)
	{
		strong_computations += memo.computations;
		strong_memo_hits += memo.hits;
	}
	LOGI("Hi-res strong-hash summary%s: draw_hits=%llu computations=%llu memo_hits=%llu.\n",
	     label, static_cast<unsigned long long>(hires_strong_draw_hits),
	     static_cast<unsigned long long>(strong_computations), static_cast<unsigned long long>(strong_memo_hits));
	ReplacementProviderStats provider_stats = replacement_provider->get_stats();
	LOGI("Hi-res keying summary%s: lookups=%llu hits=%llu misses=%llu compat_draw_hits=%llu compat_draw_ci_hits=%llu compat_draw_ci_attempts=%llu provider=on entries=%u native_sampled=%u compat=%u sampled_index=%u sampled_dupe_keys=%u sampled_dupe_entries=%u sampled_families=%u sources(phrb=%u) descriptor_paths(sampled=%llu native_checksum=%llu compat=%llu) sampled_detail(family_singleton=%llu ordered_surface_singleton=%llu exact_selector=%llu).\n",
	     label,
	     static_cast<unsigned long long>(hires_lookup_total),
	     static_cast<unsigned long long>(hires_lookup_hits),
	     static_cast<unsigned long long>(hires_lookup_misses),
	     static_cast<unsigned long long>(hires_compat_draw_time_hits),
	     static_cast<unsigned long long>(hires_compat_draw_time_ci_hits),
	     static_cast<unsigned long long>(hires_compat_draw_time_ci_attempts),
	     provider_stats.entry_count,
	     provider_stats.native_sampled_entry_count,
	     provider_stats.compat_entry_count,
	     provider_stats.sampled_index_count,
	     provider_stats.sampled_duplicate_key_count,
	     provider_stats.sampled_duplicate_entry_count,
	     provider_stats.sampled_family_count,
	     provider_stats.source_phrb_entry_count,
	     static_cast<unsigned long long>(hires_descriptor_sampled_resolutions),
	     static_cast<unsigned long long>(hires_descriptor_native_checksum_resolutions),
	     static_cast<unsigned long long>(hires_descriptor_compat_resolutions),
	     static_cast<unsigned long long>(hires_descriptor_sampled_family_singleton_resolutions),
	     static_cast<unsigned long long>(hires_descriptor_sampled_ordered_surface_singleton_resolutions),
	     static_cast<unsigned long long>(hires_descriptor_sampled_exact_selector_resolutions));
	LOGI("Hi-res native checksum detail%s: exact=%llu identity_assisted=%llu generic_fallback=%llu.\n",
	     label,
	     static_cast<unsigned long long>(hires_descriptor_native_checksum_exact_resolutions),
	     static_cast<unsigned long long>(hires_descriptor_native_checksum_identity_assisted_resolutions),
	     static_cast<unsigned long long>(hires_descriptor_native_checksum_generic_fallback_resolutions));
	// Machine-readable: every key, always, in this order. Parsed by
	// tools/regression/run_scenarios.py into a result record's hires.rules.
	const auto &c = rule_counters;
	const auto u = [](uint64_t value) { return static_cast<unsigned long long>(value); };
	LOGI("Hi-res rule counters%s: alpha_kill_draws=%llu transparent_rgb_zeroed_textures=%llu "
	     "jabo_intensity_alpha_textures=%llu ci8_as_ci4_hits=%llu formatsize_mismatch_drops=%llu "
	     "view_exceeds_key_draws=%llu stale_binding_drops=%llu overlap_binding_drops=%llu "
	     "overlap_origin_drops=%llu tlut_image_base_loads=%llu texrect_snap_lifts=%llu "
	     "texrect_bottom_round_ups=%llu texrect_bottom_round_ups_unsnapped=%llu cutout_surface_draws=%llu "
	     "strong_crc_draw_hits=%llu load_origin_recoveries=%llu sampled_duplicate_reordered_keys=%llu "
	     "compat_lookups_unkeyed=%llu compat_lookups_after_miss=%llu compat_lookups_rekey=%llu "
	     "compat_rekey_confirmed=%llu compat_gate_skipped_misses=%llu compat_ci_live_palette_textures=%llu "
	     "repoint_origin_adoptions=%llu source_layout_retires=%llu.\n",
	     label, u(c.alpha_kill_draws), u(c.transparent_rgb_zeroed_textures),
	     u(provider_stats.jabo_intensity_alpha_textures), u(c.ci8_as_ci4_hits), u(c.formatsize_mismatch_drops),
	     u(c.view_exceeds_key_draws), u(c.stale_binding_drops), u(c.overlap_binding_drops),
	     u(c.overlap_origin_drops), u(c.tlut_image_base_loads), u(c.texrect_snap_lifts),
	     u(c.texrect_bottom_round_ups), u(c.texrect_bottom_round_ups_unsnapped), u(c.cutout_surface_draws),
	     u(hires_strong_draw_hits), u(c.load_origin_recoveries), u(provider_stats.sampled_duplicate_reordered_keys),
	     u(c.compat_lookups_unkeyed), u(c.compat_lookups_after_miss), u(c.compat_lookups_rekey),
	     u(c.compat_rekey_confirmed), u(c.compat_gate_skipped_misses), u(provider_stats.compat_ci_live_palette_textures),
	     u(c.repoint_origin_adoptions), u(c.source_layout_retires));
	if (hires_jabo_lane_enabled)
		LOGI("Hi-res Jabo summary%s: entries=%u hits=%llu misses=%llu md5=%llu memo_hits=%llu.\n",
		     label,
		     provider_stats.jabo_entry_count,
		     static_cast<unsigned long long>(hires_jabo_draw_hits),
		     static_cast<unsigned long long>(hires_jabo_draw_misses),
		     static_cast<unsigned long long>(hires_jabo_md5_computations),
		     static_cast<unsigned long long>(hires_jabo_memo_hits));
	if (!include_duplicate_diagnostics)
		return;
	for (const auto &diag : replacement_provider->get_sampled_duplicate_diagnostics(10))
	{
		LOGI("Hi-res sampled duplicate: sampled_fmt=%u sampled_siz=%u tex_offset=%u stride=%u wh=%ux%u sampled_low32=%08x palette_crc=%08x fs=%u selector=%016llx total_entries=%u duplicate_entries=%u active_checksum=%016llx repl=%ux%u policy=%s replacement_id=%s sampled_object=%s source=%s.\n",
		     diag.sampled_fmt,
		     diag.sampled_siz,
		     diag.sampled_tex_offset,
		     diag.sampled_stride,
		     diag.sampled_width,
		     diag.sampled_height,
		     diag.sampled_low32,
		     diag.sampled_palette_crc,
		     diag.formatsize,
		     static_cast<unsigned long long>(diag.selector_checksum64),
		     diag.total_entry_count,
		     diag.duplicate_entry_count,
		     static_cast<unsigned long long>(diag.active_checksum64),
		     diag.active_repl_w,
		     diag.active_repl_h,
		     diag.active_policy_key.empty() ? "<none>" : diag.active_policy_key.c_str(),
		     diag.active_replacement_id.empty() ? "<none>" : diag.active_replacement_id.c_str(),
		     diag.active_sampled_object_id.empty() ? "<none>" : diag.active_sampled_object_id.c_str(),
		     diag.active_source_path.empty() ? "<none>" : diag.active_source_path.c_str());
	}
}

void HiresReplacement::tile_set(unsigned tile, const TileMeta &meta)
{
	// A tile re-pointed to a different TMEM offset must not keep serving a
	// replacement keyed against its old window, nor let the draw-time
	// compat lane CRC the old window's RDRAM address under the new
	// descriptor. Only the offset is a re-point: the load path binds the
	// render tile before its SetTile arrives (offset-equality propagation
	// against the stale meta), so stride/fmt/palette deltas here are the
	// NEW texture's own descriptor and the binding must survive them.
	// Hi-res-only state.
	if (replacement_provider && tiles[tile].meta.offset != meta.offset &&
	    (replacement_tiles[tile].valid || hires_rdram_load_addr[tile] != 0))
	{
		replacement_tiles[tile] = {};
		clear_hires_tile_binding(tile);
		// The old load address describes the old window; adopt the address
		// of a load already resident at the new offset if one is tracked
		// (GlideN64's loadInfo is per-TMEM-address and survives re-points),
		// else zero so the compat lane cannot CRC the wrong RDRAM region.
		hires_rdram_load_addr[tile] = 0;
		hires_load_tile_source[tile] = {};
		for (unsigned other = 0; other < Limits::MaxNumTiles; other++)
		{
			if (other != tile && tiles[other].meta.offset == meta.offset &&
			    hires_rdram_load_addr[other] != 0)
			{
				hires_rdram_load_addr[tile] = hires_rdram_load_addr[other];
				hires_load_tile_source[tile] = hires_load_tile_source[other];
				rule_counters.repoint_origin_adoptions++;
				break;
			}
		}
	}
}

void HiresReplacement::tmem_load_begin(unsigned tile)
{
	for (unsigned i = 0; i < Limits::MaxNumTiles; i++)
	{
		if (should_alias_hires_load_binding(tiles[tile & (Limits::MaxNumTiles - 1)].meta, tiles[i].meta))
		{
			replacement_tiles[i] = {};
			clear_hires_tile_binding(i);
		}
	}
}

void HiresReplacement::tmem_load(unsigned tile, const LoadTileInfo &info, uint32_t tmem_offset,
                                 int32_t tmem_address, int32_t rdram_pitch)
{
	auto &meta = tiles[tile].meta;
	uint32_t key_width_pixels = 0;
	uint32_t key_height_pixels = 0;
	uint32_t key_start_x = 0;
	uint32_t key_start_y = 0;
	if (info.mode == UploadMode::Block)
	{
		key_width_pixels = (info.shi - info.slo + 1) & 0xfff;
		key_height_pixels = 1;
		key_start_x = info.slo;
		key_start_y = info.tlo;
	}
	else
	{
		key_width_pixels = (((info.shi >> 2) - (info.slo >> 2)) + 1) & 0xfff;
		key_height_pixels = ((info.thi >> 2) - (info.tlo >> 2)) + 1;
		key_start_x = info.slo >> 2;
		key_start_y = info.tlo >> 2;
	}

	const bool rdram_view_ok = detail::hires_rdram_view_valid(cpu_rdram, rdram_size);
	const bool is_tlut_mode = info.mode == UploadMode::TLUT;
	if (rdram_view_ok)
	{
		const uint32_t bpp_bytes = 1u << (unsigned(info.size) - 1);
		const uint32_t row_stride_bytes = rdram_pitch * bpp_bytes;
		const uint32_t src_base_addr = info.tex_addr + ((info.tex_width * key_start_y + key_start_x) << (unsigned(info.size) - 1));

		// Persist RDRAM load address for draw-time GlideN64-compat CRC computation.
		// Propagate to all tiles sharing the same TMEM offset, since the load tile
		// (e.g. tile 7) and render tile (e.g. tile 0) are typically different but
		// point to the same TMEM region.
		{
			const uint32_t load_tmem_offset = meta.offset;
			detail::HiresLoadTileSource source;
			if (hires_gliden64_compat_crc_enabled &&
			    detail::hires_load_tile_source_supported(info.mode == UploadMode::Tile, info.size, meta.size, tmem_offset))
			{
				source.address = src_base_addr;
				source.image_width = info.tex_width;
				source.row_stride = row_stride_bytes;
				source.tmem_stride = meta.stride;
				source.format = meta.fmt;
				source.size = meta.size;
				source.valid = true;
			}
			hires_rdram_load_addr[tile & (Limits::MaxNumTiles - 1)] = info.tex_addr;
			hires_load_tile_source[tile & (Limits::MaxNumTiles - 1)] = source;
			for (unsigned alias = 0; alias < Limits::MaxNumTiles; alias++)
			{
				if (alias == (tile & (Limits::MaxNumTiles - 1)))
					continue;
				if (tiles[alias].meta.offset == load_tmem_offset)
				{
					hires_rdram_load_addr[alias] = info.tex_addr;
					hires_load_tile_source[alias] = source;
				}
			}
		}

		// The pre-upload clear invalidates bindings only on exact TMEM-offset
		// equality; a load whose write span covers a tile bound at a different
		// offset leaves that binding describing overwritten texels (the other
		// half of the kkj_13 stale-flame hole). Clear by byte-span overlap,
		// conservatively over-approximating both spans with row-stride
		// extents (over-clearing only costs a re-key on the next load).
		// LoadTlut quadricates each 16-bit entry across a full 8-byte TMEM
		// word, so its write span is 4x the source bytes. Spans are clamped
		// to the 4KB TMEM window and a wrapping load is tested as its two
		// segments.
		if (replacement_provider)
		{
			const uint32_t tmem_bytes = 0x1000;
			const uint32_t load_row_bytes = detail::compute_hires_texture_row_bytes(key_width_pixels, info.size);
			const uint32_t load_begin = uint32_t(std::max<int32_t>(tmem_address, 0)) & (tmem_bytes - 1);
			uint32_t load_bytes = key_height_pixels > 0 ?
				(key_height_pixels - 1) * meta.stride + load_row_bytes : load_row_bytes;
			if (is_tlut_mode)
				load_bytes = std::max<uint32_t>(load_bytes, key_width_pixels * key_height_pixels * bpp_bytes * 4u);
			load_bytes = std::min<uint32_t>(std::max<uint32_t>(load_bytes, 8u), tmem_bytes);
			for (unsigned i = 0; i < Limits::MaxNumTiles; i++)
			{
				if (i == (tile & (Limits::MaxNumTiles - 1)))
					continue;
				if (!replacement_tiles[i].valid && !replacement_tiles[i].hit && hires_rdram_load_addr[i] == 0)
					continue;
				const auto &t_meta = tiles[i].meta;
				if (t_meta.offset == meta.offset)
					continue; // handled by the pre-upload equality clear
				uint32_t t_rows = 1;
				uint32_t t_w = 0, t_h = 0;
				if (compute_hires_gliden64_display_dims(t_meta, tiles[i].size, t_w, t_h) && t_h > 0)
					t_rows = t_h;
				const uint32_t t_begin = t_meta.offset & (tmem_bytes - 1);
				const uint32_t t_bytes = std::min<uint32_t>(
						std::max<uint32_t>(t_rows * std::max<uint32_t>(t_meta.stride, 8u), 8u), tmem_bytes);
				// Wrap-aware overlap: split each span at the 4KB boundary.

				if (detail::hires_tmem_spans_overlap(load_begin, load_bytes, t_begin, t_bytes, tmem_bytes))
				{
					if (replacement_tiles[i].hit)
						rule_counters.overlap_binding_drops++;
					else if (hires_rdram_load_addr[i] != 0)
						rule_counters.overlap_origin_drops++;
					replacement_tiles[i] = {};
					clear_hires_tile_binding(i);
					// The tile's tracked load no longer describes its TMEM
					// window either; leaving it would let the draw-time
					// compat lane re-CRC the untouched RDRAM at the old
					// address and re-mint the binding this clear just
					// dropped (and set_tile adoption would spread it).
					hires_rdram_load_addr[i] = 0;
					hires_load_tile_source[i] = {};
				}
			}
		}

		if (detail::should_update_tlut_shadow(rdram_view_ok, is_tlut_mode))
		{
			const uint32_t bytes = key_width_pixels * key_height_pixels * bpp_bytes;
			const uint32_t tlut_region_base = 256u << 3;
			uint32_t max_copy = 0;
			uint32_t tlut_shadow_offset = 0;

			if (tmem_address >= int32_t(tlut_region_base))
			{
				const uint32_t palette_entry_base = uint32_t(tmem_address - int32_t(tlut_region_base)) >> 3;
				tlut_shadow_offset = palette_entry_base << 1;
				if (!tlut_shadow_valid)
				{
					memset(tlut_shadow, 0, sizeof(tlut_shadow));
					memset(tlut_tmem_shadow, 0, sizeof(tlut_tmem_shadow));
				}
				if (tlut_shadow_offset < sizeof(tlut_shadow))
					max_copy = std::min<uint32_t>(bytes, uint32_t(sizeof(tlut_shadow)) - tlut_shadow_offset);

				const uint32_t tlut_entry_count = bytes >> 1;
				for (uint32_t entry = 0; entry < tlut_entry_count; entry++)
				{
					const uint32_t tmem_entry = palette_entry_base + entry;
					const uint32_t tmem_byte_offset = tmem_entry * 8u;
					if (tmem_byte_offset + 8u > sizeof(tlut_tmem_shadow))
						break;

					const uint16_t word = emulate_load_tlut_entry_word(cpu_rdram, rdram_size, src_base_addr + entry * 2u);
					tlut_tmem_shadow[tmem_byte_offset + 0] = uint8_t(word & 0xff);
					tlut_tmem_shadow[tmem_byte_offset + 1] = uint8_t(word >> 8);
					memset(tlut_tmem_shadow + tmem_byte_offset + 2, 0, 6);
				}
			}

			// GlideN64 fills its keying palette (gDP.TexFilterPalette) from the texture
			// image base address, ignoring the load tile's uls/ult offset; the pack keys
			// were generated against that convention, so the keying shadow must match it.
			// The TMEM mirror above keeps the offset address (real LoadTlut semantics).
			for (uint32_t i = 0; i < max_copy; i++)
				tlut_shadow[tlut_shadow_offset + i] = wrapped_read_u8(cpu_rdram, rdram_size, info.tex_addr + i);
			tlut_shadow_valid = tlut_shadow_valid || (max_copy > 0);
			if (replacement_provider && max_copy > 0 && src_base_addr != info.tex_addr)
				rule_counters.tlut_image_base_loads++;

		}
		else if (detail::should_run_hires_lookup(
		             rdram_view_ok,
		             replacement_provider != nullptr,
		             is_tlut_mode,
		             key_width_pixels,
		             key_height_pixels))
		{
				uint32_t texture_crc = rice_crc32_wrapped(cpu_rdram, rdram_size, src_base_addr,
				                                          key_width_pixels, key_height_pixels,
				                                          uint32_t(info.size), row_stride_bytes);
				uint32_t palette_crc = 0;

				if (meta.fmt == TextureFormat::CI && tlut_shadow_valid)
				{
					palette_crc = detail::compute_hires_ci_palette_crc(
							meta.size,
							meta.palette,
						cpu_rdram,
						rdram_size,
						src_base_addr,
						key_width_pixels,
						key_height_pixels,
							row_stride_bytes,
							tlut_shadow,
							sizeof(tlut_shadow),
							tlut_shadow_valid);
				}

			const uint64_t checksum64 = detail::compose_hires_checksum64(texture_crc, palette_crc);
			uint64_t resolved_checksum64 = checksum64;
			uint64_t resolved_selector_checksum64 = 0;
			const uint16_t formatsize = formatsize_key(meta.fmt, meta.size);
			ReplacementMeta repl_meta = {};
			bool ci4_reinterpreted = false;
			ReplacementResolution resolution = {};
			bool hit = replacement_provider->resolve_upload_candidate(
				formatsize,
				uint32_t(meta.fmt),
				uint32_t(meta.size),
				meta.offset,
				row_stride_bytes,
				key_width_pixels,
				key_height_pixels,
				texture_crc,
				palette_crc,
				0,
				&resolution);
			if (hit)
			{
				repl_meta = resolution.meta;
				repl_meta.orig_w = key_width_pixels;
				repl_meta.orig_h = key_height_pixels;
				resolved_checksum64 = resolution.resolved_checksum64;
				resolved_selector_checksum64 = resolution.resolved_selector_checksum64;
			}
			// GlideN64 keys a CI texture in rendering-tile terms even when the
			// LoadTile descriptor is wider (Textures.cpp _loadHiresTexture:
			// width <<= info.size - _pTexture->size, palette bank and cimax from
			// the rendering tile's size). PM64 loads CI4 sprite sheets through
			// CI8-sized load tiles, so the as-loaded key hashes the palette with
			// CI8 extent over a 16-entry TLUT and can never match the pack.
			// Re-key the same load extent as CI4 texels with the 16-entry bank
			// palette and probe again; the texel CRC is byte-extent invariant.
			uint64_t lookup_checksum64 = checksum64;
			uint16_t lookup_formatsize = formatsize;
			uint32_t lookup_key_w = key_width_pixels;
			uint32_t lookup_key_h = key_height_pixels;
			if (!hit && meta.fmt == TextureFormat::CI && meta.size == TextureSize::Bpp8 && tlut_shadow_valid)
			{
				const uint32_t ci4_width = key_width_pixels * 2;
				const uint32_t ci4_entries = detail::compute_hires_ci_palette_entry_count(
						TextureSize::Bpp4,
						cpu_rdram,
						rdram_size,
						src_base_addr,
						ci4_width,
						key_height_pixels,
						row_stride_bytes);
				const uint32_t ci4_palette_crc = detail::compute_hires_ci_palette_crc_for_entries(
						TextureSize::Bpp4,
						detail::hires_ci4_lookup_palette(meta.size, meta.palette),
						tlut_shadow,
						sizeof(tlut_shadow),
						tlut_shadow_valid,
						ci4_entries);
				const uint16_t ci4_formatsize = formatsize_key(TextureFormat::CI, TextureSize::Bpp4);
				ReplacementResolution ci4_resolution = {};
				if (ci4_palette_crc != 0 &&
				    replacement_provider->resolve_upload_candidate(
						ci4_formatsize,
						uint32_t(TextureFormat::CI),
						uint32_t(TextureSize::Bpp4),
						meta.offset,
						row_stride_bytes,
						ci4_width,
						key_height_pixels,
						texture_crc,
						ci4_palette_crc,
						0,
						&ci4_resolution))
				{
					ReplacementMeta ci4_meta = ci4_resolution.meta;
					ci4_meta.orig_w = ci4_width;
					ci4_meta.orig_h = key_height_pixels;
					hit = true;
					resolution = ci4_resolution;
					repl_meta = ci4_meta;
					resolved_checksum64 = ci4_resolution.resolved_checksum64;
					resolved_selector_checksum64 = ci4_resolution.resolved_selector_checksum64;
					ci4_reinterpreted = true;
					lookup_checksum64 = detail::compose_hires_checksum64(texture_crc, ci4_palette_crc);
					lookup_formatsize = ci4_formatsize;
					lookup_key_w = ci4_width;
					lookup_key_h = key_height_pixels;
				}
			}
			// Residency cannot influence which candidate or selector wins.
			if (hit)
			{
				hit = resolve_hires_provider_resolution_descriptor(
					resolution,
					lookup_formatsize,
					lookup_key_w,
					lookup_key_h,
					repl_meta,
					&resolved_selector_checksum64);
				if (hit && ci4_reinterpreted)
					rule_counters.ci8_as_ci4_hits++;
				// Summary counters; a CI8-as-CI4 hit is not attributed to a sampled kind.
				if (hit && !ci4_reinterpreted)
				{
					if (resolution.kind == ReplacementResolutionKind::SampledFamilySingleton)
					{
						if (resolution.ordered_surface_singleton)
							hires_descriptor_sampled_ordered_surface_singleton_resolutions++;
						else
							hires_descriptor_sampled_family_singleton_resolutions++;
					}
					else if (resolution.kind == ReplacementResolutionKind::SampledExactSelector)
						hires_descriptor_sampled_exact_selector_resolutions++;
				}
			}
			auto &repl_state = replacement_tiles[tile & (Limits::MaxNumTiles - 1)];
			detail::write_hires_lookup_tile_state(
					repl_state,
					hit,
					resolved_checksum64,
					lookup_checksum64,
					resolved_selector_checksum64,
					lookup_formatsize,
					lookup_key_w,
					lookup_key_h);
			repl_state.jabo_source_hash = 0;
			repl_state.repl_w = static_cast<uint16_t>(repl_meta.repl_w);
			repl_state.repl_h = static_cast<uint16_t>(repl_meta.repl_h);
			repl_state.vk_image_index = repl_meta.vk_image_index;
			repl_state.keyed_rdram_addr = info.tex_addr;
			repl_state.source_layout_binding = false;
			repl_state.ci4_palette = detail::make_hires_ci4_palette_binding(
				lookup_formatsize, detail::hires_ci4_lookup_palette(meta.size, meta.palette));
			apply_hires_tile_binding(tile & (Limits::MaxNumTiles - 1), repl_state);
			for (unsigned alias_tile = 0; alias_tile < Limits::MaxNumTiles; alias_tile++)
			{
				if (alias_tile == (tile & (Limits::MaxNumTiles - 1)))
					continue;
				if (!should_apply_hires_propagated_binding(
						    tiles[tile & (Limits::MaxNumTiles - 1)].meta,
						    tiles[alias_tile].meta))
					continue;
				replacement_tiles[alias_tile] = repl_state;
				apply_hires_tile_binding(alias_tile, replacement_tiles[alias_tile]);
			}

			hires_lookup_total++;
			if (hit)
				hires_lookup_hits++;
			else
				hires_lookup_misses++;

			}
		}
}

HiresDrawBinding HiresReplacement::prepare_draw(uint32_t tile, StaticRasterizationFlags raster_flags,
                                                const DepthBlendState &depth_blend)
{
	const bool uses_texel0 = (raster_flags & RASTERIZATION_USES_TEXEL0_BIT) != 0;
	const bool uses_texel1 = (raster_flags & RASTERIZATION_USES_TEXEL1_BIT) != 0 ||
	                         (raster_flags & RASTERIZATION_USES_PIPELINED_TEXEL1_BIT) != 0;
	const unsigned base_tile = tile & 7u;
	const unsigned texel1_tile = (base_tile + 1u) & 7u;
	// Equal-offset load propagation preserves a pending render descriptor.
	// Once sampled, however, each CI4 tile must use the palette bank that
	// actually produced its replacement lookup, including the TEXEL1 peer.
	const unsigned sampled_tiles[2] = { base_tile, texel1_tile };
	const bool sampled_slots[2] = {
		uses_texel0 || (raster_flags & RASTERIZATION_COPY_BIT) != 0,
		uses_texel1 || (raster_flags & RASTERIZATION_TEX_LOD_ENABLE_BIT) != 0
	};
	if (replacement_provider)
	{
		for (unsigned slot = 0; slot < 2; slot++)
		{
			const unsigned t = sampled_tiles[slot];
			const auto &meta = tiles[t].meta;
			if (sampled_slots[slot] && hires_rdram_load_addr[t] == 0)
			{
				// Only final, matching draw views can recover a missing origin.
				// Overlapped writes retire address-only peers in the load path.
				detail::HiresLoadOrigin views[Limits::MaxNumTiles] = {};
				for (unsigned peer = 0; peer < Limits::MaxNumTiles; peer++)
				{
					const auto &peer_meta = tiles[peer].meta;
					auto &view = views[peer];
					view.format = peer_meta.fmt;
					view.size = peer_meta.size;
					view.offset = peer_meta.offset;
					view.stride = peer_meta.stride;
					compute_hires_gliden64_display_dims(peer_meta, tiles[peer].size, view.width, view.height);
					view.address = hires_rdram_load_addr[peer];
					view.source = hires_load_tile_source[peer];
				}
				const auto origin = detail::find_hires_load_origin_info(views[t], views, Limits::MaxNumTiles);
				hires_rdram_load_addr[t] = origin.address;
				hires_load_tile_source[t] = origin.source;
				if (origin.address != 0)
					rule_counters.load_origin_recoveries++;
			}
			const auto &state = replacement_tiles[t];
			if (sampled_slots[slot] && state.indexed_palette_hash != 0)
			{
				const auto palette = ReplacementPalette::from_tmem(
					tlut_shadow_valid ? tlut_tmem_shadow : nullptr, sizeof(tlut_tmem_shadow),
					meta.size == TextureSize::Bpp4, meta.palette,
					(raster_flags & RASTERIZATION_TLUT_TYPE_BIT) != 0);
				if (!(raster_flags & RASTERIZATION_TLUT_BIT) || state.indexed_palette_hash != palette.hash())
				{
					replacement_tiles[t] = {};
					clear_hires_tile_binding(t);
				}
			}
			if (state.hit && !detail::hires_ci4_binding_matches(
				state.ci4_palette, meta.fmt, meta.size, meta.palette, sampled_slots[slot]))
			{
				replacement_tiles[t] = {};
				clear_hires_tile_binding(t);
			}
		}
	}
	// GlideN64-compat RDRAM CRC fallback: if the upload path missed, try computing CRC
	// at draw time using the masked rendering view. LoadTile retains its source
	// crop/stride; LoadBlock keeps the existing TMEM-line/DXT convention.
	bool compat_gate_skipped_miss = false;
	for (unsigned slot = 0; slot < 2; slot++)
	{
		if (!sampled_slots[slot])
			continue;
		const unsigned resolve_tile = sampled_tiles[slot];
		if (!hires_gliden64_compat_crc_enabled && replacement_provider &&
		    replacement_tiles[resolve_tile].valid && !replacement_tiles[resolve_tile].hit)
			compat_gate_skipped_miss = true;
		const auto &resolve_meta = tiles[resolve_tile].meta;
		const auto &resolve_size = tiles[resolve_tile].size;
		detail::HiresCompatView compat_view;
		const bool compat_view_valid = hires_gliden64_compat_crc_enabled &&
		    replacement_provider && cpu_rdram && rdram_size > 0 &&
		    hires_rdram_load_addr[resolve_tile] != 0 &&
		    detail::compute_hires_compat_view(resolve_meta, resolve_size,
		        hires_rdram_load_addr[resolve_tile], hires_load_tile_source[resolve_tile], compat_view);
		auto &binding = replacement_tiles[resolve_tile];
		if (binding.hit && detail::hires_source_binding_must_retire(
		        binding.source_layout_binding, compat_view_valid, compat_view))
		{
			binding = {};
			clear_hires_tile_binding(resolve_tile);
			rule_counters.source_layout_retires++;
		}
		if (compat_view_valid &&
		    (!replacement_tiles[resolve_tile].hit ||
		     detail::hires_source_view_needs_rekey(compat_view,
		         replacement_tiles[resolve_tile].key_w, replacement_tiles[resolve_tile].key_h)))
		{
			const uint64_t compat_checksum64 = compute_gliden64_compat_checksum64(
				cpu_rdram, rdram_size,
				hires_rdram_load_addr[resolve_tile],
				resolve_meta, resolve_size, hires_load_tile_source[resolve_tile],
				tlut_shadow, tlut_shadow_valid,
				nullptr);

			// An upload hit for the unmasked LoadTile region must not hide
			// the exact source-backed draw key. LoadBlock subview rebasing
			// retains its established behavior.
			auto &prior_state = replacement_tiles[resolve_tile];
			const bool tlut_ia16 = (raster_flags & RASTERIZATION_TLUT_TYPE_BIT) != 0;
			if (prior_state.hit && prior_state.checksum64 == compat_checksum64 &&
			    prior_state.formatsize == formatsize_key(resolve_meta.fmt, resolve_meta.size) &&
			    (prior_state.jabo_source_hash == 0 ||
			     prior_state.jabo_source_hash == hires_jabo_binding_hash(resolve_meta, compat_view, tlut_ia16)))
			{
				rule_counters.compat_rekey_confirmed++;
				continue;
			}
			if (prior_state.hit)
			{
				rule_counters.compat_lookups_rekey++;
				prior_state = {};
				clear_hires_tile_binding(resolve_tile);
			}
			else if (prior_state.valid)
				rule_counters.compat_lookups_after_miss++;
			else
				rule_counters.compat_lookups_unkeyed++;

			const bool is_ci_compat_texture = (compat_checksum64 >> 32) != 0;
			if (is_ci_compat_texture)
				hires_compat_draw_time_ci_attempts++;

			{
				const uint16_t compat_formatsize = formatsize_key(resolve_meta.fmt, resolve_meta.size);
				const uint32_t compat_low32 = uint32_t(compat_checksum64 & 0xffffffffu);
				const uint32_t compat_palette_crc = uint32_t(compat_checksum64 >> 32u);
				ReplacementResolution compat_resolution = {};
				bool compat_hit = compat_checksum64 != 0 && replacement_provider->resolve_upload_candidate(
					compat_formatsize,
					uint32_t(resolve_meta.fmt), uint32_t(resolve_meta.size),
					resolve_meta.offset, resolve_meta.stride,
					0, 0, compat_low32, compat_palette_crc, 0, &compat_resolution);
				// GLideN64 tries StrongCRC32 only after the Rice source-view miss.
				uint64_t lookup_checksum64 = compat_checksum64;
				bool strong_hit = false;
				if (!compat_hit)
				{
					size_t texture_bytes = 0;
					if (detail::pack_hires_strong_source(cpu_rdram, rdram_size, compat_view,
					        resolve_meta.fmt, resolve_meta.size, tlut_shadow_valid ? tlut_shadow : nullptr,
					        resolve_meta.palette, hires_strong_scratch, texture_bytes))
					{
						const uint64_t strong = hires_strong_memo[resolve_tile].resolve(
							compat_checksum64, hires_strong_scratch, texture_bytes);
						strong_hit = replacement_provider->resolve_upload_candidate(
							compat_formatsize, uint32_t(resolve_meta.fmt), uint32_t(resolve_meta.size),
							resolve_meta.offset, resolve_meta.stride, 0, 0,
							uint32_t(strong), uint32_t(strong >> 32), 0, &compat_resolution);
						if (strong_hit)
							lookup_checksum64 = strong;
					}
					compat_hit = strong_hit;
				}
				// Jabo packages key by MD5 instead; try them only after the Rice key misses.
				uint64_t jabo_source_hash = 0;
				const bool jabo_hit = !compat_hit && hires_jabo_lane_enabled &&
				    resolve_hires_jabo_draw_candidate(resolve_meta, compat_view, compat_checksum64,
				        tlut_ia16, &compat_resolution, &jabo_source_hash);
				compat_hit = compat_hit || jabo_hit;

				if (compat_hit)
				{
					const uint32_t compat_w = compat_view.width, compat_h = compat_view.height;
					ReplacementMeta compat_repl_meta = compat_resolution.meta;
					compat_repl_meta.orig_w = compat_w;
					compat_repl_meta.orig_h = compat_h;

					const auto palette = ReplacementPalette::from_tmem(
						tlut_shadow_valid && (raster_flags & RASTERIZATION_TLUT_BIT) ? tlut_tmem_shadow : nullptr,
						sizeof(tlut_tmem_shadow), resolve_meta.size == TextureSize::Bpp4,
						resolve_meta.palette, tlut_ia16);
					compat_hit = resolve_hires_provider_resolution_descriptor(
						compat_resolution, compat_formatsize, compat_w, compat_h,
						compat_repl_meta, nullptr, palette.count ? &palette : nullptr);

					auto &repl_state = replacement_tiles[resolve_tile];
					detail::write_hires_lookup_tile_state(
						repl_state, compat_hit, lookup_checksum64, compat_checksum64,
						0, compat_formatsize, compat_w, compat_h);
					repl_state.repl_w = static_cast<uint16_t>(compat_repl_meta.repl_w);
					repl_state.repl_h = static_cast<uint16_t>(compat_repl_meta.repl_h);
					repl_state.vk_image_index = compat_repl_meta.vk_image_index;
					repl_state.keyed_rdram_addr = hires_rdram_load_addr[resolve_tile];
					repl_state.ci4_palette = detail::make_hires_ci4_palette_binding(compat_formatsize, resolve_meta.palette);
					repl_state.source_layout_binding = compat_view.source_layout;
					repl_state.jabo_source_hash = jabo_hit ? jabo_source_hash : 0;
					repl_state.indexed_palette_hash = compat_resolution.meta.indexed ? palette.hash() : 0;
					apply_hires_tile_binding(resolve_tile, repl_state);
					if (compat_hit)
					{
						if (strong_hit)
							hires_strong_draw_hits++;
						if (jabo_hit)
							hires_jabo_draw_hits++;
						else
						{
							hires_compat_draw_time_hits++;
							if (is_ci_compat_texture)
								hires_compat_draw_time_ci_hits++;
						}

					}
				}
			}
		}
	}
	if (compat_gate_skipped_miss)
		rule_counters.compat_gate_skipped_misses++;

	// Replacements are authored against GlideN64's texture-cache view: the
	// rendering tile's SetTileSize extent clamped by the coordinate masks.
	// The upload lane binds before the rendering tile is known and stores
	// the LOAD key window instead, which diverges for reshaped or mirrored
	// loads (e.g. an 8x16 LoadTile window rendered as a 16-wide mask-wrapped
	// tile) and skews the GPU ratio map. Rebase orig dims here, where the
	// rendering tile is known, for every bound texel tile.
	if (replacement_provider)
	{
		// texel1_tile participates only when the draw can actually sample it
		// (combiner TEXEL1 use, or tile-select LOD promoting base+1 into
		// TEXEL0). An unsampled neighbor tile is often mid-transition — its
		// binding was just propagated by a load while its render SetTile has
		// not arrived — and running the destructive guards over it would
		// wipe legitimate state the set_tile hygiene deliberately preserves.
		const bool texel1_live = uses_texel1 ||
		                         (raster_flags & RASTERIZATION_TEX_LOD_ENABLE_BIT) != 0;
		const unsigned rebase_tiles[2] = { base_tile, texel1_tile };
		for (unsigned t : rebase_tiles)
		{
			if (t == texel1_tile && t != base_tile && !texel1_live)
				continue;
			auto &repl_state = replacement_tiles[t];
			if (!repl_state.hit)
				continue;
			// Format-consistency guard: GlideN64's rice key folds the DRAW
			// tile's fmt/siz, so a binding keyed at a different formatsize
			// than the tile now sampling it can never be served by the
			// reference pipeline (its per-draw key would differ). Serving
			// across the reinterpretation is how a CI4-keyed sprite binding
			// leaked replacement pixels into PM64's RGBA16 sprite-shading
			// palette write. Drop it; the next load re-keys at the tile's
			// own identity.
			if (repl_state.formatsize != formatsize_key(tiles[t].meta.fmt, tiles[t].meta.size))
			{
				rule_counters.formatsize_mismatch_drops++;
				replacement_tiles[t] = {};
				clear_hires_tile_binding(t);
				continue;
			}
			// Stale-binding guard: the key must describe the load currently
			// resident in this tile's TMEM region; a differing last-load
			// address means the texels were overwritten since keying.
			// Expected-unreachable defense-in-depth: the load-path clears
			// and the set_tile re-point invalidation should keep the
			// keyed/load addresses in lockstep, so this firing indicates an
			// invalidation hole (stale_binding_drops is the diagnostic).
			if (repl_state.keyed_rdram_addr != 0 && hires_rdram_load_addr[t] != 0 &&
			    repl_state.keyed_rdram_addr != hires_rdram_load_addr[t])
			{
				rule_counters.stale_binding_drops++;
				replacement_tiles[t] = {};
				clear_hires_tile_binding(t);
				continue;
			}
			detail::HiresCompatView display_view;
			if (!detail::compute_hires_compat_view(tiles[t].meta, tiles[t].size,
			        hires_rdram_load_addr[t], hires_load_tile_source[t], display_view))
				continue;
			const uint32_t display_w = display_view.width, display_h = display_view.height;
			if (repl_state.orig_w != display_w || repl_state.orig_h != display_h)
			{
				repl_state.orig_w = uint16_t(display_w);
				repl_state.orig_h = uint16_t(display_h);
				apply_hires_tile_binding(t, repl_state);
			}

			// GlideN64 keys its draw-time texture lookup over the rendering
			// tile's view, so a view that reads past this binding's keyed
			// window can never be served by the reference pipeline -- the
			// CRC over the longer window is a different key. It also samples
			// RDRAM the replacement was never authored against (PM64's
			// effect-compositing buffers: 32-row keyed windows rendered as
			// 56-row sprite-stencil views whose extra rows are other live
			// buffers; serving the static repaint there replaces the dynamic
			// stencil alpha and materializes the flash quad as a box). Drop
			// the binding for this draw; the next in-window view rebases and
			// rebinds. Draw-time compat hits key over the view itself, so
			// they always pass.
			if (!detail::hires_compat_view_fits_key(display_view, repl_state.key_w, repl_state.key_h,
			        TextureSize(repl_state.formatsize >> 8), tiles[t].meta.stride))
			{
				rule_counters.view_exceeds_key_draws++;
				clear_hires_tile_binding(t);
			}
			else if (detail::hires_descriptor_index_valid(tiles[t].replacement.repl_desc_index) &&
			         detail::hires_compat_binding_needs_source_layout(display_view,
			             repl_state.key_w, repl_state.key_h,
			             TextureSize(repl_state.formatsize >> 8), tiles[t].meta.stride))
			{
				// A raw upload admitted only through source-row padding proof
				// must retire if a later draw no longer has that qualified layout.
				repl_state.source_layout_binding = true;
			}
		}
	}

	const auto &texel0_state = replacement_tiles[base_tile];
	const auto &texel1_state = replacement_tiles[texel1_tile];
	// Effective GPU-visible bindings: a hit whose view exceeded the keyed
	// window above serves native this draw and must not carry replaced-draw
	// semantics (snap exemption, HLE alpha kill).
	const bool texel0_bound = texel0_state.hit &&
	                          detail::hires_descriptor_index_valid(tiles[base_tile].replacement.repl_desc_index);
	const bool texel1_bound = texel1_state.hit &&
	                          detail::hires_descriptor_index_valid(tiles[texel1_tile].replacement.repl_desc_index);

	HiresDrawBinding binding;
	binding.replacements_attached = replacement_provider != nullptr;
	binding.samples_replacement = (texel0_bound && sampled_slots[0]) || (texel1_bound && sampled_slots[1]);
	binding.rule_counters = &rule_counters;
	binding.lifts_tex_rect_snap = texel0_bound && (uses_texel0 || (raster_flags & RASTERIZATION_COPY_BIT) != 0);

	// HLE-alpha-kill eligibility: a draw that samples a replaced texture and
	// whose final blend cycle is one of the src-alpha-over-memory closures
	// (P*A_in + M*A_mem or P*A_in + M*(1-A_in)) composes to nothing in the
	// HLE renderers packs are authored on whenever combined alpha is ~0 --
	// those renderers blend with GL src-alpha unconditionally. The faithful
	// blender writes interior pixels unblended when force_blend is off,
	// materializing quads the pack expects to vanish (effect redraws
	// choreographed by shade alpha). Flag the draw so the shading stage can
	// kill ~0-alpha pixels. Triangle lane only; fill never samples and copy
	// keeps its raw alpha-bit transport semantics.
	if ((raster_flags & (RASTERIZATION_FILL_BIT | RASTERIZATION_COPY_BIT)) == 0 &&
	    ((texel0_bound && uses_texel0) ||
	     (texel1_bound && (raster_flags & RASTERIZATION_USES_TEXEL1_BIT) != 0)))
	{
		const auto &final_blend = depth_blend.blend_cycles[
				(raster_flags & RASTERIZATION_MULTI_CYCLE_BIT) != 0 ? 1 : 0];
		if (final_blend.blend_1b == BlendMode1B::PixelAlpha &&
		    final_blend.blend_2a == BlendMode2A::MemoryColor &&
		    (final_blend.blend_2b == BlendMode2B::MemoryAlpha ||
		     final_blend.blend_2b == BlendMode2B::InvPixelAlpha))
		{
			binding.alpha_kill = true;
			rule_counters.alpha_kill_draws++;
		}
	}

	// The draw's bindings are final: mark them used before it is queued.
	touch_bound_tiles();
	return binding;
}

// The mark lives in TriangleSetup::tile (see rdp_data_structures.hpp) and is
// only read by the replacement sampling path, so it is set only while a
// provider is attached; without one, queued setups stay upstream's.
void HiresDrawBinding::mark_tex_rect(TriangleSetup &queued_setup) const
{
	if (replacements_attached)
		queued_setup.tile |= TRIANGLE_SETUP_TILE_HIRES_TEX_RECT_BIT;
}

// See detail::lift_hires_tex_rect_snap for the rule and why it leaves the
// native pass alone. The bits are only consumed GPU-side, so patching the
// queued setup here, after the draw-time CRC fallback has resolved, stays
// coherent.
void HiresDrawBinding::lift_tex_rect_snap(TriangleSetup &queued_setup, unsigned upscaling) const
{
	if (!lifts_tex_rect_snap)
		return;

	const auto lift = detail::lift_hires_tex_rect_snap(queued_setup, upscaling);
	if (!rule_counters)
		return;
	if (lift.was_snapped && upscaling > 1)
		rule_counters->texrect_snap_lifts++;
	// Without a snap only copy-mode rects round up; counted apart.
	if (lift.rounds_up_bottom)
		(lift.was_snapped ? rule_counters->texrect_bottom_round_ups : rule_counters->texrect_bottom_round_ups_unsnapped)++;
}

StaticRasterizationFlags HiresDrawBinding::raster_flags(const StaticRasterizationState &normalized,
                                                        const ConstantCombinerInputs (&constants)[2]) const
{
	StaticRasterizationFlags flags = 0;
	if (detail::hires_cutout_preserves_surface(normalized, constants, replacements_attached))
	{
		flags |= RASTERIZATION_HIRES_CUTOUT_SURFACE_BIT;
		// The bit only matters where a replaced texel can come back empty.
		if (rule_counters && samples_replacement)
			rule_counters->cutout_surface_draws++;
	}
	if (alpha_kill)
		flags |= RASTERIZATION_HIRES_ALPHA_KILL_BIT;
	return flags;
}

void HiresReplacement::touch_bound_tiles()
{
	for (unsigned i = 0; i < 8; i++)
	{
		if (replacement_provider && detail::hires_descriptor_index_valid(tiles[i].replacement.repl_desc_index))
			hires_resources.residency.touch(tiles[i].replacement.repl_desc_index, hires_frame);
	}
}

bool HiresReplacement::init_hires_resources(unsigned requested_capacity)
{
	if (!device || requested_capacity == 0 || !device->get_device_features().supports_descriptor_indexing)
		return false;

	if (hires_resources.bindless_pool && hires_resources.capacity >= requested_capacity)
		return true;

	auto pool = device->create_bindless_descriptor_pool(Vulkan::BindlessResourceType::ImageFP, 1, requested_capacity);
	if (!pool || !pool->allocate_descriptors(requested_capacity))
		return false;

	const uint8_t white_pixel[] = { 255, 255, 255, 255 };
	Vulkan::ImageInitialData initial = {};
	initial.data = white_pixel;
	initial.row_length = 1;
	initial.image_height = 1;

	auto info = Vulkan::ImageCreateInfo::immutable_2d_image(1, 1, VK_FORMAT_R8G8B8A8_UNORM, false);
	info.misc |= Vulkan::IMAGE_MISC_CONCURRENT_QUEUE_GRAPHICS_BIT |
	             Vulkan::IMAGE_MISC_CONCURRENT_QUEUE_ASYNC_COMPUTE_BIT;
	auto fallback = device->create_image(info, &initial);
	if (!fallback)
		return false;

	pool->set_texture(0, fallback->get_view());
	// Replace the live resources only after all allocations succeed.
	clear_hires_images();
	hires_resources = {};
	hires_resources.capacity = requested_capacity;
	hires_resources.residency.reset(requested_capacity, hires_residency_status.budget);
	hires_resources.images.resize(requested_capacity);
	hires_resources.fallback_image = std::move(fallback);
	hires_resources.bindless_pool = std::move(pool);
	return true;
}

void HiresReplacement::clear_hires_images()
{
	if (hires_resources.resident_images.empty())
		return;
	// Provider and pool replacement can precede normal retirement.
	// Finish GPU readers before rewriting their UPDATE_AFTER_BIND descriptors.
	flush_and_signal();
	device->wait_idle();
	for (const auto &entry : hires_resources.resident_images)
		detail::clear_hires_resident_image(entry.second, hires_resources);
	hires_resources.resident_images.clear();
}

void HiresReplacement::evict_hires_slot(uint32_t slot)
{
	// Eight unused frames cover the admitted frame-context count plus one.
	// Granite has waited those contexts' fences; no submitted or buffered draw
	// can still read this UPDATE_AFTER_BIND slot. Rewrite only at retirement,
	// not when selecting an eviction victim: earlier writes could change an
	// in-flight draw. The fallback avoids descriptors naming destroyed views.
	auto &resident = hires_resources.images[slot];
	hires_resources.resident_images.erase(resident.key);
	detail::clear_hires_resident_image(slot, hires_resources);
	for (unsigned i = 0; i < Limits::MaxNumTiles; i++)
		detail::clear_hires_evicted_binding(slot, replacement_tiles[i], tiles[i].replacement);
	hires_residency_status.evictions++;
}

// Metadata is already selected by the resolver; pressure must not re-key it.
template <typename Decode>
bool HiresReplacement::load_hires_descriptor(const HiresKey &key, ReplacementMeta &meta, Decode decode)
{
	auto itr = hires_resources.resident_images.find(key);
	if (itr != hires_resources.resident_images.end())
	{
		const auto &resident = hires_resources.images[itr->second];
		hires_resources.residency.touch(itr->second, hires_frame);
		meta.vk_image_index = itr->second;
		meta.repl_w = resident.repl_w;
		meta.repl_h = resident.repl_h;
		return true;
	}
	if (!hires_resources.bindless_pool || hires_resources.failed.count(key))
		return false;
	const auto max_dimension = device->get_gpu_properties().limits.maxImageDimension2D;
	if (!meta.repl_w || !meta.repl_h || meta.repl_w > max_dimension || meta.repl_h > max_dimension)
	{
		hires_residency_status.refused_too_large++;
		hires_resources.failed.insert(key);
		return false;
	}
	const uint64_t bytes = hires_replacement_mip_chain_bytes(meta.repl_w, meta.repl_h);
	const uint64_t start = Util::get_current_time_nsecs();
	auto result = hires_resources.residency.admit_and_load(bytes, hires_frame,
		[this](uint32_t slot) { evict_hires_slot(slot); },
		[&](uint32_t slot) {
			ReplacementImage replacement = {};
			if (!decode(replacement) || replacement.rgba8.empty() ||
			    replacement.meta.repl_w != meta.repl_w || replacement.meta.repl_h != meta.repl_h)
			{
				hires_residency_status.decode_failures++;
				return false;
			}
			if (zero_transparent_replacement_rgb(replacement.rgba8))
				rule_counters.transparent_rgb_zeroed_textures++;
			auto image = create_hires_replacement_image_with_mips(replacement);
			if (!image)
			{
				hires_residency_status.upload_failures++;
				return false;
			}
			hires_resources.bindless_pool->set_texture(slot, image->get_view());
			auto &resident = hires_resources.images[slot];
			resident.image = std::move(image);
			resident.key = key;
			resident.repl_w = meta.repl_w;
			resident.repl_h = meta.repl_h;
			hires_resources.resident_images.emplace(key, slot);
			return true;
		});
	auto &status = hires_residency_status;
	const double elapsed = double(Util::get_current_time_nsecs() - start) * 1e-6;
	status.load_ms_total += elapsed;
	status.load_ms_frame += elapsed;
	status.load_ms_max_frame = std::max(status.load_ms_max_frame, status.load_ms_frame);
	switch (result.reason)
	{
	case detail::HiresAdmission::Admitted:
		status.uploads++;
		status.upload_bytes += bytes;
		status.high_water_bytes = std::max(status.high_water_bytes, hires_resources.residency.resident_bytes);
		meta.vk_image_index = result.slot;
		return true;
	case detail::HiresAdmission::TooLarge:
		status.refused_too_large++;
		hires_resources.failed.insert(key);
		break;
	case detail::HiresAdmission::Saturated:
		status.refused_saturated += hires_resources.refused_saturated.insert(key).second;
		break;
	case detail::HiresAdmission::NoSlot:
		status.refused_no_slot += hires_resources.refused_no_slot.insert(key).second;
		break;
	case detail::HiresAdmission::LoadFailed:
		hires_resources.failed.insert(key);
		break;
	}
	return false;
}

bool HiresReplacement::resolve_hires_compat_replacement_descriptor(uint64_t checksum64, uint16_t formatsize, ReplacementMeta &meta)
{
	return resolve_hires_compat_replacement_descriptor(checksum64, formatsize, 0, meta);
}

bool HiresReplacement::resolve_hires_sampled_replacement_descriptor(uint32_t sampled_fmt,
                                                            uint32_t sampled_siz,
                                                            uint32_t sampled_tex_offset,
                                                            uint32_t sampled_stride,
                                                            uint32_t sampled_width,
                                                            uint32_t sampled_height,
                                                            uint32_t sampled_low32,
                                                            uint32_t palette_crc,
                                                            uint16_t formatsize,
                                                            uint64_t resolved_checksum64,
                                                            uint64_t selector_checksum64,
                                                            ReplacementMeta &meta)
{
	meta.vk_image_index = detail::hires_invalid_descriptor_index();

	if (!replacement_provider || !shader_enabled)
		return false;

	HiresKey key = {};
	key.checksum64 = 0; // Redundant for NativeSampled — sampled_low32 + sampled_palette_crc determine identity.
	key.formatsize = formatsize;
	key.selector_checksum64 = selector_checksum64;
	key.source = HiresKeySource::NativeSampled;
	key.sampled_fmt = sampled_fmt;
	key.sampled_siz = sampled_siz;
	key.sampled_tex_offset = sampled_tex_offset;
	key.sampled_stride = sampled_stride;
	key.sampled_width = sampled_width;
	key.sampled_height = sampled_height;
	key.sampled_low32 = sampled_low32;
	key.sampled_palette_crc = palette_crc;
	if (!load_hires_descriptor(key, meta, [&](ReplacementImage &replacement) {
		return replacement_provider->decode_sampled_rgba8_with_selector(
			sampled_fmt, sampled_siz, sampled_tex_offset, sampled_stride, sampled_width, sampled_height,
			sampled_low32, palette_crc, formatsize, selector_checksum64, &replacement);
	}))
		return false;
	hires_descriptor_sampled_resolutions++;
	return true;
}

bool HiresReplacement::resolve_hires_native_checksum_replacement_descriptor(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64, ReplacementMeta &meta, HiresNativeChecksumDetailClass detail_class)
{
	meta.vk_image_index = detail::hires_invalid_descriptor_index();

	if (!replacement_provider || !shader_enabled)
		return false;

	auto record_native_checksum_detail = [&]() {
		hires_descriptor_native_checksum_resolutions++;
		switch (detail_class)
		{
		case HiresNativeChecksumDetailClass::Exact:
			hires_descriptor_native_checksum_exact_resolutions++;
			break;
		case HiresNativeChecksumDetailClass::IdentityAssisted:
			hires_descriptor_native_checksum_identity_assisted_resolutions++;
			break;
		case HiresNativeChecksumDetailClass::GenericFallback:
			hires_descriptor_native_checksum_generic_fallback_resolutions++;
			break;
		}
	};

	HiresKey key = {};
	key.checksum64 = checksum64;
	key.formatsize = formatsize;
	key.selector_checksum64 = selector_checksum64;
	key.source = HiresKeySource::NativeChecksum;
	if (!load_hires_descriptor(key, meta, [&](ReplacementImage &replacement) {
		return replacement_provider->decode_rgba8_native_with_selector(checksum64, formatsize, selector_checksum64, &replacement);
	}))
		return false;
	record_native_checksum_detail();
	return true;
}

bool HiresReplacement::resolve_hires_provider_resolution_descriptor(const ::RDP::ReplacementResolution &resolution,
                                                            uint16_t formatsize,
                                                            uint32_t orig_w,
                                                            uint32_t orig_h,
                                                            ReplacementMeta &meta,
                                                            uint64_t *resolved_selector_checksum64,
                                                            const ReplacementPalette *palette)
{
	meta.vk_image_index = detail::hires_invalid_descriptor_index();
	if (resolved_selector_checksum64)
		*resolved_selector_checksum64 = resolution.resolved_selector_checksum64;

	if (!replacement_provider || !shader_enabled || !resolution.available)
		return false;

	// Indexed assets must be coloured with the rendering tile, never the load tile.
	if (resolution.meta.indexed && !palette)
		return false;
	ReplacementMeta resolved_meta = resolution.meta;
	resolved_meta.orig_w = orig_w;
	resolved_meta.orig_h = orig_h;

	auto finalize = [&](uint64_t selector_checksum64) {
		meta = resolved_meta;
		if (resolved_selector_checksum64)
			*resolved_selector_checksum64 = selector_checksum64;
		return true;
	};
	auto try_sampled = [&](uint64_t selector_checksum64) {
		if (!resolution.identity.valid)
			return false;
		if (!resolve_hires_sampled_replacement_descriptor(
			    resolution.identity.sampled_fmt,
			    resolution.identity.sampled_siz,
			    resolution.identity.sampled_tex_offset,
			    resolution.identity.sampled_stride,
			    resolution.identity.sampled_width,
			    resolution.identity.sampled_height,
			    resolution.identity.sampled_low32,
			    resolution.identity.sampled_palette_crc,
			    resolution.identity.formatsize,
			    resolution.resolved_checksum64,
			    selector_checksum64,
			    resolved_meta))
			return false;
		return finalize(selector_checksum64);
	};
	auto try_native_checksum = [&](uint64_t selector_checksum64, HiresNativeChecksumDetailClass detail_class) {
		if (!resolve_hires_native_checksum_replacement_descriptor(
			    resolution.resolved_checksum64,
			    formatsize,
			    selector_checksum64,
			    resolved_meta,
			    detail_class))
			return false;
		resolved_meta.orig_w = orig_w;
		resolved_meta.orig_h = orig_h;
		return finalize(selector_checksum64);
	};
	auto try_jabo = [&]() {
		if (!resolve_hires_jabo_replacement_descriptor(
			    resolution.resolved_checksum64,
			    resolution.resolved_selector_checksum64,
			    formatsize,
			    resolved_meta))
			return false;
		resolved_meta.orig_w = orig_w;
		resolved_meta.orig_h = orig_h;
		return finalize(resolution.resolved_selector_checksum64);
	};
	auto try_compat = [&](uint64_t selector_checksum64) {
		if (!resolve_hires_compat_replacement_descriptor(
			    resolution.resolved_checksum64,
			    formatsize,
			    selector_checksum64,
			    resolved_meta, resolution.meta.indexed ? palette : nullptr))
			return false;
		resolved_meta.orig_w = orig_w;
		resolved_meta.orig_h = orig_h;
		return finalize(selector_checksum64);
	};

	switch (resolution.kind)
	{
	case ReplacementResolutionKind::SampledExactSelector:
	case ReplacementResolutionKind::SampledFamilySingleton:
		return try_sampled(resolution.resolved_selector_checksum64);

	case ReplacementResolutionKind::ExactNativeSampled:
		if (!resolution.identity.valid)
			return false;
		return try_sampled(resolution.identity.selector_checksum64) ||
		       try_native_checksum(resolution.identity.selector_checksum64, HiresNativeChecksumDetailClass::Exact);

	case ReplacementResolutionKind::GenericNativeIdentity:
		if (!resolution.identity.valid)
			return false;
		return try_sampled(resolution.identity.selector_checksum64) ||
		       try_native_checksum(resolution.identity.selector_checksum64, HiresNativeChecksumDetailClass::IdentityAssisted);

	case ReplacementResolutionKind::GenericNativeChecksum:
		return try_native_checksum(resolution.resolved_selector_checksum64, HiresNativeChecksumDetailClass::GenericFallback);

	case ReplacementResolutionKind::GenericCompat:
		return try_compat(resolution.resolved_selector_checksum64);

	case ReplacementResolutionKind::JaboMd5:
		return try_jabo();

	case ReplacementResolutionKind::None:
		return false;
	}

	return false;
}

bool HiresReplacement::resolve_hires_compat_replacement_descriptor(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64, ReplacementMeta &meta, const ReplacementPalette *palette)
{
	meta.vk_image_index = detail::hires_invalid_descriptor_index();

	if (!replacement_provider || !shader_enabled)
		return false;

	HiresKey key = {};
	key.checksum64 = checksum64;
	key.formatsize = formatsize;
	key.selector_checksum64 = selector_checksum64;
	key.source = HiresKeySource::Compat;
	key.indexed_palette_hash = palette ? palette->hash() : 0;
	if (!load_hires_descriptor(key, meta, [&](ReplacementImage &replacement) {
		return replacement_provider->decode_rgba8_compat_with_selector(checksum64, formatsize, selector_checksum64, &replacement, palette);
	}))
		return false;
	hires_descriptor_compat_resolutions++;
	return true;
}

bool HiresReplacement::resolve_hires_jabo_replacement_descriptor(uint64_t key_lo, uint64_t key_hi, uint16_t formatsize, ReplacementMeta &meta)
{
	meta.vk_image_index = detail::hires_invalid_descriptor_index();

	if (!replacement_provider || !shader_enabled)
		return false;

	HiresKey key = {};
	key.checksum64 = key_lo;
	key.selector_checksum64 = key_hi;
	key.source = HiresKeySource::Jabo;
	// One MD5 can be drawn as I or a format with independent alpha.
	// Keep their decoded mip chains and resident descriptors distinct.
	key.formatsize = formatsize;
	if (!load_hires_descriptor(key, meta, [&](ReplacementImage &replacement) {
		return replacement_provider->decode_rgba8_jabo(key_lo, key_hi, formatsize & 0xffu, &replacement);
	}))
		return false;

	return true;
}

// Keys the draw view the way Jabo named its texture dump and looks the MD5 up.
// The Rice key computed for the same view gates the memo, so repeated draws of
// unchanged texels hash once.
uint64_t HiresReplacement::hires_jabo_binding_hash(const TileMeta &tile_meta, const detail::HiresCompatView &view,
                                           bool tlut_ia16) const
{
	const bool ci = tile_meta.fmt == TextureFormat::CI;
	const uint32_t palette_hash =
	    ci ? detail::hires_jabo_palette_hash(tlut_tmem_shadow, tile_meta.size, tile_meta.palette) : 0;
	return detail::hires_jabo_source_hash(cpu_rdram, rdram_size, view, tile_meta.size, palette_hash, ci && tlut_ia16);
}

bool HiresReplacement::resolve_hires_jabo_draw_candidate(const TileMeta &tile_meta, const detail::HiresCompatView &view,
                                                 uint64_t rice_checksum64, bool tlut_ia16, ReplacementResolution *out,
                                                 uint64_t *source_hash)
{
	const bool ci = tile_meta.fmt == TextureFormat::CI;
	if (ci && !tlut_shadow_valid)
		return false;

	detail::HiresJaboMemoKey memo_key;
	memo_key.source_hash = *source_hash = hires_jabo_binding_hash(tile_meta, view, tlut_ia16);
	memo_key.address = view.address;
	memo_key.width = view.width;
	memo_key.height = view.height;
	memo_key.row_stride = view.row_stride;
	memo_key.formatsize = formatsize_key(tile_meta.fmt, tile_meta.size);
	if (ci)
	{
		memo_key.palette_bank = tile_meta.palette;
		memo_key.tlut_ia16 = tlut_ia16;
		memo_key.palette_hash = detail::hires_jabo_palette_hash(tlut_tmem_shadow, tile_meta.size, tile_meta.palette);
	}

	std::array<uint8_t, 16> md5;
	auto memo = hires_jabo_memo.find(memo_key);
	if (memo != hires_jabo_memo.end())
	{
		md5 = memo->second;
		hires_jabo_memo_hits++;
	}
	else
	{
		if (!detail::compute_hires_jabo_key(cpu_rdram, rdram_size, view, tile_meta.fmt, tile_meta.size,
		                                   ci ? tlut_tmem_shadow : nullptr, tile_meta.palette, tlut_ia16,
		                                   hires_jabo_scratch, md5.data()))
			return false;
		hires_jabo_md5_computations++;
		if (hires_jabo_memo.size() >= detail::HIRES_JABO_MEMO_LIMIT)
			hires_jabo_memo.clear();
		hires_jabo_memo.emplace(memo_key, md5);
	}

	const bool hit = replacement_provider->resolve_jabo_candidate(md5.data(), out);
	if (!hit)
		hires_jabo_draw_misses++;
	return hit;
}

void HiresReplacement::clear_hires_tile_binding(unsigned tile)
{
	tiles[tile].replacement = {};
}

void HiresReplacement::apply_hires_tile_binding(unsigned tile, const ReplacementTileState &state)
{
	if (!state.hit || !detail::hires_descriptor_index_valid(state.vk_image_index) ||
	    state.orig_w == 0 || state.orig_h == 0 || state.repl_w == 0 || state.repl_h == 0)
	{
		clear_hires_tile_binding(tile);
		return;
	}

	hires_resources.residency.touch(state.vk_image_index, hires_frame);
	tiles[tile].replacement.repl_orig_w = state.orig_w;
	tiles[tile].replacement.repl_orig_h = state.orig_h;
	tiles[tile].replacement.repl_w = state.repl_w;
	tiles[tile].replacement.repl_h = state.repl_h;
	tiles[tile].replacement.repl_desc_index = state.vk_image_index;
}


// Loads the configured packages into provider and attaches it to the renderer.
// The caller has drained all queued RDP work.
HiresPackStatus configure_hires_packages(Renderer &renderer, ReplacementProvider &provider, bool enable,
                                         const std::vector<std::string> &cache_paths, unsigned budget_mb)
{
	HiresPackStatus status;
	renderer.set_replacement_provider(nullptr);
	provider.clear();
	provider.set_enabled(enable);
	if (enable)
	{
		status.state = HiresPackState::NotFound;
		renderer.get_hires().configure_gpu_budget(budget_mb);
	}
	renderer.get_hires().set_gliden64_compat_crc(false);

	const char *first_path = cache_paths.empty() ? nullptr : cache_paths.front().c_str();
	auto outcome = detail::classify_hires_configure_outcome(enable, first_path, false);
	if (outcome == detail::HiresConfigureOutcome::Disabled)
		return status;

	if (outcome == detail::HiresConfigureOutcome::MissingPath)
	{
		LOGW("Hi-res replacement enabled, but cache path is empty.\n");
		return status;
	}

	for (const auto &path : cache_paths)
	{
		const auto previous_entries = provider.entry_count();
		if (provider.load_cache_dir(path, true))
			LOGI("Hi-res replacement cache loaded: %zu entries from %s\n",
			     provider.entry_count() - previous_entries, path.c_str());
		else if (!provider.rejected_path().empty())
			LOGW("Hi-res replacement cache load failed for path: %s\n", path.c_str());
	}
	status.packages = provider.package_stats();
	for (const auto &pack : status.packages)
		LOGI("Hi-res package: path=%s records=%zu hidden_by_later=%zu.\n",
		     pack.path.c_str(), pack.records, pack.hidden_by_later);
	status.loaded_path = provider.loaded_path();
	status.rejected_path = provider.rejected_path();
	status.load_error = provider.load_error();
	if (!status.rejected_path.empty())
		status.state = HiresPackState::Rejected;
	outcome = detail::classify_hires_configure_outcome(enable, first_path,
	                                                provider.entry_count() != 0);
	if (outcome == detail::HiresConfigureOutcome::LoadFailed)
	{
		LOGI("Hi-res replacement disabled: no entries in checked paths.\n");
		return status;
	}
	if (detail::should_attach_hires_provider(outcome))
	{
		if (!renderer.set_replacement_provider(&provider))
		{
			status.state = HiresPackState::Unsupported;
			return status;
		}
		status.state = HiresPackState::Loaded;
		status.entries = provider.entry_count();
	}

	// The draw-time compat lane runs only for compat- or Jabo-bearing PHRB payloads;
	// Jabo MD5 lookups run inside it, after its Rice key misses.
	renderer.get_hires().set_gliden64_compat_crc(
		provider.has_phrb_compat_entries() || provider.has_jabo_entries());
	return status;
}
}
