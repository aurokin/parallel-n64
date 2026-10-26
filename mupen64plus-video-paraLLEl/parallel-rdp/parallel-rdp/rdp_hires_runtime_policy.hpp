#pragma once

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

inline std::vector<std::string> resolve_hires_cache_path(const std::string &system_dir,
                                                         std::string rom_name, const char *env_path)
{
	if (env_path && *env_path)
		return { env_path };

	// Only the header field's trailing spaces and NUL padding are removed.
	rom_name.erase(rom_name.find_last_not_of(std::string(" \0", 2)) + 1);
	if (system_dir.empty() || rom_name.empty())
		return {};

	const auto game_path = system_dir + "/parallel-n64/hires/" + rom_name;
	return { game_path + ".phrb", game_path };
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
