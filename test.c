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

void print_adb(uint8_t address, uint8_t reg, uint8_t *data, size_t len) {
  printf("Data received from device %d register %d\n", address, reg);
  for (int i = 0; i < len; i++) {
    printf("%02x ", data[i]);
  }
  printf("\n");
}

int main() {
  stdio_init_all();
  printf("Initialising...\n");

  // ADB

  PIO adb_pio = pio1;
  int adb_pin = 6;
  int power_pin = 7;

  gpio_init(power_pin);
  gpio_set_dir(power_pin, GPIO_IN);
  gpio_pull_up(power_pin);
  gpio_set_irq_enabled_with_callback(power_pin, 0b0100, true, power_irq);

  initAdbPio(adb_pin, print_adb);
  char op;
  uint8_t address;
  uint8_t reg;
  uint8_t data_len;
  uint8_t data[9];

  while (true) {
    if (adbReceivedServiceRequest()) {
      printf("Service request received\n");
    }
    printf("Command: (r)eset (f)lush (t)alk (l)isten\n");
    scanf(" %1c", &op);
    printf("\n");
    switch (op) {
      case 'r':
        printf("Send reset\n");
        adbSendReset();
        break;
      case 'f':
        printf("Enter address to flush\n");
        scanf("%hhu", &address);
        printf("Flushing %u\n", address);
        adbFlush(address);
        break;
      case 't':
        printf("Enter address and register\n");
        scanf("%hhu", &address);
        scanf("%hhu", &reg);
        printf("Talk %u register %u\n", address, reg);
        memset(data, 0, 9);
        adbTalk(address, reg);
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
        adbListen(address, reg, data, data_len);
      }
      default:
        break;
    }
  }
}
