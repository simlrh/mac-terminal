#ifndef FONTS_HEADER
#define FONTS_HEADER

#include <stdint.h>
#include <stdlib.h>

struct bitmap_font
{
  uint32_t height;
  uint32_t width;
  const uint8_t *data;
  uint32_t codepoints_length;
  const uint16_t *codepoints;
};

const uint8_t *find_glyph(const struct bitmap_font *font, uint16_t codepoint);

extern const struct bitmap_font normal_font;
extern const struct bitmap_font bold_font;

#endif
