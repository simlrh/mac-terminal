#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

#include "adb.pio.h"
#include "adb.h"

#define ADB_SEND_RESET() 0u
#define ADB_FLUSH(ADDRESS) (((ADDRESS << 4) + 1) << 24)
#define ADB_TALK(ADDRESS, REGISTER) ((((ADDRESS) << 4) + (0b11 << 2) + (REGISTER)) << 24)
#define ADB_LISTEN(ADDRESS, REGISTER) ((((ADDRESS) << 4) + (0b10 << 2) + (REGISTER)) << 24)

void initAdbPio(PIO pio, uint adb_sm, uint adb_pin) {
  float clock_freq = clock_get_hz(clk_sys);
  float adb_freq = 30000;

  pio_gpio_init(pio, adb_pin);
  pio_gpio_init(pio, adb_pin + 1);
  gpio_pull_up(adb_pin);

  uint adb_offset = 0;
  pio_add_program_at_offset(pio, &adb_program, adb_offset);
  pio_sm_config adb_config = adb_program_get_default_config(adb_offset);
  sm_config_set_sideset_pins(&adb_config, adb_pin);
  sm_config_set_set_pins(&adb_config, adb_pin, 1);
  sm_config_set_out_pins(&adb_config, adb_pin, 1);
  sm_config_set_in_pins(&adb_config, adb_pin);
  sm_config_set_jmp_pin(&adb_config, adb_pin);
  sm_config_set_jmp_pin(&adb_config, adb_pin);
  sm_config_set_out_shift(&adb_config, false, true, 32);
  sm_config_set_in_shift(&adb_config, false, true, 32);
  pio_sm_set_consecutive_pindirs(pio, adb_sm, adb_pin, 1, true);
  sm_config_set_clkdiv(&adb_config, clock_freq / adb_freq);

  pio_sm_init(pio, adb_sm, adb_offset, &adb_config);

  pio_sm_clear_fifos(pio, adb_sm);
  pio_sm_set_enabled(pio, adb_sm, true);
}

bool adbReceivedServiceRequest(PIO pio) {
  bool received = pio->irq & 1;
  pio->irq = 1;
  return received;
}

void adbSendReset(PIO pio, uint sm) {
  pio->txf[sm] = adb_offset_send_command;
  pio->txf[sm] = ADB_SEND_RESET();
  pio->txf[sm] = adb_offset_send_reset;
}

void adbFlush(PIO pio, uint sm, uint8_t address) {
  pio->txf[sm] = adb_offset_send_command;
  pio->txf[sm] = ADB_FLUSH(address);
}

uint8_t adbTalk(PIO pio, uint sm, uint8_t address, uint8_t reg, uint8_t *data) {
  pio->txf[sm] = adb_offset_send_command;
  pio->txf[sm] = ADB_TALK(address, reg);
  pio->txf[sm] = adb_offset_read_data;
  sleep_ms(7);
  printf("adb read pc: %d\n", pio_sm_get_pc(pio, sm));

  // Push remaining bits in ISR to FIFO
  if (pio_sm_is_rx_fifo_full(pio, sm)) {
    pio_sm_exec(pio, sm, pio_encode_push(false, true));
  } else {
    pio_sm_exec_wait_blocking(pio, sm, pio_encode_push(false, true));
  }

  // Copy fifo to buffer
  int ret = 0;
  memset(data, 0, 9);

  while (!pio_sm_is_rx_fifo_empty(pio, sm)) {
    uint32_t word = pio_sm_get(pio, sm);
    uint8_t *bytes = ((uint8_t *) &word);
    data[ret + 0] = bytes[3];
    data[ret + 1] = bytes[2];
    data[ret + 2] = bytes[1];
    data[ret + 3] = bytes[0];
    ret += 4;
  }

  pio_sm_exec(pio, sm, pio_encode_jmp(0));

  if (ret == 0) {
    return 0;
  }

  // Find start byte
  int start;
  for (start = 0; start < 9; start++) {
    if (data[start]) {
      break;
    }
  }

  if (start == 9) {
    return 0;
  }

  int i;
  // Copy data to front of buffer
  if (start > 0) {
    for (i = 0; i < 9; i++) {
      data[i] = (i + start) >= 9 ? 0 : data[i + start];
    }
  }

  // Shift out start bit
  for (i = 0; i < 8; i++) {
    data[i] = (data[i] << 7) | (data[i + 1] >> 1);
  }

  // Return length in bytes
  return ret - start - 1;
}

void adbListen(PIO pio, uint sm, uint8_t address, uint8_t reg, uint8_t *data, uint8_t data_len) {
  uint8_t buffer[12] = {0};
  int i;

  // Copy bytes into buffer in little endian order, starting with length in bits - 1
  buffer[3] = (data_len * 8) - 1;
  for (i = 1; i <= data_len; i++) {
    buffer[((i / 4) * 4) + (3 - (i % 4))] = data[i - 1];
  }

  pio->txf[sm] = adb_offset_send_command;
  pio->txf[sm] = ADB_LISTEN(address, reg);
  pio->txf[sm] = adb_offset_write_data;

  // Write to FIFO as 32 bit words
  pio->txf[sm] = *((uint32_t *) buffer);
  if (data_len > 3) {
    pio->txf[sm] = *((uint32_t *) &buffer[4]);
  }
  if (data_len > 7) {
    pio->txf[sm] = *((uint32_t *) &buffer[8]);
  }
}
