#define private public
#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/texture_replacement.hpp"
#undef private
#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_renderer.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>
#include <zlib.h>

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
		asset.legacy_formatsize = kinds[i] == 0 ? 0x0102 : 0;
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
	check(provider.compat_checksum_index_.size() == 1 && provider.compat_checksum_low32_index_.size() == 1,
	      "Jabo entries stay out of the Rice checksum indices");

	ReplacementResolution resolution = {};
	check(provider.resolve_jabo_candidate(md5, &resolution), "the Jabo MD5 resolves");
	check(resolution.kind == ReplacementResolutionKind::JaboMd5 && resolution.meta.repl_w == 2,
	      "a Jabo hit reports its kind and replacement size");
	check(resolution.resolved_checksum64 == 0x747f1fa9670b0f62ull &&
	      resolution.resolved_selector_checksum64 == 0x10715b74bec51b15ull,
	      "a Jabo hit reports the little-endian MD5 halves");
	ReplacementImage image = {};
	check(provider.decode_rgba8_jabo(resolution.resolved_checksum64, resolution.resolved_selector_checksum64, &image) &&
	      image.rgba8 == pixels[1],
	      "the Jabo asset decodes from the package blob");
	ReplacementMeta meta = {};
	check(provider.lookup(0x1234abcdull, 0x0102, &meta) && meta.repl_w == 2, "the Rice record still resolves");
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
	auto short_rgba = rgba;
	short_rgba.pop_back();
	auto overlong_rgba = rgba;
	overlong_rgba.insert(overlong_rgba.end(), {99, 98, 97, 96, 95});
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
		{"short raw RGBA", GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, short_rgba, {}},
		{"overlong raw RGBA prefix", GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, overlong_rgba, rgba},
		{"compressed RGBA", 0x80008058u, GL_RGBA, GL_UNSIGNED_BYTE, compressed, compressible},
		{"packed RGB565", GL_RGB8, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
		 {0x00, 0xf8, 0xe0, 0x07, 0x1f, 0x00, 0, 0, 0xff, 0xff, 0x10, 0x84},
		 {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255,
		  0, 0, 0, 255, 255, 255, 255, 255, 132, 130, 132, 255}},
		// Packed RGBA with trailing bytes must not be mistaken for exact raw RGBA.
		{"packed RGBA4444 prefix", GL_RGBA8, GL_RGBA, 0x8033,
		 {0x0f, 0xf0, 0xf0, 0x0f, 0xff, 0x00, 0, 0, 0xff, 0xff, 0x21, 0x84,
		  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
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
			      "short input must leave the caller's image intact");
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
		check(std::vector<uint8_t>(image.rgba8.begin(), image.rgba8.begin() + 8) ==
				  std::vector<uint8_t>({255, 0, 0, 255, 0, 255, 0, 0}),
			  "RGBA16 TLUT must use native bit replication and alpha");
		const auto first = image.rgba8;
		const auto first_hash = palette.hash();
		Renderer::HiresKey cache_key;
		cache_key.source = Renderer::HiresKeySource::Compat;
		cache_key.checksum64 = 0x12345678;
		cache_key.formatsize = fs;
		cache_key.indexed_palette_hash = first_hash;
		std::unordered_map<Renderer::HiresKey, std::vector<uint8_t>, Renderer::HiresKeyHasher> cache;
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
	check(qualified.probe_lookup(0x87654321, 0xaabbccdd, 2, &meta), "qualified indexed key must match its palette CRC");
	check(!qualified.probe_lookup(0x87654321, 0x11223344, 2, &meta), "qualified indexed key must not ignore its palette CRC");
	const auto coloured_path = dir + "/coloured.phrb";
	write_single_asset_phrb(coloured_path, "coloured", "coloured", "family", 0, 0, 0, 0, 0, 0,
							2, 0x87654321, 0, 0, 2, 2, std::vector<uint8_t>(16, 255));
	ReplacementProvider coloured;
	coloured.set_enabled(true);
	check(coloured.load_cache_dir(coloured_path), "pre-coloured CI package should load");
	check(!coloured.probe_lookup(0x87654321, 0xaabbccdd, 2, &meta), "pre-coloured CI lookup policy must remain unchanged");
	remove_tree(dir);
}

int main()
{
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
	check(provider.compat_checksum_low32_index_.empty(), "native PHRB entries should not populate compat low32 families");
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

	SampledFamilyDiagnostics sampled_diag = {};
	check(provider.describe_sampled_family(
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
	          &sampled_diag),
	      "sampled family diagnostics should be available for native PHRB records");
	check(sampled_diag.available, "sampled family diagnostics should report availability");
	check(sampled_diag.prefer_exact_formatsize, "sampled family diagnostics should prefer exact formatsize");
	check(sampled_diag.exact_formatsize_entries == 2, "sampled family diagnostics should see both selector variants");
	check(sampled_diag.generic_formatsize_entries == 0, "sampled family diagnostics should not report generic entries");
	check(sampled_diag.active_entry_count == 2, "sampled family diagnostics should stay inside the sparse palette family");
	check(sampled_diag.active_unique_checksum_count == 1, "sampled family diagnostics should preserve a single checksum alias");
	check(sampled_diag.active_unique_selector_count == 2, "sampled family diagnostics should preserve both selectors");
	check(sampled_diag.active_matching_selector_count == 0, "sampled family diagnostics should report selector conflicts");
	check(sampled_diag.active_has_any_selector, "sampled family diagnostics should report selector availability");
	check(!sampled_diag.active_has_ordered_surface_selectors, "sampled family diagnostics should not invent ordered selectors");
	check(sampled_diag.active_is_pool, "sampled family diagnostics should classify multi-selector sampled families as pools");
	check(sampled_diag.sample_repl_w == asset.width && sampled_diag.sample_repl_h == asset.height,
	      "sampled family diagnostics should preserve replacement dimensions");
	check(sampled_diag.sample_policy_key == policy_key, "sampled family diagnostics should preserve the policy key");
	check(sampled_diag.sample_sampled_object_id == sampled_object_id,
	      "sampled family diagnostics should preserve the sampled object id");
	check(!provider.lookup_ci_low32_any(
	          record.sampled_low32,
	          record.formatsize,
	          record.sampled_sparse_pcrc,
	          &sampled_meta,
	          &resolved_checksum64),
	      "compat low32 fallback should ignore native sampled records");
	ReplacementResolution ci_low32_any_resolution = {};
	check(!provider.resolve_ci_low32_candidate(
	          record.sampled_low32,
	          record.formatsize,
	          record.sampled_sparse_pcrc,
	          0,
	          0,
	          CILow32ResolutionMode::Any,
	          &ci_low32_any_resolution),
	      "typed compat low32 resolution should ignore native sampled records");

	// Capture checksum64 before add_entry() — the pointer into entries_ may be
	// invalidated by vector reallocation.
	const uint64_t sparse_checksum64 = structured_sparse_entry->checksum64;
	ReplacementProvider::Entry compat_alias = entry;
	compat_alias.source_path = dir + "/compat.phrb";
	compat_alias.phrb_policy_key.clear();
	compat_alias.phrb_sampled_object_id.clear();
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
	check(provider.compat_checksum_low32_index_.size() == 1,
	      "compat aliases should populate the explicit compat low32 index");
	check(provider.lookup_ci_low32_any(
	          record.sampled_low32,
	          record.formatsize,
	          record.sampled_sparse_pcrc,
	          &compat_meta,
	          &resolved_checksum64),
	      "compat low32 fallback should resolve explicit compat aliases");
	check(provider.find_compat_entry(
	          resolved_checksum64,
	          record.formatsize,
	          asset.selector_checksum64)->source_path == dir + "/compat.phrb",
	      "compat low32 fallback should stay inside the compat alias pool");
	check(provider.resolve_ci_low32_candidate(
	          record.sampled_low32,
	          record.formatsize,
	          record.sampled_sparse_pcrc,
	          0,
	          0,
	          CILow32ResolutionMode::Any,
	          &ci_low32_any_resolution),
	      "typed compat low32 resolution should resolve explicit compat aliases");
	check(ci_low32_any_resolution.kind == ReplacementResolutionKind::CILow32Any,
	      "typed compat low32 resolution should classify any-mode results");
	check(ci_low32_any_resolution.source_class == ResolvedEntrySourceClass::Compat,
	      "typed compat low32 resolution should classify compat-source results");
	check(ci_low32_any_resolution.resolved_checksum64 == resolved_checksum64,
	      "typed compat low32 resolution should preserve the compat checksum");
	check(ci_low32_any_resolution.matched_preferred_palette,
	      "typed compat low32 resolution should preserve preferred-palette matches");
	check(ci_low32_any_resolution.meta.repl_w == compat_meta.repl_w &&
	          ci_low32_any_resolution.meta.repl_h == compat_meta.repl_h,
	      "typed compat low32 resolution should preserve replacement dimensions");

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
	check(stats.compat_low32_family_count == 1, "provider stats should report compat low32 family count");
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
	check(deferred_entry->phrb_policy_key == "runtime-ready", "runtime-ready record should preserve its policy key");
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
	check(family_entry->phrb_policy_key == "family-runtime-ready", "family runtime-ready record should preserve its policy key");
	check(family_entry->phrb_sampled_object_id == "legacy-family-01020304-fs258",
	      "family runtime-ready record should preserve its sampled object id");
	ReplacementMeta family_meta = {};
	NativeSampledIdentity family_identity = {};
	ResolvedEntrySourceClass family_source_class = ResolvedEntrySourceClass::Unknown;
	check(family_provider.lookup_with_selector_and_identity(
	          family_checksum64,
	          258,
	          0,
	          &family_meta,
	          &family_identity,
	          &family_source_class,
	          &resolved_checksum64),
	      "family runtime-ready PHRB record should resolve through the generic exact checksum path");
	check(!family_identity.valid, "family runtime-ready PHRB record should not synthesize native sampled identity");
	check(family_source_class == ResolvedEntrySourceClass::Compat,
	      "family runtime-ready PHRB record should classify the generic winning entry as compat");
	check(family_meta.repl_w == 2 && family_meta.repl_h == 2,
	      "family runtime-ready PHRB record should preserve replacement dimensions");
	check(resolved_checksum64 == family_checksum64,
	      "family runtime-ready PHRB record should preserve the exact legacy checksum");
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
	check(!phrb_only_provider.has_compat_entries(),
	      "provider should keep compat disabled for pure native PHRB runtime loads");
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
	ReplacementResolution ordered_singleton_reserved_resolution = {};
	check(ordered_singleton_provider.resolve_reserved_ordered_surface_candidate(
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
	          &ordered_singleton_reserved_resolution),
	      "typed reserved ordered-surface resolution should resolve the explicit reserved selector path");
	check(ordered_singleton_reserved_resolution.kind ==
	          ReplacementResolutionKind::SampledOrderedSurfaceReservedSelector,
	      "typed reserved ordered-surface resolution should keep the ordered-surface path distinct from normal exact selectors");
	check(!ordered_singleton_reserved_resolution.ordered_surface_singleton,
	      "typed reserved ordered-surface resolution should not relabel the reserved selector path as family-singleton fallback");
	check(ordered_singleton_reserved_resolution.resolved_selector_checksum64 == ordered_singleton_selector,
	      "typed reserved ordered-surface resolution should preserve the reserved selector");
	check(ordered_singleton_reserved_resolution.resolved_checksum64 == ordered_singleton_checksum64,
	      "typed reserved ordered-surface resolution should preserve the ordered-surface checksum");
	ReplacementResolution ordered_singleton_exact_resolution = {};
	check(ordered_singleton_provider.resolve_sampled_candidate(
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
	          &ordered_singleton_exact_resolution),
	      "typed sampled resolution should preserve exact selector hits before falling back to sampled-family singleton logic");
	check(ordered_singleton_exact_resolution.kind == ReplacementResolutionKind::SampledExactSelector,
	      "typed sampled resolution should classify exact selector hits distinctly");
	check(!ordered_singleton_exact_resolution.ordered_surface_singleton,
	      "typed sampled resolution should not relabel exact selector hits as ordered-surface singleton fallback");
	ReplacementResolution ordered_singleton_family_resolution = {};
	check(ordered_singleton_provider.resolve_sampled_candidate(
	          2,
	          1,
	          96,
	          16,
	          2,
	          2,
	          ordered_singleton_sampled_low32,
	          ordered_singleton_palette_crc,
	          258,
	          0,
	          &ordered_singleton_family_resolution),
	      "typed sampled resolution should still expose ordered-surface singleton family fallback when no selector is requested");
	check(ordered_singleton_family_resolution.kind == ReplacementResolutionKind::SampledFamilySingleton,
	      "typed sampled resolution should classify selector-free ordered-surface singleton fallback distinctly");
	check(ordered_singleton_family_resolution.ordered_surface_singleton,
	      "typed sampled resolution should preserve ordered-surface singleton family fallback classification");
	SampledFamilyDiagnostics ordered_singleton_diag = {};
	check(ordered_singleton_provider.describe_sampled_family(
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
	          &ordered_singleton_diag),
	      "ordered-surface singleton sampled family should expose diagnostics");
	check(ordered_singleton_diag.active_has_ordered_surface_selectors,
	      "ordered-surface singleton diagnostics should report ordered selectors");
	check(ordered_singleton_diag.active_ordered_surface_selector_count == 1,
	      "ordered-surface singleton diagnostics should preserve the ordered selector count");
	check(!ordered_singleton_diag.active_is_pool,
	      "ordered-surface singleton diagnostics should not classify the family as a pool");
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
	ReplacementMeta phrb_preference_meta = {};
	NativeSampledIdentity phrb_preference_identity = {};
	ResolvedEntrySourceClass phrb_preference_source_class = ResolvedEntrySourceClass::Unknown;
	uint64_t phrb_preference_resolved_checksum64 = 0;
	uint64_t phrb_preference_resolved_selector_checksum64 = 0xffffffffu;
	check(phrb_preference_provider.lookup_with_selector_and_identity(
	          phrb_preference_checksum64,
	          258,
	          0,
	          &phrb_preference_meta,
	          &phrb_preference_identity,
	          &phrb_preference_source_class,
	          &phrb_preference_resolved_checksum64,
	          &phrb_preference_resolved_selector_checksum64),
	      "generic lookup helper should resolve native sampled PHRB entries ahead of family-runtime compat duplicates");
	check(phrb_preference_meta.repl_w == 2 && phrb_preference_meta.repl_h == 2,
	      "generic lookup helper should preserve native replacement dimensions when a compat family stub shares the checksum");
	check(phrb_preference_identity.valid,
	      "generic lookup helper should preserve native sampled identity when a compat family stub shares the checksum");
	check(phrb_preference_source_class == ResolvedEntrySourceClass::Native,
	      "generic lookup helper should classify native sampled PHRB winners explicitly");
	check(phrb_preference_identity.sampled_low32 == phrb_preference_sampled_low32 &&
	          phrb_preference_identity.sampled_palette_crc == phrb_preference_palette_crc,
	      "generic lookup helper should expose the native sampled checksum identity");
	check(phrb_preference_resolved_checksum64 == phrb_preference_checksum64 &&
	          phrb_preference_resolved_selector_checksum64 == 0,
	      "generic lookup helper should report the native exact checksum and selector");
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
	ReplacementMeta phrb_preference_compat_meta = {};
	check(phrb_preference_provider.lookup_compat_with_selector(
	          phrb_preference_checksum64,
	          258,
	          0,
	          &phrb_preference_compat_meta),
	      "compat lookup should still expose the family-runtime fallback explicitly");
	check(phrb_preference_compat_meta.repl_w == 4 && phrb_preference_compat_meta.repl_h == 1,
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
	ReplacementMeta duplicate_lookup_meta = {};
	NativeSampledIdentity duplicate_lookup_identity = {};
	ResolvedEntrySourceClass duplicate_lookup_source_class = ResolvedEntrySourceClass::Unknown;
	uint64_t duplicate_resolved_checksum64 = 0;
	uint64_t duplicate_resolved_selector_checksum64 = 0;
	check(duplicate_provider.lookup_with_selector_and_identity(
	          duplicate_diags[0].active_checksum64,
	          768,
	          duplicate_selector,
	          &duplicate_lookup_meta,
	          &duplicate_lookup_identity,
	          &duplicate_lookup_source_class,
	          &duplicate_resolved_checksum64,
	          &duplicate_resolved_selector_checksum64),
	      "selector-bearing generic lookup helper should resolve duplicate sampled entries");
	check(duplicate_lookup_identity.valid,
	      "selector-bearing generic lookup helper should preserve native sampled identity");
	check(duplicate_lookup_source_class == ResolvedEntrySourceClass::Native,
	      "selector-bearing generic lookup helper should classify sampled duplicate winners as native");
	check(duplicate_resolved_checksum64 == duplicate_diags[0].active_checksum64,
	      "selector-bearing generic lookup helper should report the resolved checksum");
	check(duplicate_resolved_selector_checksum64 == duplicate_selector,
	      "selector-bearing generic lookup helper should report the resolved selector checksum");

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
	write_file(rejected_path, {1, 2, 3});
	check(!rejected_provider.load_cache_dir(rejected_path) &&
	      rejected_provider.load_error() == ReplacementLoadStatus::Invalid,
	      "unreadable or truncated metadata reports a damaged pack");
	remove_tree(rejected_dir);

	remove_tree(dir);
	std::cout << "emu_unit_texture_replacement_provider_test: PASS" << std::endl;
	return 0;
}
