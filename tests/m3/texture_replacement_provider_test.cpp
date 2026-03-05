#include "texture_replacement.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace RDP;

static void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}

int main(int argc, char **argv)
{
	const std::string cache_dir = argc > 1 ? argv[1] : ".";
	const uint64_t sample_key = 0x78d0089f4e667e0eull; // observed Paper Mario cache hit
	const uint16_t sample_formatsize = 258;             // CI8 (fmt=2, siz=1)

	ReplacementProvider provider;
	provider.set_enabled(true);
	check(provider.load_cache_dir(cache_dir), "failed to load hi-res cache directory");
	check(provider.entry_count() > 0, "no hi-res cache entries found");

	ReplacementMeta meta = {};
	check(provider.lookup(sample_key, sample_formatsize, &meta), "expected sample key not found");
	check(meta.repl_w > 0 && meta.repl_h > 0, "invalid replacement dimensions");

	ReplacementImage image = {};
	check(provider.decode_rgba8(sample_key, sample_formatsize, &image), "failed to decode sample replacement");
	check(image.meta.repl_w == meta.repl_w && image.meta.repl_h == meta.repl_h, "meta mismatch after decode");
	check(!image.rgba8.empty(), "decoded image is empty");

	const size_t expected_size = size_t(image.meta.repl_w) * size_t(image.meta.repl_h) * 4u;
	check(image.rgba8.size() == expected_size, "decoded image does not match RGBA8 size");

	provider.set_enabled(false);
	check(!provider.lookup(sample_key, sample_formatsize, &meta), "disabled provider should not match");

	std::cout << "texture_replacement_provider_test: PASS (entries=" << provider.entry_count() << ")" << std::endl;
	return 0;
}
