#pragma once

#include <cstdint>

namespace RDP
{
namespace detail
{
enum class HiresFilterMode : uint8_t
{
	Nearest = 0,
	Linear = 1,
	Trilinear = 2,
};

enum class HiresSrgbMode : uint8_t
{
	Auto = 0,
	On = 1,
	Off = 2,
};

inline HiresFilterMode sanitize_hires_filter_mode(unsigned mode)
{
	switch (mode)
	{
	case 0:
		return HiresFilterMode::Nearest;
	case 2:
		return HiresFilterMode::Trilinear;
	default:
		return HiresFilterMode::Linear;
	}
}

inline HiresSrgbMode sanitize_hires_srgb_mode(unsigned mode)
{
	switch (mode)
	{
	case 1:
		return HiresSrgbMode::On;
	case 2:
		return HiresSrgbMode::Off;
	default:
		return HiresSrgbMode::Auto;
	}
}

inline bool hires_filter_uses_mipmaps(HiresFilterMode mode)
{
	return mode == HiresFilterMode::Trilinear;
}

inline bool resolve_hires_upload_srgb(HiresSrgbMode mode, bool replacement_srgb)
{
	switch (mode)
	{
	case HiresSrgbMode::On:
		return true;
	case HiresSrgbMode::Off:
		return false;
	case HiresSrgbMode::Auto:
	default:
		return replacement_srgb;
	}
}
}
}
