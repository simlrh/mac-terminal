#ifndef SCREEN_HEADER
#define SCREEN_HEADER

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../fonts/font.h"

typedef uint8_t color_t;
typedef uint8_t character_t;
typedef uint16_t codepoint_t;

#define DEFAULT_ACTIVE_COLOR 0xf
#define DEFAULT_INACTIVE_COLOR 0

enum font
{
  FONT_NORMAL = 0,
  FONT_BOLD = 1,
  FONT_THIN = 2,
};

struct format
{
  uint8_t rows;
  uint8_t cols;
};

enum scroll
{
  SCROLL_UP,
  SCROLL_DOWN
};

struct screen
{
  const struct format format;
  const uint8_t char_width;
  const uint8_t char_height;
  const struct bitmap_font *normal_bitmap_font;
  const struct bitmap_font *bold_bitmap_font;
  uint8_t *buffer;
};

void screen_clear_rows(struct screen *screen, size_t from_row, size_t to_row, color_t inactive, void (*yield)());

void screen_clear_cols(struct screen *screen, size_t row, size_t from_col, size_t to_col, color_t inactive,
                       void (*yield)());

void screen_scroll(struct screen *screen, enum scroll scroll, size_t from_row, size_t to_row, size_t rows,
                   color_t inactive, void (*yield)());

void screen_shift_right(struct screen *screen, size_t row, size_t col, size_t cols, color_t inactive, void (*yield)());

void screen_shift_left(struct screen *screen, size_t row, size_t col, size_t cols, color_t inactive, void (*yield)());

void screen_draw_codepoint(struct screen *screen, size_t row, size_t col, codepoint_t codepoint, enum font font,
                           bool italic, bool underlined, bool crossedout, color_t active, color_t inactive);

void screen_test_fonts(struct screen *screen, enum font font);

#endif
