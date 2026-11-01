#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "si/game_controller.h"
#include "si/pif.h"

static enum pak_type plugged_pak;
static int connected = CONT_JOYPAD;
static unsigned saves;

void DebugMessage(int level, const char *message, ...) { (void)level; (void)message; }

/* Transfer Pak storage is unrelated to these controller/mempak/rumble probes. */
void transferpak_read_command(struct transferpak *pak, uint16_t address, uint8_t *data, size_t size)
{ (void)pak; (void)address; (void)data; (void)size; assert(0); }
void transferpak_write_command(struct transferpak *pak, uint16_t address, const uint8_t *data, size_t size)
{ (void)pak; (void)address; (void)data; (void)size; assert(0); }

static int is_connected(void *userdata, enum pak_type *pak)
{
    (void)userdata;
    *pak = plugged_pak;
    return connected;
}

static void save(void *userdata) { (void)userdata; ++saves; }

int main(void)
{
    uint8_t storage[MEMPAK_SIZE] = {0};
    struct game_controller controller = {0};
    uint8_t cmd[38] = {3, 33, PIF_CMD_PAK_READ, 0, 0};
    unsigned i;

    init_game_controller(&controller, NULL, is_connected, NULL,
            NULL, save, storage, NULL, NULL);

    /* A connected controller with an empty slot returns zero data and ~CRC. */
    plugged_pak = PAK_NONE;
    memset(cmd + 5, 0xa5, 33);
    process_controller_command(&controller, cmd);
    for (i = 0; i < 32; ++i) assert(cmd[5 + i] == 0);
    assert(cmd[37] == 0xff);
    assert(!(cmd[1] & 0x80));

    cmd[0] = 35;
    cmd[1] = 1;
    cmd[2] = PIF_CMD_PAK_WRITE;
    process_controller_command(&controller, cmd);
    assert(cmd[37] == 0xff);
    for (i = 0; i < 32; ++i) cmd[5 + i] = (uint8_t)i;
    process_controller_command(&controller, cmd);
    /* CRC-8 polynomial 0x85 of bytes 00..1f is 0x33. */
    assert(cmd[37] == 0xcc);
    assert(saves == 0);

    plugged_pak = PAK_MEM;
    process_controller_command(&controller, cmd);
    assert(cmd[37] == 0x33);
    assert(saves == 1);
    assert(memcmp(storage, cmd + 5, 32) == 0);
    cmd[0] = 3;
    cmd[1] = 33;
    cmd[2] = PIF_CMD_PAK_READ;
    memset(cmd + 5, 0, 33);
    process_controller_command(&controller, cmd);
    for (i = 0; i < 32; ++i) assert(cmd[5 + i] == i);
    assert(cmd[37] == 0x33);

    plugged_pak = PAK_RUMBLE;
    cmd[3] = 0x80;
    process_controller_command(&controller, cmd);
    for (i = 0; i < 32; ++i) assert(cmd[5 + i] == 0x80);
    assert(cmd[37] == 0xb8);

    connected = CONT_NONE;
    cmd[37] = 0x5a;
    process_controller_command(&controller, cmd);
    assert(cmd[1] & 0x80);
    assert(cmd[37] == 0x5a);
    return 0;
}
