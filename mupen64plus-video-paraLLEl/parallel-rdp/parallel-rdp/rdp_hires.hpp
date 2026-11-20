// SPDX-License-Identifier: MIT

#pragma once

// Hi-res texture replacement for the RDP renderer.
//
// The renderer owns one HiresReplacement and reports RDP texture-memory events
// to it: SetTile, each TMEM load, and each draw. The component keys loads and
// draw views against the attached ReplacementProvider, keeps the replacement
// images resident in a bindless pool, and writes the GPU-visible part of a
// binding into the renderer's TileInfo::replacement so the rasterizer can
// sample it. Everything else about a binding stays here.

#include "rdp_common.hpp"
#include "rdp_data_structures.hpp"
#include "rdp_hires_ci_binding_policy.hpp"
#include "rdp_hires_compat_view.hpp"
#include "rdp_hires_jabo_key.hpp"
#include "rdp_hires_residency_policy.hpp"
#include "rdp_hires_strong_hash.hpp"
#include "texture_replacement.hpp"
#include "device.hpp"
#include <array>
#include <cstdlib>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RDP
{
struct LoadTileInfo;
class Renderer;

enum class HiresPackState { Disabled, NotFound, Rejected, Loaded, Unsupported };

struct HiresPackStatus
{
	HiresPackState state = HiresPackState::Disabled;
	ReplacementLoadStatus load_error = ReplacementLoadStatus::NotFound;
	std::string loaded_path, rejected_path;
	size_t entries = 0;
	std::vector<ReplacementPackageStats> packages;
};

namespace detail
{
// Parses PARALLEL_RDP_HIRES_SUMMARY_INTERVAL_FRAMES. Unset, non-numeric, zero,
// and negative values disable periodic summaries; oversized values saturate.
inline uint32_t parse_hires_summary_interval_frames(const char *env)
{
	if (!env)
		return 0;

	const long long value = strtoll(env, nullptr, 0);
	if (value <= 0)
		return 0;
	if (static_cast<unsigned long long>(value) > UINT32_MAX)
		return UINT32_MAX;
	return uint32_t(value);
}

// Counts presented frames and decides when a periodic hi-res summary is due.
// With interval == 0 the clock is inert: it never counts and never fires, so the
// per-frame cost of the disabled path is a single compare.
struct HiresSummaryIntervalClock
{
	uint32_t interval = 0;
	// Presented frames observed since the clock was armed. Reported as frame=K.
	uint64_t frames = 0;

	// Records one presented frame. Returns true on every interval-th frame.
	bool advance()
	{
		if (interval == 0)
			return false;
		frames++;
		return frames % interval == 0;
	}
};
}

// How often each special-case replacement rule changed an outcome since the
// provider was last attached. Each counter is bumped only where its rule alters
// a binding, key, texel or raster decision, never where it is merely evaluated,
// so a rule whose counter stays at zero across the regression suite never fired.
// Renderer-thread only; the summary reads them after the command ring drains.
// Emitted, with the provider's load- and decode-time counters, as the
// "Hi-res rule counters" summary line (see emit_hires_summary for key order).
struct HiresRuleCounters
{
	// Draws flagged for the shading stage's HLE-style ~0-alpha kill.
	uint64_t alpha_kill_draws = 0;
	// Uploaded replacement images with nonzero RGB under a zero-alpha texel.
	uint64_t transparent_rgb_zeroed_textures = 0;
	// Upload-time hits found only by re-keying a CI8 load as CI4.
	uint64_t ci8_as_ci4_hits = 0;
	// Draw slots whose hit was dropped for a tile format/size mismatch.
	uint64_t formatsize_mismatch_drops = 0;
	// Draw slots served native because the view reads past the keyed window.
	uint64_t view_exceeds_key_draws = 0;
	// Draw slots whose hit was dropped by the stale-binding (load address) guard.
	uint64_t stale_binding_drops = 0;
	// Tiles at another TMEM offset invalidated by an overlapping load: hits
	// dropped, and (separately) tracked load origins dropped without a hit.
	uint64_t overlap_binding_drops = 0;
	uint64_t overlap_origin_drops = 0;
	// TLUT loads whose keying palette was read from the texture image base
	// rather than the load tile's offset address (the two differ).
	uint64_t tlut_image_base_loads = 0;
	// Texrects whose native-resolution snap was lifted at upscale > 1, and
	// the bottom-edge round-up, split by whether a snap was present.
	uint64_t texrect_snap_lifts = 0;
	uint64_t texrect_bottom_round_ups = 0;
	uint64_t texrect_bottom_round_ups_unsnapped = 0;
	// Draws sampling a replacement whose empty texels keep the surface.
	uint64_t cutout_surface_draws = 0;
	// Draw slots whose missing load origin was recovered from a peer tile.
	uint64_t load_origin_recoveries = 0;
	// Tiles re-pointed by SetTile that adopted another tile's tracked load.
	uint64_t repoint_origin_adoptions = 0;
	// Draw slots whose source-layout-qualified hit retired with its layout.
	uint64_t source_layout_retires = 0;
	// Draw-time compat lookups by why they ran: the tile had no keyed state,
	// had a miss, or had a hit whose source view re-keyed to a different key.
	// compat_rekey_confirmed counts re-keys that matched the existing hit, so
	// no lookup ran.
	uint64_t compat_lookups_unkeyed = 0;
	uint64_t compat_lookups_after_miss = 0;
	uint64_t compat_lookups_rekey = 0;
	uint64_t compat_rekey_confirmed = 0;
	// Draws with an upload miss on a sampled tile that skipped the compat lane
	// only because no loaded pack has compat or Jabo entries.
	uint64_t compat_gate_skipped_misses = 0;
};

// What a draw's replacement bindings ask of the renderer.
struct HiresDrawBinding
{
	// TEXEL0 is replaced and sampled (or copied): a texrect may drop its
	// native-resolution snap so the replacement rasterizes at full scale.
	bool lifts_tex_rect_snap = false;
	// A replaced texel feeds a src-alpha-over-memory final blend: the shading
	// stage kills ~0-alpha pixels as the HLE renderers packs target do.
	bool alpha_kill = false;
	bool replacements_attached = false;
	// A sampled tile of this draw has a GPU-visible replacement.
	bool samples_replacement = false;
	// The component's counters, for rules decided after prepare_draw returns.
	HiresRuleCounters *rule_counters = nullptr;

	// For any texture rectangle: marks its queued setup so the shading stage
	// reconstructs replacement point samples as rectangle samples.
	void mark_tex_rect(TriangleSetup &queued_setup) const;
	// For a non-flipped texture rectangle: patches its queued setup.
	void lift_tex_rect_snap(TriangleSetup &queued_setup, unsigned upscaling) const;
	// Hi-res bits to add to the draw's normalized static raster state.
	StaticRasterizationFlags raster_flags(const StaticRasterizationState &normalized,
	                                      const ConstantCombinerInputs (&constants)[2]) const;
};

class HiresReplacement
{
public:
	explicit HiresReplacement(TileInfo (&tiles)[Limits::MaxNumTiles])
		: tiles(tiles)
	{
	}

	// Lifetime. init() decides whether the replacement shader path exists at all
	// (descriptor indexing, PARALLEL_RDP_DISABLE_HIRES_SHADER) and allocates the
	// one-slot fallback pool the shaders bind as set 3. flush_and_signal must
	// submit all renderer work; it runs before resident descriptors are rewritten.
	void init(Vulkan::Device *device, bool descriptor_indexing, std::function<void()> flush_and_signal);
	void set_rdram(const uint8_t *host_rdram, size_t size);
	// Attaching (or detaching, with nullptr) resets all bindings and counters.
	// Returns false when resources for the provider are unavailable; the
	// provider is then left detached.
	bool attach(const ReplacementProvider *provider);
	bool attached() const { return replacement_provider != nullptr; }
	bool shader_path() const { return shader_enabled; }
	void begin_frame();

	// Configuration. These survive attach().
	void configure_gpu_budget(unsigned option_mb);
	const HiresResidencyStatus &residency_status() const { return hires_residency_status; }
	void set_gliden64_compat_crc(bool enable);
	void set_filter(unsigned mode);
	// 0 = nearest, 1 = bilinear, 2 = trilinear (see HIRES_FILTER_* in shaders).
	unsigned filter_mode() const { return hires_filter_mode; }

	// Final summary, emitted once at teardown. Its line format is parsed by
	// tools/regression/run_scenarios.py and must stay stable.
	void log_summary() const;
	// Cumulative snapshot of the same counters, labelled with the presented
	// frame count so log parsers can tell it apart from the final summary.
	void log_summary_periodic(uint64_t frame) const;

	// RDP texture-memory events, in command order.
	// SetTile, before the renderer stores the new descriptor.
	void tile_set(unsigned tile, const TileMeta &meta);
	// Start of one TMEM load iteration, before it is decomposed for upload.
	void tmem_load_begin(unsigned tile);
	// The same iteration once decomposed: tmem_address is where it writes TMEM
	// and rdram_pitch the RDRAM row pitch in texels the RDP walks.
	// tmem_offset is the line offset of a load split into 4 KiB iterations.
	void tmem_load(unsigned tile, const LoadTileInfo &info, uint32_t tmem_offset,
	               int32_t tmem_address, int32_t rdram_pitch);

	// A draw sampling tile `tile` (and its TEXEL1 peer) with these raster flags,
	// after the renderer has deduced its texture state. Settles the draw's
	// bindings in the tiles' TileInfo::replacement and marks them used.
	HiresDrawBinding prepare_draw(uint32_t tile, StaticRasterizationFlags raster_flags,
	                              const DepthBlendState &depth_blend);
	// Binds the replacement descriptor set for a rasterization pass.
	void bind(Vulkan::CommandBuffer &cmd) const;

private:
	struct ReplacementTileState;
	void emit_hires_summary(const char *label, bool include_duplicate_diagnostics) const;
	void flush_and_signal() { flush_and_signal_callback(); }
	void touch_bound_tiles();

	bool init_hires_resources(unsigned requested_capacity);
	enum class HiresNativeChecksumDetailClass : uint8_t
	{
		Exact = 0,
		IdentityAssisted,
		GenericFallback
	};
	struct HiresKey;
	template <typename Decode>
	bool load_hires_descriptor(const HiresKey &key, ReplacementMeta &meta, Decode decode);
	void evict_hires_slot(uint32_t slot);
	void clear_hires_images();
	Vulkan::ImageHandle create_hires_replacement_image_with_mips(const ReplacementImage &replacement);
	bool resolve_hires_compat_replacement_descriptor(uint64_t checksum64, uint16_t formatsize, ReplacementMeta &meta);
	bool resolve_hires_compat_replacement_descriptor(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64, ReplacementMeta &meta, const ReplacementPalette *palette = nullptr);
	bool resolve_hires_jabo_replacement_descriptor(uint64_t key_lo, uint64_t key_hi, uint16_t formatsize, ReplacementMeta &meta);
	bool resolve_hires_jabo_draw_candidate(const TileMeta &tile_meta, const detail::HiresCompatView &view,
	                                       uint64_t rice_checksum64, bool tlut_ia16, ReplacementResolution *out,
	                                       uint64_t *source_hash);
	uint64_t hires_jabo_binding_hash(const TileMeta &tile_meta, const detail::HiresCompatView &view, bool tlut_ia16) const;
	bool resolve_hires_native_checksum_replacement_descriptor(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64, ReplacementMeta &meta, HiresNativeChecksumDetailClass detail_class = HiresNativeChecksumDetailClass::Exact);
	bool resolve_hires_sampled_replacement_descriptor(uint32_t sampled_fmt,
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
	                                                 ReplacementMeta &meta);
	bool resolve_hires_provider_resolution_descriptor(const ::RDP::ReplacementResolution &resolution,
	                                                  uint16_t formatsize,
	                                                  uint32_t orig_w,
	                                                  uint32_t orig_h,
	                                                  ReplacementMeta &meta,
	                                                  uint64_t *resolved_selector_checksum64 = nullptr,
	                                                  const ReplacementPalette *palette = nullptr);
	void apply_hires_tile_binding(unsigned tile, const ReplacementTileState &state);
	void clear_hires_tile_binding(unsigned tile);

	TileInfo (&tiles)[Limits::MaxNumTiles];
	Vulkan::Device *device = nullptr;
	std::function<void()> flush_and_signal_callback;
	const uint8_t *cpu_rdram = nullptr;
	size_t rdram_size = 0;
	bool shader_enabled = false;
	const ReplacementProvider *replacement_provider = nullptr;
	bool hires_gliden64_compat_crc_enabled = false;
	unsigned hires_filter_mode = 2;

	struct ReplacementTileState
	{
		uint64_t checksum64 = 0;
		uint64_t upload_checksum64 = 0;
		uint64_t selector_checksum64 = 0;
		uint16_t formatsize = 0;
		uint16_t orig_w = 0;
		uint16_t orig_h = 0;
		// Dimensions the checksum key was computed over (orig_w/orig_h get
		// rebased to the rendering view at draw time; these do not).
		uint16_t key_w = 0;
		uint16_t key_h = 0;
		uint16_t repl_w = 0;
		uint16_t repl_h = 0;
		uint32_t vk_image_index = 0xffffffffu;
		// RDRAM texture-image base the key was computed against. Serving is
		// only legitimate while the tile's TMEM region still holds a load
		// from this address; a differing hires_rdram_load_addr means the
		// binding describes overwritten texels (stale-binding guard).
		uint32_t keyed_rdram_addr = 0;
		// Nonzero for a Jabo binding: hires_jabo_source_hash of the texels and
		// palette its MD5 was computed from. The binding is kept only while
		// that hash still matches, since the Rice key does not cover them all.
		uint64_t jabo_source_hash = 0;
		uint64_t indexed_palette_hash = 0;
		detail::HiresCI4PaletteBinding ci4_palette;
		// The key or live admission depended on qualified LoadTile source rows.
		bool source_layout_binding = false;
		bool valid = false;
		bool hit = false;
	};
	ReplacementTileState replacement_tiles[Limits::MaxNumTiles] = {};
	uint32_t hires_rdram_load_addr[Limits::MaxNumTiles] = {};
	detail::HiresLoadTileSource hires_load_tile_source[Limits::MaxNumTiles] = {};
	uint8_t tlut_shadow[512] = {};
	uint8_t tlut_tmem_shadow[2048] = {};
	bool tlut_shadow_valid = false;
	uint64_t hires_lookup_total = 0;
	uint64_t hires_lookup_hits = 0;
	uint64_t hires_lookup_misses = 0;
	uint64_t hires_descriptor_sampled_resolutions = 0;
	uint64_t hires_descriptor_sampled_family_singleton_resolutions = 0;
	uint64_t hires_descriptor_sampled_ordered_surface_singleton_resolutions = 0;
	uint64_t hires_descriptor_sampled_exact_selector_resolutions = 0;
	uint64_t hires_descriptor_native_checksum_resolutions = 0;
	uint64_t hires_descriptor_native_checksum_exact_resolutions = 0;
	uint64_t hires_descriptor_native_checksum_identity_assisted_resolutions = 0;
	uint64_t hires_descriptor_native_checksum_generic_fallback_resolutions = 0;
	uint64_t hires_descriptor_compat_resolutions = 0;
	detail::HiresStrongHashMemo hires_strong_memo[Limits::MaxNumTiles];
	std::vector<uint8_t> hires_strong_scratch;
	uint64_t hires_strong_draw_hits = 0;
	uint64_t hires_compat_draw_time_hits = 0;
	uint64_t hires_compat_draw_time_ci_attempts = 0;
	uint64_t hires_compat_draw_time_ci_hits = 0;
	// Jabo MD5 lane: runs after a draw-time Rice miss, only for packages with
	// Jabo records. The memo maps draw views to MD5s and is bounded by clearing.
	bool hires_jabo_lane_enabled = false;
	uint64_t hires_jabo_draw_hits = 0;
	uint64_t hires_jabo_draw_misses = 0;
	uint64_t hires_jabo_md5_computations = 0;
	uint64_t hires_jabo_memo_hits = 0;
	HiresRuleCounters rule_counters;
	std::unordered_map<detail::HiresJaboMemoKey, std::array<uint8_t, 16>, detail::HiresJaboMemoKeyHash> hires_jabo_memo;
	std::vector<uint8_t> hires_jabo_scratch;
	enum class HiresKeySource : uint8_t
	{
		Generic = 0,
		NativeSampled = 1,
		Compat = 2,
		NativeChecksum = 3,
		GenericNative = 4,
		GenericCompat = 5,
		Jabo = 6
	};

	struct HiresKey
	{
		uint64_t checksum64 = 0;
		uint16_t formatsize = 0;
		uint64_t selector_checksum64 = 0;
		uint64_t indexed_palette_hash = 0;
		HiresKeySource source = HiresKeySource::Generic;
		uint32_t sampled_fmt = 0;
		uint32_t sampled_siz = 0;
		uint32_t sampled_tex_offset = 0;
		uint32_t sampled_stride = 0;
		uint32_t sampled_width = 0;
		uint32_t sampled_height = 0;
		uint32_t sampled_low32 = 0;
		uint32_t sampled_palette_crc = 0;

		bool operator==(const HiresKey &other) const
		{
			return checksum64 == other.checksum64 && formatsize == other.formatsize &&
			       selector_checksum64 == other.selector_checksum64 &&
			       indexed_palette_hash == other.indexed_palette_hash &&
			       source == other.source &&
			       sampled_fmt == other.sampled_fmt &&
			       sampled_siz == other.sampled_siz &&
			       sampled_tex_offset == other.sampled_tex_offset &&
			       sampled_stride == other.sampled_stride &&
			       sampled_width == other.sampled_width &&
			       sampled_height == other.sampled_height &&
			       sampled_low32 == other.sampled_low32 &&
			       sampled_palette_crc == other.sampled_palette_crc;
		}
	};

	struct HiresKeyHasher
	{
		size_t operator()(const HiresKey &key) const
		{
			return size_t(key.checksum64 ^ (uint64_t(key.formatsize) << 48) ^
			              key.selector_checksum64 ^ key.indexed_palette_hash ^ (uint64_t(key.source) << 60) ^
			              (uint64_t(key.sampled_fmt) << 2) ^
			              (uint64_t(key.sampled_siz) << 6) ^
			              (uint64_t(key.sampled_tex_offset) << 10) ^
			              (uint64_t(key.sampled_stride) << 14) ^
			              (uint64_t(key.sampled_width) << 18) ^
			              (uint64_t(key.sampled_height) << 22) ^
			              (uint64_t(key.sampled_low32) << 26) ^
			              (uint64_t(key.sampled_palette_crc) << 30));
		}
	};

	struct HiresResidentImage
	{
		Vulkan::ImageHandle image;
		HiresKey key;
		uint16_t repl_w = 0;
		uint16_t repl_h = 0;
	};

	struct HiresResources
	{
		Vulkan::BindlessDescriptorPoolHandle bindless_pool;
		Vulkan::ImageHandle fallback_image;
		std::unordered_map<HiresKey, uint32_t, HiresKeyHasher> resident_images;
		std::vector<HiresResidentImage> images;
		std::unordered_set<HiresKey, HiresKeyHasher> failed;
		std::unordered_set<HiresKey, HiresKeyHasher> refused_saturated, refused_no_slot;
		detail::HiresResidencyPolicy residency;
		unsigned capacity = 0;
	} hires_resources;
	// Configuration survives provider/cache resets.
	HiresResidencyStatus hires_residency_status;
	uint64_t hires_frame = 0;
	unsigned hires_heap_index = 0;
	bool hires_memory_budget_supported = false;

};

HiresPackStatus configure_hires_packages(Renderer &renderer, ReplacementProvider &provider, bool enable,
                                         const std::vector<std::string> &cache_paths, unsigned budget_mb);
}
