/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "crt/crt.h"
#include "adb/adb.h"

int main() {
  stdio_init_all();
  printf("Initialising...\n");

  PIO adb_pio = pio1;
  int adb_sm = 0;
  int adb_pin = 5;

  initAdbPio(adb_pio, adb_sm, adb_pin);
  char op;
  uint8_t address;
  uint8_t reg;
  uint8_t data_len;
  uint8_t data[9];
  while (true) {
    if (adbReceivedServiceRequest(adb_pio)) {
      printf("Service request received\n");
    }
    printf("Command: (r)eset (f)lush (t)alk (l)isten\n");
    scanf(" %1c", &op);
    printf("\n");
    switch (op) {
      case 'r':
        printf("Send reset\n");
        adbSendReset(adb_pio, adb_sm);
        break;
      case 'f':
        printf("Enter address to flush\n");
        scanf("%hhu", &address);
        printf("Flushing %u\n", address);
        adbFlush(adb_pio, adb_sm, address);
        break;
      case 't':
        printf("Enter address and register\n");
        scanf("%hhu", &address);
        scanf("%hhu", &reg);
        printf("Talk %u register %u\n", address, reg);
        memset(data, 0, 9);
        data_len = adbTalk(adb_pio, adb_sm, address, reg, &data[0]);
        printf("Received %u bytes:\n", data_len);
        for (int i = 0; i < data_len; i++) {
          printf("%02x ", data[i]);
        }
        printf("\n");
        break;
      case 'l': {
        printf("Enter address and register\n");
        scanf("%hhu", &address);
        scanf("%hhu", &reg);
        printf("Listen %u register %u\n", address, reg);
        printf("Enter data length (2 - 8)\n");
        scanf("%hhu", &data_len);
        for (int i = 0; i < data_len; i++) {
          printf("Enter byte %d\n", i);
          scanf("%2hhx", &data[i]);
        }
        adbListen(adb_pio, adb_sm, address, reg, data, data_len);
      }
      default:
        break;
    }
  }

  PIO video_pio = pio0;
  int vsync_pin = 2;
  int hsync_pin = 3;
  int video_pin = 4;

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
