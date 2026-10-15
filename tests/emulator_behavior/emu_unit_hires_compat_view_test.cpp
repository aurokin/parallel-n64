#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_load_origin_policy.hpp"
#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/texture_keying.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

namespace
{
using namespace RDP;
using namespace RDP::detail;

void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

void test_ci4_load_tile_padding_and_binding_lifetime()
{
	std::array<uint8_t, 8192> rdram;
	for (size_t i = 0; i < rdram.size(); i++)
		rdram[i] = uint8_t(i * 73 + (i / 640) * 19 + 17);
	TileMeta meta = {};
	meta.fmt = TextureFormat::CI;
	meta.size = TextureSize::Bpp4;
	meta.stride = 8;
	meta.mask_s = 3;
	meta.mask_t = 6;
	TileSize size = {};
	size.shi = 7 * 4;
	size.thi = 63 * 4;
	HiresLoadTileSource source;
	source.address = 1280;
	source.image_width = 4;
	source.row_stride = 4;
	source.tmem_stride = 8;
	source.format = TextureFormat::CI;
	source.size = TextureSize::Bpp8;
	source.valid = true;
	HiresCompatView view;
	check(compute_hires_compat_view(meta, size, source.address, source, view),
	      "CI8 transport can qualify its CI4 draw view");
	check(view.source_layout && view.width == 8 && view.height == 64 &&
	      view.address == source.address && view.row_stride == 4,
	      "CI4 conversion doubles source width units and preserves byte crop/stride");
	const auto texture_crc = rice_crc32_wrapped(rdram.data(), rdram.size(), view.address,
	                                           view.width, view.height, unsigned(meta.size), view.row_stride);
	check(texture_crc == 0xe28f3e17u && texture_crc == rice_crc32_wrapped(
	          rdram.data(), rdram.size(), source.address, 4, 64, unsigned(TextureSize::Bpp8), 4),
	      "qualified CI4 rows reproduce the independent CRC of the CI8 transport bytes");
	check(rice_crc32_wrapped(rdram.data(), rdram.size(), source.address, 8, 64, 0, 8) == 0x44a006bfu,
	      "the padded TMEM stride describes different source pixels");
	check(hires_compat_view_fits_key(view, 8, 64, meta.size, meta.stride),
	      "a narrow CI4 key is admitted without counting TMEM padding as extra texels");
	const bool source_binding = hires_compat_binding_needs_source_layout(view, 8, 64, meta.size, meta.stride);
	check(source_binding, "live padding-qualified raw admission acquires source dependency");
	check(!hires_source_binding_must_retire(source_binding, true, view),
	      "the admitted strip retains its unchanged qualified layout");
	check(!hires_compat_binding_needs_source_layout(view, 0, 0, meta.size, meta.stride),
	      "an unknown key window cannot establish a padding dependency");
	for (unsigned mismatch = 0; mismatch < 3; mismatch++)
	{
		auto incompatible = source;
		if (mismatch == 0)
			incompatible.format = TextureFormat::RGBA;
		else
			incompatible.size = mismatch == 1 ? TextureSize::Bpp16 : TextureSize::Bpp32;
		compute_hires_compat_view(meta, size, source.address, incompatible, view);
		check(!view.source_layout && view.row_stride == 8,
		      "other source format/size reinterpretations retain the legacy path");
		check(hires_source_binding_must_retire(source_binding, true, view),
		      "a source-dependent strip cannot retain an unsupported reinterpretation");
	}
	auto reversed_meta = meta;
	reversed_meta.size = TextureSize::Bpp8;
	auto reversed_source = source;
	reversed_source.size = TextureSize::Bpp4;
	compute_hires_compat_view(reversed_meta, size, source.address, reversed_source, view);
	check(!view.source_layout, "the CI4-to-CI8 reverse reinterpretation remains unsupported");
	auto reshaped = meta;
	reshaped.stride = 0;
	compute_hires_compat_view(reshaped, size, source.address, source, view);
	check(!view.source_layout && hires_compat_view_fits_key(view, 8, 64, meta.size, reshaped.stride),
	      "the legacy guard alone does not reject a zero-line stride transition");
	check(hires_source_binding_must_retire(source_binding, true, view),
	      "a padding-qualified binding retires before the unsupported layout retries");
	check(!hires_source_binding_must_retire(false, true, view),
	      "pending raw upload and LoadBlock bindings keep their prior reshape policy");
	compute_hires_compat_view(meta, size, source.address, {}, view);
	check(!view.source_layout && !hires_compat_binding_needs_source_layout(view, 8, 64, meta.size, meta.stride),
	      "a raw overwrite without source provenance cannot inherit the padding exemption");
	meta.mask_t = 0;
	size.thi = 64 * 4;
	compute_hires_compat_view(meta, size, source.address, source, view);
	check(!hires_compat_view_fits_key(view, 8, 64, meta.size, meta.stride) &&
	      !hires_compat_binding_needs_source_layout(view, 8, 64, meta.size, meta.stride),
	      "a real extra source row cannot acquire padding-qualified admission");
	meta.mask_s = 0;
	size.shi = 8 * 4;
	size.thi = 63 * 4;
	source.image_width = 5;
	source.row_stride = 5;
	compute_hires_compat_view(meta, size, source.address, source, view);
	check(view.width == 9 && !hires_compat_view_fits_key(view, 8, 64, meta.size, meta.stride),
	      "a real extra source column is rejected independently of padding");
	meta.stride = 16;
	meta.mask_s = 5;
	meta.mask_t = 6;
	size.shi = 31 * 4;
	source.image_width = 16;
	source.row_stride = source.tmem_stride = 16;
	compute_hires_compat_view(meta, size, source.address, source, view);
	check(view.source_layout && hires_compat_view_fits_key(view, 32, 64, meta.size, meta.stride) &&
	      !hires_compat_binding_needs_source_layout(view, 32, 64, meta.size, meta.stride),
	      "aligned CI4 source rows do not change the raw binding lifetime policy");
}

// Paper Mario's entity shadow and Mt. Lavalava smoke load I4/IA4 art through
// the 4-bit tile macro as 8-bit rows. The draw view must re-key at the 4-bit
// tile identity instead of keeping (then dropping) the 8-bit upload key.
void test_intensity_four_bit_load_tile_transport()
{
	std::array<uint8_t, 4096> rdram;
	for (size_t i = 0; i < rdram.size(); i++)
		rdram[i] = uint8_t(i * 37 + (i / 64) * 11 + 5);
	for (TextureFormat fmt : { TextureFormat::I, TextureFormat::IA })
	{
		TileMeta meta = {};
		meta.fmt = fmt;
		meta.size = TextureSize::Bpp4;
		meta.stride = 8;
		meta.mask_s = 4;
		meta.mask_t = 4;
		TileSize size = {};
		size.shi = 15 * 4;
		size.thi = 15 * 4;
		HiresLoadTileSource source;
		source.address = 512;
		source.image_width = 8;
		source.row_stride = 8;
		source.tmem_stride = 8;
		source.format = fmt;
		source.size = TextureSize::Bpp8;
		source.valid = true;
		HiresCompatView view;
		check(compute_hires_compat_view(meta, size, source.address, source, view),
		      "8-bit transport can qualify its 4-bit intensity draw view");
		check(view.source_layout && view.width == 16 && view.height == 16 && view.row_stride == 8,
		      "4-bit intensity conversion doubles source width units and preserves byte stride");
		check(hires_source_view_needs_rekey(view, 8, 16),
		      "the 8-bit upload key window re-keys at the 4-bit draw view");
		check(rice_crc32_wrapped(rdram.data(), rdram.size(), view.address, view.width, view.height,
		                         unsigned(meta.size), view.row_stride) == 0x1f33063au,
		      "the 4-bit intensity view hashes the transported bytes");

		// A crop of a wider RDRAM image keys its own rows; the legacy view would
		// hash the uncropped TMEM-stride window from the load address instead.
		auto cropped = source;
		cropped.address = 512 + 16 + 10;
		cropped.row_stride = 16;
		check(compute_hires_compat_view(meta, size, 512, cropped, view) && view.source_layout &&
		      view.address == cropped.address && view.width == 16 && view.row_stride == 16,
		      "a cropped 8-bit transport keeps its source address and RDRAM stride");
		check(rice_crc32_wrapped(rdram.data(), rdram.size(), view.address, view.width, view.height,
		                         unsigned(meta.size), view.row_stride) == 0x126164a3u,
		      "the cropped 4-bit intensity view hashes the cropped source rows");

		auto other_format = source;
		other_format.format = fmt == TextureFormat::I ? TextureFormat::IA : TextureFormat::I;
		compute_hires_compat_view(meta, size, source.address, other_format, view);
		check(!view.source_layout, "a cross-format 4-bit reinterpretation retains the legacy path");
	}
}
}

int main()
{
	test_ci4_load_tile_padding_and_binding_lifetime();
	test_intensity_four_bit_load_tile_transport();
	std::array<uint8_t, 8192> rdram;
	for (size_t i = 0; i < rdram.size(); i++)
		rdram[i] = uint8_t(i * 73 + (i / 640) * 19 + 17);
	TileMeta meta = {};
	meta.fmt = TextureFormat::RGBA;
	meta.size = TextureSize::Bpp16;
	meta.stride = 648;
	meta.mask_s = 9;
	meta.mask_t = 1;
	TileSize size = {};
	size.shi = 320 * 4;
	size.thi = 2 * 4;
	HiresLoadTileSource source;
	source.address = 1280;
	source.image_width = 320;
	source.row_stride = 640;
	source.tmem_stride = meta.stride;
	source.format = meta.fmt;
	source.size = meta.size;
	source.valid = true;
	HiresCompatView view;
	check(hires_load_tile_source_supported(true, meta.size, meta.size, 0),
	      "an unsplit equal-size LoadTile can retain its source crop");
	check(!hires_load_tile_source_supported(true, meta.size, meta.size, 648 * 6),
	      "the last split chunk cannot describe the restored full tile view");
	check(!hires_load_tile_source_supported(false, meta.size, meta.size, 0),
	      "LoadBlock cannot acquire LoadTile source-layout provenance");
	auto crc = [&]() {
		return rice_crc32_wrapped(rdram.data(), rdram.size(), view.address,
		                         view.width, view.height, unsigned(meta.size), view.row_stride);
	};
	check(compute_hires_compat_view(meta, size, 0, source, view), "source-backed strip is valid");
	check(view.source_layout && view.width == 320 && view.height == 2 && view.row_stride == 640,
	      "inclusive load bounds and padded TMEM do not enlarge the source strip");
	check(crc() == 0x5d1399a2u, "cropped source rows reproduce the independent Rice CRC");
	check(hires_source_view_needs_rekey(view, 321, 3),
	      "a raw upload hit cannot suppress the different masked source lookup");
	check(!hires_source_view_needs_rekey(view, 320, 2),
	      "an exact source-view hit does not repeatedly rekey unchanged dimensions");
	check(hires_compat_view_fits_key(view, 320, 2, meta.size, meta.stride),
	      "TMEM alignment padding does not reject an in-window source strip");
	source.address = 2560;
	compute_hires_compat_view(meta, size, 0, source, view);
	check(crc() == 0x34bed005u, "the next strip keys its own cropped source address");
	source.address = 1280;
	compute_hires_compat_view(meta, size, source.address, {}, view);
	check(!view.source_layout && view.width == 321 && view.row_stride == 648 && crc() == 0x43c809dcu,
	      "unqualified LoadBlock views retain the existing line-based convention");
	check(!hires_source_view_needs_rekey(view, 324, 3),
	      "LoadBlock retains its established subview binding policy");
	for (unsigned mismatch = 0; mismatch < 2; mismatch++)
	{
		auto incompatible = source;
		if (mismatch == 0)
			incompatible.format = TextureFormat::IA;
		else
			incompatible.size = TextureSize::Bpp8;
		compute_hires_compat_view(meta, size, source.address, incompatible, view);
		check(!view.source_layout && crc() == 0x43c809dcu,
		      "format or size reinterpretations cannot inherit source-layout units");
	}
	auto reshaped = meta;
	reshaped.stride = 320;
	compute_hires_compat_view(reshaped, size, source.address, source, view);
	check(!view.source_layout && view.row_stride == 320,
	      "an equal-offset alias with different TMEM row spacing cannot reuse source rows");
	compute_hires_compat_view(meta, size, source.address, source, view);
	check(!hires_source_binding_must_retire(true, true, view),
	      "a source binding survives an unchanged qualified draw view");
	reshaped.stride = 640;
	compute_hires_compat_view(reshaped, size, source.address, source, view);
	check(hires_compat_view_fits_key(view, 320, 2, meta.size, reshaped.stride),
	      "the legacy byte guard alone admits this source-binding stride transition");
	check(hires_source_binding_must_retire(true, true, view),
	      "a minted source binding retires before the incompatible legacy lookup can retry");
	check(!hires_source_binding_must_retire(false, true, view),
	      "pending raw-upload bindings retain their established reshape policy");
	check(hires_source_binding_must_retire(true, false, view),
	      "loss of a computable source view also retires its source binding");
	meta.stride = 256;
	meta.mask_s = 7;
	meta.mask_t = 3;
	size.shi = 124 * 4;
	size.thi = 8 * 4;
	source.address = 1984;
	source.image_width = 124;
	source.row_stride = 248;
	source.tmem_stride = meta.stride;
	compute_hires_compat_view(meta, size, 0, source, view);
	check(view.width == 124 && view.height == 8 && crc() == 0xbd26044bu,
	      "unaligned copyright rows use their source width and crop");
	view.height = 9;
	check(!hires_compat_view_fits_key(view, 124, 8, meta.size, meta.stride),
	      "actual extra sampled rows are rejected");
	view.height = 8;
	view.width = 125;
	check(!hires_compat_view_fits_key(view, 124, 8, meta.size, meta.stride),
	      "actual extra sampled columns are rejected");
	view = {};
	view.width = 32;
	view.height = 56;
	check(!hires_compat_view_fits_key(view, 32, 32, meta.size, 64),
	      "legacy out-of-window decal views retain the rejection guard");
	view.width = 15;
	view.height = 32;
	check(!hires_compat_view_fits_key(view, 15, 32, TextureSize::Bpp4, 8),
	      "legacy odd CI4 widths retain their original packed-byte guard");
	meta.stride = 32;
	meta.mask_s = meta.mask_t = 0;
	size.shi = 12 * 4;
	size.thi = 2 * 4;
	source.address = 0;
	source.image_width = 13;
	source.row_stride = 26;
	source.tmem_stride = meta.stride;
	compute_hires_compat_view(meta, size, 0, source, view);
	check(view.width == 13 && view.height == 3 && crc() == 0xb1b212ebu,
	      "odd unmasked heights are preserved without a game-specific adjustment");

	HiresLoadOrigin target;
	target.format = meta.fmt;
	target.size = meta.size;
	target.width = 13;
	target.height = 3;
	target.stride = 32;
	HiresLoadOrigin peers[2] = { target, target };
	peers[0].address = peers[1].address = 0x1000;
	peers[0].source = peers[1].source = source;
	check(find_hires_load_origin_info(target, peers, 2).source == source,
	      "agreeing aliases transfer the crop and source stride with their origin");
	peers[1].source.address += 26;
	check(find_hires_load_origin_info(target, peers, 2).address == 0,
	      "equal image bases with conflicting crops preserve native fallback");
	return 0;
}
