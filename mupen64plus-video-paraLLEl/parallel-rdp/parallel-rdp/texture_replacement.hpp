// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RDP
{
// A draw's selected TMEM TLUT bank, in native 16-bit words. count=0 is invalid.
struct ReplacementPalette
{
	std::array<uint16_t, 256> words = {};
	uint32_t count = 0;
	bool ia16 = false;

	static ReplacementPalette from_tmem(const uint8_t *tmem, size_t size, bool ci4, unsigned bank, bool ia16);
	uint64_t hash() const;
};

struct ReplacementMeta
{
	uint32_t repl_w = 0;
	uint32_t repl_h = 0;
	uint32_t orig_w = 0;
	uint32_t orig_h = 0;
	uint32_t vk_image_index = 0xffffffffu;
	bool has_mips = false;
	bool srgb = false;
	bool indexed = false;
};

struct ReplacementImage
{
	ReplacementMeta meta;
	std::vector<uint8_t> rgba8;
};

struct ReplacementProviderStats
{
	uint32_t entry_count = 0;
	uint32_t native_sampled_entry_count = 0;
	uint32_t compat_entry_count = 0;
	uint32_t sampled_index_count = 0;
	uint32_t sampled_duplicate_key_count = 0;
	uint32_t sampled_duplicate_entry_count = 0;
	uint32_t sampled_family_count = 0;
	uint32_t source_phrb_entry_count = 0;
	uint32_t jabo_entry_count = 0;
	// Special-case rule counters (see RDP::HiresRuleCounters). Decode counts
	// are per decoded texture, so a re-upload after eviction counts again.
	// Sampled keys whose duplicate tie-break kept a later entry over the first.
	uint32_t sampled_duplicate_reordered_keys = 0;
	// Jabo decodes drawn as I whose >= 254 alpha was replaced by intensity.
	uint64_t jabo_intensity_alpha_textures = 0;
	// Compat decodes found only through the palette-less CI (_ci.bmp) key.
	uint64_t compat_ci_live_palette_textures = 0;
};

struct SampledDuplicateDiagnostics
{
	uint32_t sampled_fmt = 0;
	uint32_t sampled_siz = 0;
	uint32_t sampled_tex_offset = 0;
	uint32_t sampled_stride = 0;
	uint32_t sampled_width = 0;
	uint32_t sampled_height = 0;
	uint32_t sampled_low32 = 0;
	uint32_t sampled_palette_crc = 0;
	uint16_t formatsize = 0;
	uint64_t selector_checksum64 = 0;
	uint32_t total_entry_count = 0;
	uint32_t duplicate_entry_count = 0;
	uint64_t active_checksum64 = 0;
	uint32_t active_repl_w = 0;
	uint32_t active_repl_h = 0;
	std::string active_source_path;
	std::string active_policy_key;
	std::string active_replacement_id;
	std::string active_sampled_object_id;
};

struct NativeSampledIdentity
{
	bool valid = false;
	uint32_t sampled_fmt = 0;
	uint32_t sampled_siz = 0;
	uint32_t sampled_tex_offset = 0;
	uint32_t sampled_stride = 0;
	uint32_t sampled_width = 0;
	uint32_t sampled_height = 0;
	uint32_t sampled_low32 = 0;
	uint32_t sampled_palette_crc = 0;
	uint16_t formatsize = 0;
	uint64_t selector_checksum64 = 0;
};

enum class ResolvedEntrySourceClass
{
	Unknown = 0,
	Native,
	Compat,
};

enum class ReplacementResolutionKind
{
	None = 0,
	SampledExactSelector,
	SampledFamilySingleton,
	ExactNativeSampled,
	GenericNativeIdentity,
	GenericNativeChecksum,
	GenericCompat,
	JaboMd5,
};

struct ReplacementResolution
{
	bool available = false;
	ReplacementResolutionKind kind = ReplacementResolutionKind::None;
	ReplacementMeta meta = {};
	NativeSampledIdentity identity = {};
	ResolvedEntrySourceClass source_class = ResolvedEntrySourceClass::Unknown;
	uint64_t resolved_checksum64 = 0;
	uint64_t resolved_selector_checksum64 = 0;
	bool ordered_surface_singleton = false;
};

enum class ReplacementLoadStatus { Loaded, NotFound, UnsupportedVersion, Invalid };

// Counts unique runtime keys, including palette aliases, in separate Rice and
// Jabo namespaces. Overlap is diagnostic only; lookup indices are unchanged.
struct ReplacementPackageStats
{
	std::string path;
	size_t records = 0;
	size_t hidden_by_later = 0;
	std::string hidden_by;
	bool multiple_hiders = false;
	bool mostly_hidden() const { return hidden_by_later > records / 2; }
};

class ReplacementProvider
{
public:
	bool enabled() const;
	void set_enabled(bool enable);
	// Append lets a game's standalone package and package directory share one provider.
	bool load_cache_dir(const std::string &path, bool append = false);
	ReplacementLoadStatus load_error() const { return load_error_; }
	const std::string &rejected_path() const { return rejected_path_; }
	const std::string &loaded_path() const { return loaded_path_; }
	std::vector<ReplacementPackageStats> package_stats() const;
	bool has_phrb_compat_entries() const;
	bool has_jabo_entries() const;
	// Project64/Jabo packs key replacements by a 16-byte MD5 (rdp_hires_jabo_key.hpp).
	// A hit reports the key's little-endian halves as resolved checksum/selector.
	bool resolve_jabo_candidate(const uint8_t md5[16], ReplacementResolution *out) const;
	// Jabo filenames do not encode native format. Only opaque I-format
	// repaints (all alpha >= 254) derive opacity from intensity. Lower alpha is retained.
	bool decode_rgba8_jabo(uint64_t key_lo, uint64_t key_hi, uint32_t native_format, ReplacementImage *out) const;
	bool lookup(uint64_t checksum64, uint16_t formatsize, ReplacementMeta *out) const;
	bool lookup_with_selector(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64, ReplacementMeta *out) const;
	bool resolve_upload_candidate(uint16_t formatsize,
	                              uint32_t sampled_fmt,
	                              uint32_t sampled_siz,
	                              uint32_t sampled_tex_offset,
	                              uint32_t sampled_stride,
	                              uint32_t sampled_width,
	                              uint32_t sampled_height,
	                              uint32_t sampled_low32,
	                              uint32_t palette_crc,
	                              uint64_t selector_checksum64,
	                              ReplacementResolution *out) const;
	bool lookup_sampled_with_selector(uint32_t sampled_fmt,
	                                  uint32_t sampled_siz,
	                                  uint32_t sampled_tex_offset,
	                                  uint32_t sampled_stride,
	                                  uint32_t sampled_width,
	                                  uint32_t sampled_height,
	                                  uint32_t sampled_low32,
	                                  uint32_t palette_crc,
	                                  uint16_t formatsize,
	                                  uint64_t selector_checksum64,
	                                  ReplacementMeta *out,
	                                  uint64_t *resolved_checksum64 = nullptr) const;
	bool lookup_sampled_family_unique(uint32_t sampled_fmt,
	                                  uint32_t sampled_siz,
	                                  uint32_t sampled_tex_offset,
	                                  uint32_t sampled_stride,
	                                  uint32_t sampled_width,
	                                  uint32_t sampled_height,
	                                  uint32_t sampled_low32,
	                                  uint32_t palette_crc,
	                                  uint16_t formatsize,
	                                  ReplacementMeta *out,
	                                  uint64_t *resolved_checksum64 = nullptr,
	                                  uint64_t *resolved_selector_checksum64 = nullptr) const;
	bool lookup_sampled_family_singleton(uint32_t sampled_fmt,
	                                     uint32_t sampled_siz,
	                                     uint32_t sampled_tex_offset,
	                                     uint32_t sampled_stride,
	                                     uint32_t sampled_width,
	                                     uint32_t sampled_height,
	                                     uint32_t sampled_low32,
	                                     uint32_t palette_crc,
	                                     uint16_t formatsize,
	                                     ReplacementMeta *out,
	                                     uint64_t *resolved_checksum64 = nullptr,
	                                     uint64_t *resolved_selector_checksum64 = nullptr,
	                                     bool *resolved_ordered_surface_singleton = nullptr) const;
	bool decode_sampled_rgba8_with_selector(uint32_t sampled_fmt,
	                                        uint32_t sampled_siz,
	                                        uint32_t sampled_tex_offset,
	                                        uint32_t sampled_stride,
	                                        uint32_t sampled_width,
	                                        uint32_t sampled_height,
	                                        uint32_t sampled_low32,
	                                        uint32_t palette_crc,
	                                        uint16_t formatsize,
	                                        uint64_t selector_checksum64,
	                                        ReplacementImage *out,
	                                        uint64_t *resolved_checksum64 = nullptr) const;
	static uint64_t ordered_surface_slot_selector_checksum64(uint32_t slot_index);
	bool decode_rgba8(uint64_t checksum64, uint16_t formatsize, ReplacementImage *out) const;
	bool decode_rgba8_with_selector(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64, ReplacementImage *out) const;
	bool decode_rgba8_native_with_selector(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64, ReplacementImage *out) const;
	bool decode_rgba8_compat_with_selector(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64, ReplacementImage *out, const ReplacementPalette *palette = nullptr) const;
	ReplacementProviderStats get_stats() const;
	std::vector<SampledDuplicateDiagnostics> get_sampled_duplicate_diagnostics(size_t limit = 0) const;
	void clear();
	size_t entry_count() const;

private:
	struct Entry
	{
		std::string source_path;
		std::string phrb_policy_key;
		std::string phrb_replacement_id;
		std::string phrb_sampled_object_id;
		uint64_t checksum64 = 0;
		uint64_t data_offset = 0;
		uint32_t data_size = 0;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t format = 0;
		uint16_t texture_format = 0;
		uint16_t pixel_type = 0;
		uint16_t formatsize = 0;
		uint64_t selector_checksum64 = 0;
		uint32_t sampled_fmt = 0;
		uint32_t sampled_siz = 0;
		uint32_t sampled_tex_offset = 0;
		uint32_t sampled_stride = 0;
		uint32_t sampled_width = 0;
		uint32_t sampled_height = 0;
		uint32_t sampled_low32 = 0;
		uint32_t sampled_palette_crc = 0;
		uint32_t sampled_entry_pcrc = 0;
		uint32_t sampled_sparse_pcrc = 0;
		bool has_native_sampled_identity = false;
		bool is_runtime_family_compat = false;
		// Jabo entries keep their MD5 halves in checksum64/selector_checksum64
		// and live only in jabo_index_.
		bool is_jabo = false;
		bool is_hires = false;
		bool inline_blob = false;
		std::vector<uint8_t> blob;
	};

	struct SampledLookupKey
	{
		uint32_t sampled_fmt = 0;
		uint32_t sampled_siz = 0;
		uint32_t sampled_tex_offset = 0;
		uint32_t sampled_stride = 0;
		uint32_t sampled_width = 0;
		uint32_t sampled_height = 0;
		uint32_t sampled_low32 = 0;
		uint32_t sampled_palette_crc = 0;
		uint16_t formatsize = 0;
		uint64_t selector_checksum64 = 0;

		bool operator==(const SampledLookupKey &other) const
		{
			return sampled_fmt == other.sampled_fmt &&
			       sampled_siz == other.sampled_siz &&
			       sampled_tex_offset == other.sampled_tex_offset &&
			       sampled_stride == other.sampled_stride &&
			       sampled_width == other.sampled_width &&
			       sampled_height == other.sampled_height &&
			       sampled_low32 == other.sampled_low32 &&
			       sampled_palette_crc == other.sampled_palette_crc &&
			       formatsize == other.formatsize &&
			       selector_checksum64 == other.selector_checksum64;
		}
	};

	struct SampledLookupKeyHash
	{
		size_t operator()(const SampledLookupKey &key) const;
	};

	struct SampledFamilyLookupKey
	{
		uint32_t sampled_fmt = 0;
		uint32_t sampled_siz = 0;
		uint32_t sampled_tex_offset = 0;
		uint32_t sampled_stride = 0;
		uint32_t sampled_width = 0;
		uint32_t sampled_height = 0;
		uint32_t sampled_low32 = 0;
		uint32_t sampled_palette_crc = 0;
		uint16_t formatsize = 0;

		bool operator==(const SampledFamilyLookupKey &other) const
		{
			return sampled_fmt == other.sampled_fmt &&
			       sampled_siz == other.sampled_siz &&
			       sampled_tex_offset == other.sampled_tex_offset &&
			       sampled_stride == other.sampled_stride &&
			       sampled_width == other.sampled_width &&
			       sampled_height == other.sampled_height &&
			       sampled_low32 == other.sampled_low32 &&
			       sampled_palette_crc == other.sampled_palette_crc &&
			       formatsize == other.formatsize;
		}
	};

	struct SampledFamilyLookupKeyHash
	{
		size_t operator()(const SampledFamilyLookupKey &key) const;
	};

	// A family resolves as a singleton only while every entry shares one selector.
	struct SampledFamilySelectors
	{
		uint64_t selector_checksum64 = 0;
		bool mixed = false;
	};

	// An aggregate (no member initializers), so C++11 can brace-initialize it.
	struct ChecksumLookupKey
	{
		uint64_t checksum64;
		uint64_t selector_checksum64;
		uint16_t formatsize;

		bool operator==(const ChecksumLookupKey &other) const
		{
			return checksum64 == other.checksum64 && selector_checksum64 == other.selector_checksum64 &&
			       formatsize == other.formatsize;
		}
	};

	struct ChecksumLookupKeyHash
	{
		size_t operator()(const ChecksumLookupKey &key) const;
	};

	// Both maps hold the most recently added entry, which is the one a lookup returns.
	struct ChecksumIndex
	{
		std::unordered_map<uint64_t, size_t> latest;
		std::unordered_map<ChecksumLookupKey, size_t, ChecksumLookupKeyHash> exact;
		void clear() { latest.clear(); exact.clear(); }
	};

	using JaboKey = std::array<uint64_t, 2>;
	struct JaboKeyHash
	{
		size_t operator()(const JaboKey &key) const
		{
			return size_t(key[0] ^ (key[1] * 0x9e3779b97f4a7c15ull));
		}
	};

	const Entry *find_jabo_entry(uint64_t key_lo, uint64_t key_hi) const;

	const Entry *find_any_entry(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64) const;
	const Entry *find_native_entry(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64) const;
	const Entry *find_compat_entry(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64,
	                               bool *live_palette_fallback = nullptr) const;
	const Entry *find_indexed_entry(const ChecksumIndex &index,
	                               uint64_t checksum64,
	                               uint16_t formatsize,
	                               uint64_t selector_checksum64) const;
	const Entry *find_sampled_entry(uint32_t sampled_fmt,
	                                uint32_t sampled_siz,
	                                uint32_t sampled_tex_offset,
	                                uint32_t sampled_stride,
	                                uint32_t sampled_width,
	                                uint32_t sampled_height,
	                                uint32_t sampled_low32,
	                                uint32_t palette_crc,
	                                uint16_t formatsize,
	                                uint64_t selector_checksum64) const;
	const Entry *find_unique_sampled_family_entry(uint32_t sampled_fmt,
	                                              uint32_t sampled_siz,
	                                              uint32_t sampled_tex_offset,
	                                              uint32_t sampled_stride,
	                                              uint32_t sampled_width,
	                                              uint32_t sampled_height,
	                                              uint32_t sampled_low32,
	                                              uint32_t palette_crc,
	                                              uint16_t formatsize,
	                                              uint64_t *resolved_selector_checksum64 = nullptr) const;
	const Entry *find_singleton_sampled_family_entry(uint32_t sampled_fmt,
	                                                 uint32_t sampled_siz,
	                                                 uint32_t sampled_tex_offset,
	                                                 uint32_t sampled_stride,
	                                                 uint32_t sampled_width,
	                                                 uint32_t sampled_height,
	                                                 uint32_t sampled_low32,
	                                                 uint32_t palette_crc,
	                                                 uint16_t formatsize,
	                                                 bool allow_ordered_surface_selectors,
	                                                 uint64_t *resolved_selector_checksum64 = nullptr,
	                                                 bool *resolved_ordered_surface_singleton = nullptr) const;
	ReplacementLoadStatus load_phrb(const std::string &path);
	bool read_blob(const Entry &entry, std::vector<uint8_t> &blob) const;
	bool decode_entry_rgba8(const Entry &entry, ReplacementImage *out, const ReplacementPalette *palette = nullptr) const;
	void populate_meta_from_entry(const Entry &entry, ReplacementMeta *out) const;
	void populate_identity_from_entry(const Entry &entry, NativeSampledIdentity *out) const;
	bool populate_resolution_from_entry(const Entry *entry,
	                                    ReplacementResolutionKind kind,
	                                    bool ordered_surface_singleton,
	                                    ReplacementResolution *out) const;
	static bool decode_pixels_rgba8(const Entry &entry, const std::vector<uint8_t> &pixel_data, std::vector<uint8_t> &rgba8);
	static bool decompress_if_needed(const Entry &entry, const std::vector<uint8_t> &blob, std::vector<uint8_t> &pixel_data);
	static uint32_t expected_decoded_size(const Entry &entry);
	static SampledLookupKey make_sampled_lookup_key(uint32_t sampled_fmt,
	                                                uint32_t sampled_siz,
	                                                uint32_t sampled_tex_offset,
	                                                uint32_t sampled_stride,
	                                                uint32_t sampled_width,
	                                                uint32_t sampled_height,
	                                                uint32_t sampled_low32,
	                                                uint32_t sampled_palette_crc,
	                                                uint16_t formatsize,
	                                                uint64_t selector_checksum64);
	static SampledFamilyLookupKey make_sampled_family_lookup_key(uint32_t sampled_fmt,
	                                                             uint32_t sampled_siz,
	                                                             uint32_t sampled_tex_offset,
	                                                             uint32_t sampled_stride,
	                                                             uint32_t sampled_width,
	                                                             uint32_t sampled_height,
	                                                             uint32_t sampled_low32,
	                                                             uint32_t sampled_palette_crc,
	                                                             uint16_t formatsize);
	static bool prefer_sampled_duplicate_candidate(const Entry &active, const Entry &candidate);
	void add_entry(Entry &&entry);

	bool enabled_ = false;
	std::string cache_dir_;
	std::vector<Entry> entries_;
	ChecksumIndex native_checksum_index_;
	ChecksumIndex compat_checksum_index_;
	std::unordered_map<SampledLookupKey, size_t, SampledLookupKeyHash> sampled_index_;
	std::unordered_map<SampledLookupKey, uint32_t, SampledLookupKeyHash> sampled_duplicate_index_;
	std::unordered_map<SampledFamilyLookupKey, SampledFamilySelectors, SampledFamilyLookupKeyHash> sampled_family_index_;
	std::unordered_map<JaboKey, size_t, JaboKeyHash> jabo_index_;
	std::unordered_set<SampledLookupKey, SampledLookupKeyHash> sampled_duplicate_reordered_;
	// Decode-time rule counters. Decodes run on the renderer thread only.
	mutable uint64_t jabo_intensity_alpha_decodes_ = 0;
	mutable uint64_t compat_ci_live_palette_decodes_ = 0;
	ReplacementLoadStatus load_error_ = ReplacementLoadStatus::NotFound;
	std::string rejected_path_, loaded_path_;
};
}
