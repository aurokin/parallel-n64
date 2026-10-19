#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "rdp_common.hpp"
#include "rdp_hires_compat_view.hpp"

namespace RDP
{
namespace detail
{
// Project64 Jabo texture packs name each replacement by the MD5 of the texture
// as Jabo uploaded it to Direct3D: the draw-time view (the same window as the
// GLideN64-compat key), each texel converted to Jabo's surface format and
// written little-endian, rows packed without padding.
//
//   RGBA16 -> A1R5G5B5 (the N64 value rotated right by one bit)
//   RGBA32 -> A8R8G8B8 (bytes B, G, R, A)
//   IA16   -> A8R8G8B8 (I, I, I, A)
//   IA8    -> A4R4G4B4 (A4, I4, I4, I4)
//   IA4    -> A4R4G4B4 (I4 = I3 << 1 without low-bit fill, A4 = 0xF or 0)
//   I8     -> A8R8G8B8 (I, I, I, I)
//   I4     -> A4R4G4B4 (every nibble = I)
//   CI4/8  -> palette lookup; RGBA16 TLUT -> A1R5G5B5, IA16 TLUT -> A8R8G8B8
//
// The key carries no palette field. The conversions were derived offline from
// ROM data against real pack names; the IA16 TLUT case is unverified.

// Compact MD5 (RFC 1321), written for this header.
inline void hires_md5_block(uint32_t state[4], const uint8_t *block)
{
	static const uint32_t k[64] = {
		0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
		0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu, 0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
		0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau, 0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
		0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
		0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu, 0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
		0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
		0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
		0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u, 0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u,
	};
	static const uint8_t r[16] = { 7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21 };

	uint32_t m[16];
	for (unsigned i = 0; i < 16; i++)
		m[i] = uint32_t(block[4 * i]) | uint32_t(block[4 * i + 1]) << 8 |
		       uint32_t(block[4 * i + 2]) << 16 | uint32_t(block[4 * i + 3]) << 24;

	uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
	for (unsigned i = 0; i < 64; i++)
	{
		uint32_t f, g;
		if (i < 16)
		{
			f = (b & c) | (~b & d);
			g = i;
		}
		else if (i < 32)
		{
			f = (d & b) | (~d & c);
			g = (5 * i + 1) & 15;
		}
		else if (i < 48)
		{
			f = b ^ c ^ d;
			g = (3 * i + 5) & 15;
		}
		else
		{
			f = c ^ (b | ~d);
			g = (7 * i) & 15;
		}
		const uint32_t x = a + f + k[i] + m[g];
		const unsigned s = r[(i >> 4) * 4 + (i & 3)];
		a = d;
		d = c;
		c = b;
		b += (x << s) | (x >> (32 - s));
	}
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
}

inline void hires_md5(const uint8_t *data, size_t size, uint8_t out[16])
{
	uint32_t state[4] = { 0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u };
	size_t offset = 0;
	for (; offset + 64 <= size; offset += 64)
		hires_md5_block(state, data + offset);

	uint8_t tail[128] = {};
	const size_t rest = size - offset;
	if (rest)
		memcpy(tail, data + offset, rest);
	tail[rest] = 0x80;
	const size_t tail_size = rest < 56 ? 64 : 128;
	const uint64_t bits = uint64_t(size) << 3;
	for (unsigned i = 0; i < 8; i++)
		tail[tail_size - 8 + i] = uint8_t(bits >> (8 * i));
	for (size_t block = 0; block < tail_size; block += 64)
		hires_md5_block(state, tail + block);

	for (unsigned i = 0; i < 16; i++)
		out[i] = uint8_t(state[i >> 2] >> (8 * (i & 3)));
}

// tlut_tmem_shadow mirrors the TMEM palette: entry i is big-endian at byte 8 * i.
inline uint16_t hires_jabo_tlut_entry(const uint8_t *tlut_tmem_shadow, uint32_t index)
{
	const uint8_t *entry = tlut_tmem_shadow + 8u * (index & 0xffu);
	return uint16_t(entry[0] << 8 | entry[1]);
}

// Decodes `view` from RDRAM (host words: N64 byte a lives at a ^ 3) to Jabo's
// surface bytes in `scratch` and hashes them. Returns false for formats Jabo
// packs cannot key, or a CI texture without a palette.
inline bool compute_hires_jabo_key(const uint8_t *rdram, size_t rdram_size, const HiresCompatView &view,
                                   TextureFormat fmt, TextureSize size, const uint8_t *tlut_tmem_shadow,
                                   uint32_t palette_bank, bool tlut_ia16, std::vector<uint8_t> &scratch,
                                   uint8_t out[16])
{
	if (!rdram || rdram_size == 0 || view.width == 0 || view.height == 0)
		return false;

	const bool ci = fmt == TextureFormat::CI;
	if (ci && (!tlut_tmem_shadow || size > TextureSize::Bpp8))
		return false;
	bool wide = false;
	switch (fmt)
	{
	case TextureFormat::RGBA:
		if (size < TextureSize::Bpp16)
			return false;
		wide = size == TextureSize::Bpp32;
		break;
	case TextureFormat::IA:
		if (size == TextureSize::Bpp32)
			return false;
		wide = size == TextureSize::Bpp16;
		break;
	case TextureFormat::I:
		if (size > TextureSize::Bpp8)
			return false;
		wide = size == TextureSize::Bpp8;
		break;
	case TextureFormat::CI:
		wide = tlut_ia16;
		break;
	default:
		return false;
	}

	const uint32_t mask = uint32_t(rdram_size - 1);
	auto rd8 = [&](uint32_t addr) { return rdram[(addr ^ 3u) & mask]; };
	auto nibble = [&](uint32_t row, uint32_t x) {
		const uint8_t byte = rd8(row + (x >> 1));
		return uint8_t((x & 1) ? byte & 15 : byte >> 4);
	};

	scratch.resize(size_t(view.width) * view.height * (wide ? 4 : 2));
	uint8_t *o = scratch.data();
	auto put16 = [&](uint32_t v) {
		*o++ = uint8_t(v);
		*o++ = uint8_t(v >> 8);
	};
	auto put32 = [&](uint8_t b, uint8_t g, uint8_t r, uint8_t a) {
		*o++ = b;
		*o++ = g;
		*o++ = r;
		*o++ = a;
	};
	auto put_rgba16 = [&](uint32_t v) { put16((v >> 1) | ((v & 1u) << 15)); };
	auto put_i4 = [&](uint32_t i, uint32_t a) { put16(a << 12 | i << 8 | i << 4 | i); };

	for (uint32_t y = 0; y < view.height; y++)
	{
		const uint32_t row = view.address + y * view.row_stride;
		for (uint32_t x = 0; x < view.width; x++)
		{
			if (ci)
			{
				const uint32_t index = size == TextureSize::Bpp8 ? rd8(row + x) : (palette_bank << 4) | nibble(row, x);
				const uint16_t v = hires_jabo_tlut_entry(tlut_tmem_shadow, index);
				if (tlut_ia16)
					put32(uint8_t(v >> 8), uint8_t(v >> 8), uint8_t(v >> 8), uint8_t(v));
				else
					put_rgba16(v);
				continue;
			}

			switch (unsigned(fmt) << 4 | unsigned(size))
			{
			case 0x02:
				put_rgba16(uint32_t(rd8(row + 2 * x)) << 8 | rd8(row + 2 * x + 1));
				break;
			case 0x03:
			{
				const uint32_t addr = row + 4 * x;
				put32(rd8(addr + 2), rd8(addr + 1), rd8(addr), rd8(addr + 3));
				break;
			}
			case 0x32:
			{
				const uint8_t i = rd8(row + 2 * x);
				put32(i, i, i, rd8(row + 2 * x + 1));
				break;
			}
			case 0x31:
			{
				const uint8_t t = rd8(row + x);
				put_i4(t >> 4, t & 15);
				break;
			}
			case 0x30:
			{
				const uint8_t t = nibble(row, x);
				put_i4(t & 14, (t & 1) ? 15 : 0);
				break;
			}
			case 0x41:
			{
				const uint8_t i = rd8(row + x);
				put32(i, i, i, i);
				break;
			}
			case 0x40:
			{
				const uint8_t i = nibble(row, x);
				put_i4(i, i);
				break;
			}
			}
		}
	}

	hires_md5(scratch.data(), scratch.size(), out);
	return true;
}

// The renderer clears its memo rather than grow past this many views.
constexpr size_t HIRES_JABO_MEMO_LIMIT = 8192;

// Draw-time Jabo keys are memoized on the Rice texture key plus everything the
// Jabo bytes depend on beyond it: view geometry and address, format, palette
// bank and TLUT type, and a hash of the palette entries the format can index.
// FNV-1a over exactly the RDRAM bytes compute_hires_jabo_key reads (whole
// rows, including the last nibble of odd-width 4-bit rows), mixed with the
// palette inputs. Jabo memoization and binding reuse key on this rather than
// the Rice checksum, which skips leading row bytes and the last 4-bit texel
// and is a weak sum, so it can stay equal while the hashed texels change.
inline uint64_t hires_jabo_source_hash(const uint8_t *rdram, size_t rdram_size, const HiresCompatView &view,
                                       TextureSize size, uint32_t palette_hash, bool tlut_ia16)
{
	const uint32_t mask = uint32_t(rdram_size - 1);
	const uint32_t row_bytes = ((view.width << unsigned(size)) + 1) >> 1;
	uint64_t h = 0xcbf29ce484222325ull;
	for (uint32_t y = 0; y < view.height; y++)
	{
		const uint32_t row = view.address + y * view.row_stride;
		for (uint32_t x = 0; x < row_bytes; x++)
			h = (h ^ rdram[((row + x) ^ 3u) & mask]) * 0x100000001b3ull;
	}
	h = (h ^ palette_hash) * 0x100000001b3ull;
	return (h ^ uint64_t(tlut_ia16)) * 0x100000001b3ull;
}

struct HiresJaboMemoKey
{
	uint64_t source_hash = 0;
	uint32_t address = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t row_stride = 0;
	uint32_t palette_hash = 0;
	uint16_t formatsize = 0;
	uint8_t palette_bank = 0;
	uint8_t tlut_ia16 = 0;

	bool operator==(const HiresJaboMemoKey &other) const
	{
		return source_hash == other.source_hash && address == other.address && width == other.width &&
		       height == other.height && row_stride == other.row_stride && palette_hash == other.palette_hash &&
		       formatsize == other.formatsize && palette_bank == other.palette_bank && tlut_ia16 == other.tlut_ia16;
	}
};

struct HiresJaboMemoKeyHash
{
	size_t operator()(const HiresJaboMemoKey &key) const
	{
		uint64_t h = key.source_hash * 0x9e3779b97f4a7c15ull;
		const uint64_t fields[] = {
			key.address, uint64_t(key.width) << 32 | key.height, uint64_t(key.row_stride) << 32 | key.palette_hash,
			uint64_t(key.formatsize) << 16 | uint64_t(key.palette_bank) << 8 | key.tlut_ia16,
		};
		for (uint64_t field : fields)
			h = (h ^ field) * 0x100000001b3ull + (h >> 29);
		return size_t(h);
	}
};

// FNV-1a over the palette entries a CI tile can index (one CI4 bank or all 256).
inline uint32_t hires_jabo_palette_hash(const uint8_t *tlut_tmem_shadow, TextureSize size, uint32_t palette_bank)
{
	const uint32_t first = size == TextureSize::Bpp4 ? (palette_bank & 15u) << 4 : 0;
	const uint32_t count = size == TextureSize::Bpp4 ? 16 : 256;
	uint32_t h = 2166136261u;
	for (uint32_t i = first; i < first + count; i++)
		h = (h ^ hires_jabo_tlut_entry(tlut_tmem_shadow, i)) * 16777619u;
	return h;
}

// The 128-bit MD5 split into the two little-endian halves used as lookup keys.
inline std::array<uint64_t, 2> hires_jabo_key_halves(const uint8_t md5[16])
{
	std::array<uint64_t, 2> halves = {};
	for (unsigned i = 0; i < 16; i++)
		halves[i >> 3] |= uint64_t(md5[i]) << (8 * (i & 7));
	return halves;
}
}
}
