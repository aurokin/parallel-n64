/* A hot state load drops the live, unserialized RSP task lock. */
#include "main/device.h"
#include "main/rom.h"
#include "main/savestates.h"
#include "plugin/plugin.h"
#include "r4300/cp1.h"
#include "r4300/tlb.h"
#undef NDEBUG
#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct device g_dev;
m64p_rom_settings ROM_SETTINGS;
gfx_plugin_functions gfx;
uint32_t tlb_LUT_r[0x100000], tlb_LUT_w[0x100000];
tlb tlb_e[32];
static uint32_t ram[0x800000 / 4];
static uint32_t cp0[32], fcr0, fcr31, pc, last_pc;
static unsigned int link_bit, next_event;
static int64_t regs[32], fprs[32], hi, lo;
static unsigned int load_callbacks;
uint32_t* r4300_cp0_regs(void) { return cp0; }
int64_t* r4300_regs(void) { return regs; }
int64_t* r4300_mult_hi(void) { return &hi; }
int64_t* r4300_mult_lo(void) { return &lo; }
unsigned int* r4300_llbit(void) { return &link_bit; }
uint32_t* r4300_pc(void) { return &pc; }
uint32_t* r4300_last_addr(void) { return &last_pc; }
unsigned int* r4300_next_interrupt(void) { return &next_event; }
int64_t* r4300_cp1_regs(void) { return fprs; }
uint32_t* r4300_cp1_fcr0(void) { return &fcr0; }
uint32_t* r4300_cp1_fcr31(void) { return &fcr31; }
void set_fpr_pointers(uint32_t status) { (void)status; }
void shuffle_fpr_data(uint32_t old_status, uint32_t new_status)
{ (void)old_status; (void)new_status; }
void update_x86_rounding_mode(uint32_t value) { (void)value; }
void savestates_load_set_pc(uint32_t value) { pc = value; }
int save_eventqueue_infos(char* data) { memset(data, 0xff, 4); return 4; }
void load_eventqueue_infos(char* data) { assert((unsigned char)data[0] == 0xff); }
void StateChanged(m64p_core_param param, int value)
{
    if (param == M64CORE_STATE_LOADCOMPLETE)
    {
        assert(value == 1 && g_dev.sp.rsp_task_locked == 0);
        ++load_callbacks;
    }
}
static void vi_changed(void) {}

int main(void)
{
    g_dev.ri.rdram.dram = ram;
    g_dev.ri.rdram.dram_size = sizeof(ram);
    gfx.viStatusChanged = vi_changed;
    gfx.viWidthChanged = vi_changed;
    cp0[CP0_STATUS_REG] = CP0_STATUS_FR;
    pc = 0x80000400;
    size_t size = savestates_get_m64p_size();
    unsigned char* data = malloc(size);
    assert(data != NULL && size != 0);
    assert(savestates_save_m64p(data, size));

    for (int locked = 0; locked <= 1; ++locked)
    {
        g_dev.sp.rsp_task_locked = locked;
        assert(savestates_load_m64p(data, size));
        assert(g_dev.sp.rsp_task_locked == 0);
        assert(pc == 0x80000400 && last_pc == pc);
        assert(size == savestates_get_m64p_size());
    }
    assert(load_callbacks == 2);

    g_dev.sp.rsp_task_locked = 1;
    assert(!savestates_load_m64p(data, 0));
    assert(g_dev.sp.rsp_task_locked == 1 && load_callbacks == 2);
    data[0] ^= 1;
    assert(!savestates_load_m64p(data, size));
    assert(g_dev.sp.rsp_task_locked == 1 && load_callbacks == 2);
    free(data);
    return 0;
}
