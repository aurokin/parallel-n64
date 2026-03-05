#include "mupen64plus-video-paraLLEl/z64.h"

#include <cstdlib>
#include <iostream>

static_assert(SP_INTERRUPT == 0x1, "SP_INTERRUPT constant changed");
static_assert(SI_INTERRUPT == 0x2, "SI_INTERRUPT constant changed");
static_assert(AI_INTERRUPT == 0x4, "AI_INTERRUPT constant changed");
static_assert(VI_INTERRUPT == 0x8, "VI_INTERRUPT constant changed");
static_assert(PI_INTERRUPT == 0x10, "PI_INTERRUPT constant changed");
static_assert(DP_INTERRUPT == 0x20, "DP_INTERRUPT constant changed");

static_assert(DP_STATUS_XBUS_DMA == 0x01, "DP_STATUS_XBUS_DMA constant changed");
static_assert(DP_STATUS_FREEZE == 0x02, "DP_STATUS_FREEZE constant changed");
static_assert(DP_STATUS_FLUSH == 0x04, "DP_STATUS_FLUSH constant changed");
static_assert(DP_STATUS_START_GCLK == 0x008, "DP_STATUS_START_GCLK constant changed");
static_assert(DP_STATUS_TMEM_BUSY == 0x010, "DP_STATUS_TMEM_BUSY constant changed");
static_assert(DP_STATUS_PIPE_BUSY == 0x020, "DP_STATUS_PIPE_BUSY constant changed");
static_assert(DP_STATUS_CMD_BUSY == 0x040, "DP_STATUS_CMD_BUSY constant changed");
static_assert(DP_STATUS_CBUF_READY == 0x080, "DP_STATUS_CBUF_READY constant changed");
static_assert(DP_STATUS_DMA_BUSY == 0x100, "DP_STATUS_DMA_BUSY constant changed");
static_assert(DP_STATUS_END_VALID == 0x200, "DP_STATUS_END_VALID constant changed");
static_assert(DP_STATUS_START_VALID == 0x400, "DP_STATUS_START_VALID constant changed");

int main()
{
	std::cout << "emu_unit_z64_constants_test: PASS" << std::endl;
	return EXIT_SUCCESS;
}
