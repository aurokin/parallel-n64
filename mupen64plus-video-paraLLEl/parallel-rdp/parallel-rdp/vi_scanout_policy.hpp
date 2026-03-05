#pragma once

#include "rdp_common.hpp"

namespace RDP
{
namespace detail
{
struct VIScanoutRegisters
{
	int x_start = 0;
	int y_start = 0;
	int h_start = 0;
	int v_start = 0;
	int h_end = 0;
	int v_end = 0;
	int h_res = 0;
	int v_res = 0;
	int x_add = 0;
	int y_add = 0;
	int v_sync = 0;
	int vi_width = 0;
	uint32_t vi_offset = 0;
	int max_x = 0;
	int max_y = 0;
	int v_current_line = 0;
	bool left_clamp = false;
	bool right_clamp = false;
	bool is_pal = false;
	uint32_t status = 0;
};

inline VIScanoutRegisters decode_vi_registers(const uint32_t *vi_registers)
{
	VIScanoutRegisters reg = {};

	reg.x_start = (vi_registers[unsigned(VIRegister::XScale)] >> 16) & 0xfff;
	reg.y_start = (vi_registers[unsigned(VIRegister::YScale)] >> 16) & 0xfff;
	reg.h_start = (vi_registers[unsigned(VIRegister::HStart)] >> 16) & 0x3ff;
	reg.v_start = (vi_registers[unsigned(VIRegister::VStart)] >> 16) & 0x3ff;
	reg.h_end = vi_registers[unsigned(VIRegister::HStart)] & 0x3ff;
	reg.v_end = vi_registers[unsigned(VIRegister::VStart)] & 0x3ff;
	reg.h_res = reg.h_end - reg.h_start;
	reg.v_res = (reg.v_end - reg.v_start) >> 1;
	reg.x_add = vi_registers[unsigned(VIRegister::XScale)] & 0xfff;
	reg.y_add = vi_registers[unsigned(VIRegister::YScale)] & 0xfff;
	reg.v_sync = vi_registers[unsigned(VIRegister::VSync)] & 0x3ff;
	reg.status = vi_registers[unsigned(VIRegister::Control)];
	reg.vi_width = vi_registers[unsigned(VIRegister::Width)] & 0xfff;
	reg.vi_offset = vi_registers[unsigned(VIRegister::Origin)] & 0xffffff;
	reg.v_current_line = vi_registers[unsigned(VIRegister::VCurrentLine)] & 1;

	reg.is_pal = unsigned(reg.v_sync) > (VI_V_SYNC_NTSC + 25);
	reg.h_start -= reg.is_pal ? VI_H_OFFSET_PAL : VI_H_OFFSET_NTSC;

	int v_start_offset = reg.is_pal ? VI_V_OFFSET_PAL : VI_V_OFFSET_NTSC;
	reg.v_start = (reg.v_start - v_start_offset) / 2;

	if (reg.h_start < 0)
	{
		reg.x_start -= reg.x_add * reg.h_start;
		reg.h_res += reg.h_start;
		reg.h_start = 0;
		reg.left_clamp = true;
	}

	if (reg.h_start + reg.h_res > VI_SCANOUT_WIDTH)
	{
		reg.h_res = VI_SCANOUT_WIDTH - reg.h_start;
		reg.right_clamp = true;
	}

	if (reg.v_start < 0)
	{
		reg.y_start -= reg.y_add * reg.v_start;
		reg.v_start = 0;
	}

	reg.max_x = (reg.x_start + reg.h_res * reg.x_add) >> 10;
	reg.max_y = (reg.y_start + reg.v_res * reg.y_add) >> 10;

	return reg;
}

inline void compute_scanout_memory_range(const VIScanoutRegisters &decoded, unsigned &offset, unsigned &length)
{
	bool divot = (decoded.status & VI_CONTROL_DIVOT_ENABLE_BIT) != 0;

	// Need to sample a 2-pixel border to have room for AA filter and divot.
	int aa_width = decoded.max_x + 2 + 4 + int(divot) * 2;
	// 1 pixel border on top and bottom.
	int aa_height = decoded.max_y + 1 + 4;

	int x_off = divot ? -3 : -2;
	int y_off = -2;

	if (decoded.vi_offset == 0 || decoded.h_res <= 0 || decoded.h_start >= VI_SCANOUT_WIDTH)
	{
		offset = 0;
		length = 0;
		return;
	}

	int pixel_size = ((decoded.status & VI_CONTROL_TYPE_MASK) | VI_CONTROL_TYPE_RGBA5551_BIT) == VI_CONTROL_TYPE_RGBA8888_BIT ? 4 : 2;
	uint32_t vi_offset = decoded.vi_offset;
	vi_offset &= ~(pixel_size - 1);
	vi_offset += (x_off + y_off * decoded.vi_width) * pixel_size;

	offset = vi_offset;
	length = (aa_height * decoded.vi_width + aa_width) * pixel_size;
}

inline bool need_fetch_bug_emulation(const VIScanoutRegisters &decoded, unsigned scaling_factor)
{
	// If we risk sampling same Y coordinate for two scanlines we can trigger this case,
	// so add workaround paths for it.
	return decoded.y_add < 1024 && scaling_factor == 1;
}
}
}
