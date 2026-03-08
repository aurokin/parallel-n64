#include "../mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/texture_replacement.hpp"

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace RDP;

namespace
{
void usage(const char *argv0)
{
	std::cerr
	    << "usage: " << argv0 << " --cache-dir DIR --key HEX64 --formatsize HEX16 --out-dir DIR [--label NAME]\n";
}

bool parse_u64_hex(const std::string &text, uint64_t &value)
{
	char *end = nullptr;
	value = std::strtoull(text.c_str(), &end, 16);
	return end && *end == '\0';
}

bool parse_u16_hex(const std::string &text, uint16_t &value)
{
	char *end = nullptr;
	const unsigned long parsed = std::strtoul(text.c_str(), &end, 16);
	if (!(end && *end == '\0') || parsed > 0xfffful)
		return false;
	value = static_cast<uint16_t>(parsed);
	return true;
}

bool write_ppm(const std::filesystem::path &path, const ReplacementImage &image)
{
	std::ofstream file(path, std::ios::binary);
	if (!file.good())
		return false;

	file << "P6\n" << image.meta.repl_w << " " << image.meta.repl_h << "\n255\n";
	for (size_t i = 0; i + 3 < image.rgba8.size(); i += 4)
	{
		file.put(static_cast<char>(image.rgba8[i + 0]));
		file.put(static_cast<char>(image.rgba8[i + 1]));
		file.put(static_cast<char>(image.rgba8[i + 2]));
	}
	return file.good();
}

bool write_alpha_pgm(const std::filesystem::path &path, const ReplacementImage &image)
{
	std::ofstream file(path, std::ios::binary);
	if (!file.good())
		return false;

	file << "P5\n" << image.meta.repl_w << " " << image.meta.repl_h << "\n255\n";
	for (size_t i = 0; i + 3 < image.rgba8.size(); i += 4)
		file.put(static_cast<char>(image.rgba8[i + 3]));
	return file.good();
}

void print_stats(const ReplacementImage &image)
{
	size_t zero_alpha = 0;
	size_t full_alpha = 0;
	size_t partial_alpha = 0;
	size_t nonzero_rgb_zero_alpha = 0;
	for (size_t i = 0; i + 3 < image.rgba8.size(); i += 4)
	{
		const uint8_t r = image.rgba8[i + 0];
		const uint8_t g = image.rgba8[i + 1];
		const uint8_t b = image.rgba8[i + 2];
		const uint8_t a = image.rgba8[i + 3];
		if (a == 0)
		{
			zero_alpha++;
			if (r != 0 || g != 0 || b != 0)
				nonzero_rgb_zero_alpha++;
		}
		else if (a == 255)
			full_alpha++;
		else
			partial_alpha++;
	}

	std::cout
	    << "dimensions: " << image.meta.repl_w << "x" << image.meta.repl_h << "\n"
	    << "pixels: " << (image.meta.repl_w * image.meta.repl_h) << "\n"
	    << "alpha_zero: " << zero_alpha << "\n"
	    << "alpha_full: " << full_alpha << "\n"
	    << "alpha_partial: " << partial_alpha << "\n"
	    << "alpha_zero_with_rgb: " << nonzero_rgb_zero_alpha << "\n";
}
}

int main(int argc, char **argv)
{
	std::string cache_dir;
	std::string out_dir;
	std::string label;
	uint64_t key = 0;
	uint16_t formatsize = 0;
	bool have_key = false;
	bool have_formatsize = false;

	for (int i = 1; i < argc; i++)
	{
		const std::string arg = argv[i];
		auto need_value = [&](const char *name) -> const char * {
			if (i + 1 >= argc)
			{
				std::cerr << "missing value for " << name << "\n";
				std::exit(2);
			}
			return argv[++i];
		};

		if (arg == "--cache-dir")
			cache_dir = need_value("--cache-dir");
		else if (arg == "--out-dir")
			out_dir = need_value("--out-dir");
		else if (arg == "--label")
			label = need_value("--label");
		else if (arg == "--key")
		{
			const std::string value = need_value("--key");
			if (!parse_u64_hex(value, key))
			{
				std::cerr << "invalid --key: " << value << "\n";
				return 2;
			}
			have_key = true;
		}
		else if (arg == "--formatsize")
		{
			const std::string value = need_value("--formatsize");
			if (!parse_u16_hex(value, formatsize))
			{
				std::cerr << "invalid --formatsize: " << value << "\n";
				return 2;
			}
			have_formatsize = true;
		}
		else
		{
			std::cerr << "unknown arg: " << arg << "\n";
			usage(argv[0]);
			return 2;
		}
	}

	if (cache_dir.empty() || out_dir.empty() || !have_key || !have_formatsize)
	{
		usage(argv[0]);
		return 2;
	}

	if (label.empty())
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%016" PRIx64 "_%04x", key, unsigned(formatsize));
		label = buf;
	}

	ReplacementProvider provider;
	provider.set_enabled(true);
	if (!provider.load_cache_dir(cache_dir))
	{
		std::cerr << "failed to load cache dir: " << cache_dir << "\n";
		return 1;
	}

	ReplacementImage image = {};
	if (!provider.decode_rgba8(key, formatsize, &image))
	{
		std::cerr << "decode failed for key=" << std::hex << key << " formatsize=" << formatsize << "\n";
		return 1;
	}

	std::filesystem::create_directories(out_dir);
	const auto base = std::filesystem::path(out_dir) / label;
	if (!write_ppm(base.string() + ".ppm", image))
	{
		std::cerr << "failed to write ppm\n";
		return 1;
	}
	if (!write_alpha_pgm(base.string() + ".alpha.pgm", image))
	{
		std::cerr << "failed to write alpha pgm\n";
		return 1;
	}

	print_stats(image);
	std::cout << "rgb_ppm: " << (base.string() + ".ppm") << "\n";
	std::cout << "alpha_pgm: " << (base.string() + ".alpha.pgm") << "\n";
	return 0;
}
