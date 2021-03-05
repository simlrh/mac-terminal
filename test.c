/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"

#include "test.pio.h"

static inline void crt_init(PIO pio, uint hsync_pin, uint vsync_pin, uint video_pin) {
  uint vsync_sm = 0;
  uint hsync_sm = 1;
  uint video_sm = 2;

  float clock_freq = clock_get_hz(clk_sys);
  float dot_freq = 15667200;
  float clkdiv = clock_freq / dot_freq;

  uint hsync_offset = pio_add_program(pio, &hsync_program);
  pio_sm_config hsync_config = hsync_program_get_default_config(hsync_offset);
  sm_config_set_sideset_pins(&hsync_config, hsync_pin);
  pio_gpio_init(pio, hsync_pin);
  sm_config_set_clkdiv(&hsync_config, clkdiv);
  pio_sm_set_consecutive_pindirs(pio, hsync_sm, hsync_pin, 1, true);

  uint vsync_offset = pio_add_program(pio, &vsync_program);
  pio_sm_config vsync_config = vsync_program_get_default_config(vsync_offset);
  sm_config_set_sideset_pins(&vsync_config, vsync_pin);
  pio_gpio_init(pio, vsync_pin);
  sm_config_set_clkdiv(&vsync_config, clkdiv);
  // Use ISR for arithmetic
  sm_config_set_in_shift(&vsync_config, false, false, 0);
  pio_sm_set_consecutive_pindirs(pio, vsync_sm, vsync_pin, 1, true);

  uint video_offset = pio_add_program(pio, &video_program);
  pio_sm_config video_config = video_program_get_default_config(video_offset);
  sm_config_set_set_pins(&video_config, video_pin, 1);
  sm_config_set_out_pins(&video_config, video_pin, 1);
  pio_gpio_init(pio, video_pin);
  sm_config_set_clkdiv(&video_config, clkdiv / 2);
  sm_config_set_out_shift(&video_config, false, true, 32);
  pio_sm_set_consecutive_pindirs(pio, video_sm, video_pin, 1, true);

  pio_sm_init(pio, hsync_sm, hsync_offset, &hsync_config);
  pio_sm_init(pio, vsync_sm, vsync_offset, &vsync_config);
  pio_sm_init(pio, video_sm, video_offset, &video_config);

  pio_enable_sm_mask_in_sync(pio, 0b111);
}

int main() {
  stdio_init_all();

  PIO pio = pio0;

  int pin_vsync = 2;
  int pin_hsync = 3;
  int pin_video = 4;

  crt_init(pio, pin_hsync, pin_vsync, pin_video);

  int x;
  int y;
  while(true) {
    pio_sm_put_blocking(pio, 2, (511 << 16) + 341);
    for (y = 0; y < 342; y++) {
      for (x = 0; x < 512; x+=32) {
        pio_sm_put_blocking(pio, 2, 0xAAAAAAAA);
      }
    }
  }
}
