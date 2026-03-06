#pragma once

namespace RDP
{
namespace detail
{
enum class HiresBindlessViewMode
{
	DefaultView,
	UnormView,
	SrgbView,
};

inline HiresBindlessViewMode select_hires_bindless_view_mode(bool use_srgb,
                                                              bool has_unorm_view,
                                                              bool has_srgb_view)
{
	if (use_srgb && has_srgb_view)
		return HiresBindlessViewMode::SrgbView;
	if (!use_srgb && has_unorm_view)
		return HiresBindlessViewMode::UnormView;
	return HiresBindlessViewMode::DefaultView;
}
}
}
