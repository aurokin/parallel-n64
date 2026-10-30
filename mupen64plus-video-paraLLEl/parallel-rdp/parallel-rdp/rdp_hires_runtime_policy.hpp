// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace RDP
{
namespace detail
{
enum class HiresConfigureOutcome
{
	Disabled,
	MissingPath,
	LoadFailed,
	LoadSucceeded
};

// Header bytes are in canonical (big-endian ROM) order. This is an identity
// convention, not a ROM binding: regions and hacks retaining a Name share packs.
inline std::string hires_gliden_ident(const uint8_t *header64)
{
	std::string name;
	for (unsigned i = 0x20; i < 0x34 && header64[i]; i++)
		name += char(header64[i]);
	while (!name.empty() && name.back() == ' ')
		name.pop_back();
	for (auto &c : name)
		if (c == ':' || c == '/')
			c = '-';
	return name;
}

inline std::string hires_game_ident(const uint8_t *header64)
{
	const auto name = hires_gliden_ident(header64);
	const std::string code(reinterpret_cast<const char *>(header64 + 0x3b), 4);
	const bool usable_name = !name.empty() && std::all_of(name.begin(), name.end(), [](unsigned char c) {
		return c >= 0x20 && c <= 0x7e && std::string("<>\"\\|?*").find(char(c)) == std::string::npos;
	});
	if (usable_name && code != "CPSJ" && code != "CP2J")
		return name;
	return std::all_of(code.begin(), code.end(), [](unsigned char c) { return c >= 0x20 && c <= 0x7e; }) ?
	       code : std::string();
}

inline std::string hires_ascii_fold(std::string name, bool alphanumeric_only = false)
{
	std::string result;
	for (unsigned char c : name)
	{
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		if (!alphanumeric_only || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
			result += char(c);
	}
	return result;
}

inline bool hires_has_suffix(const std::string &name, const std::string &suffix)
{
	return name.size() >= suffix.size() &&
	       hires_ascii_fold(name.substr(name.size() - suffix.size())) == suffix;
}

inline std::string hires_path_filename(const std::string &path)
{
	return path.substr(path.find_last_of("/\\") + 1);
}

enum class HiresListingLocation { Hires, LegacyCache, LegacyTextures };

// I/O supplies existing regular files/directories and immediate regular-file
// children only. No filesystem operations occur in the selection policy.
struct HiresListingEntry
{
	std::string path;
	HiresListingLocation location = HiresListingLocation::Hires;
	bool directory = false;
	std::vector<std::string> files;
};

struct HiresPackageSelection
{
	std::vector<std::string> paths;
	std::vector<std::string> near_misses;
	std::vector<std::string> unconverted;
};

inline HiresPackageSelection select_hires_packages(const std::string &ident, const std::string &gliden_ident,
                                                   const std::vector<HiresListingEntry> &listing)
{
	HiresPackageSelection result;
	if (ident.empty())
		return result;
	const auto exact = hires_ascii_fold(ident);
	const auto gliden = hires_ascii_fold(gliden_ident);
	const auto folded = hires_ascii_fold(ident, true);
	const auto folded_gliden = hires_ascii_fold(gliden_ident, true);
	std::vector<std::string> directory_packages;
	for (const auto &entry : listing)
	{
		const auto name = hires_path_filename(entry.path);
		if (name.empty() || name[0] == '.')
			continue;
		const auto lower = hires_ascii_fold(name);
		const bool phrb = !entry.directory && hires_has_suffix(name, ".phrb");
		const auto label = phrb ? name.substr(0, name.size() - 5) : name;
		const bool match = hires_ascii_fold(label) == exact;
		if (entry.location == HiresListingLocation::Hires && (entry.directory || phrb))
		{
			if (match)
			{
				if (phrb)
					result.paths.push_back(entry.path);
				else
					for (const auto &file : entry.files)
						if (!file.empty() && file[0] != '.' && hires_has_suffix(file, ".phrb"))
							directory_packages.push_back(entry.path + "/" + file);
			}
			else
			{
				const auto near = hires_ascii_fold(label, true);
				if (!near.empty() && (near == folded || near == folded_gliden))
					result.near_misses.push_back(entry.path);
			}
		}
		if (gliden.empty())
			continue;
		if (!entry.directory && entry.location != HiresListingLocation::LegacyTextures &&
		    (lower == gliden + "_hirestextures.hts" || lower == gliden + "_hirestextures.htc"))
			result.unconverted.push_back(entry.path);
		if (entry.directory && lower == gliden)
		{
			if (entry.location == HiresListingLocation::LegacyTextures)
				result.unconverted.push_back(entry.path);
			else if (entry.location == HiresListingLocation::Hires)
			{
				bool has_phrb = false, has_source = false;
				for (const auto &file : entry.files)
				{
					if (file.empty() || file[0] == '.')
						continue;
					has_phrb |= hires_has_suffix(file, ".phrb");
					has_source |= hires_has_suffix(file, ".hts") || hires_has_suffix(file, ".htc") ||
					              hires_has_suffix(file, ".png") || hires_has_suffix(file, ".dds");
				}
				if (!has_phrb && has_source)
					result.unconverted.push_back(entry.path);
			}
		}
	}
	std::sort(result.paths.begin(), result.paths.end());
	std::sort(directory_packages.begin(), directory_packages.end());
	result.paths.insert(result.paths.end(), directory_packages.begin(), directory_packages.end());
	std::sort(result.near_misses.begin(), result.near_misses.end());
	std::sort(result.unconverted.begin(), result.unconverted.end());
	return result;
}

inline HiresConfigureOutcome classify_hires_configure_outcome(bool enable, const char *cache_path, bool load_ok)
{
	if (!enable)
		return HiresConfigureOutcome::Disabled;
	if (!cache_path || !*cache_path)
		return HiresConfigureOutcome::MissingPath;
	return load_ok ? HiresConfigureOutcome::LoadSucceeded : HiresConfigureOutcome::LoadFailed;
}

inline bool should_attach_hires_provider(HiresConfigureOutcome outcome)
{
	return outcome == HiresConfigureOutcome::LoadSucceeded;
}

inline constexpr uint32_t hires_invalid_descriptor_index()
{
	return 0xffffffffu;
}

inline bool hires_descriptor_index_valid(uint32_t index)
{
	return index != hires_invalid_descriptor_index();
}
}
}
