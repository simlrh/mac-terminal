#include <stdlib.h>
#include <stdio.h>

#include "font.h"

static int16_t find_glyph_index(const struct bitmap_font *font,
                                uint16_t codepoint) {
  size_t first = 0;
  size_t last = font->codepoints_length - 1;
  size_t middle = (first + last) / 2;

  while (first <= last) {
    if (font->codepoints[middle] == codepoint)
      return middle;

    if (font->codepoints[middle] < codepoint)
      first = middle + 1;
    else
      last = middle - 1;

    middle = (first + last) / 2;
  }

  return -1;
}

const uint8_t *find_glyph(const struct bitmap_font *font,
                          uint16_t codepoint) {
  // TODO: move null character in erus font
  if (codepoint == 0) {
    codepoint = 32;
  }

  int16_t index = find_glyph_index(font, codepoint);

  if (index == -1)
    return NULL;

  return font->data + (index * font->height);
}
