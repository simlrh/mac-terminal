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
#include "fonts/font.h"

#include "fonts/font_data.h"

#include "terminal/screen.h"

#define LINE_BYTES 64

void yield() {}

void power_irq(uint gpio, uint32_t events) {
 printf("Power button pressed\n");
}

void drawFrame(uint8_t *buffer) {
  memset(buffer, 0, VIDEO_BUFFER_SIZE);
  for (int y = 0; y < 342; y++) {
    if (y > 36 && y < 305) {
      buffer[y * LINE_BYTES + 1] = 0b00000100;
      buffer[(y + 1) * LINE_BYTES - 2] = 0b00100000;
    }
    if (y == 36 || y == 305) {
      buffer[y * LINE_BYTES + 1] = 0b00000111;
      buffer[(y + 1) * LINE_BYTES - 2] = 0b11100000;
      for (int x = 2; x < LINE_BYTES - 2; x++) {
        buffer[x + y * LINE_BYTES] = 0xFF;
      }
    }
  }
}

void drawText(struct screen *screen, char *str, int col, int row) {
  int len = strlen(str);
  int line_chars = 512 / 6;
  memset(screen->buffer, 0, VIDEO_BUFFER_SIZE);
  for (int x = 0; x + col < line_chars && x < len; x++) {
    screen_draw_codepoint(screen, row, col + x, str[x], FONT_NORMAL, false, false, false, 0xf, 0x0);
  }
}

int main() {
  stdio_init_all();
  printf("Initialising...\n");

  // VIDEO

  PIO video_pio = pio0;
  int vsync_pin = 2;
  int hsync_pin = 3;
  int video_pin = 4;
  int brightness_pin = 5;

  gpio_init(brightness_pin);
  gpio_set_dir(brightness_pin, GPIO_OUT);
  gpio_put(brightness_pin, 1);

  video_buffers *buffers = createVideoBuffers();
  initVideoPIO(video_pio, video_pin, hsync_pin, vsync_pin);
  initVideoDMA(buffers);
  startVideo(buffers, video_pio);

  struct screen screen = {
    .format = {
      .cols = 80,
      .rows = 24
    },
    .char_width = 6,
    .char_height = 11,
    .normal_bitmap_font = &normal_font,
    .bold_bitmap_font = &bold_font,
    .buffer = *buffers->frontBuffer,
  };

  drawFrame(screen.buffer);
  screen_test_fonts(&screen, FONT_NORMAL);

  sleep_ms(3000);

  for (int s = 1; s < 24; s++) {
    sleep_ms(250);
    screen_scroll(&screen, SCROLL_UP, 1, 24, 1, 0, yield);
  }
  screen_test_fonts(&screen, FONT_NORMAL);

  // ADB

  PIO adb_pio = pio1;
  int adb_sm = 0;
  int adb_pin = 6;
  int power_pin = 7;

  gpio_init(power_pin);
  gpio_set_dir(power_pin, GPIO_IN);
  gpio_pull_up(power_pin);
  gpio_set_irq_enabled_with_callback(power_pin, 0b0100, true, power_irq);

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
