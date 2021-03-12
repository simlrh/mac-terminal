/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "crt/crt.h"

int main() {
  stdio_init_all();
  printf("Initialising...\n");

  int vsync_pin = 2;
  int hsync_pin = 3;
  int video_pin = 4;

  PIO video_pio = pio0;

  video_buffers *buffers = createVideoBuffers();
  initVideoPIO(video_pio, video_pin, hsync_pin, vsync_pin);
  initVideoDMA(buffers);
  startVideo(buffers, video_pio);

  char patterns[3] = {
    0xF0,
    0xAA,
    0x00,
  };

  while(true) {
    {
      int line_size = 512 / 8;
      memset(buffers->backBuffer, 0, VIDEO_BUFFER_SIZE);
      for (int y = 0; y < 342; y++) {
        (*buffers->backBuffer)[y * line_size] = 0b00000011;
        (*buffers->backBuffer)[(y + 1) * line_size - 1] = 0b11000000;
        if (y <2 || y > 339) {
          for (int x = 0; x < line_size; x++) {
            (*buffers->backBuffer)[x + y * line_size] = 0xFF;
          }
        }
      }
      swapBuffers(buffers);
      sleep_ms(5000);
    }
    for (int i = 0; i < 3; i++) {
      memset(buffers->backBuffer, patterns[i], VIDEO_BUFFER_SIZE);
      swapBuffers(buffers);
      printf("Current pattern: %8X\n", patterns[i]);
      sleep_ms(5000);
    }
  }
}
