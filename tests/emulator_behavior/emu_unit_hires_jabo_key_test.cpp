#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_jabo_key.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using namespace RDP;
using namespace RDP::detail;

void check(bool condition, const std::string &message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

std::string hex(const uint8_t digest[16])
{
	char text[33];
	for (unsigned i = 0; i < 16; i++)
		std::snprintf(text + 2 * i, 3, "%02x", digest[i]);
	return text;
}

std::string md5_of(const std::string &text)
{
	uint8_t digest[16];
	hires_md5(reinterpret_cast<const uint8_t *>(text.data()), text.size(), digest);
	return hex(digest);
}

// The offline reference generator's texel pattern (scratch-jabo make_vectors.py).
std::vector<uint8_t> pattern(size_t count, unsigned seed)
{
	std::vector<uint8_t> bytes(count);
	for (size_t i = 0; i < count; i++)
		bytes[i] = uint8_t(i * 37 + seed * 11 + (i >> 3));
	return bytes;
}

struct Fixture
{
	static constexpr uint32_t base = 0x1231; // odd, so every byte crosses the ^3 host swizzle
	std::vector<uint8_t> rdram = std::vector<uint8_t>(1u << 16);
	uint8_t tlut[2048] = {};

	explicit Fixture(const std::vector<uint8_t> &texels)
	{
		for (size_t i = 0; i < texels.size(); i++)
			rdram[(base + i) ^ 3u] = texels[i];
	}

	// LoadTLUT leaves entry i big-endian at TMEM shadow byte 8 * i.
	void set_palette(const std::vector<uint8_t> &big_endian_entries)
	{
		for (size_t i = 0; i < big_endian_entries.size() / 2; i++)
		{
			tlut[8 * i] = big_endian_entries[2 * i];
			tlut[8 * i + 1] = big_endian_entries[2 * i + 1];
		}
	}

	bool key(TextureFormat fmt, TextureSize size, uint32_t w, uint32_t h, uint32_t bpl, std::string &out,
	         uint32_t bank = 0, bool ia16 = false, bool with_palette = true)
	{
		HiresCompatView view;
		view.address = base;
		view.width = w;
		view.height = h;
		view.row_stride = bpl;
		std::vector<uint8_t> scratch;
		uint8_t digest[16];
		if (!compute_hires_jabo_key(rdram.data(), rdram.size(), view, fmt, size, with_palette ? tlut : nullptr,
		                            bank, ia16, scratch, digest))
			return false;
		out = hex(digest);
		return true;
	}
};

void test_md5_matches_rfc1321_suite()
{
	check(md5_of("") == "d41d8cd98f00b204e9800998ecf8427e", "MD5 of the empty string");
	check(md5_of("abc") == "900150983cd24fb0d6963f7d28e17f72", "MD5 of abc");
	check(md5_of("abcdefghijklmnopqrstuvwxyz") == "c3fcd3d76192e4007dfb496cca67e13b", "MD5 of the alphabet");
	check(md5_of("12345678901234567890123456789012345678901234567890123456789012345678901234567890") ==
	          "57edf4a22be3c955ac49da2e2107b67a",
	      "MD5 across a block boundary");
	check(md5_of(std::string(56, 'a')) == "3b0c8ac703f828b04c6c197006d17218", "MD5 when padding needs a second block");
}

// Expected digests come from the offline reference (jabo_hash.py), which was
// verified against real Jabo pack names for every format but the IA16 TLUT.
void test_strided_views_match_reference_vectors()
{
	struct Vector
	{
		const char *name;
		TextureFormat fmt;
		TextureSize size;
		const char *md5;
	};
	const Vector vectors[] = {
		{ "rgba16", TextureFormat::RGBA, TextureSize::Bpp16, "06cb2aa35ee35b55e95f7265d6ddfa6a" },
		{ "rgba32", TextureFormat::RGBA, TextureSize::Bpp32, "5a48375109874db39847e3a5bf50ac12" },
		{ "ia16", TextureFormat::IA, TextureSize::Bpp16, "d1ed2724dffd7879973e96b5137f7bee" },
		{ "ia8", TextureFormat::IA, TextureSize::Bpp8, "509cfd16cb238fa884c68c7387b0500f" },
		{ "ia4", TextureFormat::IA, TextureSize::Bpp4, "4bab99e3305d50a88f6a8a2012f62af1" },
		{ "i8", TextureFormat::I, TextureSize::Bpp8, "1c35275bf2dc687b3a92b0204900b42d" },
		{ "i4", TextureFormat::I, TextureSize::Bpp4, "4934a42298bb76648e8bde7ab07537d5" },
		{ "ci8", TextureFormat::CI, TextureSize::Bpp8, "79c30bca52f0ed15bce423897cbb4538" },
		{ "ci4", TextureFormat::CI, TextureSize::Bpp4, "4d4aa37477d256fcf6311bbf26532e80" },
	};
	const uint32_t w = 8, h = 4;
	for (const auto &vector : vectors)
	{
		const uint32_t bpl = ((w << unsigned(vector.size)) >> 1) + 8; // eight junk bytes per row
		Fixture fixture(pattern(size_t(bpl) * h, unsigned(vector.size) + unsigned(vector.fmt)));
		fixture.set_palette(pattern(512, 99));
		std::string md5;
		check(fixture.key(vector.fmt, vector.size, w, h, bpl, md5), std::string(vector.name) + " is keyable");
		check(md5 == vector.md5, std::string(vector.name) + " digest " + md5 + " matches the reference");
	}
}

void test_real_pack_name()
{
	// SM64 Reloaded (Jabo) names a black 64x32 RGBA16 texture 620F0B67A91F7F74151BC5BE745B7110.
	Fixture fixture(std::vector<uint8_t>(4096));
	std::string md5;
	check(fixture.key(TextureFormat::RGBA, TextureSize::Bpp16, 64, 32, 128, md5) &&
	          md5 == "620f0b67a91f7f74151bc5be745b7110",
	      "a black 64x32 RGBA16 view reproduces the SM64 pack name");
}

void test_palette_bank_type_and_absence()
{
	const uint32_t bpl = 12;
	const auto texels = pattern(bpl * 4, 2);
	const auto palette = pattern(512, 99);
	Fixture banked(texels);
	banked.set_palette(palette);
	std::string bank3;
	check(banked.key(TextureFormat::CI, TextureSize::Bpp4, 8, 4, bpl, bank3, 3), "CI4 bank 3 is keyable");

	// Moving bank 3's entries to bank 0 must give the same digest at bank 0.
	Fixture shifted(texels);
	std::vector<uint8_t> moved(palette.begin() + 3 * 32, palette.begin() + 4 * 32);
	shifted.set_palette(moved);
	std::string bank0;
	check(shifted.key(TextureFormat::CI, TextureSize::Bpp4, 8, 4, bpl, bank0, 0) && bank0 == bank3,
	      "CI4 indexes the rendering tile's palette bank");

	std::string ia16;
	Fixture ci8(pattern(16 * 4, 3));
	ci8.set_palette(palette);
	check(ci8.key(TextureFormat::CI, TextureSize::Bpp8, 8, 4, 16, ia16, 0, true) &&
	          ia16 == "1bce89f69b8c7360f999e68fb10c6c8a",
	      "an IA16 TLUT decodes to A8R8G8B8 as in the reference (unverified against packs)");
	std::string none;
	check(!ci8.key(TextureFormat::CI, TextureSize::Bpp8, 8, 4, 16, none, 0, false, false),
	      "CI without a palette has no key");
}

void test_unkeyable_formats()
{
	Fixture fixture(pattern(256, 1));
	std::string md5;
	check(!fixture.key(TextureFormat::RGBA, TextureSize::Bpp8, 8, 4, 16, md5), "RGBA8 has no Jabo surface");
	check(!fixture.key(TextureFormat::I, TextureSize::Bpp16, 8, 4, 16, md5), "I16 has no Jabo surface");
	check(!fixture.key(TextureFormat::YUV, TextureSize::Bpp16, 8, 4, 16, md5), "YUV has no Jabo surface");
	check(!fixture.key(TextureFormat::RGBA, TextureSize::Bpp16, 0, 4, 16, md5), "an empty view has no key");
}

void test_memo_palette_hash_covers_only_indexable_entries()
{
	uint8_t tlut[2048] = {};
	const uint32_t bank2 = hires_jabo_palette_hash(tlut, TextureSize::Bpp4, 2);
	tlut[8 * 0x05] = 0x12; // bank 0 entry
	check(hires_jabo_palette_hash(tlut, TextureSize::Bpp4, 2) == bank2, "another CI4 bank does not change the hash");
	tlut[8 * 0x25 + 1] = 0x34; // bank 2 entry, low byte
	check(hires_jabo_palette_hash(tlut, TextureSize::Bpp4, 2) != bank2, "the tile's own CI4 bank changes the hash");
	const uint32_t ci8 = hires_jabo_palette_hash(tlut, TextureSize::Bpp8, 0);
	tlut[8 * 0xff] = 0x56;
	check(hires_jabo_palette_hash(tlut, TextureSize::Bpp8, 0) != ci8, "CI8 hashes all 256 entries");
}
}

int main()
{
	test_md5_matches_rfc1321_suite();
	test_strided_views_match_reference_vectors();
	test_real_pack_name();
	test_palette_bank_type_and_absence();
	test_unkeyable_formats();
	test_memo_palette_hash_covers_only_indexable_entries();
	std::cout << "emu_unit_hires_jabo_key_test: PASS" << std::endl;
	return 0;
}
