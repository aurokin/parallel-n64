#include "vi/vi_controller.h"

#include <stdio.h>
#include <stdlib.h>

static void expect_rate(const char* what, const struct vi_controller* vi,
      double expected)
{
   double actual = vi_actual_refresh_rate(vi);
   double error = actual - expected;

   if (error < -0.000001 || error > 0.000001)
   {
      fprintf(stderr, "%s: expected %.9f Hz, got %.9f Hz\n", what, expected, actual);
      exit(EXIT_FAILURE);
   }
}

int main(void)
{
   struct vi_controller vi = {0};

   expect_rate("uninitialized controller", NULL, 0.0);
   expect_rate("zero frame period", &vi, 0.0);
   vi.clock = 48681812;
   vi.delay = 5000;
   expect_rate("power-on interval is not a frame", &vi, 0.0);
   vi.delay = 500000;
   expect_rate("unprogrammed V_SYNC interval", &vi, 0.0);

   vi.regs[VI_V_SYNC_REG] = 0x20d;
   vi.delay = 0;
   expect_rate("programmed V_SYNC without a period", &vi, 0.0);
   vi.delay = 789000;
   expect_rate("fork NTSC period", &vi, 61.700648923);
   vi.delay = 811125;
   expect_rate("upstream NTSC period", &vi, 60.017644629);
   vi.clock = 49656530;
   vi.regs[VI_V_SYNC_REG] = 0x271;
   vi.delay = 993125;
   expect_rate("PAL period", &vi, 50.000281938);
   vi.clock = 48628316;
   vi.regs[VI_V_SYNC_REG] = 0x20d;
   vi.delay = 810075;
   expect_rate("MPAL period", &vi, 60.029399747);

   puts("VI refresh rate: ok");
   return EXIT_SUCCESS;
}
