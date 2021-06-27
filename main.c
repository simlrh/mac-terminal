/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/watchdog.h"

#include "crt/crt.h"
#include "adb/adb.h"
#include "fonts/font.h"

#include "fonts/font_data.h"

#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "terminal/terminal_config_ui.h"
#include "terminal/keys.h"
#include "adb/keyboard.h"

#define SERIAL_RX_BUF_SIZE 8192
char SerialRxBuf[SERIAL_RX_BUF_SIZE];
volatile int SerialRxBufHead = 0;
volatile int SerialRxBufTail = 0;

#define SERIAL_TX_BUF_SIZE 64
char SerialTxBuf[SERIAL_TX_BUF_SIZE];
int SerialTxBufHead = 0;
int SerialTxBufTail = 0;

#define LOCAL_BUFFER_SIZE 64
static character_t local_buffer[LOCAL_BUFFER_SIZE];
static size_t local_head = 0;
static size_t local_tail = 0;

#define KEYBOARD_BUFFER_SIZE 256
uint8_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];
volatile int keyboard_buffer_head = 0;
volatile int keyboard_buffer_tail = 0;

struct keyboard global_keyboard;

#define FONT_WIDTH 6
#define FONT_HEIGHT 11

#define CHAR_HEIGHT    11
#define CHAR_WIDTH     6

#define UART_ID uart0

#define UART_TX_PIN 0
#define UART_RX_PIN 1

static struct screen screen_24_rows = {
    .format =
        {
            .rows = 24,
            .cols = 80,
        },
    .char_width = CHAR_WIDTH,
    .char_height = CHAR_HEIGHT,
    .buffer = NULL,
    .normal_bitmap_font = &normal_font,
    .bold_bitmap_font = &bold_font,
};

static struct screen screen_30_rows = {
    .format =
        {
            .rows = 30,
            .cols = 80,
        },
    .char_width = CHAR_WIDTH,
    .char_height = CHAR_HEIGHT,
    .buffer = NULL,
    .normal_bitmap_font = &normal_font,
    .bold_bitmap_font = &bold_font,
};

#define MAX_COLS 80
#define MAX_ROWS 30
#define TAB_STOPS_SIZE (MAX_COLS / 8)

static struct visual_cell visual_cells[MAX_ROWS * MAX_COLS];
uint8_t tab_stops[TAB_STOPS_SIZE];

struct screen *get_screen(struct format format) {
  if (format.cols == 80) {
    if (format.rows == 24)
      return &screen_24_rows;
    else if (format.rows == 30)
      return &screen_30_rows;
  }

  return NULL;
}

struct terminal *global_terminal = NULL;
struct terminal_config_ui *global_terminal_config_ui = NULL;

struct terminal_config terminal_config = {
    .format_rows = FORMAT_24_ROWS,
    .monochrome_transform = MONOCHROME_TRANSFORM_LUMINANCE,

    .baud_rate = BAUD_RATE_115200,
    .stop_bits = STOP_BITS_1,
    .parity = PARITY_NONE,
#ifdef TERMINAL_SERIAL_INVERTED
    .serial_inverted = false,
#endif

    .charset = CHARSET_UTF8,
    .keyboard_compatibility = KEYBOARD_COMPATIBILITY_PC,
    .keyboard_layout = KEYBOARD_LAYOUT_US,
    .receive_c1_mode = C1_MODE_8BIT,
    .transmit_c1_mode = C1_MODE_7BIT,

    .auto_wrap_mode = true,
    .screen_mode = false,

    .send_receive_mode = true,

    .new_line_mode = false,
    .cursor_key_mode = false,
    .auto_repeat_mode = true,
    .ansi_mode = true,
    .backspace_mode = false,
    .application_keypad_mode = false,

    .flow_control = true,

    .start_up = START_UP_MESSAGE,
};

void yield() {
  if (keyboard_buffer_tail != keyboard_buffer_head) {
    uint8_t scancode = keyboard_buffer[keyboard_buffer_tail];
    keyboard_buffer_tail++;

    if (keyboard_buffer_tail == KEYBOARD_BUFFER_SIZE) {
      keyboard_buffer_tail = 0;
    }

    keyboard_handle_code(&global_keyboard, scancode);
    if (global_terminal) {
      terminal_keyboard_handle_key(
          global_terminal, global_keyboard.lshift || global_keyboard.rshift,
          global_keyboard.lalt, global_keyboard.ralt,
          global_keyboard.lctrl || global_keyboard.rctrl, global_keyboard.keys[0]);
    }
  }
  
  while(uart_is_writable(UART_ID) && SerialTxBufTail != SerialTxBufHead) { // while Tx is free and there is data to send
    uart_putc_raw(UART_ID, SerialTxBuf[SerialTxBufTail++]);// send the byte
    SerialTxBufTail = SerialTxBufTail % SERIAL_TX_BUF_SIZE; // advance the tail of the queue
  }
}

static void uart_transmit(character_t *characters, size_t size, size_t head) {
  if (!global_terminal->send_receive_mode) {
    while (size--) {
      local_buffer[local_head] = *characters;
      local_head++;
      characters++;

      if (local_head == LOCAL_BUFFER_SIZE)
        local_head = 0;
    }
  }

  SerialTxBufHead = head;
}

static void screen_draw_codepoint_callback(struct format format, size_t row,
                                           size_t col, codepoint_t codepoint,
                                           enum font font, bool italic,
                                           bool underlined, bool crossedout,
                                           color_t active, color_t inactive) {
  screen_draw_codepoint(get_screen(format), row, col, codepoint, font,
                        italic, underlined, crossedout, active, inactive);
}

static void screen_clear_rows_callback(struct format format, size_t from_row,
                                       size_t to_row, color_t inactive) {
  screen_clear_rows(get_screen(format), from_row, to_row, inactive, yield);
}

static void screen_clear_cols_callback(struct format format, size_t row,
                                       size_t from_col, size_t to_col,
                                       color_t inactive) {
  screen_clear_cols(get_screen(format), row, from_col, to_col, inactive, yield);
}

static void screen_scroll_callback(struct format format, enum scroll scroll,
                                   size_t from_row, size_t to_row, size_t rows,
                                   color_t inactive) {
  screen_scroll(get_screen(format), scroll, from_row, to_row, rows, inactive,
                yield);
}

static void screen_shift_right_callback(struct format format, size_t row,
                                        size_t col, size_t cols,
                                        color_t inactive) {
  screen_shift_right(get_screen(format), row, col, cols, inactive, yield);
}

static void screen_shift_left_callback(struct format format, size_t row,
                                       size_t col, size_t cols,
                                       color_t inactive) {
  screen_shift_left(get_screen(format), row, col, cols, inactive, yield);
}

static void screen_test_callback(struct format format,
                                 enum screen_test screen_test) {
  struct screen *screen = get_screen(format);
  switch (screen_test) {
  case SCREEN_TEST_FONT1:
    screen_test_fonts(screen, FONT_NORMAL);
    break;
  case SCREEN_TEST_FONT2:
    screen_test_fonts(screen, FONT_BOLD);
    break;
  }
}

static void activate_config() {
  terminal_config_ui_activate(global_terminal_config_ui);
}

static void write_config(struct terminal_config *terminal_config_copy) {
  memcpy(&terminal_config, terminal_config_copy, sizeof(terminal_config));
}

static void keyboard_set_leds_callback(struct lock_state state) {
}

static void reset_callback() {
  watchdog_enable(1,1);
}

struct repeating_timer timer;

bool repeating_timer_callback(struct repeating_timer *t) {
  terminal_timer_tick(global_terminal);
  return true;
}

void initTimer() {
  add_repeating_timer_ms(-1, repeating_timer_callback, NULL, &timer);
}

void on_uart_rx() {
  while (uart_is_readable(UART_ID)) {
    SerialRxBuf[SerialRxBufHead++]  = uart_getc(UART_ID); // store the byte in the ring buffer
    if(SerialRxBufHead >= SERIAL_RX_BUF_SIZE) SerialRxBufHead = 0;
  }
}

void initSerial(void) {
  uint data_bits = 8;
  uint stop_bits = 1;
  uart_parity_t parity;

  switch (terminal_config.stop_bits) {
  case STOP_BITS_1:
    stop_bits = 1;
    break;
  case STOP_BITS_2:
    stop_bits = 2;
    break;
  }

  switch (terminal_config.parity) {
  case PARITY_NONE:
    parity = UART_PARITY_NONE;
    break;
  case PARITY_EVEN:
    parity = UART_PARITY_EVEN;
    break;
  case PARITY_ODD:
    parity = UART_PARITY_ODD;
    break;
  }

  int baud = terminal_config_get_baud_rate(&terminal_config);

  uart_init(UART_ID, baud);
  uart_set_hw_flow(UART_ID, false, false);
  uart_set_format(UART_ID, data_bits, stop_bits, parity);
  uart_set_fifo_enabled(UART_ID, false);

  gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

  int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

  irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
  irq_set_enabled(UART_IRQ, true);

  uart_set_irq_enables(UART_ID, true, false);
}

void adb_handler(uint8_t address, uint8_t reg, uint8_t *data, size_t len) {
  if (address == 2 && reg == 0) {
    for (size_t i = 0; i < len; i++) {
      keyboard_buffer[keyboard_buffer_head] = data[i];
      keyboard_buffer_head++;

      if (keyboard_buffer_head == KEYBOARD_BUFFER_SIZE) {
        keyboard_buffer_head = 0;
      }
    }
  }
}

int main() {

  keyboard_init(&global_keyboard);

  // ADB
  PIO adb_pio = pio1;
  int adb_pin = 6;

  initAdbPio(adb_pin, adb_handler);

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

#define LINE_BYTES 64

  memset(buffers->frontBuffer, 0, VIDEO_BUFFER_SIZE);
  for (int y = 0; y < 342; y++) {
    if (y > 36 && y < 305) {
      (*buffers->frontBuffer)[y * LINE_BYTES + 1] = 0b00000100;
      (*buffers->frontBuffer)[(y + 1) * LINE_BYTES - 2] = 0b00100000;
    }
    if (y == 36 || y == 305) {
      (*buffers->frontBuffer)[y * LINE_BYTES + 1] = 0b00000111;
      (*buffers->frontBuffer)[(y + 1) * LINE_BYTES - 2] = 0b11100000;
      for (int x = 2; x < LINE_BYTES - 2; x++) {
        (*buffers->frontBuffer)[x + y * LINE_BYTES] = 0xFF;
      }
    }
  }

  screen_24_rows.buffer = screen_30_rows.buffer = *buffers->frontBuffer;

  struct terminal terminal;
  struct terminal_callbacks callbacks = {
      .keyboard_set_leds = keyboard_set_leds_callback,
      .uart_transmit = uart_transmit,
      .screen_draw_codepoint = screen_draw_codepoint_callback,
      .screen_clear_rows = screen_clear_rows_callback,
      .screen_clear_cols = screen_clear_cols_callback,
      .screen_scroll = screen_scroll_callback,
      .screen_shift_left = screen_shift_left_callback,
      .screen_shift_right = screen_shift_right_callback,
      .screen_test = screen_test_callback,
      .reset = reset_callback,
      .yield = yield,
      .activate_config = activate_config,
      .write_config = write_config};
  terminal_init(&terminal, &callbacks, visual_cells, tab_stops, TAB_STOPS_SIZE,
                &terminal_config, SerialTxBuf, SERIAL_TX_BUF_SIZE);
  global_terminal = &terminal;

  initTimer();
  initSerial();

  struct terminal_config_ui terminal_config_ui;
  global_terminal_config_ui = &terminal_config_ui;
  terminal_config_ui_init(&terminal_config_ui, &terminal, &terminal_config);

  while (true) {
    yield();

    terminal_screen_update(&terminal);
    terminal_keyboard_repeat_key(&terminal);

    if (terminal_config_ui.activated)
      continue;

    if (local_tail != local_head) {
      size_t size = 0;
      if (local_tail < local_head)
        size = local_head - local_tail;
      else
        size = local_head + (LOCAL_BUFFER_SIZE - local_tail);

      while (size--) {
        character_t character = local_buffer[local_tail];
        terminal_uart_receive_character(&terminal, character);
        local_tail++;

        if (local_tail == LOCAL_BUFFER_SIZE)
          local_tail = 0;
      }
    }

    if (SerialRxBufHead != SerialRxBufTail) {
      int size = 0;
      if (SerialRxBufTail < SerialRxBufHead)
        size = SerialRxBufHead - SerialRxBufTail;
      else
        size = SerialRxBufHead + (SERIAL_RX_BUF_SIZE - SerialRxBufTail);

      terminal_uart_flow_control(&terminal, size);

      while (size--) {
        yield();

        if (terminal_config_ui.activated)
          break;

        terminal_uart_flow_control(&terminal, size);

        character_t character = SerialRxBuf[SerialRxBufTail];
        SerialRxBufTail++;

        terminal_uart_receive_character(&terminal, character);
        if (SerialRxBufTail == SERIAL_RX_BUF_SIZE)
          SerialRxBufTail = 0;
      }
    } else {
      terminal_uart_flow_control(&terminal, 0);
    }
  }
}
