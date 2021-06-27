#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"

#include "adb.pio.h"
#include "adb.h"

#define ADB_SEND_RESET() 0u
#define ADB_FLUSH(ADDRESS) (((ADDRESS << 4) + 1) << 24)
#define ADB_TALK(ADDRESS, REGISTER) ((((ADDRESS) << 4) + (0b11 << 2) + (REGISTER)) << 24)
#define ADB_LISTEN(ADDRESS, REGISTER) (((ADDRESS) << 4) + (0b10 << 2) + (REGISTER))

#define ADB_PIO pio1
#define ADB_PIO_IRQ PIO1_IRQ_0
#define ADB_SM 0
#define TIMEOUT_SM 1

volatile uint8_t current_device_address;
volatile uint8_t current_device_register;
volatile uint8_t next_device_address = 2;
volatile uint8_t next_device_register = 0;

adb_handler_t global_adb_handler = NULL;

void adbDataReceived(void);

struct repeating_timer adb_timer;

bool adbPollCallback(struct repeating_timer *t) {
  current_device_address = next_device_address;
  current_device_register = next_device_register;
  pio_sm_exec(ADB_PIO, ADB_SM, pio_encode_jmp(0));
  ADB_PIO->txf[ADB_SM] = ADB_TALK(current_device_address, current_device_register);
  next_device_register = 0;
  return true;
}

void initAdbPio(uint adb_pin, adb_handler_t handler) {
  global_adb_handler = handler;

  float clock_freq = clock_get_hz(clk_sys);
  float adb_freq = 60000;

  pio_gpio_init(ADB_PIO, adb_pin);
  gpio_pull_up(adb_pin);

  uint adb_offset = 0;
  pio_add_program_at_offset(ADB_PIO, &adb_program, adb_offset);
  pio_sm_config adb_program_config = adb_program_get_default_config(adb_offset);
  sm_config_set_sideset_pins(&adb_program_config, adb_pin);
  sm_config_set_set_pins(&adb_program_config, adb_pin, 1);
  sm_config_set_out_pins(&adb_program_config, adb_pin, 1);
  sm_config_set_in_pins(&adb_program_config, adb_pin);
  sm_config_set_jmp_pin(&adb_program_config, adb_pin);
  sm_config_set_out_shift(&adb_program_config, false, true, 32);
  sm_config_set_in_shift(&adb_program_config, false, true, 32);
  pio_sm_set_consecutive_pindirs(ADB_PIO, ADB_SM, adb_pin, 1, true);
  sm_config_set_clkdiv(&adb_program_config, clock_freq / adb_freq);

  pio_sm_init(ADB_PIO, ADB_SM, adb_offset, &adb_program_config);

  uint timeout_offset = pio_add_program(ADB_PIO, &data_timeout_program);
  pio_sm_config timeout_config = data_timeout_program_get_default_config(timeout_offset);
  sm_config_set_jmp_pin(&timeout_config, adb_pin);
  sm_config_set_clkdiv(&timeout_config, clock_freq / adb_freq);

  pio_sm_init(ADB_PIO, TIMEOUT_SM, timeout_offset, &timeout_config);

  ADB_PIO->inte0 = PIO_IRQ0_INTE_SM0_BITS | PIO_IRQ0_INTE_SM1_BITS;

  irq_set_exclusive_handler(ADB_PIO_IRQ, adbDataReceived);
  irq_set_enabled(ADB_PIO_IRQ, true);

  pio_sm_set_enabled(ADB_PIO, TIMEOUT_SM, true);
  pio_sm_set_enabled(ADB_PIO, ADB_SM, true);

  add_repeating_timer_ms(-11, adbPollCallback, NULL, &adb_timer);
}

bool adbReceivedServiceRequest() {
  bool received = ADB_PIO->irq & 1;
  ADB_PIO->irq = 1;
  return received;
}

void adbSendReset() {
  pio_sm_set_enabled(ADB_PIO, ADB_SM, false);
  pio_sm_restart(ADB_PIO, ADB_SM);
  pio_sm_exec(ADB_PIO, ADB_SM, pio_encode_jmp(0));
  pio_sm_exec(ADB_PIO, ADB_SM, pio_encode_set(pio_pindirs, 1));
  pio_sm_exec(ADB_PIO, ADB_SM, pio_encode_set(pio_pins, 0));
  sleep_ms(3);
  pio_sm_exec(ADB_PIO, ADB_SM, pio_encode_set(pio_pins, 1));
  pio_sm_clear_fifos(ADB_PIO, ADB_SM);
  pio_sm_set_enabled(ADB_PIO, ADB_SM, true);
}

void adbFlush(uint8_t address) {
  pio_sm_set_enabled(ADB_PIO, ADB_SM, false);
  pio_sm_restart(ADB_PIO, ADB_SM);
  pio_sm_clear_fifos(ADB_PIO, ADB_SM);
  pio_sm_exec(ADB_PIO, ADB_SM, pio_encode_jmp(0));

  ADB_PIO->txf[ADB_SM] = ADB_FLUSH(address);

  pio_sm_set_enabled(ADB_PIO, ADB_SM, true);
}

void adbTalk(uint8_t address, uint8_t reg) {
  next_device_address = address;
  next_device_register = reg;
}

void adbListen(uint8_t address, uint8_t reg, uint8_t *data, uint8_t data_len) {
  uint8_t buffer[12] = {0};
  int i;

  buffer[3] = ADB_LISTEN(address, reg);
  buffer[2] = (data_len * 8) - 1;
  // Copy bytes into buffer in little endian order
  for (i = 0; i < data_len; i++) {
    buffer[((i / 4) * 4) + (3 - ((i + 2) % 4))] = data[i];
  }

  // Write to FIFO as 32 bit words
  ADB_PIO->txf[ADB_SM] = *((uint32_t *) buffer);
  if (data_len > 3) {
    ADB_PIO->txf[ADB_SM] = *((uint32_t *) &buffer[4]);
  }
  if (data_len > 7) {
    ADB_PIO->txf[ADB_SM] = *((uint32_t *) &buffer[8]);
  }
}

uint8_t data[12];

void adbDataReceived() {
  if (ADB_PIO->irq & 1) {
    ADB_PIO->irq = 1;
    next_device_address = next_device_address == 15 ? 2 : next_device_address + 1;
    next_device_register = 0;
  }
  if (ADB_PIO->irq & 2) {
    ADB_PIO->irq = 2;
    pio_sm_exec(ADB_PIO, ADB_SM, pio_encode_jmp(0));

    // Push remaining bits in ISR to FIFO
    pio_sm_exec(ADB_PIO, ADB_SM, pio_encode_push(false, true));

    // Copy fifo to buffer
    int ret = 0;
    memset(data, 0, 9);

    while (!pio_sm_is_rx_fifo_empty(ADB_PIO, ADB_SM)) {
      uint32_t word = pio_sm_get(ADB_PIO, ADB_SM);
      uint8_t *bytes = ((uint8_t *) &word);
      data[ret + 0] = bytes[3];
      data[ret + 1] = bytes[2];
      data[ret + 2] = bytes[1];
      data[ret + 3] = bytes[0];
      ret += 4;
    }

    pio_sm_exec(ADB_PIO, ADB_SM, pio_encode_jmp(0));

    if (ret == 0) {
      return;
    }

    // Find start byte
    int start;
    for (start = 0; start < 9; start++) {
      if (data[start]) {
        break;
      }
    }

    if (start == 9) {
      return;
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
    size_t len = ret - start - 1;

    if (global_adb_handler) {
      global_adb_handler(current_device_address, current_device_register, data, len);
    }
  }
}
