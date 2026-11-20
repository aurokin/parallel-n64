// SPDX-License-Identifier: MIT

#include "texture_replacement.hpp"
#include "logging.hpp"
#include <algorithm>
#include <functional>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <map>
#include <tuple>
#include <sys/stat.h>
#include <zlib.h>

namespace RDP
{
namespace
{
constexpr uint32_t GL_TEXFMT_GZ = 0x80000000u;

constexpr uint16_t GL_COLOR_INDEX = 0x1900;
constexpr uint16_t GL_RGB = 0x1907;
constexpr uint16_t GL_RGBA = 0x1908;
constexpr uint16_t GL_LUMINANCE = 0x1909;

constexpr uint16_t GL_UNSIGNED_BYTE = 0x1401;
constexpr uint16_t GL_UNSIGNED_SHORT_4_4_4_4 = 0x8033;
constexpr uint16_t GL_UNSIGNED_SHORT_5_5_5_1 = 0x8034;
constexpr uint16_t GL_UNSIGNED_SHORT_5_6_5 = 0x8363;

constexpr uint32_t GL_RGB8 = 0x8051;
constexpr uint32_t GL_RGBA8 = 0x8058;
constexpr uint64_t ORDERED_SURFACE_SELECTOR_TAG = 0x5352464300000000ull;
constexpr uint64_t ORDERED_SURFACE_SELECTOR_MASK = 0xffffffff00000000ull;

inline bool is_ordered_surface_selector(uint64_t selector_checksum64)
{
	return (selector_checksum64 & ORDERED_SURFACE_SELECTOR_MASK) == ORDERED_SURFACE_SELECTOR_TAG;
}

inline uint64_t compose_hires_checksum64(uint32_t texture_crc, uint32_t palette_crc)
{
	return (uint64_t(palette_crc) << 32) | uint64_t(texture_crc);
}



#pragma pack(push, 1)

struct PHRBHeader
{
	char magic[4] = {};
	uint32_t version = 0;
	uint32_t record_count = 0;
	uint32_t asset_count = 0;
	uint32_t record_table_offset = 0;
	uint32_t asset_table_offset = 0;
	uint32_t string_table_offset = 0;
	uint32_t blob_offset = 0;
};

struct PHRBRecordV4
{
	uint32_t policy_key_offset = 0;
	uint32_t sampled_object_id_offset = 0;
	uint32_t record_flags = 0;
	uint32_t fmt = 0;
	uint32_t siz = 0;
	uint32_t tex_offset = 0;
	uint32_t stride = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t formatsize = 0;
	uint32_t sampled_low32 = 0;
	uint32_t sampled_entry_pcrc = 0;
	uint32_t sampled_sparse_pcrc = 0;
	uint32_t asset_candidate_count = 0;
};

// Version 8 appends a record key: Rice (the V4 identity fields) or a Jabo MD5.
struct PHRBRecordV8
{
	PHRBRecordV4 v4;
	uint32_t key_kind = 0;
	uint8_t key[16] = {};
};

constexpr uint32_t PHRB_RECORD_FLAG_RUNTIME_READY = 1u << 0;
constexpr uint32_t PHRB_KEY_KIND_RICE = 0;
constexpr uint32_t PHRB_KEY_KIND_JABO_MD5 = 1;

struct PHRBAssetV7
{
	uint32_t record_index = 0;
	uint32_t replacement_id_offset = 0;
	uint32_t legacy_source_path_offset = 0;
	uint32_t rgba_rel_path_offset = 0;
	uint32_t variant_group_id_offset = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t format = 0;
	uint32_t texture_format = 0;
	uint32_t pixel_type = 0;
	uint32_t legacy_formatsize = 0;
	uint64_t selector_checksum64 = 0;
	uint64_t legacy_checksum64 = 0;
	uint64_t rgba_blob_offset = 0;
	uint64_t rgba_blob_size = 0;
};

#pragma pack(pop)

static_assert(sizeof(PHRBHeader) == 32, "PHRBHeader must stay packed on disk.");
static_assert(sizeof(PHRBRecordV4) == 56, "PHRBRecordV4 must stay packed on disk.");
static_assert(sizeof(PHRBRecordV8) == 76, "PHRBRecordV8 must stay packed on disk.");
static_assert(sizeof(PHRBAssetV7) == 76, "PHRBAssetV7 must stay packed on disk.");

inline bool phrb_magic_ok(const char magic[4])
{
	return magic[0] == 'P' && magic[1] == 'H' && magic[2] == 'R' && magic[3] == 'B';
}

template <typename T>
inline bool read_pod_from_blob(const std::vector<uint8_t> &blob, size_t offset, T &value)
{
	if (offset + sizeof(T) > blob.size())
		return false;
	std::memcpy(&value, blob.data() + offset, sizeof(T));
	return true;
}

inline bool read_c_string_from_blob(const uint8_t *data, size_t size, uint32_t offset, std::string &value)
{
	if (!data || offset >= size)
		return false;
	const char *str = reinterpret_cast<const char *>(data + offset);
	size_t len = 0;
	while (offset + len < size && str[len] != '\0')
		len++;
	if (offset + len >= size)
		return false;
	value.assign(str, len);
	return true;
}

template <typename T>
inline bool read_exact(std::ifstream &file, T &value)
{
	file.read(reinterpret_cast<char *>(&value), sizeof(value));
	return file.good();
}

inline bool read_exact(std::ifstream &file, std::vector<uint8_t> &blob)
{
	if (blob.empty())
		return true;
	file.read(reinterpret_cast<char *>(blob.data()), static_cast<std::streamsize>(blob.size()));
	return file.good();
}

inline bool has_suffix(const std::string &name, const char *suffix)
{
	const size_t name_len = name.size();
	const size_t suffix_len = std::strlen(suffix);
	if (name_len < suffix_len)
		return false;

	for (size_t i = 0; i < suffix_len; i++)
	{
		char a = name[name_len - suffix_len + i];
		char b = suffix[i];
		if (a >= 'A' && a <= 'Z')
			a = char(a - 'A' + 'a');
		if (b >= 'A' && b <= 'Z')
			b = char(b - 'A' + 'a');
		if (a != b)
			return false;
	}

	return true;
}

inline bool is_phrb_cache_path(const std::string &name)
{
	return has_suffix(name, ".phrb");
}

inline uint8_t expand_4_to_8(uint32_t v)
{
	return static_cast<uint8_t>((v << 4) | v);
}

inline uint8_t expand_5_to_8(uint32_t v)
{
	return static_cast<uint8_t>((v << 3) | (v >> 2));
}

inline uint8_t expand_6_to_8(uint32_t v)
{
	return static_cast<uint8_t>((v << 2) | (v >> 4));
}

template <typename T>
inline void push_unique(std::vector<T> &values, const T &value)
{
	for (const auto &existing : values)
		if (existing == value)
			return;
	values.push_back(value);
}

template <typename T>
inline void hash_combine(size_t &seed, const T &value)
{
	seed ^= std::hash<T>{}(value) + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
}
}

bool ReplacementProvider::enabled() const
{
	return enabled_;
}

void ReplacementProvider::set_enabled(bool enable)
{
	enabled_ = enable;
}

size_t ReplacementProvider::SampledLookupKeyHash::operator()(const SampledLookupKey &key) const
{
	size_t seed = 0;
	hash_combine(seed, key.sampled_fmt);
	hash_combine(seed, key.sampled_siz);
	hash_combine(seed, key.sampled_tex_offset);
	hash_combine(seed, key.sampled_stride);
	hash_combine(seed, key.sampled_width);
	hash_combine(seed, key.sampled_height);
	hash_combine(seed, key.sampled_low32);
	hash_combine(seed, key.sampled_palette_crc);
	hash_combine(seed, key.formatsize);
	hash_combine(seed, key.selector_checksum64);
	return seed;
}

size_t ReplacementProvider::SampledFamilyLookupKeyHash::operator()(const SampledFamilyLookupKey &key) const
{
	size_t seed = 0;
	hash_combine(seed, key.sampled_fmt);
	hash_combine(seed, key.sampled_siz);
	hash_combine(seed, key.sampled_tex_offset);
	hash_combine(seed, key.sampled_stride);
	hash_combine(seed, key.sampled_width);
	hash_combine(seed, key.sampled_height);
	hash_combine(seed, key.sampled_low32);
	hash_combine(seed, key.sampled_palette_crc);
	hash_combine(seed, key.formatsize);
	return seed;
}

ReplacementProvider::SampledLookupKey ReplacementProvider::make_sampled_lookup_key(uint32_t sampled_fmt,
                                                                                    uint32_t sampled_siz,
                                                                                    uint32_t sampled_tex_offset,
                                                                                    uint32_t sampled_stride,
                                                                                    uint32_t sampled_width,
                                                                                    uint32_t sampled_height,
                                                                                    uint32_t sampled_low32,
                                                                                    uint32_t sampled_palette_crc,
                                                                                    uint16_t formatsize,
                                                                                    uint64_t selector_checksum64)
{
	SampledLookupKey key = {};
	key.sampled_fmt = sampled_fmt;
	key.sampled_siz = sampled_siz;
	key.sampled_tex_offset = sampled_tex_offset;
	key.sampled_stride = sampled_stride;
	key.sampled_width = sampled_width;
	key.sampled_height = sampled_height;
	key.sampled_low32 = sampled_low32;
	key.sampled_palette_crc = sampled_palette_crc;
	key.formatsize = formatsize;
	key.selector_checksum64 = selector_checksum64;
	return key;
}

ReplacementProvider::SampledFamilyLookupKey ReplacementProvider::make_sampled_family_lookup_key(uint32_t sampled_fmt,
                                                                                                 uint32_t sampled_siz,
                                                                                                 uint32_t sampled_tex_offset,
                                                                                                 uint32_t sampled_stride,
                                                                                                 uint32_t sampled_width,
                                                                                                 uint32_t sampled_height,
                                                                                                 uint32_t sampled_low32,
                                                                                                 uint32_t sampled_palette_crc,
                                                                                                 uint16_t formatsize)
{
	SampledFamilyLookupKey key = {};
	key.sampled_fmt = sampled_fmt;
	key.sampled_siz = sampled_siz;
	key.sampled_tex_offset = sampled_tex_offset;
	key.sampled_stride = sampled_stride;
	key.sampled_width = sampled_width;
	key.sampled_height = sampled_height;
	key.sampled_low32 = sampled_low32;
	key.sampled_palette_crc = sampled_palette_crc;
	key.formatsize = formatsize;
	return key;
}

bool ReplacementProvider::prefer_sampled_duplicate_candidate(const Entry &active, const Entry &candidate)
{
	auto compare_string_pref = [](const std::string &a, const std::string &b) -> int {
		const bool a_empty = a.empty();
		const bool b_empty = b.empty();
		if (a_empty != b_empty)
			return a_empty ? 1 : -1;
		if (a < b)
			return -1;
		if (a > b)
			return 1;
		return 0;
	};

	auto compare_uint32 = [](uint32_t a, uint32_t b) -> int {
		if (a < b)
			return -1;
		if (a > b)
			return 1;
		return 0;
	};

	int cmp = compare_string_pref(candidate.phrb_replacement_id, active.phrb_replacement_id);
	if (cmp != 0)
		return cmp < 0;

	cmp = compare_string_pref(candidate.source_path, active.source_path);
	if (cmp != 0)
		return cmp < 0;

	cmp = compare_uint32(candidate.width, active.width);
	if (cmp != 0)
		return cmp < 0;

	cmp = compare_uint32(candidate.height, active.height);
	if (cmp != 0)
		return cmp < 0;

	cmp = compare_uint32(candidate.data_size, active.data_size);
	if (cmp != 0)
		return cmp < 0;

	if (candidate.blob.size() != active.blob.size())
		return candidate.blob.size() < active.blob.size();

	if (std::lexicographical_compare(candidate.blob.begin(), candidate.blob.end(), active.blob.begin(), active.blob.end()))
		return true;
	if (std::lexicographical_compare(active.blob.begin(), active.blob.end(), candidate.blob.begin(), candidate.blob.end()))
		return false;

	return false;
}

void ReplacementProvider::add_entry(Entry &&entry)
{
	const size_t index = entries_.size();
	entries_.push_back(std::move(entry));
	const Entry &stored = entries_.back();
	if (stored.is_jabo)
	{
		// Like the checksum indices, a later package's entry wins.
		jabo_index_[JaboKey{ stored.checksum64, stored.selector_checksum64 }] = index;
		return;
	}
	if (stored.has_native_sampled_identity)
		native_checksum_index_[stored.checksum64].push_back(index);
	else
		compat_checksum_index_[stored.checksum64].push_back(index);
	if (stored.has_native_sampled_identity)
	{
		auto sampled_key = make_sampled_lookup_key(
			stored.sampled_fmt,
			stored.sampled_siz,
			stored.sampled_tex_offset,
			stored.sampled_stride,
			stored.sampled_width,
			stored.sampled_height,
			stored.sampled_low32,
			stored.sampled_palette_crc,
			stored.formatsize,
			stored.selector_checksum64);
		auto duplicate_it = sampled_index_.find(sampled_key);
		if (duplicate_it != sampled_index_.end())
		{
			auto inserted = sampled_duplicate_index_.emplace(sampled_key, 1u);
			if (!inserted.second)
				inserted.first->second++;
			if (prefer_sampled_duplicate_candidate(entries_[duplicate_it->second], stored))
			{
				duplicate_it->second = index;
				sampled_duplicate_reordered_.insert(sampled_key);
			}
		}
		else
			sampled_index_[sampled_key] = index;

		auto sampled_family_key = make_sampled_family_lookup_key(
			stored.sampled_fmt,
			stored.sampled_siz,
			stored.sampled_tex_offset,
			stored.sampled_stride,
			stored.sampled_width,
			stored.sampled_height,
			stored.sampled_low32,
			stored.sampled_palette_crc,
			stored.formatsize);
		sampled_family_index_[sampled_family_key].push_back(index);
	}
}

void ReplacementProvider::clear()
{
	load_error_ = ReplacementLoadStatus::NotFound;
	rejected_path_.clear();
	loaded_path_.clear();
	cache_dir_.clear();
	entries_.clear();
	native_checksum_index_.clear();
	compat_checksum_index_.clear();
	sampled_index_.clear();
	sampled_duplicate_index_.clear();
	sampled_family_index_.clear();
	jabo_index_.clear();
	sampled_duplicate_reordered_.clear();
	jabo_intensity_alpha_decodes_ = 0;
	compat_ci_live_palette_decodes_ = 0;
}

size_t ReplacementProvider::entry_count() const
{
	return entries_.size();
}

bool ReplacementProvider::load_cache_dir(const std::string &path, bool append)
{
	if (!append)
		clear();
	const auto previous_entries = entries_.size();
	cache_dir_ = path;

	std::vector<std::string> files;
	DIR *dir = opendir(path.c_str());
	if (dir)
	{
		for (;;)
		{
			dirent *ent = readdir(dir);
			if (!ent)
				break;
			if (ent->d_name[0] == '.')
				continue;
			const std::string name = ent->d_name;
			if (!is_phrb_cache_path(name))
				continue;
			files.push_back(path + "/" + name);
		}
		closedir(dir);
	}
	else if (is_phrb_cache_path(path))
	{
		files.push_back(path);
	}
	else
	{
		return false;
	}

	auto sort_files = [](std::vector<std::string> &candidate_files) {
		std::sort(candidate_files.begin(), candidate_files.end());
	};
	sort_files(files);

	for (const auto &file : files)
	{
		const auto status = load_phrb(file);
		if (status == ReplacementLoadStatus::Loaded)
		{
			if (loaded_path_.empty())
				loaded_path_ = file;
		}
		else if (status != ReplacementLoadStatus::NotFound && rejected_path_.empty())
		{
			load_error_ = status;
			rejected_path_ = file;
		}
	}

	return entries_.size() > previous_entries;
}

std::vector<ReplacementPackageStats> ReplacementProvider::package_stats() const
{
	std::vector<ReplacementPackageStats> packages;
	std::map<std::string, size_t> package_ids;
	for (const auto &entry : entries_)
		if (package_ids.emplace(entry.source_path, packages.size()).second)
		{
			ReplacementPackageStats stats;
			stats.path = entry.source_path;
			packages.push_back(std::move(stats));
		}

	// The winner stays fixed while walking backwards. last_seen deduplicates
	// repeated keys within a package (each package's entries are contiguous).
	using Key = std::tuple<bool, uint64_t, uint16_t, uint64_t>;
	struct Owner { size_t winner, last_seen; };
	std::map<Key, Owner> owners;
	for (auto it = entries_.rbegin(); it != entries_.rend(); ++it)
	{
		const auto id = package_ids.at(it->source_path);
		const Key key(it->is_jabo, it->checksum64, it->is_jabo ? 0 : it->formatsize, it->selector_checksum64);
		auto inserted = owners.emplace(key, Owner{ id, id });
		auto &owner = inserted.first->second;
		if (!inserted.second && owner.last_seen == id)
			continue;
		auto &stats = packages[id];
		stats.records++;
		if (owner.winner != id)
		{
			stats.hidden_by_later++;
			const auto &winner = packages[owner.winner].path;
			if (stats.hidden_by.empty())
				stats.hidden_by = winner;
			else if (stats.hidden_by != winner)
				stats.multiple_hiders = true;
		}
		owner.last_seen = id;
	}
	return packages;
}

bool ReplacementProvider::has_phrb_compat_entries() const
{
	for (const auto &entry : entries_)
	{
		if (!entry.has_native_sampled_identity && !entry.is_jabo && is_phrb_cache_path(entry.source_path))
			return true;
	}

	return false;
}

bool ReplacementProvider::has_jabo_entries() const
{
	return !jabo_index_.empty();
}

const ReplacementProvider::Entry *ReplacementProvider::find_jabo_entry(uint64_t key_lo, uint64_t key_hi) const
{
	auto it = jabo_index_.find(JaboKey{ key_lo, key_hi });
	return it != jabo_index_.end() ? &entries_[it->second] : nullptr;
}

bool ReplacementProvider::resolve_jabo_candidate(const uint8_t md5[16], ReplacementResolution *out) const
{
	if (!enabled_ || !out || jabo_index_.empty())
		return false;

	JaboKey key = {};
	for (unsigned i = 0; i < 16; i++)
		key[i >> 3] |= uint64_t(md5[i]) << (8 * (i & 7));
	return populate_resolution_from_entry(find_jabo_entry(key[0], key[1]), ReplacementResolutionKind::JaboMd5, false, out);
}

bool ReplacementProvider::decode_rgba8_jabo(uint64_t key_lo, uint64_t key_hi, uint32_t native_format, ReplacementImage *out) const
{
	if (!enabled_ || !out)
		return false;

	const Entry *entry = find_jabo_entry(key_lo, key_hi);
	if (!entry || !decode_entry_rgba8(*entry, out))
		return false;

	// Rice's I import treats alpha 254/255 as opaque for an intensity mask.
	// Apply this only in the Jabo namespace: its MD5 has no format metadata,
	// so the converter cannot decide which interpretation a draw needs.
	// IA has independent alpha; alpha below 254 preserves the entire image.
	if (native_format == 4)
	{
		bool opaque = true;
		for (size_t i = 3; i < out->rgba8.size(); i += 4)
			opaque = opaque && out->rgba8[i] >= 254;
		if (opaque)
		{
			bool changed = false;
			for (size_t i = 0; i < out->rgba8.size(); i += 4)
			{
				const uint8_t alpha = uint8_t((unsigned(out->rgba8[i]) + out->rgba8[i + 1] + out->rgba8[i + 2]) / 3);
				changed = changed || alpha != out->rgba8[i + 3];
				out->rgba8[i + 3] = alpha;
			}
			jabo_intensity_alpha_decodes_ += changed;
		}
	}
	return true;
}

const ReplacementProvider::Entry *ReplacementProvider::find_any_entry(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64) const
{
	if (const auto *native = find_native_entry(checksum64, formatsize, selector_checksum64))
		return native;
	return find_compat_entry(checksum64, formatsize, selector_checksum64);
}

const ReplacementProvider::Entry *ReplacementProvider::find_indexed_entry(const std::unordered_map<uint64_t, std::vector<size_t>> &index_map,
                                                                          uint64_t checksum64,
                                                                          uint16_t formatsize,
                                                                          uint64_t selector_checksum64) const
{
	auto it = index_map.find(checksum64);
	if (it == index_map.end())
		return nullptr;

	const auto &indices = it->second;
	auto find_matching = [&](uint16_t candidate_formatsize, uint64_t candidate_selector) -> const ReplacementProvider::Entry * {
		for (auto itr = indices.rbegin(); itr != indices.rend(); ++itr)
		{
			const auto &entry = entries_[*itr];
			if (entry.formatsize == candidate_formatsize && entry.selector_checksum64 == candidate_selector)
				return &entry;
		}
		return nullptr;
	};

	if (selector_checksum64 != 0)
	{
		if (const auto *entry = find_matching(formatsize, selector_checksum64))
			return entry;
		if (const auto *entry = find_matching(0, selector_checksum64))
			return entry;
	}

	if (const auto *entry = find_matching(formatsize, 0))
		return entry;
	if (const auto *entry = find_matching(0, 0))
		return entry;

	if (selector_checksum64 == 0 && formatsize == 0 && !indices.empty())
		return &entries_[indices.back()];

	return nullptr;
}

const ReplacementProvider::Entry *ReplacementProvider::find_native_entry(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64) const
{
	return find_indexed_entry(native_checksum_index_, checksum64, formatsize, selector_checksum64);
}

const ReplacementProvider::Entry *ReplacementProvider::find_compat_entry(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64,
                                                                        bool *live_palette_fallback) const
{
	if (live_palette_fallback)
		*live_palette_fallback = false;
	if (const auto *entry = find_indexed_entry(compat_checksum_index_, checksum64, formatsize, selector_checksum64))
		return entry;
	// Rice _ci.bmp names without PALCRC intentionally use the draw's live palette.
	// Keep the existing exact-checksum policy for pre-coloured assets.
	if ((formatsize == 2 || formatsize == 258) && (checksum64 >> 32) != 0)
	{
		const auto *entry = find_indexed_entry(compat_checksum_index_, uint32_t(checksum64), formatsize, selector_checksum64);
		if (entry && entry->texture_format == GL_COLOR_INDEX)
		{
			if (live_palette_fallback)
				*live_palette_fallback = true;
			return entry;
		}
	}
	return nullptr;
}

const ReplacementProvider::Entry *ReplacementProvider::find_sampled_entry(uint32_t sampled_fmt,
                                                                          uint32_t sampled_siz,
                                                                          uint32_t sampled_tex_offset,
                                                                          uint32_t sampled_stride,
                                                                          uint32_t sampled_width,
                                                                          uint32_t sampled_height,
                                                                          uint32_t sampled_low32,
                                                                          uint32_t palette_crc,
                                                                          uint16_t formatsize,
                                                                          uint64_t selector_checksum64) const
{
	auto find_matching = [&](uint16_t candidate_formatsize, uint64_t candidate_selector) -> const Entry * {
		auto key = make_sampled_lookup_key(
			sampled_fmt,
			sampled_siz,
			sampled_tex_offset,
			sampled_stride,
			sampled_width,
			sampled_height,
			sampled_low32,
			palette_crc,
			candidate_formatsize,
			candidate_selector);
		auto it = sampled_index_.find(key);
		if (it == sampled_index_.end())
			return nullptr;
		return &entries_[it->second];
	};

	if (selector_checksum64 != 0)
	{
		if (const Entry *entry = find_matching(formatsize, selector_checksum64))
			return entry;
		if (const Entry *entry = find_matching(0, selector_checksum64))
			return entry;
	}

	if (const Entry *entry = find_matching(formatsize, 0))
		return entry;
	if (const Entry *entry = find_matching(0, 0))
		return entry;

	return nullptr;
}

const ReplacementProvider::Entry *ReplacementProvider::find_unique_sampled_family_entry(uint32_t sampled_fmt,
                                                                                        uint32_t sampled_siz,
                                                                                        uint32_t sampled_tex_offset,
                                                                                        uint32_t sampled_stride,
                                                                                        uint32_t sampled_width,
                                                                                        uint32_t sampled_height,
                                                                                        uint32_t sampled_low32,
                                                                                        uint32_t palette_crc,
                                                                                        uint16_t formatsize,
                                                                                        uint64_t *resolved_selector_checksum64) const
{
	return find_singleton_sampled_family_entry(
		sampled_fmt,
		sampled_siz,
		sampled_tex_offset,
		sampled_stride,
		sampled_width,
		sampled_height,
		sampled_low32,
		palette_crc,
		formatsize,
		false,
		resolved_selector_checksum64,
		nullptr);
}

const ReplacementProvider::Entry *ReplacementProvider::find_singleton_sampled_family_entry(uint32_t sampled_fmt,
                                                                                            uint32_t sampled_siz,
                                                                                            uint32_t sampled_tex_offset,
                                                                                            uint32_t sampled_stride,
                                                                                            uint32_t sampled_width,
                                                                                            uint32_t sampled_height,
                                                                                            uint32_t sampled_low32,
                                                                                            uint32_t palette_crc,
                                                                                            uint16_t formatsize,
                                                                                            bool allow_ordered_surface_selectors,
                                                                                            uint64_t *resolved_selector_checksum64,
                                                                                            bool *resolved_ordered_surface_singleton) const
{
	auto find_family = [&](uint16_t candidate_formatsize) -> const std::vector<size_t> * {
		auto key = make_sampled_family_lookup_key(
			sampled_fmt,
			sampled_siz,
			sampled_tex_offset,
			sampled_stride,
			sampled_width,
			sampled_height,
			sampled_low32,
			palette_crc,
			candidate_formatsize);
		auto it = sampled_family_index_.find(key);
		if (it == sampled_family_index_.end())
			return nullptr;
		return &it->second;
	};

	const std::vector<size_t> *exact_family = find_family(formatsize);
	const std::vector<size_t> *generic_family = find_family(0);
	if (!exact_family && !generic_family)
		return nullptr;

	const std::vector<size_t> &active_family = exact_family ? *exact_family : *generic_family;
	std::vector<uint64_t> unique_selectors;
	for (size_t index : active_family)
		push_unique(unique_selectors, entries_[index].selector_checksum64);

	if (unique_selectors.size() != 1)
		return nullptr;

	const uint64_t selected_selector_checksum64 = unique_selectors.front();
	const bool ordered_surface_singleton =
		selected_selector_checksum64 != 0 && is_ordered_surface_selector(selected_selector_checksum64);
	if (ordered_surface_singleton && !allow_ordered_surface_selectors)
		return nullptr;

	const Entry *entry = find_sampled_entry(
		sampled_fmt,
		sampled_siz,
		sampled_tex_offset,
		sampled_stride,
		sampled_width,
		sampled_height,
		sampled_low32,
		palette_crc,
		formatsize,
		selected_selector_checksum64);
	if (!entry)
		return nullptr;

	if (resolved_selector_checksum64)
		*resolved_selector_checksum64 = selected_selector_checksum64;
	if (resolved_ordered_surface_singleton)
		*resolved_ordered_surface_singleton = ordered_surface_singleton;
	return entry;
}

ReplacementPalette ReplacementPalette::from_tmem(const uint8_t *tmem, size_t size, bool ci4, unsigned bank, bool ia16)
{
	ReplacementPalette palette;
	const unsigned first = ci4 ? (bank & 15u) * 16u : 0u;
	const unsigned count = ci4 ? 16u : 256u;
	if (!tmem || size < (first + count) * 8u)
		return palette;
	palette.count = count;
	palette.ia16 = ia16;
	for (unsigned i = 0; i < count; i++)
	{
		const auto *entry = tmem + (first + i) * 8u;
		// The identity mirror stores emulate_load_tlut_entry_word() byte pairs.
		// Native vram16 reads the RDRAM halfword little-endian before TMEM upload.
		palette.words[i] = uint16_t(entry[1]) | (uint16_t(entry[0]) << 8);
	}
	return palette;
}

uint64_t ReplacementPalette::hash() const
{
	if (count != 16 && count != 256)
		return 0;
	uint64_t hash = 0xcbf29ce484222325ull;
	auto add = [&](uint8_t byte) { hash = (hash ^ byte) * 0x100000001b3ull; };
	add(ia16 ? 1 : 0);
	add(count == 16 ? 4 : 8);
	for (unsigned i = 0; i < count; i++)
	{
		add(uint8_t(words[i]));
		add(uint8_t(words[i] >> 8));
	}
	return hash ? hash : 1;
}

void ReplacementProvider::populate_meta_from_entry(const Entry &entry, ReplacementMeta *out) const
{
	if (!out)
		return;

	out->repl_w = entry.width;
	out->repl_h = entry.height;
	out->orig_w = 0;
	out->orig_h = 0;
	out->vk_image_index = 0xffffffffu;
	out->has_mips = false;
	out->srgb = false;
	out->indexed = entry.texture_format == GL_COLOR_INDEX;
}

void ReplacementProvider::populate_identity_from_entry(const Entry &entry, NativeSampledIdentity *out) const
{
	if (!out)
		return;

	out->valid = entry.has_native_sampled_identity;
	out->sampled_fmt = entry.sampled_fmt;
	out->sampled_siz = entry.sampled_siz;
	out->sampled_tex_offset = entry.sampled_tex_offset;
	out->sampled_stride = entry.sampled_stride;
	out->sampled_width = entry.sampled_width;
	out->sampled_height = entry.sampled_height;
	out->sampled_low32 = entry.sampled_low32;
	out->sampled_palette_crc = entry.sampled_palette_crc;
	out->formatsize = entry.formatsize;
	out->selector_checksum64 = entry.selector_checksum64;
}

bool ReplacementProvider::populate_resolution_from_entry(const Entry *entry,
                                                         ReplacementResolutionKind kind,
                                                         bool ordered_surface_singleton,
                                                         ReplacementResolution *out) const
{
	if (!entry || !out)
		return false;

	out->available = true;
	out->kind = kind;
	populate_meta_from_entry(*entry, &out->meta);
	populate_identity_from_entry(*entry, &out->identity);
	out->source_class = entry->has_native_sampled_identity ? ResolvedEntrySourceClass::Native : ResolvedEntrySourceClass::Compat;
	out->resolved_checksum64 = entry->checksum64;
	out->resolved_selector_checksum64 = entry->selector_checksum64;
	out->ordered_surface_singleton = ordered_surface_singleton;
	return true;
}

bool ReplacementProvider::lookup(uint64_t checksum64, uint16_t formatsize, ReplacementMeta *out) const
{
	return lookup_with_selector(checksum64, formatsize, 0, out);
}

uint64_t ReplacementProvider::ordered_surface_slot_selector_checksum64(uint32_t slot_index)
{
	return ORDERED_SURFACE_SELECTOR_TAG | uint64_t(slot_index);
}

bool ReplacementProvider::lookup_with_selector(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64, ReplacementMeta *out) const
{
	if (!enabled_ || !out)
		return false;

	const Entry *entry = find_any_entry(checksum64, formatsize, selector_checksum64);
	if (!entry)
		return false;

	populate_meta_from_entry(*entry, out);
	return true;
}

bool ReplacementProvider::resolve_upload_candidate(uint16_t formatsize,
                                                   uint32_t sampled_fmt,
                                                   uint32_t sampled_siz,
                                                   uint32_t sampled_tex_offset,
                                                   uint32_t sampled_stride,
                                                   uint32_t sampled_width,
                                                   uint32_t sampled_height,
                                                   uint32_t sampled_low32,
                                                   uint32_t palette_crc,
                                                   uint64_t selector_checksum64,
                                                   ReplacementResolution *out) const
{
	if (!enabled_ || !out)
		return false;

	bool ordered_surface_singleton = false;
	const Entry *family_entry = find_singleton_sampled_family_entry(
		sampled_fmt,
		sampled_siz,
		sampled_tex_offset,
		sampled_stride,
		sampled_width,
		sampled_height,
		sampled_low32,
		palette_crc,
		formatsize,
		true,
		nullptr,
		&ordered_surface_singleton);
	if (family_entry)
		return populate_resolution_from_entry(
			family_entry,
			ReplacementResolutionKind::SampledFamilySingleton,
			ordered_surface_singleton,
			out);

	const Entry *sampled_entry = find_sampled_entry(
		sampled_fmt, sampled_siz, sampled_tex_offset, sampled_stride,
		sampled_width, sampled_height, sampled_low32, palette_crc,
		formatsize, selector_checksum64);
	if (sampled_entry)
		return populate_resolution_from_entry(
			sampled_entry,
			ReplacementResolutionKind::SampledExactSelector,
			false,
			out);

	// Steps 3-4 (native-checksum, compat) need checksum64 — derive from sampled fields.
	const uint64_t checksum64 = compose_hires_checksum64(sampled_low32, palette_crc);

	const Entry *native_entry = find_native_entry(checksum64, formatsize, selector_checksum64);
	if (native_entry)
		return populate_resolution_from_entry(
			native_entry,
			ReplacementResolutionKind::ExactNativeSampled,
			false,
			out);

	const Entry *compat_entry = find_compat_entry(checksum64, formatsize, selector_checksum64);
	if (!compat_entry)
		return false;
	return populate_resolution_from_entry(compat_entry, ReplacementResolutionKind::GenericCompat, false, out);
}

bool ReplacementProvider::lookup_sampled_with_selector(uint32_t sampled_fmt,
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
                                                       uint64_t *resolved_checksum64) const
{
	if (!enabled_ || !out)
		return false;

	const Entry *entry = find_sampled_entry(
		sampled_fmt,
		sampled_siz,
		sampled_tex_offset,
		sampled_stride,
		sampled_width,
		sampled_height,
		sampled_low32,
		palette_crc,
		formatsize,
		selector_checksum64);
	if (!entry)
		return false;

	populate_meta_from_entry(*entry, out);
	if (resolved_checksum64)
		*resolved_checksum64 = entry->checksum64;
	return true;
}

bool ReplacementProvider::lookup_sampled_family_unique(uint32_t sampled_fmt,
                                                       uint32_t sampled_siz,
                                                       uint32_t sampled_tex_offset,
                                                       uint32_t sampled_stride,
                                                       uint32_t sampled_width,
                                                       uint32_t sampled_height,
                                                       uint32_t sampled_low32,
                                                       uint32_t palette_crc,
                                                       uint16_t formatsize,
                                                       ReplacementMeta *out,
                                                       uint64_t *resolved_checksum64,
                                                       uint64_t *resolved_selector_checksum64) const
{
	if (!enabled_ || !out)
		return false;

	const Entry *entry = find_unique_sampled_family_entry(
		sampled_fmt,
		sampled_siz,
		sampled_tex_offset,
		sampled_stride,
		sampled_width,
		sampled_height,
		sampled_low32,
		palette_crc,
		formatsize,
		resolved_selector_checksum64);
	if (!entry)
		return false;

	populate_meta_from_entry(*entry, out);
	if (resolved_checksum64)
		*resolved_checksum64 = entry->checksum64;
	return true;
}

bool ReplacementProvider::lookup_sampled_family_singleton(uint32_t sampled_fmt,
                                                          uint32_t sampled_siz,
                                                          uint32_t sampled_tex_offset,
                                                          uint32_t sampled_stride,
                                                          uint32_t sampled_width,
                                                          uint32_t sampled_height,
                                                          uint32_t sampled_low32,
                                                          uint32_t palette_crc,
                                                          uint16_t formatsize,
                                                          ReplacementMeta *out,
                                                          uint64_t *resolved_checksum64,
                                                          uint64_t *resolved_selector_checksum64,
                                                          bool *resolved_ordered_surface_singleton) const
{
	if (!enabled_ || !out)
		return false;

	const Entry *entry = find_singleton_sampled_family_entry(
		sampled_fmt,
		sampled_siz,
		sampled_tex_offset,
		sampled_stride,
		sampled_width,
		sampled_height,
		sampled_low32,
		palette_crc,
		formatsize,
		true,
		resolved_selector_checksum64,
		resolved_ordered_surface_singleton);
	if (!entry)
		return false;

	populate_meta_from_entry(*entry, out);
	if (resolved_checksum64)
		*resolved_checksum64 = entry->checksum64;
	return true;
}

bool ReplacementProvider::decode_sampled_rgba8_with_selector(uint32_t sampled_fmt,
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
                                                             uint64_t *resolved_checksum64) const
{
	if (!enabled_ || !out)
		return false;

	const Entry *entry = find_sampled_entry(
		sampled_fmt,
		sampled_siz,
		sampled_tex_offset,
		sampled_stride,
		sampled_width,
		sampled_height,
		sampled_low32,
		palette_crc,
		formatsize,
		selector_checksum64);
	if (!entry)
		return false;

	if (!decode_entry_rgba8(*entry, out))
		return false;

	if (resolved_checksum64)
		*resolved_checksum64 = entry->checksum64;
	return true;
}

uint32_t ReplacementProvider::expected_decoded_size(const Entry &entry)
{
	uint64_t bpp = 0;
	if (entry.texture_format == GL_COLOR_INDEX && entry.pixel_type == GL_UNSIGNED_BYTE)
		bpp = 1;
	else if (entry.texture_format == GL_RGBA && entry.pixel_type == GL_UNSIGNED_BYTE)
		bpp = 4;
	else if (entry.texture_format == GL_RGB && entry.pixel_type == GL_UNSIGNED_BYTE)
		bpp = 3;
	else if (entry.texture_format == GL_RGB && entry.pixel_type == GL_UNSIGNED_SHORT_5_6_5)
		bpp = 2;
	else if (entry.texture_format == GL_RGBA &&
	         (entry.pixel_type == GL_UNSIGNED_SHORT_4_4_4_4 || entry.pixel_type == GL_UNSIGNED_SHORT_5_5_5_1))
		bpp = 2;
	else if (entry.texture_format == GL_LUMINANCE && entry.pixel_type == GL_UNSIGNED_BYTE)
		bpp = 1;
	else if (entry.format == GL_RGBA8)
		bpp = 4;
	else if (entry.format == GL_RGB8)
		bpp = 3;

	const uint64_t pixels = uint64_t(entry.width) * uint64_t(entry.height);
	const uint64_t bytes = pixels * bpp;
	if (bpp == 0 || bytes > UINT32_MAX)
		return 0;
	return static_cast<uint32_t>(bytes);
}

bool ReplacementProvider::decompress_if_needed(const Entry &entry, const std::vector<uint8_t> &blob, std::vector<uint8_t> &pixel_data)
{
	if ((entry.format & GL_TEXFMT_GZ) == 0)
	{
		pixel_data = blob;
		return true;
	}

	const uint32_t expected_size = expected_decoded_size(entry);
	if (expected_size == 0)
		return false;

	pixel_data.resize(expected_size);
	uLongf dst_len = expected_size;
	const int ret = uncompress(pixel_data.data(), &dst_len, blob.data(), static_cast<uLong>(blob.size()));
	if (ret != Z_OK)
		return false;

	if (dst_len != expected_size)
		pixel_data.resize(static_cast<size_t>(dst_len));
	return true;
}

bool ReplacementProvider::decode_pixels_rgba8(const Entry &entry, const std::vector<uint8_t> &pixel_data, std::vector<uint8_t> &rgba8)
{
	const uint64_t pixel_count = uint64_t(entry.width) * uint64_t(entry.height);
	if (pixel_count == 0 || pixel_count > (SIZE_MAX / 4))
		return false;

	rgba8.resize(static_cast<size_t>(pixel_count) * 4);

	if (entry.texture_format == GL_RGBA && entry.pixel_type == GL_UNSIGNED_BYTE)
	{
		if (pixel_data.size() < rgba8.size())
			return false;
		std::copy(pixel_data.begin(), pixel_data.begin() + ptrdiff_t(rgba8.size()), rgba8.begin());
		return true;
	}

	if (entry.texture_format == GL_RGB && entry.pixel_type == GL_UNSIGNED_BYTE)
	{
		if (pixel_data.size() < size_t(pixel_count) * 3)
			return false;
		for (size_t i = 0; i < size_t(pixel_count); i++)
		{
			rgba8[4 * i + 0] = pixel_data[3 * i + 0];
			rgba8[4 * i + 1] = pixel_data[3 * i + 1];
			rgba8[4 * i + 2] = pixel_data[3 * i + 2];
			rgba8[4 * i + 3] = 255;
		}
		return true;
	}

	if (entry.texture_format == GL_RGB && entry.pixel_type == GL_UNSIGNED_SHORT_5_6_5)
	{
		if (pixel_data.size() < size_t(pixel_count) * 2)
			return false;
		for (size_t i = 0; i < size_t(pixel_count); i++)
		{
			const uint16_t v = uint16_t(pixel_data[2 * i + 0]) | (uint16_t(pixel_data[2 * i + 1]) << 8);
			rgba8[4 * i + 0] = expand_5_to_8((v >> 11) & 0x1f);
			rgba8[4 * i + 1] = expand_6_to_8((v >> 5) & 0x3f);
			rgba8[4 * i + 2] = expand_5_to_8(v & 0x1f);
			rgba8[4 * i + 3] = 255;
		}
		return true;
	}

	if (entry.texture_format == GL_RGBA && entry.pixel_type == GL_UNSIGNED_SHORT_5_5_5_1)
	{
		if (pixel_data.size() < size_t(pixel_count) * 2)
			return false;
		for (size_t i = 0; i < size_t(pixel_count); i++)
		{
			const uint16_t v = uint16_t(pixel_data[2 * i + 0]) | (uint16_t(pixel_data[2 * i + 1]) << 8);
			rgba8[4 * i + 0] = expand_5_to_8((v >> 11) & 0x1f);
			rgba8[4 * i + 1] = expand_5_to_8((v >> 6) & 0x1f);
			rgba8[4 * i + 2] = expand_5_to_8((v >> 1) & 0x1f);
			rgba8[4 * i + 3] = (v & 1) ? 255 : 0;
		}
		return true;
	}

	if (entry.texture_format == GL_RGBA && entry.pixel_type == GL_UNSIGNED_SHORT_4_4_4_4)
	{
		if (pixel_data.size() < size_t(pixel_count) * 2)
			return false;
		for (size_t i = 0; i < size_t(pixel_count); i++)
		{
			const uint16_t v = uint16_t(pixel_data[2 * i + 0]) | (uint16_t(pixel_data[2 * i + 1]) << 8);
			rgba8[4 * i + 0] = expand_4_to_8((v >> 12) & 0x0f);
			rgba8[4 * i + 1] = expand_4_to_8((v >> 8) & 0x0f);
			rgba8[4 * i + 2] = expand_4_to_8((v >> 4) & 0x0f);
			rgba8[4 * i + 3] = expand_4_to_8(v & 0x0f);
		}
		return true;
	}

	if (entry.texture_format == GL_LUMINANCE && entry.pixel_type == GL_UNSIGNED_BYTE)
	{
		if (pixel_data.size() < size_t(pixel_count))
			return false;
		for (size_t i = 0; i < size_t(pixel_count); i++)
		{
			const uint8_t l = pixel_data[i];
			rgba8[4 * i + 0] = l;
			rgba8[4 * i + 1] = l;
			rgba8[4 * i + 2] = l;
			rgba8[4 * i + 3] = 255;
		}
		return true;
	}

	if (pixel_data.size() == rgba8.size())
	{
		std::copy(pixel_data.begin(), pixel_data.end(), rgba8.begin());
		return true;
	}

	return false;
}

bool ReplacementProvider::read_blob(const Entry &entry, std::vector<uint8_t> &blob) const
{
	if (entry.inline_blob)
	{
		blob = entry.blob;
		return true;
	}

	std::ifstream file(entry.source_path, std::ifstream::in | std::ifstream::binary);
	if (!file.good())
		return false;

	file.seekg(static_cast<std::streamoff>(entry.data_offset), std::ifstream::beg);
	if (!file.good())
		return false;

	blob.resize(entry.data_size);
	return read_exact(file, blob);
}

bool ReplacementProvider::decode_rgba8(uint64_t checksum64, uint16_t formatsize, ReplacementImage *out) const
{
	return decode_rgba8_with_selector(checksum64, formatsize, 0, out);
}

bool ReplacementProvider::decode_rgba8_with_selector(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64, ReplacementImage *out) const
{
	if (!enabled_ || !out)
		return false;

	const Entry *entry = find_any_entry(checksum64, formatsize, selector_checksum64);
	if (!entry)
		return false;

	return decode_entry_rgba8(*entry, out);
}

bool ReplacementProvider::decode_rgba8_native_with_selector(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64, ReplacementImage *out) const
{
	if (!enabled_ || !out)
		return false;

	const Entry *entry = find_native_entry(checksum64, formatsize, selector_checksum64);
	if (!entry)
		return false;

	return decode_entry_rgba8(*entry, out);
}

bool ReplacementProvider::decode_rgba8_compat_with_selector(uint64_t checksum64, uint16_t formatsize, uint64_t selector_checksum64, ReplacementImage *out, const ReplacementPalette *palette) const
{
	if (!enabled_ || !out)
		return false;

	bool live_palette_fallback = false;
	const Entry *entry = find_compat_entry(checksum64, formatsize, selector_checksum64, &live_palette_fallback);
	if (!entry || !decode_entry_rgba8(*entry, out, palette))
		return false;
	compat_ci_live_palette_decodes_ += live_palette_fallback;
	return true;
}

bool ReplacementProvider::decode_entry_rgba8(const Entry &entry, ReplacementImage *out, const ReplacementPalette *palette) const
{
	if (!out)
		return false;

	std::vector<uint8_t> blob;
	if (!read_blob(entry, blob))
		return false;

	ReplacementImage image;
	image.meta.repl_w = entry.width;
	image.meta.repl_h = entry.height;
	image.meta.vk_image_index = 0xffffffffu;
	const uint64_t pixel_count = uint64_t(entry.width) * uint64_t(entry.height);
	// Only exact raw RGBA can reuse the blob without retaining an oversized allocation.
	if ((entry.format & GL_TEXFMT_GZ) == 0 &&
	    entry.texture_format == GL_RGBA && entry.pixel_type == GL_UNSIGNED_BYTE &&
	    pixel_count != 0 && pixel_count <= SIZE_MAX / 4 && blob.size() == size_t(pixel_count) * 4)
	{
		image.rgba8 = std::move(blob);
		*out = std::move(image);
		return true;
	}

	std::vector<uint8_t> pixels;
	if (!decompress_if_needed(entry, blob, pixels))
		return false;

	if (entry.texture_format == GL_COLOR_INDEX)
	{
		const bool ci4 = entry.formatsize == 2;
		const uint32_t count = ci4 ? 16u : 256u;
		const uint64_t pixel_count = uint64_t(entry.width) * entry.height;
		if (!palette || palette->count != count || entry.pixel_type != GL_UNSIGNED_BYTE ||
			(entry.formatsize != 2 && entry.formatsize != 258) ||
			pixels.size() != pixel_count || pixel_count > SIZE_MAX / 4)
			return false;
		image.meta.indexed = true;
		image.rgba8.resize(size_t(pixel_count) * 4);
		for (size_t i = 0; i < pixels.size(); i++)
		{
			if (pixels[i] >= count)
				return false;
			const uint16_t word = palette->words[pixels[i]];
			auto *rgba = image.rgba8.data() + i * 4;
			if (palette->ia16)
			{
				rgba[0] = rgba[1] = rgba[2] = uint8_t(word >> 8);
				rgba[3] = uint8_t(word);
			}
			else
			{
				rgba[0] = expand_5_to_8((word >> 11) & 31);
				rgba[1] = expand_5_to_8((word >> 6) & 31);
				rgba[2] = expand_5_to_8((word >> 1) & 31);
				rgba[3] = (word & 1) ? 255 : 0;
			}
		}
	}
	else if (!decode_pixels_rgba8(entry, pixels, image.rgba8))
		return false;

	*out = std::move(image);
	return true;
}

ReplacementProviderStats ReplacementProvider::get_stats() const
{
	ReplacementProviderStats stats = {};
	stats.entry_count = uint32_t(entries_.size());
	stats.sampled_index_count = uint32_t(sampled_index_.size());
	stats.sampled_duplicate_key_count = uint32_t(sampled_duplicate_index_.size());
	stats.sampled_family_count = uint32_t(sampled_family_index_.size());
	stats.sampled_duplicate_reordered_keys = uint32_t(sampled_duplicate_reordered_.size());
	stats.jabo_intensity_alpha_textures = jabo_intensity_alpha_decodes_;
	stats.compat_ci_live_palette_textures = compat_ci_live_palette_decodes_;
	for (const auto &it : sampled_duplicate_index_)
		stats.sampled_duplicate_entry_count += it.second;

	for (const auto &entry : entries_)
	{
		if (entry.is_jabo)
			stats.jabo_entry_count++;
		else if (entry.has_native_sampled_identity)
			stats.native_sampled_entry_count++;
		else
			stats.compat_entry_count++;

		if (entry.source_path.find(".phrb") != std::string::npos)
			stats.source_phrb_entry_count++;
	}

	return stats;
}

std::vector<SampledDuplicateDiagnostics> ReplacementProvider::get_sampled_duplicate_diagnostics(size_t limit) const
{
	std::vector<SampledDuplicateDiagnostics> diagnostics;
	diagnostics.reserve(sampled_duplicate_index_.size());
	for (const auto &it : sampled_duplicate_index_)
	{
		const auto active_it = sampled_index_.find(it.first);
		if (active_it == sampled_index_.end())
			continue;

		const Entry &active_entry = entries_[active_it->second];
		SampledDuplicateDiagnostics diag = {};
		diag.sampled_fmt = it.first.sampled_fmt;
		diag.sampled_siz = it.first.sampled_siz;
		diag.sampled_tex_offset = it.first.sampled_tex_offset;
		diag.sampled_stride = it.first.sampled_stride;
		diag.sampled_width = it.first.sampled_width;
		diag.sampled_height = it.first.sampled_height;
		diag.sampled_low32 = it.first.sampled_low32;
		diag.sampled_palette_crc = it.first.sampled_palette_crc;
		diag.formatsize = it.first.formatsize;
		diag.selector_checksum64 = it.first.selector_checksum64;
		diag.total_entry_count = 1u + it.second;
		diag.duplicate_entry_count = it.second;
		diag.active_checksum64 = active_entry.checksum64;
		diag.active_repl_w = active_entry.width;
		diag.active_repl_h = active_entry.height;
		diag.active_source_path = active_entry.source_path;
		diag.active_policy_key = active_entry.phrb_policy_key;
		diag.active_replacement_id = active_entry.phrb_replacement_id;
		diag.active_sampled_object_id = active_entry.phrb_sampled_object_id;
		diagnostics.push_back(std::move(diag));
	}

	std::sort(diagnostics.begin(), diagnostics.end(), [](const SampledDuplicateDiagnostics &a, const SampledDuplicateDiagnostics &b) {
		if (a.duplicate_entry_count != b.duplicate_entry_count)
			return a.duplicate_entry_count > b.duplicate_entry_count;
		if (a.sampled_low32 != b.sampled_low32)
			return a.sampled_low32 < b.sampled_low32;
		if (a.sampled_palette_crc != b.sampled_palette_crc)
			return a.sampled_palette_crc < b.sampled_palette_crc;
		if (a.formatsize != b.formatsize)
			return a.formatsize < b.formatsize;
		return a.selector_checksum64 < b.selector_checksum64;
	});

	if (limit != 0 && diagnostics.size() > limit)
		diagnostics.resize(limit);

	return diagnostics;
}



ReplacementLoadStatus ReplacementProvider::load_phrb(const std::string &path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
		return errno == ENOENT ? ReplacementLoadStatus::NotFound : ReplacementLoadStatus::Invalid;

	file.seekg(0, std::ios::end);
	const std::streamoff size = file.tellg();
	if (size < std::streamoff(sizeof(PHRBHeader)))
		return ReplacementLoadStatus::Invalid;
	file.seekg(0, std::ios::beg);

	// Read only the metadata portion (header + records + assets + string table).
	// Blob data is left on disk and loaded on demand via read_blob() with inline_blob=false.
	PHRBHeader header = {};
	{
		std::vector<uint8_t> header_buf(sizeof(PHRBHeader));
		if (!read_exact(file, header_buf))
			return ReplacementLoadStatus::Invalid;
		memcpy(&header, header_buf.data(), sizeof(PHRBHeader));
	}
	if (!phrb_magic_ok(header.magic))
		return ReplacementLoadStatus::Invalid;
	// hts2phrb writes version 7, or 8 when a record carries a Jabo MD5.
	if (header.version < 7 || header.version > 8)
	{
		LOGW("Hi-res PHRB %s is version %u; this core reads versions 7 and 8.%s\n", path.c_str(), header.version,
		     header.version < 7 ? " Reconvert the source pack with tools/hts2phrb.py." : "");
		return ReplacementLoadStatus::UnsupportedVersion;
	}
	if (header.string_table_offset > header.blob_offset || header.blob_offset > static_cast<size_t>(size))
		return ReplacementLoadStatus::Invalid;

	const bool version8 = header.version >= 8;
	const size_t record_size = version8 ? sizeof(PHRBRecordV8) : sizeof(PHRBRecordV4);
	const size_t asset_size = sizeof(PHRBAssetV7);
	const size_t metadata_size = static_cast<size_t>(header.blob_offset);
	if (header.record_table_offset < sizeof(PHRBHeader) ||
	    header.record_table_offset > header.asset_table_offset ||
	    header.asset_table_offset > header.string_table_offset ||
	    header.record_count > (header.asset_table_offset - header.record_table_offset) / record_size ||
	    header.asset_count > (header.string_table_offset - header.asset_table_offset) / asset_size)
		return ReplacementLoadStatus::Invalid;
	// Read the metadata region: from after the header through end of string table (up to blob_offset).
	std::vector<uint8_t> blob(metadata_size);
	file.seekg(0, std::ios::beg);
	if (!read_exact(file, blob))
		return ReplacementLoadStatus::Invalid;

	// Validate every candidate slice before publishing entries from any record.
	uint32_t asset_total = 0;
	for (uint32_t i = 0; i < header.record_count; i++)
	{
		uint32_t count = 0;
		const size_t count_offset = offsetof(PHRBRecordV4, asset_candidate_count);
		if (!read_pod_from_blob(blob, size_t(header.record_table_offset) + size_t(i) * record_size + count_offset, count) ||
		    count > header.asset_count - asset_total)
			return ReplacementLoadStatus::Invalid;
		asset_total += count;
	}
	if (asset_total != header.asset_count)
		return ReplacementLoadStatus::Invalid;

	const uint8_t *string_blob = blob.data() + header.string_table_offset;
	const size_t string_blob_size = size_t(header.blob_offset - header.string_table_offset);
	const size_t rgba_blob_size = size_t(static_cast<size_t>(size) - header.blob_offset);
	uint32_t loaded_count = 0;

	uint32_t asset_base = 0;
	for (uint32_t record_index = 0; record_index < header.record_count; record_index++)
	{
		PHRBRecordV4 record = {};
		uint32_t key_kind = PHRB_KEY_KIND_RICE;
		uint8_t record_key[16] = {};
		if (version8)
		{
			PHRBRecordV8 record_v8 = {};
			const size_t record_offset = size_t(header.record_table_offset) + size_t(record_index) * sizeof(PHRBRecordV8);
			if (!read_pod_from_blob(blob, record_offset, record_v8))
				return ReplacementLoadStatus::Invalid;
			record = record_v8.v4;
			key_kind = record_v8.key_kind;
			memcpy(record_key, record_v8.key, sizeof(record_key));
		}
		else
		{
			const size_t record_offset = size_t(header.record_table_offset) + size_t(record_index) * sizeof(PHRBRecordV4);
			if (!read_pod_from_blob(blob, record_offset, record))
				return ReplacementLoadStatus::Invalid;
		}
		const uint32_t record_flags = record.record_flags;

		std::string policy_key;
		if (!read_c_string_from_blob(string_blob, string_blob_size, record.policy_key_offset, policy_key))
			policy_key = "phrb-record";
		std::string sampled_object_id;
		if (!read_c_string_from_blob(string_blob, string_blob_size, record.sampled_object_id_offset, sampled_object_id))
			sampled_object_id.clear();

		// Unknown key kinds come from a newer converter; their keys cannot match.
		if ((record_flags & PHRB_RECORD_FLAG_RUNTIME_READY) == 0 ||
		    (key_kind != PHRB_KEY_KIND_RICE && key_kind != PHRB_KEY_KIND_JABO_MD5))
		{
			asset_base += record.asset_candidate_count;
			continue;
		}

		auto add_sampled_entry = [&](uint32_t palette_crc,
		                            uint64_t selector_checksum64,
		                            const std::string &replacement_id,
		                            uint32_t width,
		                            uint32_t height,
		                            uint32_t format,
		                            uint32_t texture_format,
		                            uint32_t pixel_type,
		                            uint64_t rgba_offset,
		                            uint32_t rgba_size) {
			Entry entry = {};
			entry.source_path = path;
			entry.phrb_policy_key = policy_key;
			entry.phrb_replacement_id = replacement_id;
			entry.phrb_sampled_object_id = sampled_object_id;
			entry.checksum64 = (uint64_t(palette_crc) << 32u) | uint64_t(record.sampled_low32);
			entry.data_offset = uint64_t(header.blob_offset) + rgba_offset;
			entry.data_size = rgba_size;
			entry.width = width;
			entry.height = height;
			entry.format = format;
			entry.texture_format = texture_format;
			entry.pixel_type = pixel_type;
			entry.formatsize = uint16_t(record.formatsize);
			entry.selector_checksum64 = selector_checksum64;
			entry.sampled_fmt = record.fmt;
			entry.sampled_siz = record.siz;
			entry.sampled_tex_offset = record.tex_offset;
			entry.sampled_stride = record.stride;
			entry.sampled_width = record.width;
			entry.sampled_height = record.height;
			entry.sampled_low32 = record.sampled_low32;
			entry.sampled_palette_crc = palette_crc;
			entry.sampled_entry_pcrc = record.sampled_entry_pcrc;
			entry.sampled_sparse_pcrc = record.sampled_sparse_pcrc;
			entry.has_native_sampled_identity = true;
			entry.is_hires = true;
			entry.inline_blob = false;
			add_entry(std::move(entry));
			loaded_count++;
		};
		auto add_family_entry = [&](uint64_t legacy_checksum64,
		                           uint16_t legacy_formatsize,
		                           uint64_t selector_checksum64,
		                           const std::string &replacement_id,
		                           uint32_t width,
		                           uint32_t height,
		                           uint32_t format,
		                           uint32_t texture_format,
		                           uint32_t pixel_type,
		                           uint64_t rgba_offset,
		                           uint32_t rgba_size) {
			if (legacy_checksum64 == 0)
				return;
			Entry entry = {};
			entry.source_path = path;
			entry.phrb_policy_key = policy_key;
			entry.phrb_replacement_id = replacement_id;
			entry.phrb_sampled_object_id = sampled_object_id;
			entry.checksum64 = legacy_checksum64;
			entry.data_offset = uint64_t(header.blob_offset) + rgba_offset;
			entry.data_size = rgba_size;
			entry.width = width;
			entry.height = height;
			entry.format = format;
			entry.texture_format = texture_format;
			entry.pixel_type = pixel_type;
			entry.formatsize = legacy_formatsize != 0 ? legacy_formatsize : uint16_t(record.formatsize);
			entry.selector_checksum64 = selector_checksum64;
			entry.sampled_low32 = record.sampled_low32;
			entry.sampled_palette_crc = uint32_t(legacy_checksum64 >> 32u);
			entry.sampled_entry_pcrc = record.sampled_entry_pcrc;
			entry.sampled_sparse_pcrc = record.sampled_sparse_pcrc;
			entry.has_native_sampled_identity = false;
			entry.is_runtime_family_compat = true;
			entry.is_hires = true;
			entry.inline_blob = false;
			add_entry(std::move(entry));
			loaded_count++;
		};
		auto add_jabo_entry = [&](const std::string &replacement_id,
		                          uint32_t width,
		                          uint32_t height,
		                          uint32_t format,
		                          uint32_t texture_format,
		                          uint32_t pixel_type,
		                          uint64_t rgba_offset,
		                          uint32_t rgba_size) {
			Entry entry = {};
			entry.source_path = path;
			entry.phrb_policy_key = policy_key;
			entry.phrb_replacement_id = replacement_id;
			entry.phrb_sampled_object_id = sampled_object_id;
			for (unsigned i = 0; i < 16; i++)
				(i < 8 ? entry.checksum64 : entry.selector_checksum64) |= uint64_t(record_key[i]) << (8 * (i & 7));
			entry.data_offset = uint64_t(header.blob_offset) + rgba_offset;
			entry.data_size = rgba_size;
			entry.width = width;
			entry.height = height;
			entry.format = format;
			entry.texture_format = texture_format;
			entry.pixel_type = pixel_type;
			entry.is_jabo = true;
			entry.is_hires = true;
			entry.inline_blob = false;
			add_entry(std::move(entry));
			loaded_count++;
		};
		const bool family_runtime_record =
			record.fmt == 0 &&
			record.siz == 0 &&
			record.tex_offset == 0 &&
			record.stride == 0 &&
			record.width == 0 &&
			record.height == 0;

		for (uint32_t ai = 0; ai < record.asset_candidate_count; ai++)
		{
			const uint32_t asset_index = asset_base + ai;
			PHRBAssetV7 asset = {};
			const size_t asset_offset = size_t(header.asset_table_offset) + size_t(asset_index) * sizeof(PHRBAssetV7);
			if (!read_pod_from_blob(blob, asset_offset, asset))
				return ReplacementLoadStatus::Invalid;
			const uint32_t width = asset.width;
			const uint32_t height = asset.height;
			const uint32_t format = asset.format;
			const uint32_t texture_format = asset.texture_format;
			const uint32_t pixel_type = asset.pixel_type;
			const uint64_t rgba_offset = asset.rgba_blob_offset;
			const uint64_t rgba_size = asset.rgba_blob_size;
			const uint16_t legacy_formatsize = uint16_t(asset.legacy_formatsize);
			const uint64_t selector_checksum64 = asset.selector_checksum64;
			const uint64_t legacy_checksum64 = asset.legacy_checksum64;
			std::string replacement_id;
			read_c_string_from_blob(string_blob, string_blob_size, asset.replacement_id_offset, replacement_id);

			if (rgba_offset > rgba_blob_size || rgba_size > (rgba_blob_size - rgba_offset))
				continue;
			if (width == 0 || height == 0 || rgba_size == 0 || rgba_size > uint64_t(UINT32_MAX))
				continue;
			const uint32_t rgba_size_u32 = uint32_t(rgba_size);

			if (key_kind == PHRB_KEY_KIND_JABO_MD5)
			{
				add_jabo_entry(replacement_id, width, height, format, texture_format, pixel_type, rgba_offset,
				               rgba_size_u32);
				continue;
			}

			if (family_runtime_record)
			{
				add_family_entry(
					legacy_checksum64,
					legacy_formatsize,
					selector_checksum64,
					replacement_id,
					width,
					height,
					format,
					texture_format,
					pixel_type,
					rgba_offset,
					rgba_size_u32);
				continue;
			}

			bool added = false;
			if (record.sampled_sparse_pcrc != 0)
			{
				add_sampled_entry(
					record.sampled_sparse_pcrc,
					selector_checksum64,
					replacement_id,
					width,
					height,
					format,
					texture_format,
					pixel_type,
					rgba_offset,
					rgba_size_u32);
				added = true;
			}
			if (record.sampled_entry_pcrc != 0 && record.sampled_entry_pcrc != record.sampled_sparse_pcrc)
			{
				add_sampled_entry(
					record.sampled_entry_pcrc,
					selector_checksum64,
					replacement_id,
					width,
					height,
					format,
					texture_format,
					pixel_type,
					rgba_offset,
					rgba_size_u32);
				added = true;
			}
			if (!added)
				add_sampled_entry(
					0,
					selector_checksum64,
					replacement_id,
					width,
					height,
					format,
					texture_format,
					pixel_type,
					rgba_offset,
					rgba_size_u32);
		}
		asset_base += record.asset_candidate_count;
	}

	return loaded_count ? ReplacementLoadStatus::Loaded : ReplacementLoadStatus::Invalid;
}

}
