#define private public
#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/texture_replacement.hpp"
#undef private
#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <new>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>
#include <zlib.h>

// While tracking, record the largest single allocation and the total allocated,
// so malformed-package tests can show that loading allocates only a little.
static bool allocation_tracking = false;
static size_t largest_allocation = 0;
static size_t total_allocation = 0;

void *operator new(size_t size)
{
	if (allocation_tracking)
	{
		largest_allocation = std::max(largest_allocation, size);
		total_allocation += size;
	}
	if (void *ptr = std::malloc(size ? size : 1))
		return ptr;
	throw std::bad_alloc();
}

// GCC assumes the standard operator new and flags free() as a mismatch.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif
void operator delete(void *ptr) noexcept
{
	std::free(ptr);
}

void operator delete(void *ptr, size_t) noexcept
{
	std::free(ptr);
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace
{
using namespace RDP;

#pragma pack(push, 1)

struct PHRBHeader
{
	char magic[4];
	uint32_t version;
	uint32_t record_count;
	uint32_t asset_count;
	uint32_t record_table_offset;
	uint32_t asset_table_offset;
	uint32_t string_table_offset;
	uint32_t blob_offset;
};

struct PHRBRecordV4
{
	uint32_t policy_key_offset;
	uint32_t sampled_object_id_offset;
	uint32_t record_flags;
	uint32_t fmt;
	uint32_t siz;
	uint32_t tex_offset;
	uint32_t stride;
	uint32_t width;
	uint32_t height;
	uint32_t formatsize;
	uint32_t sampled_low32;
	uint32_t sampled_entry_pcrc;
	uint32_t sampled_sparse_pcrc;
	uint32_t asset_candidate_count;
};

struct PHRBRecordV8
{
	PHRBRecordV4 v4;
	uint32_t key_kind;
	uint8_t key[16];
};

struct PHRBAssetV7
{
	uint32_t record_index;
	uint32_t replacement_id_offset;
	uint32_t legacy_source_path_offset;
	uint32_t rgba_rel_path_offset;
	uint32_t variant_group_id_offset;
	uint32_t width;
	uint32_t height;
	uint32_t format;
	uint32_t texture_format;
	uint32_t pixel_type;
	uint32_t legacy_formatsize;
	uint64_t selector_checksum64;
	uint64_t legacy_checksum64;
	uint64_t rgba_blob_offset;
	uint64_t rgba_blob_size;
};

#pragma pack(pop)

static_assert(sizeof(PHRBHeader) == 32, "PHRBHeader must stay packed on disk.");
static_assert(sizeof(PHRBRecordV4) == 56, "PHRBRecordV4 must stay packed on disk.");
static_assert(sizeof(PHRBRecordV8) == 76, "PHRBRecordV8 must stay packed on disk.");
static_assert(sizeof(PHRBAssetV7) == 76, "PHRBAssetV7 must stay packed on disk.");

constexpr uint16_t GL_RGB = 0x1907;
constexpr uint16_t GL_RGBA = 0x1908;
constexpr uint16_t GL_UNSIGNED_BYTE = 0x1401;
constexpr uint16_t GL_UNSIGNED_SHORT_5_6_5 = 0x8363;
constexpr uint32_t GL_RGB8 = 0x8051;
constexpr uint32_t GL_RGBA8 = 0x8058;
constexpr uint32_t PHRB_RECORD_FLAG_RUNTIME_READY = 1u << 0;
constexpr int32_t TXCACHE_FORMAT_VERSION = 0x08000000;

static void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}

static std::string make_temp_dir()
{
	char tmpl[] = "/tmp/parallel-n64-texture-provider-XXXXXX";
	char *dir = mkdtemp(tmpl);
	check(dir != nullptr, "mkdtemp failed");
	return dir;
}

static void remove_tree(const std::string &dir)
{
	std::string command = "rm -rf '" + dir + "'";
	int ret = std::system(command.c_str());
	(void)ret;
}

static void write_file(const std::string &path, const std::vector<uint8_t> &data)
{
	std::ofstream file(path, std::ios::binary);
	check(bool(file), "failed to open temporary PHRB file");
	file.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
	check(bool(file), "failed to write temporary PHRB file");
}

template <typename T>
static void append_bytes(std::vector<uint8_t> &data, const T &value)
{
	const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
	data.insert(data.end(), bytes, bytes + sizeof(T));
}

static void write_legacy_hts(const std::string &path,
                             uint64_t checksum64,
                             uint16_t formatsize,
                             uint32_t width,
                             uint32_t height,
                             uint32_t format,
                             uint16_t texture_format,
                             uint16_t pixel_type,
                             const std::vector<uint8_t> &blob)
{
	const int32_t version = TXCACHE_FORMAT_VERSION;
	const int32_t config = 0;
	const int64_t storage_pos = 16;
	const int32_t storage_size = 1;
	const uint64_t record_offset = 16 + 4 + 8 + 8;
	const int64_t packed = int64_t(record_offset | (uint64_t(formatsize) << 48u));
	const uint8_t is_hires = 1;
	const uint32_t data_size = uint32_t(blob.size());

	std::vector<uint8_t> data;
	append_bytes(data, version);
	append_bytes(data, config);
	append_bytes(data, storage_pos);
	append_bytes(data, storage_size);
	append_bytes(data, checksum64);
	append_bytes(data, packed);
	append_bytes(data, width);
	append_bytes(data, height);
	append_bytes(data, format);
	append_bytes(data, texture_format);
	append_bytes(data, pixel_type);
	append_bytes(data, is_hires);
	append_bytes(data, formatsize);
	append_bytes(data, data_size);
	data.insert(data.end(), blob.begin(), blob.end());

	write_file(path, data);
}

static void write_legacy_htc(const std::string &path,
                             uint64_t checksum64,
                             uint16_t formatsize,
                             uint32_t width,
                             uint32_t height,
                             uint32_t format,
                             uint16_t texture_format,
                             uint16_t pixel_type,
                             const std::vector<uint8_t> &blob)
{
	gzFile fp = gzopen(path.c_str(), "wb");
	check(fp != nullptr, "failed to open temporary HTC file");

	const int32_t version = TXCACHE_FORMAT_VERSION;
	const int32_t config = 0;
	const uint8_t is_hires = 1;
	const uint32_t data_size = uint32_t(blob.size());

	check(gzwrite(fp, &version, sizeof(version)) == int(sizeof(version)), "failed to write HTC version");
	check(gzwrite(fp, &config, sizeof(config)) == int(sizeof(config)), "failed to write HTC config");
	check(gzwrite(fp, &checksum64, sizeof(checksum64)) == int(sizeof(checksum64)), "failed to write HTC checksum");
	check(gzwrite(fp, &width, sizeof(width)) == int(sizeof(width)), "failed to write HTC width");
	check(gzwrite(fp, &height, sizeof(height)) == int(sizeof(height)), "failed to write HTC height");
	check(gzwrite(fp, &format, sizeof(format)) == int(sizeof(format)), "failed to write HTC format");
	check(gzwrite(fp, &texture_format, sizeof(texture_format)) == int(sizeof(texture_format)), "failed to write HTC texture format");
	check(gzwrite(fp, &pixel_type, sizeof(pixel_type)) == int(sizeof(pixel_type)), "failed to write HTC pixel type");
	check(gzwrite(fp, &is_hires, sizeof(is_hires)) == int(sizeof(is_hires)), "failed to write HTC is_hires");
	check(gzwrite(fp, &formatsize, sizeof(formatsize)) == int(sizeof(formatsize)), "failed to write HTC formatsize");
	check(gzwrite(fp, &data_size, sizeof(data_size)) == int(sizeof(data_size)), "failed to write HTC data size");
	check(gzwrite(fp, blob.data(), unsigned(blob.size())) == int(blob.size()), "failed to write HTC payload");
	check(gzclose(fp) == Z_OK, "failed to close temporary HTC file");
}

static void write_single_asset_phrb(const std::string &path,
                                    const std::string &policy_key,
                                    const std::string &replacement_id,
                                    const std::string &sampled_object_id,
                                    uint32_t fmt,
                                    uint32_t siz,
                                    uint32_t tex_offset,
                                    uint32_t stride,
                                    uint32_t width,
                                    uint32_t height,
                                    uint32_t formatsize,
                                    uint32_t sampled_low32,
                                    uint32_t palette_crc,
                                    uint64_t selector_checksum64,
                                    uint32_t repl_width,
                                    uint32_t repl_height,
                                    const std::vector<uint8_t> &blob,
                                    uint32_t record_flags = PHRB_RECORD_FLAG_RUNTIME_READY,
                                    uint64_t legacy_checksum64 = 0,
                                    uint32_t asset_format = GL_RGBA8,
                                    uint16_t asset_texture_format = GL_RGBA,
                                    uint16_t asset_pixel_type = GL_UNSIGNED_BYTE)
{
	if (legacy_checksum64 == 0)
		legacy_checksum64 = (uint64_t(palette_crc) << 32u) | uint64_t(sampled_low32);
	std::string strings;
	strings.append(policy_key);
	strings.push_back('\0');
	const uint32_t replacement_id_offset = uint32_t(strings.size());
	strings.append(replacement_id);
	strings.push_back('\0');
	const uint32_t sampled_object_id_offset = uint32_t(strings.size());
	strings.append(sampled_object_id);
	strings.push_back('\0');

	PHRBHeader header = {};
	header.magic[0] = 'P';
	header.magic[1] = 'H';
	header.magic[2] = 'R';
	header.magic[3] = 'B';
	header.version = 7;
	header.record_count = 1;
	header.asset_count = 1;
	header.record_table_offset = sizeof(PHRBHeader);
	header.asset_table_offset = header.record_table_offset + sizeof(PHRBRecordV4);
	header.string_table_offset = header.asset_table_offset + uint32_t(sizeof(PHRBAssetV7));
	header.blob_offset = header.string_table_offset + uint32_t(strings.size());

	PHRBRecordV4 record = {};
	record.policy_key_offset = 0;
	record.sampled_object_id_offset = sampled_object_id_offset;
	record.record_flags = record_flags;
	record.fmt = fmt;
	record.siz = siz;
	record.tex_offset = tex_offset;
	record.stride = stride;
	record.width = width;
	record.height = height;
	record.formatsize = formatsize;
	record.sampled_low32 = sampled_low32;
	record.sampled_entry_pcrc = palette_crc;
	record.sampled_sparse_pcrc = palette_crc;
	record.asset_candidate_count = 1;

	PHRBAssetV7 asset = {};
	asset.record_index = 0;
	asset.replacement_id_offset = replacement_id_offset;
	asset.width = repl_width;
	asset.height = repl_height;
	asset.format = asset_format;
	asset.texture_format = asset_texture_format;
	asset.pixel_type = asset_pixel_type;
	asset.legacy_formatsize = formatsize;
	asset.selector_checksum64 = selector_checksum64;
	asset.legacy_checksum64 = legacy_checksum64;
	asset.rgba_blob_offset = 0;
	asset.rgba_blob_size = uint64_t(blob.size());

	std::vector<uint8_t> data(sizeof(header) + sizeof(record) + sizeof(asset) + strings.size() + blob.size());
	size_t offset = 0;
	std::memcpy(data.data() + offset, &header, sizeof(header));
	offset += sizeof(header);
	std::memcpy(data.data() + offset, &record, sizeof(record));
	offset += sizeof(record);
	std::memcpy(data.data() + offset, &asset, sizeof(asset));
	offset += sizeof(asset);
	std::memcpy(data.data() + offset, strings.data(), strings.size());
	offset += strings.size();
	std::memcpy(data.data() + offset, blob.data(), blob.size());

	write_file(path, data);
}

// One version-8 package holding a Rice family record (key kind 0), a Jabo MD5
// record (kind 1) and a record of an unknown future kind (2), one RGBA8 asset each.
static void test_phrb_v8_jabo_records()
{
	const std::string dir = make_temp_dir();
	const uint8_t md5[16] = { 0x62, 0x0f, 0x0b, 0x67, 0xa9, 0x1f, 0x7f, 0x74,
	                          0x15, 0x1b, 0xc5, 0xbe, 0x74, 0x5b, 0x71, 0x10 };
	const std::string strings = std::string("record") + '\0';
	const uint32_t kinds[3] = { 0, 1, 2 };
	const std::vector<uint8_t> pixels[3] = {
		std::vector<uint8_t>(16, 0x11), std::vector<uint8_t>(16, 0x22), std::vector<uint8_t>(16, 0x33)
	};

	PHRBHeader header = {};
	std::memcpy(header.magic, "PHRB", 4);
	header.version = 8;
	header.record_count = 3;
	header.asset_count = 3;
	header.record_table_offset = sizeof(PHRBHeader);
	header.asset_table_offset = header.record_table_offset + 3 * sizeof(PHRBRecordV8);
	header.string_table_offset = header.asset_table_offset + 3 * sizeof(PHRBAssetV7);
	header.blob_offset = header.string_table_offset + uint32_t(strings.size());

	std::vector<uint8_t> data;
	append_bytes(data, header);
	for (uint32_t kind : kinds)
	{
		PHRBRecordV8 record = {};
		record.v4.record_flags = PHRB_RECORD_FLAG_RUNTIME_READY;
		record.v4.asset_candidate_count = 1;
		record.key_kind = kind;
		if (kind != 0)
			std::memcpy(record.key, md5, sizeof(md5));
		append_bytes(data, record);
	}
	for (uint32_t i = 0; i < 3; i++)
	{
		PHRBAssetV7 asset = {};
		asset.record_index = i;
		asset.width = 2;
		asset.height = 2;
		asset.format = GL_RGBA8;
		asset.texture_format = GL_RGBA;
		asset.pixel_type = GL_UNSIGNED_BYTE;
		asset.legacy_formatsize = kinds[i] == 0 ? 0x0104 : 0;
		asset.legacy_checksum64 = kinds[i] == 0 ? 0x1234abcdull : 0;
		asset.rgba_blob_offset = 16 * i;
		asset.rgba_blob_size = 16;
		append_bytes(data, asset);
	}
	data.insert(data.end(), strings.begin(), strings.end());
	for (const auto &blob : pixels)
		data.insert(data.end(), blob.begin(), blob.end());
	write_file(dir + "/jabo.phrb", data);

	ReplacementProvider provider;
	check(provider.load_cache_dir(dir), "provider should load a version-8 PHRB");
	provider.set_enabled(true);
	const auto stats = provider.get_stats();
	check(stats.entry_count == 2, "the unknown key kind record is skipped");
	check(stats.jabo_entry_count == 1 && stats.compat_entry_count == 1,
	      "Jabo entries are counted apart from Rice compat entries");
	check(provider.has_jabo_entries(), "a Jabo record enables Jabo lookups");
	check(provider.compat_checksum_index_.latest.size() == 1,
	      "Jabo entries stay out of the Rice checksum indices");

	ReplacementResolution resolution = {};
	check(provider.resolve_jabo_candidate(md5, &resolution), "the Jabo MD5 resolves");
	check(resolution.kind == ReplacementResolutionKind::JaboMd5 && resolution.meta.repl_w == 2,
	      "a Jabo hit reports its kind and replacement size");
	check(resolution.resolved_checksum64 == 0x747f1fa9670b0f62ull &&
	      resolution.resolved_selector_checksum64 == 0x10715b74bec51b15ull,
	      "a Jabo hit reports the little-endian MD5 halves");
	ReplacementImage image = {};
	check(provider.decode_rgba8_jabo(resolution.resolved_checksum64, resolution.resolved_selector_checksum64, 4, &image) &&
	      image.rgba8 == pixels[1],
	      "the Jabo asset decodes from the package blob");
	// The same Jabo key can be interpreted under different native formats.
	const std::vector<uint8_t> opaque = {
		0, 0, 0, 254, 30, 60, 90, 255, 255, 0, 1, 254, 255, 255, 255, 255
	};
	std::copy(opaque.begin(), opaque.end(), data.begin() + header.blob_offset);
	std::copy(opaque.begin(), opaque.end(), data.begin() + header.blob_offset + 16);
	write_file(dir + "/jabo.phrb", data);
	check(provider.load_cache_dir(dir), "reload opaque Jabo pixels");
	provider.set_enabled(true);
	for (uint32_t fmt = 0; fmt <= 4; fmt++)
	{
		check(provider.decode_rgba8_jabo(resolution.resolved_checksum64, resolution.resolved_selector_checksum64, fmt, &image),
		      "decode the same MD5 with a draw's native format");
		auto expected = opaque;
		if (fmt == 4)
		{
			expected[3] = 0;
			expected[7] = 60;
			expected[11] = 85;
		}
		check(image.rgba8 == expected, "only native I derives alpha for 254/255, preserving RGB and integer average");
	}
	check(provider.get_stats().jabo_intensity_alpha_textures == 1,
	      "only the native-I decode that rewrote alpha counts as an intensity-alpha texture");
	auto authored = opaque;
	authored[7] = 253;
	std::copy(authored.begin(), authored.end(), data.begin() + header.blob_offset + 16);
	write_file(dir + "/jabo.phrb", data);
	check(provider.load_cache_dir(dir), "reload a Jabo image with authored transparency");
	provider.set_enabled(true);
	check(provider.decode_rgba8_jabo(resolution.resolved_checksum64, resolution.resolved_selector_checksum64, 4, &image) &&
	      image.rgba8 == authored, "one alpha below 254 preserves the entire I image");
	check(provider.get_stats().jabo_intensity_alpha_textures == 0,
	      "a preserved I image is not counted, and reloading resets the counter");

	check(provider.decode_rgba8_compat_with_selector(0x1234abcdull, 0x0104, 0, &image) &&
	      image.rgba8 == opaque, "a non-Jabo native I record keeps opaque authored alpha");

	ReplacementMeta meta = {};
	check(provider.lookup(0x1234abcdull, 0x0104, &meta) && meta.repl_w == 2, "the Rice record still resolves");
	uint8_t other[16];
	std::memcpy(other, md5, sizeof(other));
	other[15] ^= 1;
	check(!provider.resolve_jabo_candidate(other, &resolution), "a different MD5 misses");

	provider.set_enabled(false);
	check(!provider.resolve_jabo_candidate(md5, &resolution), "a disabled provider resolves nothing");
	remove_tree(dir);
}
}

static std::vector<uint8_t> compress_zlib(const std::vector<uint8_t> &bytes)
{
	std::vector<uint8_t> compressed(compressBound(bytes.size()));
	uLongf size = compressed.size();
	check(compress2(compressed.data(), &size, bytes.data(), bytes.size(), 6) == Z_OK, "zlib fixture compression");
	compressed.resize(size);
	return compressed;
}

static void test_rgba8_decode_payloads()
{
	const auto dir = make_temp_dir();
	const std::vector<uint8_t> rgba = {
		1, 2, 3, 0, 17, 33, 65, 127, 255, 128, 64, 255,
		4, 5, 6, 1, 7, 8, 9, 128, 10, 11, 12, 254};
	const std::vector<uint8_t> compressible = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	// Equal stored/decoded lengths must still take the decompression path.
	const std::vector<uint8_t> compressed = {
		0x78, 0x9c, 0x63, 0x60, 0x64, 0x62, 0x66, 0x61, 0x65, 0x63, 0xe7, 0xe0,
		0xe4, 0xe2, 0xe6, 0xe1, 0x65, 0x80, 0x03, 0x00, 0x05, 0x6d, 0x00, 0x5c};
	struct DecodeCase
	{
		const char *name;
		uint32_t format;
		uint16_t texture_format;
		uint16_t pixel_type;
		std::vector<uint8_t> blob;
		std::vector<uint8_t> expected;
	};
	const DecodeCase cases[] = {
		{"exact raw RGBA", GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, rgba, rgba},
		{"compressed RGBA", 0x80008058u, GL_RGBA, GL_UNSIGNED_BYTE, compressed, compressible},
		// The loader cannot see decoded sizes; decode must refuse a short stream.
		{"compressed RGBA decoding short", 0x80008058u, GL_RGBA, GL_UNSIGNED_BYTE,
		 compress_zlib(std::vector<uint8_t>(compressible.begin(), compressible.end() - 1)), {}},
		{"packed RGB565", GL_RGB8, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
		 {0x00, 0xf8, 0xe0, 0x07, 0x1f, 0x00, 0, 0, 0xff, 0xff, 0x10, 0x84},
		 {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255,
		  0, 0, 0, 255, 255, 255, 255, 255, 132, 130, 132, 255}},
		// Packed RGBA is two bytes per pixel even though GL_RGBA8 names the internal format.
		{"packed RGBA4444", GL_RGBA8, GL_RGBA, 0x8033,
		 {0x0f, 0xf0, 0xf0, 0x0f, 0xff, 0x00, 0, 0, 0xff, 0xff, 0x21, 0x84},
		 {255, 0, 0, 255, 0, 255, 255, 0, 0, 0, 255, 255,
		  0, 0, 0, 0, 255, 255, 255, 255, 136, 68, 34, 17}},
	};
	for (const auto &test : cases)
	{
		const auto path = dir + "/decode.phrb";
		write_single_asset_phrb(path, "decode", "decode", "family", 0, 0, 0, 0, 0, 0,
		                        768, 0x12345678, 0, 0, 3, 2, test.blob,
		                        PHRB_RECORD_FLAG_RUNTIME_READY, 0x12345678,
		                        test.format, test.texture_format, test.pixel_type);
		ReplacementProvider provider;
		provider.set_enabled(true);
		check(provider.load_cache_dir(path), test.name);
		ReplacementImage image;
		image.meta.repl_w = 97;
		image.meta.indexed = true;
		image.rgba8 = {42};
		const bool decoded = provider.decode_rgba8(0x12345678, 768, &image);
		check(decoded == !test.expected.empty(), test.name);
		if (decoded)
		{
			check(image.rgba8 == test.expected, test.name);
			check(image.meta.repl_w == 3 && image.meta.repl_h == 2 &&
			      image.meta.orig_w == 0 && image.meta.orig_h == 0 &&
			      image.meta.vk_image_index == 0xffffffffu &&
			      !image.meta.has_mips && !image.meta.srgb && !image.meta.indexed,
			      "decoded image metadata must replace the caller's prior metadata");
		}
		else
			check(image.rgba8 == std::vector<uint8_t>({42}) && image.meta.repl_w == 97 && image.meta.indexed,
			      "failed decode must leave the caller's image intact");
	}
	ReplacementProvider provider;
	ReplacementProvider::Entry invalid;
	invalid.inline_blob = true;
	invalid.format = GL_RGBA8;
	invalid.texture_format = GL_RGBA;
	invalid.pixel_type = GL_UNSIGNED_BYTE;
	const std::pair<uint32_t, uint32_t> invalid_dims[] = {{0, 2}, {3, 0}, {UINT32_MAX, UINT32_MAX}};
	for (const auto &dims : invalid_dims)
	{
		invalid.width = dims.first;
		invalid.height = dims.second;
		invalid.blob = (dims.first == 0 || dims.second == 0) ?
		               std::vector<uint8_t>{} : std::vector<uint8_t>{1, 2, 3, 4};
		ReplacementImage image;
		check(!provider.decode_entry_rgba8(invalid, &image), "zero or overflowing RGBA dimensions must fail");
	}
	remove_tree(dir);
}

static void test_indexed_replacements()
{
	const auto dir = make_temp_dir();
	std::array<uint8_t, 2048> tmem = {};
	auto set_word = [&](unsigned index, uint16_t word) {
		tmem[index * 8] = uint8_t(word >> 8);
		tmem[index * 8 + 1] = uint8_t(word);
	};
	set_word(0, 0xf801); // Red, opaque.
	set_word(1, 0x07c0); // Green, transparent.
	set_word(15, 0x003f); // Blue, opaque.
	set_word(48, 0xffff); // Bank 3: white, opaque.
	set_word(49, 0x8421); // Half intensity, opaque.
	set_word(63, 0);
	set_word(255, 0x1234);
	// tmem_update.comp reads vram16[(addr >> 1) ^ 1] from word-swapped RDRAM.
	// The CPU identity mirror instead writes the byte-swapped emulated word.
	const uint8_t rdram[] = {0xc0, 0x07, 0x01, 0xf8};
	const uint16_t native_red = uint16_t(rdram[2]) | (uint16_t(rdram[3]) << 8);
	const uint16_t mirrored_red = (uint16_t(rdram[2]) << 8) | uint16_t(rdram[3]);
	auto mirror = tmem;
	mirror[0] = uint8_t(mirrored_red);
	mirror[1] = uint8_t(mirrored_red >> 8);
	check(ReplacementPalette::from_tmem(mirror.data(), mirror.size(), true, 0, false).words[0] == native_red,
		  "palette mirror byte order must reproduce native vram16 TLUT words");
	for (unsigned siz = 0; siz < 2; siz++)
	{
		const uint16_t fs = uint16_t(2 | (siz << 8));
		const std::vector<uint8_t> indices = {0, 1, uint8_t(siz ? 255 : 15), 0};
		const auto path = dir + (siz ? "/ci8.phrb" : "/ci4.phrb");
		write_single_asset_phrb(path, "ci", "ci", "family", 0, 0, 0, 0, 0, 0,
								fs, 0x12345678, 0, 0, 2, 2, compress_zlib(indices),
								PHRB_RECORD_FLAG_RUNTIME_READY, 0x12345678,
								0x800080e5u, 0x1900, GL_UNSIGNED_BYTE);
		ReplacementProvider provider;
		provider.set_enabled(true);
		check(provider.load_cache_dir(path), "indexed PHRB should load");
		ReplacementResolution resolution;
		check(provider.resolve_upload_candidate(fs, 2, siz, 0, 0, 2, 2, 0x12345678, 0xaabbccdd, 0, &resolution),
			  "texture-only indexed key should resolve with a live palette CRC");
		check(resolution.meta.indexed, "indexed metadata must reach the renderer");
		ReplacementImage image;
		check(!provider.decode_rgba8_compat_with_selector(0x12345678, fs, 0, &image),
			  "indexed decode without a live palette must fail");
		auto palette = ReplacementPalette::from_tmem(tmem.data(), tmem.size(), siz == 0, 0, false);
		check(provider.decode_rgba8_compat_with_selector(0x12345678, fs, 0, &image, &palette),
			  "CI4/CI8 RGBA16 expansion should decode");
		check(provider.get_stats().compat_ci_live_palette_textures == 0,
			  "an exact texture-only key is not a live-palette fallback");
		check(provider.decode_rgba8_compat_with_selector(0xaabbccdd12345678ull, fs, 0, &image, &palette) &&
			  provider.get_stats().compat_ci_live_palette_textures == 1,
			  "a palette-qualified key decoded through the texture-only CI entry counts once");
		check(std::vector<uint8_t>(image.rgba8.begin(), image.rgba8.begin() + 8) ==
				  std::vector<uint8_t>({255, 0, 0, 255, 0, 255, 0, 0}),
			  "RGBA16 TLUT must use native bit replication and alpha");
		const auto first = image.rgba8;
		const auto first_hash = palette.hash();
		HiresReplacement::HiresKey cache_key;
		cache_key.source = HiresReplacement::HiresKeySource::Compat;
		cache_key.checksum64 = 0x12345678;
		cache_key.formatsize = fs;
		cache_key.indexed_palette_hash = first_hash;
		std::unordered_map<HiresReplacement::HiresKey, std::vector<uint8_t>, HiresReplacement::HiresKeyHasher> cache;
		cache.emplace(cache_key, first);
		auto changed = palette;
		changed.words[0] = 0x003f;
		check(changed.hash() != first_hash, "palette bytes must distinguish cached expansions");
		check(provider.decode_rgba8_compat_with_selector(0x12345678, fs, 0, &image, &changed) && image.rgba8 != first,
			  "one indexed record and two palettes must produce two images");
		auto changed_key = cache_key;
		changed_key.indexed_palette_hash = changed.hash();
		check(cache.find(changed_key) == cache.end(), "a changed TLUT must miss the resident-image cache");
		cache.emplace(changed_key, image.rgba8);
		check(cache.size() == 2 && cache.at(cache_key) == first && cache.at(changed_key) == image.rgba8,
			  "the actual renderer cache key must retain distinct palette expansions");
		check(ReplacementPalette::from_tmem(tmem.data(), tmem.size(), siz == 0, 0, false).hash() == first_hash,
			  "the same palette should reuse its cache identity");
		palette.ia16 = true;
		check(palette.hash() != first_hash, "TLUT type must distinguish cached expansions");
		check(provider.decode_rgba8_compat_with_selector(0x12345678, fs, 0, &image, &palette), "IA16 TLUT should decode");
		check(std::vector<uint8_t>(image.rgba8.begin(), image.rgba8.begin() + 4) ==
				  std::vector<uint8_t>({248, 248, 248, 1}), "IA16 must use high-byte intensity and low-byte alpha");
		if (siz)
			check(std::vector<uint8_t>(image.rgba8.begin() + 8, image.rgba8.begin() + 12) ==
					  std::vector<uint8_t>({18, 18, 18, 52}), "CI8 should address entry 255");
		else
		{
			auto bank = ReplacementPalette::from_tmem(tmem.data(), tmem.size(), true, 3, false);
			check(bank.hash() != first_hash, "CI4 bank selection must affect the cache identity");
			check(provider.decode_rgba8_compat_with_selector(0x12345678, fs, 0, &image, &bank), "CI4 bank 3 should decode");
			check(std::vector<uint8_t>(image.rgba8.begin(), image.rgba8.begin() + 8) ==
					  std::vector<uint8_t>({255, 255, 255, 255, 132, 132, 132, 255}), "CI4 must select the nonzero bank");
			bank.ia16 = true;
			check(provider.decode_rgba8_compat_with_selector(0x12345678, fs, 0, &image, &bank) && image.rgba8[4] == 132 && image.rgba8[7] == 33,
				  "CI4 nonzero bank should also use IA16");
		}
		auto invalid = ReplacementPalette::from_tmem(nullptr, 0, siz == 0, 0, false);
		check(!provider.decode_rgba8_compat_with_selector(0x12345678, fs, 0, &image, &invalid), "invalid TLUT must fail");
	}
	const auto qualified_path = dir + "/qualified.phrb";
	write_single_asset_phrb(qualified_path, "ci-pal", "ci-pal", "family", 0, 0, 0, 0, 0, 0,
							2, 0x87654321, 0xaabbccdd, 0, 2, 2, {0, 1, 0, 1},
							PHRB_RECORD_FLAG_RUNTIME_READY, 0xaabbccdd87654321ull, 0x80e5, 0x1900);
	ReplacementProvider qualified;
	qualified.set_enabled(true);
	check(qualified.load_cache_dir(qualified_path), "palette-qualified indexed package should load");
	ReplacementMeta meta;
	check(qualified.lookup(0xaabbccdd87654321ull, 2, &meta), "qualified indexed key must match its palette CRC");
	check(!qualified.lookup(0x1122334487654321ull, 2, &meta), "qualified indexed key must not ignore its palette CRC");
	const auto coloured_path = dir + "/coloured.phrb";
	write_single_asset_phrb(coloured_path, "coloured", "coloured", "family", 0, 0, 0, 0, 0, 0,
							2, 0x87654321, 0, 0, 2, 2, std::vector<uint8_t>(16, 255));
	ReplacementProvider coloured;
	coloured.set_enabled(true);
	check(coloured.load_cache_dir(coloured_path), "pre-coloured CI package should load");
	check(!coloured.lookup(0xaabbccdd87654321ull, 2, &meta), "pre-coloured CI lookup policy must remain unchanged");
	remove_tree(dir);
}

// Version-8 family records let the same tiny fixture exercise both key lanes
// without depending on converter behavior or copyrighted inputs.
struct OverlapKey
{
	bool jabo;
	uint64_t checksum;
	uint64_t selector = 0;
	uint16_t formatsize = 258;
};

static void write_overlap_package(const std::string &path, const std::vector<OverlapKey> &keys)
{
	PHRBHeader header = {};
	std::memcpy(header.magic, "PHRB", 4);
	header.version = 8;
	header.record_count = header.asset_count = uint32_t(keys.size());
	header.record_table_offset = sizeof(header);
	header.asset_table_offset = header.record_table_offset + header.record_count * sizeof(PHRBRecordV8);
	header.string_table_offset = header.asset_table_offset + header.asset_count * sizeof(PHRBAssetV7);
	header.blob_offset = header.string_table_offset + 1;
	std::vector<uint8_t> data;
	append_bytes(data, header);
	for (const auto &key : keys)
	{
		PHRBRecordV8 record = {};
		record.v4.record_flags = PHRB_RECORD_FLAG_RUNTIME_READY;
		record.v4.asset_candidate_count = 1;
		record.key_kind = key.jabo ? 1 : 0;
		for (unsigned i = 0; i < 8; i++)
		{
			record.key[i] = uint8_t(key.checksum >> (8 * i));
			record.key[i + 8] = uint8_t(key.selector >> (8 * i));
		}
		append_bytes(data, record);
	}
	for (size_t i = 0; i < keys.size(); i++)
	{
		PHRBAssetV7 asset = {};
		asset.record_index = uint32_t(i);
		asset.width = asset.height = 1;
		asset.format = GL_RGBA8;
		asset.texture_format = GL_RGBA;
		asset.pixel_type = GL_UNSIGNED_BYTE;
		asset.legacy_checksum64 = keys[i].checksum;
		asset.selector_checksum64 = keys[i].selector;
		asset.legacy_formatsize = keys[i].formatsize;
		asset.rgba_blob_size = 4;
		append_bytes(data, asset);
	}
	data.insert(data.end(), 5, 0); // Empty string table and one shared RGBA texel.
	write_file(path, data);
}

static void test_package_overlap()
{
	const auto dir = make_temp_dir();
	const auto a = dir + "/a.phrb", b = dir + "/b.phrb";
	std::vector<OverlapKey> full;
	for (uint64_t i = 1; i <= 20; i++)
		full.push_back({ false, i });
	ReplacementProvider provider;
	write_overlap_package(a, full);
	write_overlap_package(b, full);
	check(provider.load_cache_dir(dir), "two overlapping packages load");
	auto stats = provider.package_stats();
	check(stats.size() == 2 && stats[0].records == 20 && stats[0].hidden_by_later == 20 &&
	      stats[0].mostly_hidden() && stats[0].hidden_by == b && !stats[1].mostly_hidden(),
	      "a fully overridden package is flagged and names the later winner");
	provider.set_enabled(true);
	const auto *winner = provider.find_any_entry(1, 258, 0);
	check(winner && winner->source_path == b,
	      "overlap diagnostics preserve later-wins lookup");

	write_overlap_package(b, { full[0] });
	check(provider.load_cache_dir(dir), "a 5 percent add-on after the full pack loads");
	stats = provider.package_stats();
	check(stats[0].hidden_by_later == 1 && !stats[0].mostly_hidden() && !stats[1].mostly_hidden(),
	      "a small later add-on does not warn");
	write_overlap_package(a, { full[0] });
	write_overlap_package(b, full);
	check(provider.load_cache_dir(dir), "an add-on before the full pack loads");
	stats = provider.package_stats();
	check(stats[0].mostly_hidden() && stats[0].records == 1 && !stats[1].mostly_hidden(),
	      "the add-on sorted before the full pack warns");

	write_overlap_package(a, full);
	write_overlap_package(b, std::vector<OverlapKey>(full.begin(), full.begin() + 10));
	check(provider.load_cache_dir(dir), "half-overlap packages load");
	stats = provider.package_stats();
	check(stats[0].hidden_by_later == 10 && !stats[0].mostly_hidden(), "exactly 50 percent does not warn");

	write_overlap_package(a, { { false, 1 }, { true, 1 } });
	write_overlap_package(b, { { false, 1 }, { false, 1 } });
	check(provider.load_cache_dir(dir), "mixed lanes and a duplicate key load");
	stats = provider.package_stats();
	check(stats[0].records == 2 && stats[0].hidden_by_later == 1 && !stats[0].mostly_hidden() &&
	      stats[1].records == 1 && stats[1].hidden_by_later == 0,
	      "Rice and Jabo keys are counted separately and package-local duplicate keys count once");
	write_overlap_package(b, { { true, 1 } });
	check(provider.load_cache_dir(dir), "a Jabo-only overlay loads");
	stats = provider.package_stats();
	check(stats[0].hidden_by_later == 1 && !stats[0].mostly_hidden(), "Jabo MD5 overlap does not hide a Rice key");

	write_overlap_package(a, { { false, 1, 2, 258 } });
	write_overlap_package(b, { { false, 1, 3, 258 }, { false, 1, 2, 2 } });
	check(provider.load_cache_dir(dir), "Rice selector and format variants load");
	stats = provider.package_stats();
	check(stats[0].hidden_by_later == 0, "Rice overlap requires checksum, selector and formatsize");
	provider.clear();
	check(provider.package_stats().empty(), "provider reset clears overlap accounting");
	remove_tree(dir);
}

// A small valid package for the mutation table: record 0 is a sampled Rice
// record with two palette CRCs (two entries) and a raw 2x2 RGBA asset; record 1
// holds a zlib asset and is a family record (version 7) or Jabo MD5 (version 8);
// record 2 is not runtime-ready and has an empty payload. Three entries load.
struct MutationPack
{
	uint32_t version = 7;
	size_t record_size = 0;
	PHRBHeader header = {};
	std::vector<uint8_t> bytes;

	size_t record_field(unsigned record, size_t field) const
	{
		return header.record_table_offset + record * record_size + field;
	}

	size_t asset_field(unsigned asset, size_t field) const
	{
		return header.asset_table_offset + asset * sizeof(PHRBAssetV7) + field;
	}

	template <typename T>
	void poke(size_t offset, T value)
	{
		check(offset + sizeof(T) <= bytes.size(), "mutation stays inside the package");
		std::memcpy(bytes.data() + offset, &value, sizeof(T));
	}
};

static const std::vector<uint8_t> mutation_pixels = {
	255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 128};

static MutationPack build_mutation_pack(uint32_t version, std::vector<uint8_t> zlib_payload = {})
{
	if (zlib_payload.empty())
		zlib_payload = compress_zlib(mutation_pixels);
	const std::string strings = std::string("policy\0object\0replacement\0", 26);
	const uint32_t object_offset = 7;
	const uint32_t replacement_offset = 14;

	MutationPack pack;
	pack.version = version;
	pack.record_size = version >= 8 ? sizeof(PHRBRecordV8) : sizeof(PHRBRecordV4);
	PHRBHeader &header = pack.header;
	std::memcpy(header.magic, "PHRB", 4);
	header.version = version;
	header.record_count = 3;
	header.asset_count = 3;
	header.record_table_offset = sizeof(PHRBHeader);
	header.asset_table_offset = header.record_table_offset + uint32_t(3 * pack.record_size);
	header.string_table_offset = header.asset_table_offset + uint32_t(3 * sizeof(PHRBAssetV7));
	header.blob_offset = header.string_table_offset + uint32_t(strings.size());
	append_bytes(pack.bytes, header);

	for (uint32_t r = 0; r < 3; r++)
	{
		PHRBRecordV8 record = {};
		record.v4.policy_key_offset = 0;
		record.v4.sampled_object_id_offset = object_offset;
		record.v4.record_flags = r == 2 ? 0 : PHRB_RECORD_FLAG_RUNTIME_READY;
		record.v4.formatsize = 768;
		record.v4.sampled_low32 = 0x11110000u + r;
		record.v4.asset_candidate_count = 1;
		if (r != 1)
		{
			record.v4.siz = 3;
			record.v4.width = 2;
			record.v4.height = 2;
			record.v4.stride = 8;
			record.v4.sampled_entry_pcrc = 0xaaaa0000u;
			record.v4.sampled_sparse_pcrc = 0xbbbb0000u;
		}
		if (r == 1 && version >= 8)
		{
			record.key_kind = 1;
			for (unsigned i = 0; i < 16; i++)
				record.key[i] = uint8_t(0xa0 + i);
		}
		if (version >= 8)
			append_bytes(pack.bytes, record);
		else
			append_bytes(pack.bytes, record.v4);
	}

	const uint64_t payload_sizes[3] = { mutation_pixels.size(), zlib_payload.size(), 0 };
	uint64_t payload_offset = 0;
	for (uint32_t a = 0; a < 3; a++)
	{
		PHRBAssetV7 asset = {};
		asset.record_index = a;
		asset.replacement_id_offset = replacement_offset;
		asset.width = 2;
		asset.height = 2;
		asset.format = a == 1 ? 0x80008058u : GL_RGBA8;
		asset.texture_format = GL_RGBA;
		asset.pixel_type = GL_UNSIGNED_BYTE;
		asset.legacy_formatsize = 768;
		asset.legacy_checksum64 = 0x2222000011110001ull;
		asset.rgba_blob_offset = payload_offset;
		asset.rgba_blob_size = payload_sizes[a];
		payload_offset += payload_sizes[a];
		append_bytes(pack.bytes, asset);
	}
	pack.bytes.insert(pack.bytes.end(), strings.begin(), strings.end());
	pack.bytes.insert(pack.bytes.end(), mutation_pixels.begin(), mutation_pixels.end());
	pack.bytes.insert(pack.bytes.end(), zlib_payload.begin(), zlib_payload.end());
	return pack;
}

template <typename Map, typename KeyText>
static void append_sorted_map(std::ostringstream &out, const char *name, const Map &map, KeyText key_text)
{
	std::vector<std::string> lines;
	for (const auto &item : map)
		lines.push_back(key_text(item));
	std::sort(lines.begin(), lines.end());
	out << name << ' ' << lines.size() << '\n';
	for (const auto &line : lines)
		out << line << '\n';
}

static std::string sampled_key_text(const ReplacementProvider::SampledLookupKey &key)
{
	std::ostringstream out;
	out << key.sampled_fmt << ',' << key.sampled_siz << ',' << key.sampled_tex_offset << ',' << key.sampled_stride << ','
	    << key.sampled_width << ',' << key.sampled_height << ',' << key.sampled_low32 << ',' << key.sampled_palette_crc << ','
	    << key.formatsize << ',' << key.selector_checksum64;
	return out.str();
}

// Everything a load can change, other than the rejection report.
static std::string provider_state(const ReplacementProvider &provider)
{
	std::ostringstream out;
	out << "tables " << provider.string_tables_.size() << " entries " << provider.entries_.size() << '\n';
	for (const auto &entry : provider.entries_)
		out << entry.source_path << '|' << entry.phrb_policy_key << '|' << entry.phrb_replacement_id << '|'
		    << entry.phrb_sampled_object_id << '|' << entry.checksum64 << '|' << entry.selector_checksum64 << '|'
		    << entry.formatsize << '|' << entry.data_offset << '|' << entry.data_size << '|' << entry.width << 'x'
		    << entry.height << '|' << entry.format << '|' << entry.is_jabo << entry.has_native_sampled_identity << '\n';
	auto checksum_text = [](const ReplacementProvider::ChecksumIndex &index, std::ostringstream &target, const char *name) {
		append_sorted_map(target, name, index.latest, [](const std::pair<const uint64_t, size_t> &item) {
			return std::to_string(item.first) + "=" + std::to_string(item.second);
		});
		append_sorted_map(target, name, index.exact, [](const std::pair<const ReplacementProvider::ChecksumLookupKey, size_t> &item) {
			return std::to_string(item.first.checksum64) + "," + std::to_string(item.first.selector_checksum64) + "," +
			       std::to_string(item.first.formatsize) + "=" + std::to_string(item.second);
		});
	};
	checksum_text(provider.native_checksum_index_, out, "native");
	checksum_text(provider.compat_checksum_index_, out, "compat");
	append_sorted_map(out, "sampled", provider.sampled_index_, [](const std::pair<const ReplacementProvider::SampledLookupKey, size_t> &item) {
		return sampled_key_text(item.first) + "=" + std::to_string(item.second);
	});
	append_sorted_map(out, "duplicates", provider.sampled_duplicate_index_, [](const std::pair<const ReplacementProvider::SampledLookupKey, uint32_t> &item) {
		return sampled_key_text(item.first) + "=" + std::to_string(item.second);
	});
	append_sorted_map(out, "reordered", provider.sampled_duplicate_reordered_, [](const ReplacementProvider::SampledLookupKey &key) {
		return sampled_key_text(key);
	});
	append_sorted_map(out, "family", provider.sampled_family_index_,
	                  [](const std::pair<const ReplacementProvider::SampledFamilyLookupKey, ReplacementProvider::SampledFamilySelectors> &item) {
		const auto &key = item.first;
		return std::to_string(key.sampled_low32) + "," + std::to_string(key.sampled_palette_crc) + "," +
		       std::to_string(key.formatsize) + "," + std::to_string(key.sampled_width) + "=" +
		       std::to_string(item.second.selector_checksum64) + (item.second.mixed ? " mixed" : "");
	});
	append_sorted_map(out, "jabo", provider.jabo_index_, [](const std::pair<const std::array<uint64_t, 2>, size_t> &item) {
		return std::to_string(item.first[0]) + "," + std::to_string(item.first[1]) + "=" + std::to_string(item.second);
	});
	for (const auto &package : provider.package_stats())
		out << "package " << package.path << ' ' << package.records << ' ' << package.hidden_by_later << '\n';
	return out.str();
}

enum class MutationOutcome
{
	Rejected,
	Loads,
	// Loads, but the loader cannot see the payload problem; decode refuses it.
	DecodeFails,
};

struct MutationCase
{
	std::string name;
	std::function<void(MutationPack &)> mutate;
	MutationOutcome outcome;
	// Rejected: a fragment of the reason, naming the check that fired.
	std::string reason;
	size_t entries = 3;
	bool version8_only = false;
	std::vector<uint8_t> zlib_payload = {};
};

static std::vector<MutationCase> mutation_cases(const MutationPack &base)
{
	using O = MutationOutcome;
	std::vector<MutationCase> cases;
	cases.push_back({"unmodified", [](MutationPack &) {}, O::Loads, ""});

	const size_t size = base.bytes.size();
	const size_t boundaries[] = {
		0, 31, sizeof(PHRBHeader), base.header.asset_table_offset, base.header.string_table_offset,
		base.header.blob_offset, base.header.blob_offset + mutation_pixels.size(), size - 1 };
	for (size_t boundary : boundaries)
		for (size_t length : { boundary, boundary + 1 })
			if (length < size && (length == boundary || boundary + 1 < size))
				cases.push_back({"truncated to " + std::to_string(length) + " bytes",
				                 [length](MutationPack &pack) { pack.bytes.resize(length); }, O::Rejected, ""});

	struct HeaderField { const char *name; size_t offset; const char *zero_reason; const char *max_reason; };
	const HeaderField header_fields[] = {
		{"record_count", offsetof(PHRBHeader, record_count), "asset table offset", "record count"},
		{"asset_count", offsetof(PHRBHeader, asset_count), "string table offset", "asset count"},
		{"record_table_offset", offsetof(PHRBHeader, record_table_offset), "record table offset", "record table offset"},
		{"asset_table_offset", offsetof(PHRBHeader, asset_table_offset), "asset table offset", "asset table offset"},
		{"string_table_offset", offsetof(PHRBHeader, string_table_offset), "string table offset", "string table offset"},
		{"blob_offset", offsetof(PHRBHeader, blob_offset), "precedes the string table", "over the limit"},
	};
	for (const auto &field : header_fields)
	{
		const size_t offset = field.offset;
		cases.push_back({std::string("header ") + field.name + " zero",
		                 [offset](MutationPack &pack) { pack.poke<uint32_t>(offset, 0); }, O::Rejected, field.zero_reason});
		cases.push_back({std::string("header ") + field.name + " max",
		                 [offset](MutationPack &pack) { pack.poke<uint32_t>(offset, UINT32_MAX); }, O::Rejected, field.max_reason});
	}

	// Tables that overlap their neighbours, and a payload section that cuts into the strings.
	cases.push_back({"asset table overlaps records", [](MutationPack &pack) {
		pack.poke<uint32_t>(offsetof(PHRBHeader, asset_table_offset), pack.header.record_table_offset); },
		O::Rejected, "asset table offset"});
	cases.push_back({"string table overlaps assets", [](MutationPack &pack) {
		pack.poke<uint32_t>(offsetof(PHRBHeader, string_table_offset), pack.header.asset_table_offset + 8); },
		O::Rejected, "string table offset"});
	cases.push_back({"payloads overlap strings", [](MutationPack &pack) {
		pack.poke<uint32_t>(offsetof(PHRBHeader, blob_offset), pack.header.string_table_offset + 1); },
		O::Rejected, "NUL"});
	cases.push_back({"header claims the maximum tables in a small file", [](MutationPack &pack) {
		const uint32_t count = 1u << 18;
		const uint32_t asset_table = uint32_t(sizeof(PHRBHeader) + count * pack.record_size);
		pack.poke<uint32_t>(offsetof(PHRBHeader, record_count), count);
		pack.poke<uint32_t>(offsetof(PHRBHeader, asset_count), count);
		pack.poke<uint32_t>(offsetof(PHRBHeader, asset_table_offset), asset_table);
		pack.poke<uint32_t>(offsetof(PHRBHeader, string_table_offset), uint32_t(asset_table + count * sizeof(PHRBAssetV7)));
		pack.poke<uint32_t>(offsetof(PHRBHeader, blob_offset), uint32_t(asset_table + count * sizeof(PHRBAssetV7)));
	}, O::Rejected, "past the end"});

	struct RecordField { const char *name; size_t offset; O zero; const char *zero_reason; const char *max_reason; };
	const RecordField record_fields[] = {
		{"policy_key_offset", offsetof(PHRBRecordV4, policy_key_offset), O::Loads, "", "string offset"},
		{"sampled_object_id_offset", offsetof(PHRBRecordV4, sampled_object_id_offset), O::Loads, "", "string offset"},
		{"formatsize", offsetof(PHRBRecordV4, formatsize), O::Loads, "", "formatsize"},
		{"asset_candidate_count", offsetof(PHRBRecordV4, asset_candidate_count), O::Rejected, "names record", "claims"},
	};
	for (unsigned record = 0; record < 2; record++)
		for (const auto &field : record_fields)
		{
			const size_t offset = base.record_field(record, field.offset);
			const std::string name = "record " + std::to_string(record) + " " + field.name;
			cases.push_back({name + " zero", [offset](MutationPack &pack) { pack.poke<uint32_t>(offset, 0); },
			                 field.zero, field.zero_reason});
			cases.push_back({name + " max", [offset](MutationPack &pack) { pack.poke<uint32_t>(offset, UINT32_MAX); },
			                 O::Rejected, field.max_reason});
		}
	cases.push_back({"record 1 key kind unknown", [](MutationPack &pack) {
		pack.poke<uint32_t>(pack.record_field(1, offsetof(PHRBRecordV8, key_kind)), UINT32_MAX); },
		O::Loads, "", 2, true});

	struct AssetField { const char *name; size_t offset; bool wide; O zero; const char *zero_reason; const char *max_reason; };
	const AssetField asset_fields[] = {
		{"record_index", offsetof(PHRBAssetV7, record_index), false, O::Rejected, "names record", "names record"},
		{"replacement_id_offset", offsetof(PHRBAssetV7, replacement_id_offset), false, O::Loads, "", "string offset"},
		{"legacy_source_path_offset", offsetof(PHRBAssetV7, legacy_source_path_offset), false, O::Loads, "", "string offset"},
		{"rgba_rel_path_offset", offsetof(PHRBAssetV7, rgba_rel_path_offset), false, O::Loads, "", "string offset"},
		{"variant_group_id_offset", offsetof(PHRBAssetV7, variant_group_id_offset), false, O::Loads, "", "string offset"},
		{"width", offsetof(PHRBAssetV7, width), false, O::Rejected, "each side", "each side"},
		{"height", offsetof(PHRBAssetV7, height), false, O::Rejected, "each side", "each side"},
		{"texture_format", offsetof(PHRBAssetV7, texture_format), false, O::Rejected, "unsupported", "unsupported"},
		{"pixel_type", offsetof(PHRBAssetV7, pixel_type), false, O::Rejected, "unsupported", "unsupported"},
		{"legacy_formatsize", offsetof(PHRBAssetV7, legacy_formatsize), false, O::Loads, "", "formatsize"},
		{"rgba_blob_offset", offsetof(PHRBAssetV7, rgba_blob_offset), true, O::DecodeFails, "", "outside"},
		{"rgba_blob_size", offsetof(PHRBAssetV7, rgba_blob_size), true, O::Rejected, "compressed bytes", "outside"},
	};
	for (const auto &field : asset_fields)
	{
		// Asset 1 is the zlib payload; asset 0 starts at payload offset 0 already.
		const size_t offset = base.asset_field(1, field.offset);
		const std::string name = std::string("asset 1 ") + field.name;
		const bool wide = field.wide;
		cases.push_back({name + " zero", [offset, wide](MutationPack &pack) {
			if (wide) pack.poke<uint64_t>(offset, 0); else pack.poke<uint32_t>(offset, 0); },
			field.zero, field.zero_reason});
		cases.push_back({name + " max", [offset, wide](MutationPack &pack) {
			if (wide) pack.poke<uint64_t>(offset, UINT64_MAX); else pack.poke<uint32_t>(offset, UINT32_MAX); },
			O::Rejected, field.max_reason});
	}
	cases.push_back({"asset 0 raw payload one byte short", [](MutationPack &pack) {
		pack.poke<uint64_t>(pack.asset_field(0, offsetof(PHRBAssetV7, rgba_blob_size)), mutation_pixels.size() - 1); },
		O::Rejected, "stores 15 bytes"});
	cases.push_back({"asset 0 raw payload one byte long", [](MutationPack &pack) {
		pack.poke<uint64_t>(pack.asset_field(0, offsetof(PHRBAssetV7, rgba_blob_size)), mutation_pixels.size() + 1); },
		O::Rejected, "stores 17 bytes"});
	cases.push_back({"asset 0 raw payload marked compressed", [](MutationPack &pack) {
		pack.poke<uint32_t>(pack.asset_field(0, offsetof(PHRBAssetV7, format)), 0x80008058u); },
		O::DecodeFails, ""});
	cases.push_back({"asset 1 compressed payload marked raw", [](MutationPack &pack) {
		pack.poke<uint32_t>(pack.asset_field(1, offsetof(PHRBAssetV7, format)), GL_RGBA8); },
		O::Rejected, "bytes per pixel"});
	cases.push_back({"asset 1 over the pixel limit", [](MutationPack &pack) {
		pack.poke<uint32_t>(pack.asset_field(1, offsetof(PHRBAssetV7, width)), 16384);
		pack.poke<uint32_t>(pack.asset_field(1, offsetof(PHRBAssetV7, height)), 16384); },
		O::Rejected, "pixels"});
	cases.push_back({"asset 1 declares a huge decoded size", [](MutationPack &pack) {
		pack.poke<uint32_t>(pack.asset_field(1, offsetof(PHRBAssetV7, width)), 8192);
		pack.poke<uint32_t>(pack.asset_field(1, offsetof(PHRBAssetV7, height)), 8192); },
		O::DecodeFails, ""});
	cases.push_back({"asset 1 shares asset 0's payload", [](MutationPack &pack) {
		pack.poke<uint32_t>(pack.asset_field(1, offsetof(PHRBAssetV7, format)), GL_RGBA8);
		pack.poke<uint64_t>(pack.asset_field(1, offsetof(PHRBAssetV7, rgba_blob_offset)), 0);
		pack.poke<uint64_t>(pack.asset_field(1, offsetof(PHRBAssetV7, rgba_blob_size)), mutation_pixels.size()); },
		O::Loads, ""});
	// Payload ranges of assets that do not load are still checked; their other fields are not.
	cases.push_back({"asset 2 (not runtime-ready) width max", [](MutationPack &pack) {
		pack.poke<uint32_t>(pack.asset_field(2, offsetof(PHRBAssetV7, width)), UINT32_MAX); },
		O::Loads, ""});
	cases.push_back({"asset 2 (not runtime-ready) payload outside", [](MutationPack &pack) {
		pack.poke<uint64_t>(pack.asset_field(2, offsetof(PHRBAssetV7, rgba_blob_size)), 1); },
		O::Rejected, "outside"});
	cases.push_back({"string table without final NUL", [](MutationPack &pack) {
		pack.bytes[pack.header.blob_offset - 1] = 'x'; },
		O::Rejected, "NUL"});

	auto short_stream = mutation_pixels;
	short_stream.pop_back();
	auto long_stream = mutation_pixels;
	long_stream.push_back(7);
	cases.push_back({"zlib payload decodes one byte short", [](MutationPack &) {}, O::DecodeFails, "", 3, false,
	                 compress_zlib(short_stream)});
	cases.push_back({"zlib payload decodes one byte long", [](MutationPack &) {}, O::DecodeFails, "", 3, false,
	                 compress_zlib(long_stream)});
	return cases;
}

// Counts payloads, since asset 0 backs two entries.
static size_t count_decode_failures(const ReplacementProvider &provider)
{
	std::vector<std::pair<uint64_t, uint32_t>> failures;
	for (const auto &entry : provider.entries_)
	{
		ReplacementImage image;
		if (!provider.decode_entry_rgba8(entry, &image))
			failures.emplace_back(entry.data_offset, entry.data_size);
		else
			check(image.rgba8 == mutation_pixels, "a decoded mutation-pack entry keeps its pixels");
	}
	std::sort(failures.begin(), failures.end());
	return size_t(std::unique(failures.begin(), failures.end()) - failures.begin());
}

// Every length, count, offset and size the loader reads, mutated one at a time.
// A rejected package must not change a provider that already holds another one.
static void test_package_mutations()
{
	const auto dir = make_temp_dir();
	const auto good_path = dir + "/a-good.phrb";
	const auto bad_path = dir + "/b-mutated.phrb";
	for (uint32_t version : { 7u, 8u })
	{
		const MutationPack base = build_mutation_pack(version);
		write_file(good_path, base.bytes);
		ReplacementProvider good_only;
		check(good_only.load_cache_dir(good_path), "the mutation base package loads");
		const std::string good_state = provider_state(good_only);

		for (const auto &test : mutation_cases(base))
		{
			if (test.version8_only && version < 8)
				continue;
			const std::string name = "v" + std::to_string(version) + " " + test.name;
			MutationPack pack = test.zlib_payload.empty() ? base : build_mutation_pack(version, test.zlib_payload);
			test.mutate(pack);
			write_file(bad_path, pack.bytes);

			ReplacementProvider provider;
			largest_allocation = 0;
			allocation_tracking = true;
			const bool loaded = provider.load_cache_dir(bad_path);
			allocation_tracking = false;
			check(largest_allocation < (64u << 10), (name + ": loading allocates no large block").c_str());

			if (test.outcome == MutationOutcome::Rejected)
			{
				check(!loaded && provider.load_error() == ReplacementLoadStatus::Invalid &&
				      provider.rejected_path() == bad_path && provider.entry_count() == 0,
				      (name + ": rejected").c_str());
				check(provider.rejected_reason().find(test.reason) != std::string::npos,
				      (name + ": rejected by the intended check, got: " + provider.rejected_reason()).c_str());

				// Next to an accepted package, in either order, the rejected one leaves no trace.
				for (const bool good_first : { true, false })
				{
					ReplacementProvider layered;
					if (good_first)
					{
						check(layered.load_cache_dir(good_path), (name + ": base loads first").c_str());
						check(!layered.load_cache_dir(bad_path, true), (name + ": appended mutation adds nothing").c_str());
					}
					else
					{
						check(!layered.load_cache_dir(bad_path), (name + ": mutation alone adds nothing").c_str());
						check(layered.load_cache_dir(good_path, true), (name + ": base appends").c_str());
					}
					check(provider_state(layered) == good_state, (name + ": provider state matches the base alone").c_str());
				}
				ReplacementProvider directory;
				check(directory.load_cache_dir(dir), (name + ": directory still loads the base").c_str());
				check(directory.rejected_path() == bad_path && provider_state(directory) == good_state,
				      (name + ": directory load matches the base alone").c_str());
				continue;
			}

			check(loaded && provider.load_error() == ReplacementLoadStatus::NotFound && provider.rejected_reason().empty(),
			      (name + ": loads").c_str());
			check(provider.entry_count() == test.entries, (name + ": entry count").c_str());
			const size_t failures = count_decode_failures(provider);
			check(failures == (test.outcome == MutationOutcome::DecodeFails ? 1u : 0u), (name + ": decode result").c_str());
		}
	}
	remove_tree(dir);
}

// Many records sharing one long string must not copy it per entry.
static void test_shared_string_table()
{
	const auto dir = make_temp_dir();
	const auto path = dir + "/strings.phrb";
	const uint32_t records = 2000;
	const std::string long_string(64u << 10, 's');

	PHRBHeader header = {};
	std::memcpy(header.magic, "PHRB", 4);
	header.version = 7;
	header.record_count = records;
	header.asset_count = records;
	header.record_table_offset = sizeof(PHRBHeader);
	header.asset_table_offset = header.record_table_offset + records * uint32_t(sizeof(PHRBRecordV4));
	header.string_table_offset = header.asset_table_offset + records * uint32_t(sizeof(PHRBAssetV7));
	header.blob_offset = header.string_table_offset + uint32_t(long_string.size() + 1);
	std::vector<uint8_t> data;
	append_bytes(data, header);
	for (uint32_t r = 0; r < records; r++)
	{
		PHRBRecordV4 record = {};
		record.record_flags = PHRB_RECORD_FLAG_RUNTIME_READY;
		record.siz = 3;
		record.width = record.height = 1;
		record.formatsize = 768;
		record.sampled_low32 = r;
		record.sampled_entry_pcrc = 0xaaaa0000u;
		record.sampled_sparse_pcrc = 0xbbbb0000u;
		record.asset_candidate_count = 1;
		append_bytes(data, record);
	}
	for (uint32_t a = 0; a < records; a++)
	{
		PHRBAssetV7 asset = {};
		asset.record_index = a;
		asset.width = asset.height = 1;
		asset.format = GL_RGBA8;
		asset.texture_format = GL_RGBA;
		asset.pixel_type = GL_UNSIGNED_BYTE;
		asset.rgba_blob_offset = 0;
		asset.rgba_blob_size = 4;
		append_bytes(data, asset);
	}
	data.insert(data.end(), long_string.begin(), long_string.end());
	data.push_back(0);
	data.insert(data.end(), { 1, 2, 3, 4 });
	write_file(path, data);

	ReplacementProvider provider;
	total_allocation = 0;
	allocation_tracking = true;
	const bool loaded = provider.load_cache_dir(path);
	allocation_tracking = false;
	check(loaded && provider.entry_count() == 2 * records, "the shared-string package loads every entry");
	// Copying three 64 KiB strings into each of 4000 entries would allocate about 750 MiB.
	check(total_allocation < (8u << 20), "entries share one string table instead of copying strings");
	check(std::string(provider.entries_.back().phrb_policy_key) == long_string, "entries still read their strings");
	remove_tree(dir);
}

int main()
{
	test_package_mutations();
	test_shared_string_table();
	test_package_overlap();
	test_rgba8_decode_payloads();
	test_indexed_replacements();
	if (const char *load_path = std::getenv("PARALLEL_N64_PROVIDER_LOAD_CACHE_PATH"))
	{
		ReplacementProvider provider;
		if (!provider.load_cache_dir(load_path))
		{
			std::cerr << "FAIL: provider could not load cache path " << load_path << std::endl;
			return 1;
		}

		if (std::getenv("PARALLEL_N64_PROVIDER_VERIFY_ROUND_TRIP"))
		{
			provider.set_enabled(true);
			check(provider.get_stats().entry_count == 1, "converter package must load one entry");
			ReplacementImage image;
			check(provider.decode_rgba8(0xaaaabbbb11111111ull, 258, &image), "converter pixel must decode");
			check(image.rgba8.size() >= 4 && image.rgba8[0] == 255 && image.rgba8[1] == 0 &&
			      image.rgba8[2] == 0 && image.rgba8[3] == 255, "converter preserves the red source pixel");
		}

		const auto stats = provider.get_stats();
		std::cout
			<< "provider_load_ok path=" << load_path
			<< " entries=" << stats.entry_count
			<< " native_sampled=" << stats.native_sampled_entry_count
			<< " compat=" << stats.compat_entry_count
			<< " phrb=" << stats.source_phrb_entry_count
			<< " jabo=" << stats.jabo_entry_count
			<< std::endl;
		return 0;
	}

	test_phrb_v8_jabo_records();

	const std::string dir = make_temp_dir();
	const std::string path = dir + "/sample.phrb";

	const std::string policy_key = "policy";
	const std::string sampled_object_id = "sampled-fmt2-siz1-off288-stride296-wh296x6-fs258-low32940cea6e";
	std::string strings;
	strings.append(policy_key);
	strings.push_back('\0');
	const uint32_t sampled_object_id_offset = uint32_t(strings.size());
	strings.append(sampled_object_id);
	strings.push_back('\0');
	const std::array<uint8_t, 16> rgba = {
		0x10, 0x20, 0x30, 0xff,
		0x40, 0x50, 0x60, 0xff,
		0x70, 0x80, 0x90, 0xff,
		0xa0, 0xb0, 0xc0, 0xff,
	};

	PHRBHeader header = {};
	header.magic[0] = 'P';
	header.magic[1] = 'H';
	header.magic[2] = 'R';
	header.magic[3] = 'B';
	header.version = 7;
	header.record_count = 1;
	header.asset_count = 2;
	header.record_table_offset = sizeof(PHRBHeader);
	header.asset_table_offset = header.record_table_offset + uint32_t(sizeof(PHRBRecordV4));
	header.string_table_offset = header.asset_table_offset + uint32_t(2 * sizeof(PHRBAssetV7));
	header.blob_offset = header.string_table_offset + uint32_t(strings.size());

	PHRBRecordV4 record = {};
	record.policy_key_offset = 0;
	record.sampled_object_id_offset = sampled_object_id_offset;
	record.record_flags = PHRB_RECORD_FLAG_RUNTIME_READY;
	record.fmt = 2;
	record.siz = 1;
	record.tex_offset = 0x120;
	record.stride = 296;
	record.width = 296;
	record.height = 6;
	record.formatsize = 258;
	record.sampled_low32 = 0x940cea6eu;
	record.sampled_entry_pcrc = 0x11223344u;
	record.sampled_sparse_pcrc = 0x55667788u;
	record.asset_candidate_count = 2;

	PHRBAssetV7 asset = {};
	asset.record_index = 0;
	asset.width = 2;
	asset.height = 2;
	asset.format = GL_RGBA8;
	asset.texture_format = GL_RGBA;
	asset.pixel_type = GL_UNSIGNED_BYTE;
	asset.legacy_formatsize = 258;
	asset.selector_checksum64 = 0x1122334455667788ull;
	asset.legacy_checksum64 = (uint64_t(record.sampled_sparse_pcrc) << 32u) | uint64_t(record.sampled_low32);
	asset.rgba_blob_offset = 0;
	asset.rgba_blob_size = uint64_t(rgba.size());

	PHRBAssetV7 second_asset = asset;
	second_asset.selector_checksum64 = 0x123456789abcdef0ull;
	second_asset.legacy_checksum64 = (uint64_t(record.sampled_sparse_pcrc) << 32u) | uint64_t(record.sampled_low32);
	second_asset.rgba_blob_offset = uint64_t(rgba.size());

	std::vector<uint8_t> data(sizeof(header) + sizeof(record) + 2 * sizeof(PHRBAssetV7) + strings.size() + 2 * rgba.size());
	size_t offset = 0;
	std::memcpy(data.data() + offset, &header, sizeof(header));
	offset += sizeof(header);
	std::memcpy(data.data() + offset, &record, sizeof(record));
	offset += sizeof(record);
	std::memcpy(data.data() + offset, &asset, sizeof(asset));
	offset += sizeof(asset);
	std::memcpy(data.data() + offset, &second_asset, sizeof(second_asset));
	offset += sizeof(second_asset);
	std::memcpy(data.data() + offset, strings.data(), strings.size());
	offset += strings.size();
	std::memcpy(data.data() + offset, rgba.data(), rgba.size());
	offset += rgba.size();
	std::memcpy(data.data() + offset, rgba.data(), rgba.size());

	write_file(path, data);

	ReplacementProvider provider;
	check(provider.load_cache_dir(path), "provider should load synthetic PHRB");
	provider.set_enabled(true);
	check(provider.entry_count() == 4, "provider should load one entry for each selector and distinct palette CRC");

	const auto &entry = provider.entries_[0];
	check(entry.sampled_fmt == record.fmt, "sampled fmt should be preserved");
	check(entry.sampled_siz == record.siz, "sampled siz should be preserved");
	check(entry.sampled_tex_offset == record.tex_offset, "sampled tex_offset should be preserved");
	check(entry.sampled_stride == record.stride, "sampled stride should be preserved");
	check(entry.sampled_width == record.width, "sampled width should be preserved");
	check(entry.sampled_height == record.height, "sampled height should be preserved");
	check(entry.sampled_low32 == record.sampled_low32, "sampled low32 should be preserved");
	check(entry.sampled_palette_crc == record.sampled_sparse_pcrc, "sampled palette crc should be preserved");
	check(entry.sampled_entry_pcrc == record.sampled_entry_pcrc, "sampled entry pcrc should be preserved");
	check(entry.sampled_sparse_pcrc == record.sampled_sparse_pcrc, "sampled sparse pcrc should be preserved");
	check(entry.has_native_sampled_identity, "PHRB entry should retain native sampled identity");
	check(entry.phrb_policy_key == policy_key, "PHRB policy_key should be preserved");
	check(entry.phrb_sampled_object_id == sampled_object_id, "PHRB sampled_object_id should be preserved");
	check(entry.selector_checksum64 == asset.selector_checksum64, "selector should be preserved");
	check(entry.formatsize == record.formatsize, "formatsize should remain preserved");
	check(provider.sampled_index_.size() == 4, "sampled lookup index should contain both palette aliases for both selectors");
	check(provider.sampled_family_index_.size() == 2, "sampled family index should collapse selectors into palette families");
	check(!provider.has_phrb_compat_entries(), "pure native PHRB inputs should not trigger compat-bearing runtime auto-enable");

	const auto *structured_sparse_entry = provider.find_sampled_entry(
		record.fmt,
		record.siz,
		record.tex_offset,
		record.stride,
		record.width,
		record.height,
		record.sampled_low32,
		record.sampled_sparse_pcrc,
		record.formatsize,
		asset.selector_checksum64);
	check(structured_sparse_entry != nullptr, "structured sparse sampled entry should be discoverable");
	check(structured_sparse_entry->checksum64 == ((uint64_t(record.sampled_sparse_pcrc) << 32u) | uint64_t(record.sampled_low32)),
	      "structured sparse sampled entry should resolve the sparse alias checksum");

	ReplacementMeta sampled_meta = {};
	uint64_t resolved_checksum64 = 0;
	check(provider.lookup_sampled_with_selector(
	          record.fmt,
	          record.siz,
	          record.tex_offset,
	          record.stride,
	          record.width,
	          record.height,
	          record.sampled_low32,
	          record.sampled_sparse_pcrc,
	          record.formatsize,
	          asset.selector_checksum64,
	          &sampled_meta,
	          &resolved_checksum64),
	      "structured sampled lookup should resolve the sparse alias");
	check(sampled_meta.repl_w == asset.width, "structured sampled lookup should preserve replacement width");
	check(sampled_meta.repl_h == asset.height, "structured sampled lookup should preserve replacement height");
	check(resolved_checksum64 == ((uint64_t(record.sampled_sparse_pcrc) << 32u) | uint64_t(record.sampled_low32)),
	      "structured sampled lookup should return the resolved sparse alias checksum");

	check(provider.lookup_sampled_with_selector(
	          record.fmt,
	          record.siz,
	          record.tex_offset,
	          record.stride,
	          record.width,
	          record.height,
	          record.sampled_low32,
	          record.sampled_entry_pcrc,
	          record.formatsize,
	          asset.selector_checksum64,
	          &sampled_meta,
	          &resolved_checksum64),
	      "structured sampled lookup should resolve the entry alias too");
	check(resolved_checksum64 == ((uint64_t(record.sampled_entry_pcrc) << 32u) | uint64_t(record.sampled_low32)),
	      "structured sampled lookup should return the resolved entry alias checksum");
	check(!provider.lookup_sampled_with_selector(
	          record.fmt,
	          record.siz,
	          record.tex_offset,
	          record.stride,
	          record.width,
	          record.height,
	          record.sampled_low32,
	          record.sampled_sparse_pcrc,
	          record.formatsize,
	          0xaaaaaaaa55555555ull,
	          &sampled_meta,
	          &resolved_checksum64),
	      "structured sampled lookup should not silently collapse selector conflicts");
	uint64_t resolved_selector_checksum64 = 0xffffffffu;
	check(!provider.lookup_sampled_family_unique(
	          record.fmt,
	          record.siz,
	          record.tex_offset,
	          record.stride,
	          record.width,
	          record.height,
	          record.sampled_low32,
	          record.sampled_sparse_pcrc,
	          record.formatsize,
	          &sampled_meta,
	          &resolved_checksum64,
	          &resolved_selector_checksum64),
	      "family-unique sampled lookup should stay disabled for multi-selector pools");


	// Capture checksum64 before add_entry() — the pointer into entries_ may be
	// invalidated by vector reallocation.
	const uint64_t sparse_checksum64 = structured_sparse_entry->checksum64;
	ReplacementProvider::Entry compat_alias = entry;
	compat_alias.source_path = dir + "/compat.phrb";
	compat_alias.phrb_policy_key = "";
	compat_alias.phrb_sampled_object_id = "";
	compat_alias.sampled_palette_crc = 0;
	compat_alias.sampled_entry_pcrc = 0;
	compat_alias.sampled_sparse_pcrc = 0;
	compat_alias.has_native_sampled_identity = false;
	compat_alias.inline_blob = true;
	compat_alias.blob.resize(compat_alias.data_size);
	compat_alias.blob[0] = 0xfe;
	provider.add_entry(std::move(compat_alias));

	ReplacementMeta compat_meta = {};
	check(provider.lookup_with_selector(
	          sparse_checksum64,
	          record.formatsize,
	          asset.selector_checksum64,
	          &compat_meta),
	      "checksum lookup should still find later compat aliases");
	check(provider.find_compat_entry(
	          sparse_checksum64,
	          record.formatsize,
	          asset.selector_checksum64)->source_path == dir + "/compat.phrb",
	      "compat alias should be reachable via explicit compat lookup");

	ReplacementImage compat_image = {};
	check(provider.decode_rgba8_compat_with_selector(
	          sparse_checksum64,
	          record.formatsize,
	          asset.selector_checksum64,
	          &compat_image),
	      "compat decode should resolve the explicit compat alias");
	check(!compat_image.rgba8.empty() && compat_image.rgba8[0] == 0xfe,
	      "compat decode should expose compat alias payload");

	ReplacementMeta native_meta = {};
	check(provider.lookup_sampled_with_selector(
	          record.fmt,
	          record.siz,
	          record.tex_offset,
	          record.stride,
	          record.width,
	          record.height,
	          record.sampled_low32,
	          record.sampled_sparse_pcrc,
	          record.formatsize,
	          asset.selector_checksum64,
	          &native_meta,
	          &resolved_checksum64),
	      "structured sampled lookup should continue to resolve native sampled entries");
	check(native_meta.repl_w == asset.width && native_meta.repl_h == asset.height,
	      "structured sampled lookup should ignore checksum-only compat aliases");
	check(resolved_checksum64 == ((uint64_t(record.sampled_sparse_pcrc) << 32u) | uint64_t(record.sampled_low32)),
	      "structured sampled lookup should keep the native sparse alias checksum");

	ReplacementImage native_image = {};
	check(provider.decode_sampled_rgba8_with_selector(
	          record.fmt,
	          record.siz,
	          record.tex_offset,
	          record.stride,
	          record.width,
	          record.height,
	          record.sampled_low32,
	          record.sampled_sparse_pcrc,
	          record.formatsize,
	          asset.selector_checksum64,
	          &native_image,
	          &resolved_checksum64),
	      "structured sampled decode should resolve native sampled entries directly");
	check(!native_image.rgba8.empty() && native_image.rgba8[0] == rgba[0],
	      "structured sampled decode should ignore checksum-only compat aliases");
	check(resolved_checksum64 == ((uint64_t(record.sampled_sparse_pcrc) << 32u) | uint64_t(record.sampled_low32)),
	      "structured sampled decode should preserve the native sparse alias checksum");

	ReplacementProviderStats stats = provider.get_stats();
	check(stats.entry_count == 5, "provider stats should report total entry count");
	check(stats.native_sampled_entry_count == 4, "provider stats should report native sampled entry count");
	check(stats.compat_entry_count == 1, "provider stats should report compat entry count");
	check(stats.sampled_index_count == 4, "provider stats should report sampled lookup entry count");
	check(stats.sampled_duplicate_key_count == 0, "provider stats should report zero sampled duplicate keys for unique inputs");
	check(stats.sampled_duplicate_entry_count == 0, "provider stats should report zero sampled duplicate entries for unique inputs");
	check(stats.sampled_family_count == 2, "provider stats should report sampled family count");
	check(stats.source_phrb_entry_count == 5, "provider stats should report PHRB-backed entries");
	check(provider.has_phrb_compat_entries(), "compat-bearing PHRB inputs should be detectable for runtime auto-enable");

	ReplacementResolution upload_sampled_exact_resolution = {};
	const uint64_t upload_checksum64 = (uint64_t(record.sampled_sparse_pcrc) << 32u) | uint64_t(record.sampled_low32);
	check(provider.resolve_upload_candidate(
		      record.formatsize,
		      record.fmt,
		      record.siz,
		      record.tex_offset,
		      record.stride,
		      record.width,
		      record.height,
		      record.sampled_low32,
		      record.sampled_sparse_pcrc,
		      asset.selector_checksum64,
		      &upload_sampled_exact_resolution),
	      "upload candidate resolution should resolve multi-selector native sampled entries via sampled-exact lookup");
	check(upload_sampled_exact_resolution.kind == ReplacementResolutionKind::SampledExactSelector,
	      "upload candidate resolution should classify multi-selector sampled hits as SampledExactSelector");
	check(upload_sampled_exact_resolution.identity.valid,
	      "upload candidate resolution should preserve native sampled identity for sampled-exact hits");
	check(upload_sampled_exact_resolution.source_class == ResolvedEntrySourceClass::Native,
	      "upload candidate resolution should classify sampled-exact hits as native source");
	check(upload_sampled_exact_resolution.resolved_selector_checksum64 == asset.selector_checksum64,
	      "upload candidate resolution should preserve the resolved selector for sampled-exact hits");

	ReplacementResolution upload_wrong_selector_resolution = {};
	bool upload_wrong_selector_hit = provider.resolve_upload_candidate(
		record.formatsize,
		record.fmt,
		record.siz,
		record.tex_offset,
		record.stride,
		record.width,
		record.height,
		record.sampled_low32,
		record.sampled_sparse_pcrc,
		0xdeadbeefcafebabeull,
		&upload_wrong_selector_resolution);
	check(!upload_wrong_selector_hit ||
	      (upload_wrong_selector_resolution.kind != ReplacementResolutionKind::SampledExactSelector &&
	       upload_wrong_selector_resolution.kind != ReplacementResolutionKind::SampledFamilySingleton),
	      "upload candidate resolution should not resolve via sampled paths when the selector does not match");

	const std::string legacy_cache_dir = make_temp_dir();
	const uint64_t legacy_checksum64 = 0x5566778811223344ull;
	const uint16_t legacy_formatsize = 258;
	const std::vector<uint8_t> legacy_blob = {
		0x11, 0x22, 0x33, 0xff,
	};
	const std::string legacy_hts_path = legacy_cache_dir + "/legacy.hts";
	const std::string legacy_htc_path = legacy_cache_dir + "/legacy.htc";
	write_legacy_hts(legacy_hts_path, legacy_checksum64, legacy_formatsize, 1, 1, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, legacy_blob);
	write_legacy_htc(legacy_htc_path, legacy_checksum64, legacy_formatsize, 1, 1, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, legacy_blob);

	ReplacementProvider legacy_provider;
	check(!legacy_provider.load_cache_dir(legacy_hts_path),
	      "provider should reject legacy HTS runtime inputs");
	check(!legacy_provider.load_cache_dir(legacy_htc_path),
	      "provider should reject legacy HTC runtime inputs");
	check(legacy_provider.entry_count() == 0,
	      "provider should keep zero entries after rejecting legacy runtime inputs");
	remove_tree(legacy_cache_dir);

	const std::string deferred_dir = make_temp_dir();
	const uint32_t deferred_palette_crc = 0x1234abcdU;
	const uint32_t deferred_low32 = 0x89abcdefU;
	const uint64_t deferred_checksum64 = (uint64_t(deferred_palette_crc) << 32u) | uint64_t(deferred_low32);
	const std::vector<uint8_t> deferred_runtime_rgba = {
		0x01, 0x02, 0x03, 0xff,
		0x04, 0x05, 0x06, 0xff,
		0x07, 0x08, 0x09, 0xff,
		0x0a, 0x0b, 0x0c, 0xff,
	};
	const std::vector<uint8_t> deferred_canonical_rgba = {
		0x55, 0x66, 0x77, 0xff,
		0x88, 0x99, 0xaa, 0xff,
		0xbb, 0xcc, 0xdd, 0xff,
		0xee, 0xf0, 0xf1, 0xff,
	};
	write_single_asset_phrb(
		deferred_dir + "/runtime-ready.phrb",
		"runtime-ready",
		"runtime-ready-replacement",
		"sampled-fmt2-siz1-off64-stride32-wh2x2-fs258-low3289abcdef",
		2,
		1,
		64,
		32,
		2,
		2,
		258,
		deferred_low32,
		deferred_palette_crc,
		0,
		2,
		2,
		deferred_runtime_rgba,
		PHRB_RECORD_FLAG_RUNTIME_READY);
	write_single_asset_phrb(
		deferred_dir + "/runtime-deferred.phrb",
		"runtime-deferred",
		"runtime-deferred-replacement",
		"legacy-family-89abcdef-fs258",
		0,
		0,
		0,
		0,
		0,
		0,
		258,
		deferred_low32,
		0,
		0,
		2,
		2,
		deferred_canonical_rgba,
		0);

	ReplacementProvider deferred_provider;
	check(deferred_provider.load_cache_dir(deferred_dir), "provider should load directories with mixed runtime-ready and runtime-deferred PHRB files");
	deferred_provider.set_enabled(true);
	check(deferred_provider.entry_count() == 1, "runtime-deferred PHRB records should be ignored by the runtime loader");
	const auto *deferred_entry = deferred_provider.find_any_entry(deferred_checksum64, 258, 0);
	check(deferred_entry != nullptr, "runtime-ready PHRB record should still load");
	check(std::string(deferred_entry->phrb_policy_key) == "runtime-ready", "runtime-ready record should preserve its policy key");
	check(deferred_provider.find_any_entry(uint64_t(deferred_low32), 258, 0) == nullptr,
	      "runtime-deferred record should not synthesize a runtime lookup entry");
	ReplacementProviderStats deferred_stats = deferred_provider.get_stats();
	check(deferred_stats.source_phrb_entry_count == 1, "runtime-deferred PHRB records should not count as loaded source entries");
	check(deferred_stats.native_sampled_entry_count == 1, "runtime-deferred PHRB records should not inflate native sampled entry counts");
	remove_tree(deferred_dir);

	const std::string family_dir = make_temp_dir();
	const uint32_t family_low32 = 0x01020304u;
	const uint32_t family_palette_crc = 0x99887766u;
	const uint64_t family_checksum64 = (uint64_t(family_palette_crc) << 32u) | uint64_t(family_low32);
	const std::vector<uint8_t> family_rgba = {
		0x12, 0x34, 0x56, 0xff,
		0x78, 0x9a, 0xbc, 0xff,
		0xde, 0xf0, 0x11, 0xff,
		0x22, 0x33, 0x44, 0xff,
	};
	write_single_asset_phrb(
		family_dir + "/family-runtime-ready.phrb",
		"family-runtime-ready",
		"family-runtime-ready-replacement",
		"legacy-family-01020304-fs258",
		0,
		0,
		0,
		0,
		0,
		0,
		258,
		family_low32,
		0,
		0,
		2,
		2,
		family_rgba,
		PHRB_RECORD_FLAG_RUNTIME_READY,
		family_checksum64);

	ReplacementProvider family_provider;
	check(family_provider.load_cache_dir(family_dir), "provider should load runtime-ready family PHRB records");
	family_provider.set_enabled(true);
	check(family_provider.entry_count() == 1, "family runtime-ready PHRB record should synthesize one compat entry");
	check(family_provider.find_native_entry(family_checksum64, 258, 0) == nullptr,
	      "family runtime-ready PHRB record should not pretend to be a native sampled entry");
	const auto *family_entry = family_provider.find_compat_entry(family_checksum64, 258, 0);
	check(family_entry != nullptr, "family runtime-ready PHRB record should load into the compat checksum path");
	check(std::string(family_entry->phrb_policy_key) == "family-runtime-ready", "family runtime-ready record should preserve its policy key");
	check(std::string(family_entry->phrb_sampled_object_id) == "legacy-family-01020304-fs258",
	      "family runtime-ready record should preserve its sampled object id");
	ReplacementImage family_image = {};
	check(family_provider.decode_rgba8(family_checksum64, 258, &family_image),
	      "family runtime-ready PHRB record should decode through the generic exact checksum path");
	check(family_image.rgba8 == family_rgba,
	      "family runtime-ready PHRB record should preserve its payload");
	ReplacementProviderStats family_stats = family_provider.get_stats();
	check(family_stats.entry_count == 1, "family stats should report one loaded entry");
	check(family_stats.native_sampled_entry_count == 0, "family stats should not count family stubs as native sampled entries");
	check(family_stats.compat_entry_count == 1, "family stats should count family stubs as compat entries");
	check(family_stats.source_phrb_entry_count == 1, "family stats should count the loaded family PHRB entry");
	check(family_provider.has_phrb_compat_entries(), "family-runtime PHRB records should trigger compat-bearing PHRB detection");
	remove_tree(family_dir);

	const std::string legacy_payload_dir = make_temp_dir();
	const uint32_t legacy_payload_low32 = 0x0f1e2d3cu;
	const uint32_t legacy_payload_palette_crc = 0x55667788u;
	const uint64_t legacy_payload_checksum64 = (uint64_t(legacy_payload_palette_crc) << 32u) | uint64_t(legacy_payload_low32);
	const std::vector<uint8_t> legacy_payload_blob = {
		0x00, 0xf8,
		0xe0, 0x07,
		0x1f, 0x00,
		0xff, 0xff,
	};
	const std::vector<uint8_t> legacy_payload_expected_rgba = {
		0xff, 0x00, 0x00, 0xff,
		0x00, 0xff, 0x00, 0xff,
		0x00, 0x00, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff,
	};
	write_single_asset_phrb(
		legacy_payload_dir + "/legacy-payload-runtime-ready.phrb",
		"legacy-payload-runtime-ready",
		"legacy-payload-runtime-ready-replacement",
		"legacy-family-0f1e2d3c-fs258",
		0,
		0,
		0,
		0,
		0,
		0,
		258,
		legacy_payload_low32,
		0,
		0,
		2,
		2,
		legacy_payload_blob,
		PHRB_RECORD_FLAG_RUNTIME_READY,
		legacy_payload_checksum64,
		GL_RGB8,
		GL_RGB,
		GL_UNSIGNED_SHORT_5_6_5);

	ReplacementProvider legacy_payload_provider;
	check(legacy_payload_provider.load_cache_dir(legacy_payload_dir), "provider should load v7 legacy-payload PHRB records");
	legacy_payload_provider.set_enabled(true);
	check(legacy_payload_provider.entry_count() == 1, "legacy-payload runtime-ready PHRB record should synthesize one compat entry");
	const auto *legacy_payload_entry = legacy_payload_provider.find_compat_entry(legacy_payload_checksum64, 258, 0);
	check(legacy_payload_entry != nullptr, "legacy-payload runtime-ready PHRB record should load into the compat checksum path");
	check(legacy_payload_entry->format == GL_RGB8, "legacy-payload runtime-ready record should preserve the stored format");
	check(legacy_payload_entry->texture_format == GL_RGB, "legacy-payload runtime-ready record should preserve the stored texture format");
	check(legacy_payload_entry->pixel_type == GL_UNSIGNED_SHORT_5_6_5, "legacy-payload runtime-ready record should preserve the stored pixel type");
	ReplacementImage legacy_payload_image = {};
	check(legacy_payload_provider.decode_rgba8(legacy_payload_checksum64, 258, &legacy_payload_image),
	      "legacy-payload runtime-ready PHRB record should decode through the generic exact checksum path");
	check(legacy_payload_image.rgba8 == legacy_payload_expected_rgba,
	      "legacy-payload runtime-ready PHRB record should decode legacy payload bytes to the expected RGBA output");
	remove_tree(legacy_payload_dir);

	const std::string mixed_dir = make_temp_dir();
	const uint32_t mixed_palette_crc = 0xa1b2c3d4u;
	const uint32_t mixed_sampled_low32 = 0x0badbeefu;
	const uint64_t mixed_checksum64 = (uint64_t(mixed_palette_crc) << 32u) | uint64_t(mixed_sampled_low32);
	const std::vector<uint8_t> mixed_native_rgba = {
		0x11, 0x22, 0x33, 0xff,
		0x44, 0x55, 0x66, 0xff,
		0x77, 0x88, 0x99, 0xff,
		0xaa, 0xbb, 0xcc, 0xff,
	};
	const std::vector<uint8_t> mixed_compat_rgba = {
		0xde, 0xad, 0xbe, 0xff,
		0xef, 0x10, 0x20, 0xff,
		0x30, 0x40, 0x50, 0xff,
		0x60, 0x70, 0x80, 0xff,
	};
	write_single_asset_phrb(
		mixed_dir + "/a-native.phrb",
		"mixed-native",
		"mixed-native-replacement",
		"sampled-fmt2-siz1-off32-stride16-wh2x2-fs258-low320badbeef",
		2,
		1,
		32,
		16,
		2,
		2,
		258,
		mixed_sampled_low32,
		mixed_palette_crc,
		0,
		2,
		2,
		mixed_native_rgba);
	write_legacy_htc(
		mixed_dir + "/z-compat.htc",
		mixed_checksum64,
		258,
		4,
		1,
		GL_RGBA8,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		mixed_compat_rgba);

	ReplacementProvider phrb_only_provider;
	check(phrb_only_provider.load_cache_dir(mixed_dir),
	      "provider should load the PHRB entry from mixed directories and ignore legacy runtime files");
	phrb_only_provider.set_enabled(true);
	check(phrb_only_provider.entry_count() == 1,
	      "provider should fence out legacy runtime files when PHRB data is available");
	check(!phrb_only_provider.has_phrb_compat_entries(),
	      "provider should not report compat-bearing PHRB state for pure native loads");
	const auto *mixed_native_entry = phrb_only_provider.find_native_entry(mixed_checksum64, 258, 0);
	check(mixed_native_entry != nullptr, "provider should expose the native PHRB entry from mixed directories");
	check(mixed_native_entry->source_path.find(".phrb") != std::string::npos,
	      "provider should keep the PHRB-backed entry active");
	check(phrb_only_provider.find_compat_entry(mixed_checksum64, 258, 0) == nullptr,
	      "provider should not expose legacy compat entries at runtime");
	remove_tree(mixed_dir);


	const std::string ordered_singleton_dir = make_temp_dir();
	const uint32_t ordered_singleton_palette_crc = 0x13572468u;
	const uint32_t ordered_singleton_sampled_low32 = 0x24681357u;
	const uint64_t ordered_singleton_selector = ReplacementProvider::ordered_surface_slot_selector_checksum64(0);
	const uint64_t ordered_singleton_checksum64 =
		(uint64_t(ordered_singleton_palette_crc) << 32u) | uint64_t(ordered_singleton_sampled_low32);
	const std::vector<uint8_t> ordered_singleton_rgba = {
		0x21, 0x43, 0x65, 0xff,
		0x87, 0xa9, 0xcb, 0xff,
		0xed, 0x0f, 0x10, 0xff,
		0x32, 0x54, 0x76, 0xff,
	};
	write_single_asset_phrb(
		ordered_singleton_dir + "/ordered-singleton.phrb",
		"ordered-singleton",
		"ordered-singleton-replacement",
		"sampled-fmt2-siz1-off96-stride16-wh2x2-fs258-low3224681357",
		2,
		1,
		96,
		16,
		2,
		2,
		258,
		ordered_singleton_sampled_low32,
		ordered_singleton_palette_crc,
		ordered_singleton_selector,
		2,
		2,
		ordered_singleton_rgba);

	ReplacementProvider ordered_singleton_provider;
	check(ordered_singleton_provider.load_cache_dir(ordered_singleton_dir),
	      "provider should load ordered-surface singleton sampled families");
	ordered_singleton_provider.set_enabled(true);
	ReplacementMeta ordered_singleton_meta = {};
	uint64_t ordered_singleton_resolved_checksum64 = 0;
	uint64_t ordered_singleton_resolved_selector_checksum64 = 0xffffffffu;
	check(!ordered_singleton_provider.lookup_sampled_family_unique(
	          2,
	          1,
	          96,
	          16,
	          2,
	          2,
	          ordered_singleton_sampled_low32,
	          ordered_singleton_palette_crc,
	          258,
	          &ordered_singleton_meta,
	          &ordered_singleton_resolved_checksum64,
	          &ordered_singleton_resolved_selector_checksum64),
	      "family-unique sampled lookup should continue to reject ordered-surface singleton families");
	bool ordered_singleton_flag = false;
	check(ordered_singleton_provider.lookup_sampled_family_singleton(
	          2,
	          1,
	          96,
	          16,
	          2,
	          2,
	          ordered_singleton_sampled_low32,
	          ordered_singleton_palette_crc,
	          258,
	          &ordered_singleton_meta,
	          &ordered_singleton_resolved_checksum64,
	          &ordered_singleton_resolved_selector_checksum64,
	          &ordered_singleton_flag),
	      "family-singleton sampled lookup should resolve ordered-surface singleton families");
	check(ordered_singleton_flag,
	      "family-singleton sampled lookup should classify ordered-surface singleton families explicitly");
	check(ordered_singleton_resolved_selector_checksum64 == ordered_singleton_selector,
	      "family-singleton sampled lookup should preserve the ordered-surface selector");
	check(ordered_singleton_resolved_checksum64 == ordered_singleton_checksum64,
	      "family-singleton sampled lookup should preserve the singleton checksum");
	check(ordered_singleton_meta.repl_w == 2 && ordered_singleton_meta.repl_h == 2,
	      "family-singleton sampled lookup should preserve ordered-surface singleton replacement dimensions");
	ReplacementResolution ordered_singleton_resolution = {};
	check(ordered_singleton_provider.resolve_upload_candidate(
	          258,
	          2,
	          1,
	          96,
	          16,
	          2,
	          2,
	          ordered_singleton_sampled_low32,
	          ordered_singleton_palette_crc,
	          0,
	          &ordered_singleton_resolution),
	      "typed upload resolution should resolve ordered-surface singleton sampled families");
	check(ordered_singleton_resolution.kind == ReplacementResolutionKind::SampledFamilySingleton,
	      "typed upload resolution should classify ordered-surface singleton families as sampled-family singletons");
	check(ordered_singleton_resolution.ordered_surface_singleton,
	      "typed upload resolution should preserve ordered-surface singleton classification");
	check(ordered_singleton_resolution.identity.valid,
	      "typed upload resolution should preserve sampled identity for ordered-surface singletons");
	check(ordered_singleton_resolution.resolved_selector_checksum64 == ordered_singleton_selector,
	      "typed upload resolution should preserve the ordered-surface selector");
	ReplacementImage ordered_singleton_image = {};
	check(ordered_singleton_provider.decode_sampled_rgba8_with_selector(
	          2,
	          1,
	          96,
	          16,
	          2,
	          2,
	          ordered_singleton_sampled_low32,
	          ordered_singleton_palette_crc,
	          258,
	          ordered_singleton_selector,
	          &ordered_singleton_image,
	          &ordered_singleton_resolved_checksum64),
	      "ordered-surface singleton sampled decode should resolve via the explicit selector path");
	check(ordered_singleton_image.rgba8 == ordered_singleton_rgba,
	      "ordered-surface singleton sampled decode should preserve the payload");
	remove_tree(ordered_singleton_dir);


	const std::string phrb_preference_dir = make_temp_dir();
	const uint32_t phrb_preference_palette_crc = 0x2468ace0u;
	const uint32_t phrb_preference_sampled_low32 = 0x13579bdfu;
	const uint64_t phrb_preference_checksum64 =
		(uint64_t(phrb_preference_palette_crc) << 32u) | uint64_t(phrb_preference_sampled_low32);
	const std::vector<uint8_t> phrb_preference_native_rgba = {
		0x01, 0x23, 0x45, 0xff,
		0x67, 0x89, 0xab, 0xff,
		0xcd, 0xef, 0x10, 0xff,
		0x32, 0x54, 0x76, 0xff,
	};
	const std::vector<uint8_t> phrb_preference_family_rgba = {
		0xf1, 0xe2, 0xd3, 0xff,
		0xc4, 0xb5, 0xa6, 0xff,
		0x97, 0x88, 0x79, 0xff,
		0x6a, 0x5b, 0x4c, 0xff,
	};
	write_single_asset_phrb(
		phrb_preference_dir + "/a-native.phrb",
		"phrb-native-first",
		"phrb-native-first-replacement",
		"sampled-fmt2-siz1-off64-stride16-wh2x2-fs258-low3213579bdf",
		2,
		1,
		64,
		16,
		2,
		2,
		258,
		phrb_preference_sampled_low32,
		phrb_preference_palette_crc,
		0,
		2,
		2,
		phrb_preference_native_rgba);
	write_single_asset_phrb(
		phrb_preference_dir + "/z-family.phrb",
		"phrb-family-fallback",
		"phrb-family-fallback-replacement",
		"family-runtime-record",
		0,
		0,
		0,
		0,
		0,
		0,
		258,
		phrb_preference_sampled_low32,
		phrb_preference_palette_crc,
		0,
		4,
		1,
		phrb_preference_family_rgba,
		PHRB_RECORD_FLAG_RUNTIME_READY,
		phrb_preference_checksum64);

	ReplacementProvider phrb_preference_provider;
	check(phrb_preference_provider.load_cache_dir(phrb_preference_dir),
	      "provider should load native and family-runtime PHRB duplicates");
	phrb_preference_provider.set_enabled(true);
	check(phrb_preference_provider.entry_count() == 2,
	      "provider should expose both native sampled and family-runtime compat records");
	check(phrb_preference_provider.find_native_entry(phrb_preference_checksum64, 258, 0) != nullptr,
	      "native PHRB duplicate should remain visible explicitly");
	check(phrb_preference_provider.find_compat_entry(phrb_preference_checksum64, 258, 0) != nullptr,
	      "family-runtime PHRB duplicate should remain visible explicitly");
	const auto *phrb_preference_entry = phrb_preference_provider.find_any_entry(phrb_preference_checksum64, 258, 0);
	check(phrb_preference_entry != nullptr, "generic entry lookup should still resolve the duplicate checksum");
	check(phrb_preference_entry->has_native_sampled_identity,
	      "generic entry lookup should prefer native sampled PHRB entries over later family-runtime compat records");
	check(phrb_preference_entry->width == 2 && phrb_preference_entry->height == 2,
	      "generic entry lookup should preserve the native replacement dimensions");
	ReplacementResolution phrb_preference_resolution = {};
	check(phrb_preference_provider.resolve_upload_candidate(
	          258,
	          0, 0, 0, 0, 0, 0, phrb_preference_sampled_low32, phrb_preference_palette_crc, 0,
	          &phrb_preference_resolution),
	      "upload candidate resolution should prefer native sampled PHRB entries over family-runtime compat duplicates");
	check(phrb_preference_resolution.kind != ReplacementResolutionKind::GenericCompat &&
	          phrb_preference_resolution.kind != ReplacementResolutionKind::None,
	      "upload candidate resolution should not fall through to compat when native sampled exists");
	check(phrb_preference_resolution.identity.valid,
	      "upload candidate resolution should preserve native sampled identity");
	const auto *phrb_preference_compat = phrb_preference_provider.find_compat_entry(phrb_preference_checksum64, 258, 0);
	check(phrb_preference_compat && phrb_preference_compat->width == 4 && phrb_preference_compat->height == 1,
	      "compat lookup should preserve the family-runtime fallback dimensions");
	ReplacementImage phrb_preference_image = {};
	check(phrb_preference_provider.decode_rgba8_with_selector(
	          phrb_preference_checksum64,
	          258,
	          0,
	          &phrb_preference_image),
	      "generic checksum decode should resolve native sampled PHRB entries ahead of family-runtime compat duplicates");
	check(!phrb_preference_image.rgba8.empty() && phrb_preference_image.rgba8[0] == phrb_preference_native_rgba[0],
	      "generic checksum decode should expose the native sampled payload when a compat family stub shares the checksum");
	remove_tree(phrb_preference_dir);

	const std::string duplicate_dir = make_temp_dir();
	const uint32_t duplicate_palette_crc = 0x31415926u;
	const uint32_t duplicate_sampled_low32 = 0x7701ac09u;
	const uint64_t duplicate_selector = 0x0000000071c71cddull;
	const std::vector<uint8_t> duplicate_a_rgba = {
		0x10, 0x20, 0x30, 0xff,
		0x40, 0x50, 0x60, 0xff,
		0x70, 0x80, 0x90, 0xff,
		0xa0, 0xb0, 0xc0, 0xff,
	};
	const std::vector<uint8_t> duplicate_b_rgba = {
		0xca, 0xfe, 0xba, 0xff,
		0xbe, 0x11, 0x22, 0xff,
		0x33, 0x44, 0x55, 0xff,
		0x66, 0x77, 0x88, 0xff,
	};
	write_single_asset_phrb(
		duplicate_dir + "/a-duplicate.phrb",
		"duplicate-a",
		"duplicate-a",
		"sampled-fmt0-siz3-off0-stride400-wh200x2-fs768-low327701ac09",
		0,
		3,
		0,
		400,
		200,
		2,
		768,
		duplicate_sampled_low32,
		duplicate_palette_crc,
		duplicate_selector,
		2,
		2,
		duplicate_a_rgba);
	write_single_asset_phrb(
		duplicate_dir + "/b-duplicate.phrb",
		"duplicate-b",
		"duplicate-z",
		"sampled-fmt0-siz3-off0-stride400-wh200x2-fs768-low327701ac09",
		0,
		3,
		0,
		400,
		200,
		2,
		768,
		duplicate_sampled_low32,
		duplicate_palette_crc,
		duplicate_selector,
		2,
		2,
		duplicate_b_rgba);

	ReplacementProvider duplicate_provider;
	check(duplicate_provider.load_cache_dir(duplicate_dir), "provider should load duplicate sampled key cache");
	duplicate_provider.set_enabled(true);
	ReplacementProviderStats duplicate_stats = duplicate_provider.get_stats();
	check(duplicate_stats.entry_count == 2, "duplicate provider should report both entries");
	check(duplicate_stats.native_sampled_entry_count == 2, "duplicate provider should report both native sampled entries");
	check(duplicate_stats.sampled_index_count == 1, "duplicate provider should collapse duplicate sampled lookup keys to one active entry");
	check(duplicate_stats.sampled_duplicate_key_count == 1, "duplicate provider should report one sampled duplicate key");
	check(duplicate_stats.sampled_duplicate_entry_count == 1, "duplicate provider should report one extra sampled duplicate entry");
	auto duplicate_diags = duplicate_provider.get_sampled_duplicate_diagnostics();
	check(duplicate_diags.size() == 1, "duplicate provider should expose one sampled duplicate diagnostic");
	check(duplicate_diags[0].sampled_low32 == duplicate_sampled_low32, "duplicate diagnostic should preserve sampled low32");
	check(duplicate_diags[0].sampled_palette_crc == duplicate_palette_crc, "duplicate diagnostic should preserve palette crc");
	check(duplicate_diags[0].formatsize == 768, "duplicate diagnostic should preserve formatsize");
	check(duplicate_diags[0].selector_checksum64 == duplicate_selector, "duplicate diagnostic should preserve selector");
	check(duplicate_diags[0].total_entry_count == 2, "duplicate diagnostic should report total entry count");
	check(duplicate_diags[0].duplicate_entry_count == 1, "duplicate diagnostic should report extra duplicate entries");
	check(duplicate_diags[0].active_policy_key == "duplicate-a", "duplicate diagnostic should report the stable active policy");
	check(duplicate_diags[0].active_replacement_id == "duplicate-a", "duplicate diagnostic should report the stable replacement id");
	check(duplicate_diags[0].active_repl_w == 2 && duplicate_diags[0].active_repl_h == 2,
	      "duplicate diagnostic should preserve active replacement dimensions");

	ReplacementImage duplicate_image = {};
	check(duplicate_provider.decode_sampled_rgba8_with_selector(
	          0,
	          3,
	          0,
	          400,
	          200,
	          2,
	          duplicate_sampled_low32,
	          duplicate_palette_crc,
	          768,
	          duplicate_selector,
	          &duplicate_image),
	      "duplicate sampled decode should still resolve the active entry");
	check(duplicate_image.rgba8 == duplicate_a_rgba, "duplicate sampled decode should resolve the stable replacement-id winner");
	check(duplicate_stats.sampled_duplicate_reordered_keys == 0,
	      "a first-loaded winner is not a duplicate reordering");

	// Swap the replacement ids so the later package carries the preferred one.
	auto write_duplicate = [&](const char *file, const char *id, const std::vector<uint8_t> &rgba) {
		write_single_asset_phrb(duplicate_dir + file, id, id,
		                        "sampled-fmt0-siz3-off0-stride400-wh200x2-fs768-low327701ac09",
		                        0, 3, 0, 400, 200, 2, 768, duplicate_sampled_low32, duplicate_palette_crc,
		                        duplicate_selector, 2, 2, rgba);
	};
	write_duplicate("/a-duplicate.phrb", "duplicate-z", duplicate_a_rgba);
	write_duplicate("/b-duplicate.phrb", "duplicate-a", duplicate_b_rgba);
	check(duplicate_provider.load_cache_dir(duplicate_dir), "provider should reload the swapped duplicate cache");
	check(duplicate_provider.get_stats().sampled_duplicate_reordered_keys == 1,
	      "a later package winning the tie-break counts its sampled key once");
	check(duplicate_provider.decode_sampled_rgba8_with_selector(0, 3, 0, 400, 200, 2, duplicate_sampled_low32,
	                                                             duplicate_palette_crc, 768, duplicate_selector,
	                                                             &duplicate_image) &&
	      duplicate_image.rgba8 == duplicate_b_rgba,
	      "the reordered key serves the later package's entry");
	remove_tree(duplicate_dir);

	const std::string rejected_dir = make_temp_dir();
	ReplacementProvider rejected_provider;
	check(!rejected_provider.load_cache_dir(rejected_dir + "/missing.phrb") &&
	      rejected_provider.load_error() == ReplacementLoadStatus::NotFound && rejected_provider.rejected_path().empty(),
	      "missing pack is distinct from rejected pack");
	PHRBHeader unsupported = {};
	memcpy(unsupported.magic, "PHRB", 4);
	unsupported.version = 9;
	std::vector<uint8_t> header_bytes(sizeof(unsupported));
	memcpy(header_bytes.data(), &unsupported, sizeof(unsupported));
	const auto rejected_path = rejected_dir + "/newer.phrb";
	write_file(rejected_path, header_bytes);
	check(!rejected_provider.load_cache_dir(rejected_path) &&
	      rejected_provider.load_error() == ReplacementLoadStatus::UnsupportedVersion &&
	      rejected_provider.rejected_path() == rejected_path && rejected_provider.entry_count() == 0,
	      "unsupported PHRB version reports rejection without loading entries");
	unsupported.version = 6;
	memcpy(header_bytes.data(), &unsupported, sizeof(unsupported));
	write_file(rejected_path, header_bytes);
	check(!rejected_provider.load_cache_dir(rejected_path) &&
	      rejected_provider.load_error() == ReplacementLoadStatus::UnsupportedVersion,
	      "a pre-version-7 PHRB is rejected as unsupported rather than damaged");
	write_file(rejected_path, {1, 2, 3});
	check(!rejected_provider.load_cache_dir(rejected_path) &&
	      rejected_provider.load_error() == ReplacementLoadStatus::Invalid,
	      "unreadable or truncated metadata reports a damaged pack");
	remove_tree(rejected_dir);

	remove_tree(dir);
	std::cout << "emu_unit_texture_replacement_provider_test: PASS" << std::endl;
	return 0;
}
