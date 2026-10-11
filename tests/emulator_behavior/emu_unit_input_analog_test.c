#include "plugin/emulate_game_controller_via_input_plugin.h"
#include "api/m64p_plugin.h"
#include <libretro.h>
#include <stdio.h>
#include <stdlib.h>

static bool environment(unsigned command, void *data)
{
   (void)command;
   (void)data;
   return false;
}

retro_environment_t environ_cb = environment;
struct retro_rumble_interface rumble;
int pad_pak_types[4];
int pad_present[4];
int astick_deadzone;
int astick_sensitivity = 100;
int astick_snap_active;
int astick_snap_max_angle;
int astick_snap_min_displacement_percent;
m64p_rom_header ROM_HEADER;
bool alternate_mapping;
CONTROL Controls[4];
void CALL inputInitiateControllers(CONTROL_INFO info);

static int16_t stick_x, stick_y;

static int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
   (void)port;
   if (device != RETRO_DEVICE_ANALOG || index != RETRO_DEVICE_INDEX_ANALOG_LEFT)
      return 0;
   return id == RETRO_DEVICE_ID_ANALOG_X ? stick_x : stick_y;
}

retro_input_state_t input_cb = input_state;

static void check_axes(int16_t x, int16_t y, int expected_x, int expected_y)
{
   int channel = 0;
   BUTTONS keys;
   stick_x = x;
   stick_y = y;
   keys.Value = egcvip_get_input(&channel);
   if (keys.X_AXIS != expected_x || keys.Y_AXIS != expected_y)
   {
      fprintf(stderr, "stick (%d,%d): expected (%d,%d), got (%d,%d)\n",
            x, y, expected_x, expected_y, keys.X_AXIS, keys.Y_AXIS);
      exit(1);
   }
}

int main(void)
{
   CONTROL_INFO info = {0};
   pad_present[0] = 1;
   info.Controls = Controls;
   inputInitiateControllers(info);

   /* Exercise the production callback-to-N64 conversion, including the one
    * corner whose squared int16 axes overflow a signed 32-bit sum. */
   check_axes(INT16_MIN, INT16_MIN, -80, 80);
   check_axes(INT16_MIN, INT16_MAX, -80, -80);
   check_axes(INT16_MAX, INT16_MIN, 80, 80);
   check_axes(INT16_MAX, INT16_MAX, 80, -80);
   check_axes(INT16_MIN, 0, -80, 0);
   check_axes(0, INT16_MIN, 0, 80);
   check_axes(INT16_MAX, 0, 80, 0);
   check_axes(0, INT16_MAX, 0, -80);
   check_axes(0, 0, 0, 0);
   check_axes(-23000, -23000, -56, 56);

   astick_deadzone = 4096;
   check_axes(-1024, -1024, 0, 0);
   check_axes(4096, 0, 0, 0);
   check_axes(INT16_MIN, 0, -80, 0);
   check_axes(INT16_MIN, INT16_MIN, -83, 83);
   puts("production analog conversion passed");
   return 0;
}
