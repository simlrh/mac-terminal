#include "screen.h"
#include <stdio.h>

#include <string.h>

#define REPLACEMENT_CODEPOINT 32 

#define ROWS screen->format.rows
#define COLS screen->format.cols

#define CHAR_WIDTH_PIXELS 6
#define CHAR_HEIGHT_LINES 11

#define SCREEN_WIDTH_PIXELS 512
#define SCREEN_HEIGHT_PIXELS 342
#define SCREEN_WIDTH_BYTES 64

#define X_MARGIN 16
#define Y_MARGIN 39

void setSixBitsAt(uint8_t *buffer, uint8_t sixBits, int row, int col, int line) {
  if (row < 0 || col < 0) {
    return;
  }
  int x = X_MARGIN + col * CHAR_WIDTH_PIXELS;
  int y = Y_MARGIN + row * CHAR_HEIGHT_LINES + line;

  sixBits &= 0b00111111;

  int xByte = x / 8;
  int xOffset = x % 8;

  uint8_t current = buffer[y * SCREEN_WIDTH_BYTES + xByte];
  buffer[y * SCREEN_WIDTH_BYTES + xByte] = (
    ((sixBits << 2) >> xOffset) +
    (current & ~(0b11111100 >> xOffset))
  );

  if (xOffset > 2) {
    current = buffer[y * SCREEN_WIDTH_BYTES + xByte + 1];
    buffer[y * SCREEN_WIDTH_BYTES + xByte + 1] = (
      (sixBits << (10 - xOffset)) +
      (current & (0b11111111 >> (xOffset - 2)))
    );
  }
}

uint8_t getSixBitsAt(uint8_t *buffer, int row, int col, int line) {
  int x = X_MARGIN + col * CHAR_WIDTH_PIXELS;
  int y = Y_MARGIN + row * CHAR_HEIGHT_LINES + line;

  int xByte = x / 8;
  int xOffset = x % 8;

  uint8_t value = ((buffer[y * SCREEN_WIDTH_BYTES + xByte] << xOffset) & 0b11111100) >> 2;

  if (xOffset > 2) {
    uint8_t secondByte = buffer[y * SCREEN_WIDTH_BYTES + xByte + 1] >> (10 - xOffset);
    value += secondByte;
  }

  return value;
}

void clear_line(struct screen *screen, color_t inactive, size_t row, size_t line, size_t from_col, size_t to_col) {
  uint8_t sixBits = inactive == 0xf ? 0xff : 0;

  uint32_t startPixel = X_MARGIN + from_col * CHAR_WIDTH_PIXELS;
  uint32_t endPixel = X_MARGIN + to_col * CHAR_WIDTH_PIXELS;

  uint32_t offset = (Y_MARGIN + (row * CHAR_HEIGHT_LINES) + line) * SCREEN_WIDTH_BYTES;
  uint16_t startByte = offset + (startPixel / 8 + ((startPixel % 8) ? 1 : 0));
  uint16_t endByte = offset + (endPixel / 8);

  setSixBitsAt(screen->buffer, sixBits, row, from_col, line);
  memset(screen->buffer + startByte, sixBits, endByte - startByte);
  setSixBitsAt(screen->buffer, sixBits, row, to_col - 1, line);
}

void screen_clear_rows(struct screen *screen, size_t from_row, size_t to_row,
                       color_t inactive, void (*yield)()) {
  if (to_row <= from_row) {
    return;
  }

  if (to_row > ROWS) {
    return;
  }

  for (size_t i = from_row; i < to_row; i++) {
    for (size_t j = 0; j < CHAR_HEIGHT_LINES; j++) {
      clear_line(screen, inactive, i, j, 0, COLS);
      yield();
    }
  }
}

void screen_clear_cols(struct screen *screen, size_t row, size_t from_col,
                       size_t to_col, color_t inactive, void (*yield)()) {
  if (row >= ROWS) {
    return;
  }

  if (to_col <= from_col) {
    return;
  }

  if (to_col > COLS) {
    return;
  }

  for (size_t i = 0; i < CHAR_HEIGHT_LINES; i++) {
    clear_line(screen, inactive, row, i, from_col, to_col);

    yield();
  }
}

void copy_cols(struct screen *screen, size_t from_row, size_t from_col, size_t to_row, size_t to_col, size_t cols, void (*yield)()) {
  uint8_t tmp[SCREEN_WIDTH_BYTES];

  for (int j = 0; j < CHAR_HEIGHT_LINES; j++) {
    int from_y = Y_MARGIN + from_row * CHAR_HEIGHT_LINES + j;
    memcpy(tmp, screen->buffer + (from_y * SCREEN_WIDTH_BYTES), SCREEN_WIDTH_BYTES);

    for (int i = 0; i < cols; i++) {
      uint8_t sixBits = getSixBitsAt(screen->buffer, from_row, from_col + i, j);
      setSixBitsAt(tmp, sixBits, 0, to_col + i, -Y_MARGIN);
    }

    int to_y = Y_MARGIN + to_row * CHAR_HEIGHT_LINES + j;
    memcpy(screen->buffer + (to_y * SCREEN_WIDTH_BYTES), tmp, SCREEN_WIDTH_BYTES);

    yield();
  }
}

void screen_shift_right(struct screen *screen, size_t row, size_t col,
                        size_t cols, color_t inactive, void (*yield)()) {
  if (row >= ROWS) {
    return;
  }

  if (col >= COLS) {
    return;
  }

  if (col + cols > COLS) {
    return;
  }

  copy_cols(screen, row, col, row, col + cols, COLS - col - cols, yield);

  screen_clear_cols(screen, row, col, col + cols, inactive, yield);
}

void screen_shift_left(struct screen *screen, size_t row, size_t col,
                       size_t cols, color_t inactive, void (*yield)()) {
  if (row >= ROWS)
    return;

  if (col >= COLS)
    return;

  if (col + cols > COLS)
    return;

  copy_cols(screen, row, col, row, col - cols, COLS - col, yield);

  screen_clear_cols(screen, row, COLS - cols, COLS, inactive, yield);
}

void screen_scroll(struct screen *screen, enum scroll scroll, size_t from_row,
                   size_t to_row, size_t rows, color_t inactive,
                   void (*yield)()) {
  if (to_row <= from_row)
    return;

  if (to_row > ROWS)
    return;

  if (to_row <= from_row + rows) {
    screen_clear_rows(screen, from_row, to_row, inactive, yield);
    return;
  }

  if (scroll == SCROLL_DOWN) {
    for (int i = to_row - from_row - 1; i >= 0; i--) {
      if (from_row + rows + i >= ROWS) {
        continue;
      }
      copy_cols(screen, from_row + i, 0, from_row + rows + i, 0, COLS, yield);
    }

    screen_clear_rows(screen, from_row, from_row + rows, inactive, yield);
  } else if (scroll == SCROLL_UP) {
    for (int i = 0; i < to_row - from_row; i++) {
      if (from_row + i < rows) {
        continue;
      }
      copy_cols(screen, from_row + i, 0, from_row - rows + i, 0, COLS, yield);
    }

    screen_clear_rows(screen, to_row - rows, to_row, inactive, yield);
  }
}

void screen_draw_codepoint(struct screen *screen, size_t row, size_t col,
                           codepoint_t codepoint, enum font font, bool italic,
                           bool underlined, bool crossedout, color_t active,
                           color_t inactive) {
  if (row >= ROWS) {
    return;
  }

  if (col >= COLS) {
    return;
  }

  const struct bitmap_font *bitmap_font;
  if (font == FONT_BOLD) {
    bitmap_font = screen->bold_bitmap_font;
  } else {
    bitmap_font = screen->normal_bitmap_font;
  }

  const unsigned char *glyph = find_glyph(bitmap_font, codepoint);

  if (!glyph) {
    glyph = find_glyph(bitmap_font, REPLACEMENT_CODEPOINT);
  }

  size_t underlined_line = CHAR_HEIGHT_LINES - 1;
  size_t crossedout_line = CHAR_HEIGHT_LINES - 5;

  for (size_t char_line = 0; char_line < CHAR_HEIGHT_LINES; char_line++) {
    uint8_t pixels = inactive == DEFAULT_ACTIVE_COLOR ? 0xff : 0;

    if (glyph) {
      if (char_line < bitmap_font->height) {
        pixels = active == DEFAULT_ACTIVE_COLOR ? glyph[char_line] : ~glyph[char_line];
      }

      if (underlined && char_line == underlined_line) {
        pixels ^= active == DEFAULT_ACTIVE_COLOR ? 0xff : 0;
      }

      if (crossedout && char_line == crossedout_line) {
        pixels = active == DEFAULT_ACTIVE_COLOR ? 0xff : 0;
      }
    }

    setSixBitsAt(screen->buffer, pixels, row, col, char_line);
  }
}

void screen_test_fonts(struct screen *screen, enum font font) {
  const struct bitmap_font *bitmap_font;
  if (font == FONT_BOLD) {
    bitmap_font = screen->bold_bitmap_font;
  } else {
    bitmap_font = screen->normal_bitmap_font;
  }

  for (size_t row = 0; row < 24; row++) {
    for (size_t col = 0; col < 80; col++) {
      codepoint_t codepoint = ((row * 80) + col);

      if (codepoint < bitmap_font->codepoints_length) {
        screen_draw_codepoint(
          screen,
          row,
          col,
          bitmap_font->codepoints[codepoint],
          font,
          false,
          false,
          false,
          0xf,
          0
        );
      }
    }
  }
}
