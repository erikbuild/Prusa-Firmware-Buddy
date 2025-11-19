#!/usr/bin/env python3
import codecs
from pathlib import Path
import sys
import argparse
import logging
from PIL import Image
import math
import numpy as np

logger = logging.getLogger('font.py')
chars_per_row = 16


def png2font(src_img, char_w, char_h, columns, rows):
    """Convert PNG image to binary font format, returning the charset data."""

    # Convert image to RGB
    src_rgb = src_img.convert('RGB')
    src_w, src_h = src_rgb.size

    # Initialize charset data with zeros
    # Constants for 4 bits per pixel (2 pixels per byte)
    char_size = (char_h * char_w + 1) >> 1  # character size in bytes
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
            char_pix_offs = char_y * char_w + char_x  # character pixel offset [pixels]
            offs = char_offs + char_pix_offs >> 1  # total offset in charset [bytes]

            # Calculate bit shift (4 bits per pixel, 2 pixels per byte)
            i = char_pix_offs % 2
            i = 4 - (i * 4)

            # Update character pixel data
            charset_data[offs] |= opacity << i

    return charset_data


def bin2cc(charset_data, dst_file, var_name, w, h, charset_enum):
    """Convert binary font data to C++ header format."""

    font_name = f'font_{var_name}_{w}x{h}'

    dst_file.write(f'#include "font_character_sets.hpp"\n')
    dst_file.write(f'constexpr uint8_t {font_name}_data[] = {{\n')

    for byte in charset_data:
        dst_file.write(f'    0x{byte:02x},\n')

    dst_file.write('};\n')
    dst_file.write(
        f'constexpr font_t {font_name} = {{ {w}, {h}, FontCharacterSet::{charset_enum}, {font_name}_data }};\n'
    )


def remove_red_dots(image: np.array) -> np.array:
    for i in range(image.shape[0]):
        for j in range(image.shape[1]):
            if image[i, j, 0] == 255 and image[i, j, 1] == 0 and image[i, j,
                                                                       2] == 0:
                image[i, j] = np.array([255, 255, 255])
    return image


def char_to_int(char) -> int:
    char_bytes = char.encode("utf-32", "little")
    if char_bytes.startswith(codecs.BOM_UTF32):
        char_bytes = char_bytes[len(codecs.BOM_UTF32):]
    # char_bytes = char_bytes.removeprefix(codecs.BOM_UTF32) # New in version 3.9.
    return int.from_bytes(char_bytes, "little")


def check_path(src_path: Path):
    if not src_path.exists():
        logger.error('no %s found', src_path)
        return 1
    return 0


def check_resource_mode(resource, resource_name):
    if resource.mode != "RGB":
        logger.error('%s mode is %s instead of required RGB', resource_name,
                     resource.mode)
        return 1
    return 0


# Map charset options to their file paths
def get_charset_paths(charset_option: str):
    """Get required_chars and ipp_path based on charset option."""
    charset_map = {
        'full': ('full-chars.txt', 'src/guiapi/include/fnt-full-indices.ipp'),
        'latin':
        ('latin-chars.txt', 'src/guiapi/include/fnt-latin-indices.ipp'),
        'digits':
        ('digits-chars.txt', 'src/guiapi/include/fnt-digits-indices.ipp'),
        'latin_and_katakana':
        ('latin-and-katakana-chars.txt',
         'src/guiapi/include/fnt-latin-and-katakana-indices.ipp'),
        'latin_and_cyrillic':
        ('latin-and-cyrillic-chars.txt',
         'src/guiapi/include/fnt-latin-and-cyrillic-indices.ipp'),
    }

    if charset_option not in charset_map:
        logger.error('Invalid charset_option: %s. Must be one of: %s',
                     charset_option, ', '.join(charset_map.keys()))
        return None, None

    return charset_map[charset_option]


# based on charset_option ("full" / "latin" / "digits" / "latin_and_katakana" / "latin_and_cyrillic") different paths will be used
def cmd_create_font(charset_option: str, src_png_path: Path,
                    src_png_jap_path: Path, src_png_ukr_path: Path,
                    char_width: int, char_height: int, dst_cc_path: Path,
                    font_type: str):

    # Infer paths from charset option
    required_chars_file, ipp_file = get_charset_paths(charset_option)
    if required_chars_file is None:
        return 1

    required_chars_path = Path(required_chars_file)
    ipp_path = Path(ipp_file)

    if check_path(required_chars_path) or check_path(
            src_png_path) or check_path(src_png_jap_path) or check_path(
                src_png_ukr_path):
        return 1

    # Extract current character set
    char_list = []
    with open(required_chars_path.resolve()) as file:
        chars = file.read()
        char_list = chars.split()

    if not char_list:
        logger.error(
            'font.py::cmd_create_font_png: unsupported charset_option %s or unable to open required_chars_path',
            charset_option)
        return 1

    # Add space back in the set
    char_list.append(' ')
    char_list = sorted(char_list)
    fail = False

    with Image.open(src_png_path.resolve()) as src_lat_png, Image.open(
            src_png_jap_path.resolve()) as src_jap_png, Image.open(
                src_png_ukr_path.resolve()) as src_ukr_png:
        # Some font have no need for japanese at all, but I leave it here just in case its char set is changed and japanese characters are needed
        # Each font have to have its own katakana alphabet anyway
        if check_resource_mode(src_lat_png, "LATIN") or check_resource_mode(
                src_jap_png, "KATAKANA") or check_resource_mode(
                    src_ukr_png, "CYRILLIC"):
            return 1

        num_of_rows = math.ceil(len(char_list) / chars_per_row)
        output_image = Image.new(
            "RGB", (chars_per_row * char_width, num_of_rows * char_height),
            color="white")

        print("IPP:", ipp_path)

        x = 0
        y = 0
        with open(str(ipp_path.resolve()), "w") as file:
            for ch in char_list:
                # CYRILLIC
                if ord(ch) >= 0x0400 and ord(ch) <= 0x04FF:
                    cyrill_index = (ord(ch) - 0x0400)
                    srcX = cyrill_index % chars_per_row
                    srcY = cyrill_index // chars_per_row
                    src_png = src_ukr_png

                # JAPANESE
                elif (ord(ch) >= 0x30A0
                      and ord(ch) <= 0x30FF) or ch == '、' or ch == '。':
                    # SPECIAL CASES (comma and dot are appended to katakana fonts)
                    if ch == '、' or ch == '。':
                        # Hardcoded coordinates in our katakana font source png
                        srcX = 0 if ch == '、' else 1
                        srcY = 6
                    else:
                        # KATAKANA
                        katakana_index = (ord(ch) - 0x30A0)
                        srcX = katakana_index % chars_per_row
                        srcY = katakana_index // chars_per_row
                    src_png = src_jap_png

                else:
                    char_int = char_to_int(ch)
                    char_int -= 32  # this is where out standard ASCII bitmap starts
                    srcX = char_int % chars_per_row
                    srcY = char_int // chars_per_row
                    src_png = src_lat_png

                x_max, y_max = src_png.size
                if (srcY + 1) * char_height > y_max or char_int < 0:
                    # Unsupported character
                    logger.error("Unsupported character found: \"%c\"", ch)
                    fail = True
                    continue

                char_crop = src_png.crop(
                    (srcX * char_width, srcY * char_height,
                     (srcX + 1) * char_width, (srcY + 1) * char_height))

                # Append character to our font png
                output_image.paste(char_crop, (x * char_width, y * char_height,
                                               (x + 1) * char_width,
                                               (y + 1) * char_height))

                # Write index
                file.write("{},\n".format(hex(char_to_int(ch))))

                x += 1
                if (x >= chars_per_row):
                    x = 0
                    y += 1
        if fail:
            logger.error(
                "Remove / Replace the unsupported characters in PO files, rerun \"new_translations.sh\" and regenerate fonts again"
            )
            return 1

        image = remove_red_dots(np.array(output_image, dtype=np.uint8))
        output_image = Image.fromarray(image, "RGB")

        # Generate C++ header directly from the in-memory image
        logger.info('Generating C++ header: %s', dst_cc_path)
        # Calculate number of rows for the charset
        num_of_rows = math.ceil(len(char_list) / chars_per_row)

        # Convert the generated PNG to C++ header
        charset_data = png2font(output_image, char_width, char_height,
                                chars_per_row, num_of_rows)

        with open(dst_cc_path, 'w') as out_file:
            bin2cc(charset_data, out_file, font_type, char_width, char_height,
                   charset_option)

        logger.info('Successfully generated C++ header')

        return 0


def main():
    parser = argparse.ArgumentParser(
        description='Font utility - create font C++ headers')
    parser.add_argument('--verbose', '-v', action='count', default=0)

    parser.add_argument('--src-latin',
                        dest='src_png',
                        type=Path,
                        required=True,
                        help='source PNG file with Latin characters')
    parser.add_argument('--src-katakana',
                        dest='src_png_jap',
                        type=Path,
                        required=True,
                        help='source PNG file with Katakana characters')
    parser.add_argument('--src-cyrillic',
                        dest='src_png_ukr',
                        type=Path,
                        required=True,
                        help='source PNG file with Cyrillic characters')
    parser.add_argument(
        '--charset',
        dest='charset_option',
        type=str,
        required=True,
        help=
        'character set option (full, latin, digits, latin_and_katakana, latin_and_cyrillic)'
    )
    parser.add_argument('--width',
                        dest='char_width',
                        type=int,
                        required=True,
                        help='character width in pixels')
    parser.add_argument('--height',
                        dest='char_height',
                        type=int,
                        required=True,
                        help='character height in pixels')
    parser.add_argument('--output',
                        dest='dst_cc',
                        type=Path,
                        required=True,
                        help='output C++ header file path')
    parser.add_argument('--type',
                        dest='font_type',
                        type=str,
                        required=True,
                        help='font type (regular, bold, ...)')

    args = parser.parse_args()
    logging.basicConfig(format='%(levelname)-8s %(message)s',
                        level=logging.WARNING - args.verbose * 10)

    retval = cmd_create_font(args.charset_option, args.src_png,
                             args.src_png_jap, args.src_png_ukr,
                             args.char_width, args.char_height, args.dst_cc,
                             args.font_type)

    sys.exit(retval if retval is not None else 0)


if __name__ == '__main__':
    main()
