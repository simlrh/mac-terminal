#!/usr/bin/env python

import sys
import argparse
from bdfparser import Font

def compile_font(fontfile):
    font = Font(fontfile)

    glyphs = [glyph for glyph in font.iterglyphs()]

    glyph_bytes = [item for sublist in 
        [glyph.draw().todata(4) for glyph in glyphs]
        for item in sublist
    ]

    codepoints = [glyph.cp() for glyph in glyphs]

    return {
        'height': font.headers['fbby'],
        'width': font.headers['fbbx'],
        'data': glyph_bytes,
        'codepoints_length': len(glyphs),
        'codepoints': codepoints,
    }

def format_bytes_literal(b):
    return '{{0x{data}}}'.format(data=',0x'.join(b))

def format_ints_literal(i):
    return '{{{data}}}'.format(data=','.join(str(x) for x in i))

def main(args):
    normal_font = compile_font(args['font'])

    count = normal_font['codepoints_length']
    glyph_literal = format_bytes_literal(normal_font['data'])
    codepoints_literal = format_ints_literal(normal_font['codepoints'])

    out = (
        '#include <stdint.h>\n\n'
        '#include "font.h"\n\n'
        f'const uint8_t glyph_data[{count * normal_font["height"]}] = {glyph_literal};\n'
        f'const uint16_t codepoints[{count}] = {codepoints_literal};\n'
        'const struct bitmap_font normal_font = {\n'
        f'  .height = {normal_font["height"]},\n'
        f'  .width = {normal_font["width"]},\n'
        '  .data = (const uint8_t *) glyph_data,\n'
        f'  .codepoints_length = {count},\n'
        '  .codepoints = (const uint16_t *) codepoints,\n'
        '};\n\n'
    )

    if args['bold'] == None:
        out += (
            'const struct bitmap_font bold_font = {\n'
            f'  .height = {normal_font["height"]},\n'
            f'  .width = {normal_font["width"]},\n'
            '  .data = (const uint8_t *) glyph_data,\n'
            f'  .codepoints_length = {count},\n'
            '  .codepoints = (const uint16_t *) codepoints,\n'
            '};'
        )
    else:
        bold_font = compile_font(args['bold'])
        count = bold_font['codepoints_length']
        glyph_literal = format_bytes_literal(bold_font['data'])
        codepoints_literal = format_ints_literal(bold_font['codepoints'])

        out += (
            f'const uint8_t bold_glyph_data[{count * bold_font["height"]}] = {glyph_literal};\n'
            f'const uint16_t bold_codepoints[{count}] = {codepoints_literal};\n'
            'const struct bitmap_font bold_font = {\n'
            f'  .height = {bold_font["height"]},\n'
            f'  .width = {bold_font["width"]},\n'
            '  .data = (const uint8_t *) bold_glyph_data,\n'
            f'  .codepoints_length = {count},\n'
            '  .codepoints = (const uint16_t *) bold_codepoints,\n'
            '};'
        )

    with open(args['outfile'], 'w') as f:
        f.write(out)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate a C file from a BDF font file')
    parser.add_argument('--bold', '-b', help='an optional bold variant bdf file', default=None)
    parser.add_argument('--outfile', '-o', help='file to output', default='font_data.c')
    parser.add_argument('font', help='the font to convert')
    args = parser.parse_args()
    main(vars(args))
