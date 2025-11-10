#!/usr/bin/env python3

import argparse
import sys
from PIL import Image


def png2font(src_filename, out_filename, char_w, char_h, columns, rows):

    # Load source PNG
    src_img = Image.open(src_filename)
    src_rgb = src_img.convert('RGB')
    src_w, src_h = src_rgb.size

    # Initialize charset data with zeros
    # Constants for 4 bits per pixel (2 pixels per byte)
    char_bpr = (char_w + 1) >> 1  # bytes per row
    char_size = char_h * char_bpr  # character size in bytes
    char_count = columns * rows  # character count
    charset_size = char_size * char_count  # charset size in bytes
    charset_data = bytearray(charset_size)

    # Process each source pixel
    src_pixels = src_rgb.load()
    for y in range(src_h):
        for x in range(src_w):
            r, g, b = src_pixels[x, y]
            luminance = (r + g + b) // 3
            opacity = 255 - luminance
            opacity = opacity >> 4

            # Calculate character code and position
            char_code = (x // char_w) + columns * (y // char_h)
            char_offs = char_code * char_size  # character offset in charset [bytes]
            char_x = x % char_w  # character pixel x-coord (0..char_w-1)
            char_y = y % char_h  # character pixel y-coord (0..char_h-1)
            char_row_offs = char_y * char_bpr  # character row offset [bytes]
            char_pix_offs = char_x >> 1  # character pixel offset [bytes]
            offs = char_offs + char_row_offs + char_pix_offs  # total offset in charset [bytes]

            # Calculate bit shift (4 bits per pixel, 2 pixels per byte)
            i = char_x % 2
            i = 4 - (i * 4)

            # Update character pixel data
            charset_data[offs] |= opacity << i

    with open(out_filename, 'wb') as out_file:
        out_file.write(charset_data)


def main():
    parser = argparse.ArgumentParser(
        description='png2font - convert PNG image to binary font format')

    parser.add_argument('--source',
                        dest='source',
                        required=True,
                        help='source png file name')
    parser.add_argument('--output',
                        dest='output',
                        required=True,
                        help='output charset binary file name')
    parser.add_argument('--width',
                        dest='width',
                        type=int,
                        required=True,
                        help='character width')
    parser.add_argument('--height',
                        dest='height',
                        type=int,
                        required=True,
                        help='character height')
    parser.add_argument('--columns',
                        dest='columns',
                        type=int,
                        required=True,
                        help='charset columns')
    parser.add_argument('--rows',
                        dest='rows',
                        type=int,
                        required=True,
                        help='charset rows')

    args = parser.parse_args()

    try:
        png2font(args.source, args.output, args.width, args.height,
                 args.columns, args.rows)
        return 0
    except Exception as e:
        print(f'Error: {e}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
