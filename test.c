/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "test.pio.h"
#include "crt.h"

int main() {
  stdio_init_all();
  printf("Initialising...\n");

  int vsync_pin = 2;
  int hsync_pin = 3;
  int video_pin = 4;

  PIO pio = pio0;
  initVideoPIO(pio, video_pin, hsync_pin, vsync_pin);
  video_buffers *buffers = createVideoBuffers();
  initVideoDMA(buffers);
  startVideo(buffers, pio);

  char patterns[3] = {
    0xFF,
    0xAA,
    0x00,
  };

  while(true) {
    for (int i = 0; i < 3; i++) {
      memset(buffers->backBuffer, patterns[i], VIDEO_BUFFER_SIZE);
      swapBuffers(buffers);
      printf("Current pattern: %8X\n", patterns[i]);
      sleep_ms(5000);
    }
  }
}
