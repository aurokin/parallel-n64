#include "rdp.hpp"
#include "rdp_command_ingest.hpp"
#include "rdp_frame_mapping.hpp"
#include "rdp_init_policy.hpp"
#include "timeline_trace_file.hpp"
#include "Gfx #1.3.h"
#include "parallel.h"
#include "z64.h"
#include "parallel-rdp/parallel-rdp/rdp_hires_runtime_policy.hpp"
#include "parallel-rdp/parallel-rdp/rdp_hires_capability_policy.hpp"
#include <assert.h>
#include <stdlib.h>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>

using namespace Vulkan;
using namespace std;

extern retro_log_printf_t log_cb;
extern retro_environment_t environ_cb;

namespace RDP
{
const struct retro_hw_render_interface_vulkan *vulkan;

static int cmd_cur;
static int cmd_ptr;
static uint32_t cmd_data[0x00040000 >> 2];
static uint64_t pending_timeline_value, timeline_value;

static unique_ptr<Util::TimelineTraceFile> timeline_trace_file;
static unique_ptr<CommandProcessor> frontend;
static unique_ptr<Device> device;
static unique_ptr<Context> context;
static QueryPoolHandle begin_ts, end_ts;

static vector<retro_vulkan_image> retro_images;
static vector<ImageHandle> retro_image_handles;
unsigned width, height;
unsigned overscan;
unsigned upscaling = 1;
unsigned downscaling_steps = 0;
bool native_texture_lod = false;
bool native_tex_rect = true;
bool synchronous = true, divot_filter = true, gamma_dither = true;
bool vi_aa = true, vi_scale = true, dither_filter = true;
bool interlacing = true;
bool hires_textures = false;
unsigned hires_filter = 2;
unsigned hires_memory_mb = 0;
string hires_cache_path;
string hires_ident, hires_gliden_name;
static detail::HiresPackageSelection hires_selection;

static unsigned hires_messages_sent = 0;

enum HiresMessage { PackLoaded, NoPack, PackRejected, GPUUnsupported, BudgetFull, ReadErrors, PackOverlap };

void reset_hires_messages()
{
	hires_messages_sent = 0;
}

static void hires_message_once(HiresMessage event, const char *text)
{
	if (hires_messages_sent & (1u << event))
		return;
	hires_messages_sent |= 1u << event;
	retro_message_ext extended = { text, 6000, 1, RETRO_LOG_INFO,
	                               RETRO_MESSAGE_TARGET_OSD, RETRO_MESSAGE_TYPE_NOTIFICATION, -1 };
	if (environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &extended))
		return;
	retro_message message = { text, 360 };
	environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &message);
}

// Only inspect immediate files of game-named directories. The root is listed
// once, and all matching and ordering decisions remain in the MIT policy.
static void read_hires_listing(const string &root, vector<detail::HiresListingEntry> &listing)
{
	DIR *dir = opendir(root.c_str());
	if (!dir)
		return;
	while (auto *entry = readdir(dir))
	{
		if (entry->d_name[0] == '.')
			continue;
		const string path = root + "/" + entry->d_name;
		struct stat info;
		if (stat(path.c_str(), &info) != 0 || (!S_ISDIR(info.st_mode) && !S_ISREG(info.st_mode)))
			continue;
		detail::HiresListingEntry item;
		item.path = path;
		item.directory = S_ISDIR(info.st_mode);
		const auto name = detail::hires_ascii_fold(entry->d_name);
		if (item.directory &&
		    (name == detail::hires_ascii_fold(hires_ident) || name == detail::hires_ascii_fold(hires_gliden_name)))
		{
			if (DIR *children = opendir(path.c_str()))
			{
				while (auto *child = readdir(children))
				{
					if (child->d_name[0] == '.')
						continue;
					const auto child_path = path + "/" + child->d_name;
					if (stat(child_path.c_str(), &info) == 0 && S_ISREG(info.st_mode))
						item.files.emplace_back(child->d_name);
				}
				closedir(children);
			}
		}
		listing.push_back(std::move(item));
	}
	closedir(dir);
}

static vector<string> find_hires_packages()
{
	const char *override_path = getenv("PARALLEL_RDP_HIRES_CACHE_PATH");
	hires_selection = detail::resolve_hires_cache_path(hires_textures, hires_cache_path, hires_ident,
	                                                   hires_gliden_name, override_path, {});
	if (!hires_selection.paths.empty() || !hires_textures || hires_cache_path.empty() || hires_ident.empty())
		return hires_selection.paths;
	vector<detail::HiresListingEntry> listing;
	read_hires_listing(hires_cache_path + "/parallel-n64/hires", listing);
	hires_selection = detail::resolve_hires_cache_path(hires_textures, hires_cache_path, hires_ident,
	                                                   hires_gliden_name, override_path, listing);
	for (const auto &source : detail::hires_legacy_sources(hires_cache_path, hires_gliden_name))
	{
		struct stat info;
		if (stat(source.path.c_str(), &info) == 0 &&
		    (source.directory ? S_ISDIR(info.st_mode) : S_ISREG(info.st_mode)))
			hires_selection.unconverted.push_back(source.path);
	}
	for (const auto &path : hires_selection.different_game)
		log_cb(RETRO_LOG_WARN, "Hi-res pack for a different game (not loaded): %s; expected ident=%s.\n", path.c_str(), hires_ident.c_str());
	for (const auto &path : hires_selection.near_misses)
		log_cb(RETRO_LOG_WARN, "Hi-res near miss (not loaded): %s; expected ident=%s.\n", path.c_str(), hires_ident.c_str());
	for (const auto &path : hires_selection.unconverted)
		log_cb(RETRO_LOG_INFO, "Hi-res unconverted pack: %s.\n", path.c_str());
	return hires_selection.paths;
}

static void show_hires_status()
{
	if (!hires_textures || !frontend)
		return;
	const auto &pack = frontend->get_hires_pack_status();
	const auto &residency = frontend->get_hires_residency_status();
	char message[1024];
	if (!pack.rejected_path.empty())
	{
		std::snprintf(message, sizeof(message), "Hi-res textures: %s not loaded (%s)",
		              detail::hires_path_filename(pack.rejected_path).c_str(),
		              pack.load_error == ReplacementLoadStatus::UnsupportedVersion ?
		                  "unsupported PHRB version; reconvert the pack or use a newer core" : "unreadable or damaged");
		hires_message_once(PackRejected, message);
	}
	if (pack.state == HiresPackState::Loaded)
	{
		const string budget = residency.budget ?
		    std::to_string(residency.budget / (1024 * 1024)) + " MB memory budget" : "unlimited memory budget";
		const auto count = pack.packages.size();
		const auto name = hires_ident.empty() ? detail::hires_path_filename(pack.loaded_path) : hires_ident;
		std::snprintf(message, sizeof(message), "Hi-res textures: %zu pack%s for %s - %zu texture%s, %s",
		              count, count == 1 ? "" : "s", name.c_str(), pack.entries,
		              pack.entries == 1 ? "" : "s", budget.c_str());
		hires_message_once(PackLoaded, message);
		for (const auto &package : pack.packages)
			if (package.mostly_hidden())
			{
				const auto hider = package.multiple_hiders ? string("later packs") :
				                   "\"" + detail::hires_path_filename(package.hidden_by) + "\"";
				std::snprintf(message, sizeof(message), "Hi-res textures: \"%s\" is mostly hidden by %s. Later names win. Remove one, or rename to change the order.",
				              detail::hires_path_filename(package.path).c_str(), hider.c_str());
				hires_message_once(PackOverlap, message);
				break;
			}
	}
	else if (pack.state == HiresPackState::NotFound)
	{
		const char *override_path = getenv("PARALLEL_RDP_HIRES_CACHE_PATH");
		if (override_path && *override_path)
			std::snprintf(message, sizeof(message), "Hi-res textures: no pack in %s", override_path);
		else if (!hires_selection.different_game.empty())
			std::snprintf(message, sizeof(message), "Hi-res textures: \"%s\" contains a pack for a different game. This game's ident is \"%s\".",
			              detail::hires_path_filename(hires_selection.different_game.front()).c_str(), hires_ident.c_str());
		else if (!hires_selection.near_misses.empty())
			std::snprintf(message, sizeof(message), "Hi-res textures: \"%s\" was not loaded. This game's folder must be named \"%s\".",
			              detail::hires_path_filename(hires_selection.near_misses.front()).c_str(), hires_ident.c_str());
		else if (!hires_selection.unconverted.empty())
			std::snprintf(message, sizeof(message), "Hi-res textures: found an unconverted pack (%s). Run python3 tools/hts2phrb.py --cache <pack> --rom <ROM> --install <system dir>.",
			              detail::hires_path_filename(hires_selection.unconverted.front()).c_str());
		else if (hires_ident.empty())
			std::snprintf(message, sizeof(message), "Hi-res textures: this ROM has no name or game code in its header.");
		else
			std::snprintf(message, sizeof(message), "Hi-res textures: no pack for \"%s\" in system/parallel-n64/hires/",
			              hires_ident.c_str());
		hires_message_once(NoPack, message);
	}
	else if (pack.state == HiresPackState::Unsupported)
		hires_message_once(GPUUnsupported, "Hi-res textures unavailable: GPU descriptor or frame resources unsupported");
	if (residency.refused_saturated)
	{
		std::snprintf(message, sizeof(message), "Hi-res textures exceed the %llu MB memory budget; some show at original resolution. Raise \"Hi-res texture memory\".",
		              (unsigned long long)(residency.budget / (1024 * 1024)));
		hires_message_once(BudgetFull, message);
	}
	if (residency.decode_failures)
	{
		std::snprintf(message, sizeof(message), "Hi-res textures: %llu texture%s could not be read; the pack may be damaged",
		              (unsigned long long)residency.decode_failures, residency.decode_failures == 1 ? "" : "s");
		hires_message_once(ReadErrors, message);
	}
}

void process_commands()
{
	detail::CommandIngestState state = {};
	state.cmd_cur = cmd_cur;
	state.cmd_ptr = cmd_ptr;
	state.cmd_data = cmd_data;

	detail::CommandIngestHooks hooks = {};
	hooks.frontend_available = bool(frontend);
	hooks.synchronous = synchronous;

	struct CallbackState
	{
		CommandProcessor *frontend = nullptr;
	} cb_state;
	cb_state.frontend = frontend.get();
	hooks.userdata = &cb_state;

	hooks.enqueue_command = [](void *userdata, unsigned num_words, const uint32_t *words) {
		auto *cb = static_cast<CallbackState *>(userdata);
		cb->frontend->enqueue_command(num_words, words);
	};
	hooks.signal_timeline = [](void *userdata) -> uint64_t {
		auto *cb = static_cast<CallbackState *>(userdata);
		return cb->frontend->signal_timeline();
	};
	hooks.wait_for_timeline = [](void *userdata, uint64_t timeline) {
		auto *cb = static_cast<CallbackState *>(userdata);
		cb->frontend->wait_for_timeline(timeline);
	};
	hooks.raise_dp_interrupt = [](void *) {
		*gfx_info.MI_INTR_REG |= DP_INTERRUPT;
		gfx_info.CheckInterrupts();
	};

	detail::process_command_ingest(
			state,
			DRAM,
			SP_DMEM,
			*GET_GFX_INFO(DPC_START_REG),
			*GET_GFX_INFO(DPC_END_REG),
			*GET_GFX_INFO(DPC_CURRENT_REG),
			*GET_GFX_INFO(DPC_STATUS_REG),
			hooks);

	cmd_cur = state.cmd_cur;
	cmd_ptr = state.cmd_ptr;
}

static QueryPoolHandle refresh_begin_ts;

void profile_refresh_begin()
{
	if (device)
		refresh_begin_ts = device->write_calibrated_timestamp();
}

void profile_refresh_end()
{
	if (device)
	{
		device->register_time_interval("Emulation", refresh_begin_ts, device->write_calibrated_timestamp(), "refresh");
		refresh_begin_ts.reset();
	}
}

void begin_frame()
{
	unsigned mask = vulkan->get_sync_index_mask(vulkan->handle);
	unsigned num_frames = detail::sync_mask_to_num_frames(mask);

	if (num_frames != retro_images.size())
	{
		retro_images.resize(num_frames);
		retro_image_handles.resize(num_frames);
	}

	vulkan->wait_sync_index(vulkan->handle);
	if (!begin_ts)
		begin_ts = device->write_calibrated_timestamp();

	//frontend->wait_for_timeline(pending_timeline_value);
	//pending_timeline_value = timeline_value;
}

bool init()
{
	if (!detail::init_prerequisites_met(context.get(), vulkan))
		return false;

	unsigned mask = vulkan->get_sync_index_mask(vulkan->handle);
	const detail::SyncFrameCounts sync_counts = detail::compute_sync_frame_counts(mask);
	const unsigned num_frames = sync_counts.num_frames;
	const unsigned num_sync_frames = sync_counts.num_sync_frames;

	retro_images.resize(num_frames);
	retro_image_handles.resize(num_frames);

	device.reset(new Device);
	device->set_context(*context);
	device->init_frame_contexts(num_sync_frames);
	log_cb(RETRO_LOG_INFO, "Using %u sync frames for parallel-RDP.\n", num_sync_frames);
	device->set_queue_lock(
			[]() { vulkan->lock_queue(vulkan->handle); },
			[]() { vulkan->unlock_queue(vulkan->handle); });

	const auto &features = device->get_device_features();
	detail::HostMemoryImportPlan host_mem_plan = detail::plan_host_memory_import(
			reinterpret_cast<uintptr_t>(gfx_info.RDRAM),
			features.supports_external_memory_host,
			features.host_memory_properties.minImportedHostPointerAlignment);

	detail::HiresDescriptorFeatureSupport hires_support = {};
	hires_support.descriptor_indexing = features.vk12_features.descriptorIndexing;
	hires_support.runtime_descriptor_array = features.vk12_features.runtimeDescriptorArray;
	hires_support.sampled_image_array_non_uniform_indexing =
			features.vk12_features.shaderSampledImageArrayNonUniformIndexing;
	hires_support.descriptor_binding_variable_descriptor_count =
			features.vk12_features.descriptorBindingVariableDescriptorCount;
	hires_support.descriptor_binding_partially_bound =
			features.vk12_features.descriptorBindingPartiallyBound;
	hires_support.descriptor_binding_update_after_bind =
			features.vk12_features.descriptorBindingSampledImageUpdateAfterBind;
	const uint32_t max_hires_update_after_bind_sampled_images =
			features.vk12_props.maxDescriptorSetUpdateAfterBindSampledImages;
	const detail::HiresDescriptorRequirement hires_requirement =
			detail::validate_hires_descriptor_support(
					hires_support,
					max_hires_update_after_bind_sampled_images);
	const bool hires_capabilities_ok = detail::should_enable_hires_after_capability_check(
			hires_textures,
			hires_requirement);

	uintptr_t aligned_rdram = host_mem_plan.aligned_rdram;
	uintptr_t offset = host_mem_plan.offset;

	if (!features.supports_external_memory_host)
		log_cb(RETRO_LOG_WARN, "VK_EXT_external_memory_host is not supported by this device. Application might run slower because of this.\n");

	CommandProcessorFlags flags = 0;
	switch (upscaling)
	{
		case 2:
			flags |= COMMAND_PROCESSOR_FLAG_UPSCALING_2X_BIT;
			log_cb(RETRO_LOG_INFO, "Using 2x upscaling!\n");
			break;

		case 4:
			flags |= COMMAND_PROCESSOR_FLAG_UPSCALING_4X_BIT;
			log_cb(RETRO_LOG_INFO, "Using 4x upscaling!\n");
			break;

		case 8:
			flags |= COMMAND_PROCESSOR_FLAG_UPSCALING_8X_BIT;
			log_cb(RETRO_LOG_INFO, "Using 8x upscaling!\n");
			break;

		default:
			break;
	}

	frontend.reset(new CommandProcessor(*device, reinterpret_cast<void *>(aligned_rdram),
				offset, 8 * 1024 * 1024, 4 * 1024 * 1024, flags));

	if (!detail::ensure_frontend_device_supported(frontend))
	{
		log_cb(RETRO_LOG_ERROR, "This device probably does not support 8/16-bit storage. Make sure you're using up-to-date drivers!\n");
		return false;
	}

	RDP::Quirks quirks;
	quirks.set_native_texture_lod(native_texture_lod);
	quirks.set_native_resolution_tex_rect(native_tex_rect);
	frontend->set_quirks(quirks);

	const auto cache_paths = find_hires_packages();
	if (hires_textures)
	{
		log_cb(RETRO_LOG_INFO,
		       "Hi-res capability check: descriptor_indexing=%u runtime_descriptor_array=%u sampled_image_array_non_uniform_indexing=%u descriptor_binding_variable_descriptor_count=%u descriptor_binding_partially_bound=%u descriptor_binding_update_after_bind=%u maxDescriptorSetUpdateAfterBindSampledImages=%u cache_path=%s.\n",
		       hires_support.descriptor_indexing ? 1u : 0u,
		       hires_support.runtime_descriptor_array ? 1u : 0u,
		       hires_support.sampled_image_array_non_uniform_indexing ? 1u : 0u,
		       hires_support.descriptor_binding_variable_descriptor_count ? 1u : 0u,
		       hires_support.descriptor_binding_partially_bound ? 1u : 0u,
		       hires_support.descriptor_binding_update_after_bind ? 1u : 0u,
		       max_hires_update_after_bind_sampled_images,
		       cache_paths.empty() ? "<empty>" : cache_paths.front().c_str());
	}

	if (hires_textures && !hires_capabilities_ok)
	{
		hires_message_once(GPUUnsupported, "Hi-res textures unavailable: this GPU lacks descriptor indexing");
		log_cb(RETRO_LOG_WARN,
		       "Hi-res textures requested, but disabled: %s (maxDescriptorSetUpdateAfterBindSampledImages=%u, required>=%u).\n",
		       detail::describe_hires_descriptor_requirement(hires_requirement),
		       max_hires_update_after_bind_sampled_images,
		       detail::hires_min_update_after_bind_sampled_images());
	}

	frontend->configure_hires_replacement(hires_capabilities_ok, cache_paths, hires_memory_mb);
	if (frontend->get_hires_pack_status().state == HiresPackState::Loaded)
	{
		log_cb(RETRO_LOG_INFO,
		       "Hi-res textures enabled (path=%s, filter=%u).\n",
		       cache_paths.empty() ? "<empty>" : cache_paths.front().c_str(), hires_filter);
	}
	show_hires_status();
	frontend->set_hires_filter(hires_filter);

	timeline_value = 0;
	pending_timeline_value = 0;
	width = 0;
	height = 0;
	return true;
}

void deinit()
{
	detail::clear_deinit_state(begin_ts, end_ts, retro_image_handles, retro_images, frontend, device, context);
	timeline_trace_file.reset();
}

static void complete_frame_error()
{
	static const char error_tex[] =
		"ooooooooooooooooooooooooo"
		"ooXXXXXoooXXXXXoooXXXXXoo"
		"ooXXooooooXoooXoooXoooXoo"
		"ooXXXXXoooXXXXXoooXXXXXoo"
		"ooXXXXXoooXoXoooooXoXoooo"
		"ooXXooooooXooXooooXooXooo"
		"ooXXXXXoooXoooXoooXoooXoo"
		"ooooooooooooooooooooooooo";

	auto info = Vulkan::ImageCreateInfo::immutable_2d_image(50, 16, VK_FORMAT_R8G8B8A8_UNORM, false);
	info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	info.misc = IMAGE_MISC_MUTABLE_SRGB_BIT;

	Vulkan::ImageInitialData data = {};

	uint32_t tex_data[16][50];
	for (unsigned y = 0; y < 16; y++)
		for (unsigned x = 0; x < 50; x++)
			tex_data[y][x] = error_tex[25 * (y >> 1) + (x >> 1)] != 'o' ? 0xffffffffu : 0u;
	data.data = tex_data;
	auto image = device->create_image(info, &data);

	unsigned index = vulkan->get_sync_index(vulkan->handle);
	assert(index < retro_images.size());

	retro_images[index].image_view = image->get_view().get_view();
	retro_images[index].image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	retro_images[index].create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	retro_images[index].create_info.image = image->get_image();
	retro_images[index].create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	retro_images[index].create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	retro_images[index].create_info.subresourceRange.baseMipLevel = 0;
	retro_images[index].create_info.subresourceRange.baseArrayLayer = 0;
	retro_images[index].create_info.subresourceRange.levelCount = 1;
	retro_images[index].create_info.subresourceRange.layerCount = 1;
	retro_images[index].create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	retro_images[index].create_info.components.r = VK_COMPONENT_SWIZZLE_R;
	retro_images[index].create_info.components.g = VK_COMPONENT_SWIZZLE_G;
	retro_images[index].create_info.components.b = VK_COMPONENT_SWIZZLE_B;
	retro_images[index].create_info.components.a = VK_COMPONENT_SWIZZLE_A;

	vulkan->set_image(vulkan->handle, &retro_images[index], 0, nullptr, VK_QUEUE_FAMILY_IGNORED);
	width = image->get_width();
	height = image->get_height();
	retro_image_handles[index] = image;

	device->flush_frame();
}

void complete_frame()
{
	if (!frontend)
	{
		complete_frame_error();
		device->next_frame_context();
		return;
	}

	timeline_value = frontend->signal_timeline();

	detail::forward_vi_registers(
			[&](VIRegister reg, uint32_t value) {
				frontend->set_vi_register(reg, value);
			},
			gfx_info);

	ScanoutOptions opts = detail::make_scanout_options(
			vi_aa, vi_scale, dither_filter, divot_filter, gamma_dither, interlacing, downscaling_steps, overscan);
	auto image = frontend->scanout(opts);
	unsigned index = vulkan->get_sync_index(vulkan->handle);

	if (!image)
	{
		auto info = Vulkan::ImageCreateInfo::immutable_2d_image(1, 1, VK_FORMAT_R8G8B8A8_UNORM);
		info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		info.misc = IMAGE_MISC_MUTABLE_SRGB_BIT;
		info.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		image = device->create_image(info);

		auto cmd = device->request_command_buffer();
		cmd->image_barrier(*image,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
		cmd->clear_image(*image, {});
		cmd->image_barrier(*image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
		device->submit(cmd);
	}

	assert(index < retro_images.size());

	retro_images[index].image_view = image->get_view().get_view();
	retro_images[index].image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	retro_images[index].create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	retro_images[index].create_info.image = image->get_image();
	retro_images[index].create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	retro_images[index].create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	retro_images[index].create_info.subresourceRange.baseMipLevel = 0;
	retro_images[index].create_info.subresourceRange.baseArrayLayer = 0;
	retro_images[index].create_info.subresourceRange.levelCount = 1;
	retro_images[index].create_info.subresourceRange.layerCount = 1;
	retro_images[index].create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	retro_images[index].create_info.components.r = VK_COMPONENT_SWIZZLE_R;
	retro_images[index].create_info.components.g = VK_COMPONENT_SWIZZLE_G;
	retro_images[index].create_info.components.b = VK_COMPONENT_SWIZZLE_B;
	retro_images[index].create_info.components.a = VK_COMPONENT_SWIZZLE_A;

	vulkan->set_image(vulkan->handle, &retro_images[index], 0, nullptr, VK_QUEUE_FAMILY_IGNORED);
	width = image->get_width();
	height = image->get_height();
	retro_image_handles[index] = image;

	end_ts = device->write_calibrated_timestamp();
	device->register_time_interval("Emulation", begin_ts, end_ts, "frame");
	begin_ts.reset();
	end_ts.reset();

	RDP::Quirks quirks;
	quirks.set_native_texture_lod(native_texture_lod);
	quirks.set_native_resolution_tex_rect(native_tex_rect);
	frontend->set_quirks(quirks);

	frontend->begin_frame_context();
	show_hires_status();
}
}

// Creates the Context every negotiation path starts from.
static bool create_context(PFN_vkGetInstanceProcAddr get_instance_proc_addr)
{
	if (!Vulkan::Context::init_loader(get_instance_proc_addr))
		return false;

	::RDP::context.reset(new Vulkan::Context);

	::Vulkan::Context::SystemHandles handles;

	if (const char *env = getenv("PARALLEL_RDP_TIMELINE_TRACE"))
	{
		::RDP::timeline_trace_file.reset(new Util::TimelineTraceFile(env));
		handles.timeline_trace_file = ::RDP::timeline_trace_file.get();
	}

	::RDP::context->set_system_handles(handles);
	return true;
}

static void hand_device_to_frontend(struct retro_vulkan_context *frontend_context)
{
	frontend_context->gpu = ::RDP::context->get_gpu();
	frontend_context->device = ::RDP::context->get_device();
	frontend_context->queue = ::RDP::context->get_queue_info().queues[Vulkan::QUEUE_INDEX_GRAPHICS];
	frontend_context->queue_family_index = ::RDP::context->get_queue_info().family_indices[Vulkan::QUEUE_INDEX_GRAPHICS];
	frontend_context->presentation_queue = ::RDP::context->get_queue_info().queues[Vulkan::QUEUE_INDEX_GRAPHICS];
	frontend_context->presentation_queue_family_index = ::RDP::context->get_queue_info().family_indices[Vulkan::QUEUE_INDEX_GRAPHICS];

	// Frontend owns the device.
	::RDP::context->release_device();
}

// Negotiation v1: the frontend created the instance itself, so the backend
// can only assume Vulkan 1.1 core and enables no Vulkan 1.2/1.3 features.
bool parallel_create_device(struct retro_vulkan_context *frontend_context, VkInstance instance, VkPhysicalDevice gpu,
                            VkSurfaceKHR surface, PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                            const char **required_device_extensions, unsigned num_required_device_extensions,
                            const char **required_device_layers, unsigned num_required_device_layers,
                            const VkPhysicalDeviceFeatures *required_features)
{
	if (!create_context(get_instance_proc_addr))
		return false;

	if (!::RDP::context->init_device_from_instance(
				instance, gpu, surface, required_device_extensions, num_required_device_extensions,
				required_features))
	{
		::RDP::context.reset();
		return false;
	}

	hand_device_to_frontend(frontend_context);
	return true;
}

// Negotiation v2: the backend creates the instance and device through the
// frontend's wrappers, which add what the frontend needs, so it knows the
// instance version and enables the device's Vulkan 1.2/1.3 features.
VkInstance parallel_create_instance(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                                    const VkApplicationInfo *app,
                                    retro_vulkan_create_instance_wrapper_t create_instance_wrapper,
                                    void *opaque)
{
	if (!create_context(get_instance_proc_addr))
		return VK_NULL_HANDLE;

	struct Factory final : Vulkan::InstanceFactory
	{
		VkInstance create_instance(const VkInstanceCreateInfo *info) override
		{
			return wrapper(opaque, info);
		}

		retro_vulkan_create_instance_wrapper_t wrapper = nullptr;
		void *opaque = nullptr;
	} factory;

	factory.wrapper = create_instance_wrapper;
	factory.opaque = opaque;

	::RDP::context->set_application_info(app);
	::RDP::context->set_instance_factory(&factory);
	bool created = ::RDP::context->init_instance(nullptr, 0);
	::RDP::context->set_instance_factory(nullptr);

	if (!created)
	{
		::RDP::context.reset();
		return VK_NULL_HANDLE;
	}

	// Frontend owns the instance.
	::RDP::context->release_instance();
	return ::RDP::context->get_instance();
}

bool parallel_create_device2(struct retro_vulkan_context *frontend_context, VkInstance instance, VkPhysicalDevice gpu,
                             VkSurfaceKHR surface, PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                             retro_vulkan_create_device_wrapper_t create_device_wrapper, void *opaque)
{
	// The frontend calls create_instance first. A failed call keeps the
	// context: the frontend may retry with gpu == VK_NULL_HANDLE.
	if (!::RDP::context || ::RDP::context->get_instance() != instance ||
	    Vulkan::Context::get_instance_proc_addr() != get_instance_proc_addr)
		return false;

	struct Factory final : Vulkan::DeviceFactory
	{
		VkDevice create_device(VkPhysicalDevice physical_device, const VkDeviceCreateInfo *info) override
		{
			return wrapper(physical_device, opaque, info);
		}

		retro_vulkan_create_device_wrapper_t wrapper = nullptr;
		void *opaque = nullptr;
	} factory;

	factory.wrapper = create_device_wrapper;
	factory.opaque = opaque;

	::RDP::context->set_device_factory(&factory);
	bool created = ::RDP::context->init_device(gpu, surface, nullptr, 0);
	::RDP::context->set_device_factory(nullptr);

	if (!created)
		return false;

	hand_device_to_frontend(frontend_context);
	return true;
}

static const VkApplicationInfo parallel_app_info = {
	VK_STRUCTURE_TYPE_APPLICATION_INFO,
	nullptr,
	"paraLLEl-RDP",
	0,
	"Granite",
	0,
	VK_API_VERSION_1_1,
};

const VkApplicationInfo *parallel_get_application_info(void)
{
	return &parallel_app_info;
}

extern "C" uint32_t parallel_get_noise_state(void)
{
	return RDP::frontend ? RDP::frontend->get_noise_state() : 0;
}

extern "C" void parallel_set_noise_state(uint32_t base_primitive_index)
{
	if (RDP::frontend)
		RDP::frontend->set_noise_state(base_primitive_index);
}
