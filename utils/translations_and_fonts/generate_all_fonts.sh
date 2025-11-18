#!/bin/sh

set -ex

# Fonts for BIG layout displays
./utils/translations_and_fonts/generate_single_font.sh 11 19 bold full
./utils/translations_and_fonts/generate_single_font.sh 13 22 bold full
./utils/translations_and_fonts/generate_single_font.sh 9 16 regular full
./utils/translations_and_fonts/generate_single_font.sh 30 53 bold digits

# Fonts for SMALL layout displays (with japanese)
./utils/translations_and_fonts/generate_single_font.sh 7 13 regular latin_and_katakana
./utils/translations_and_fonts/generate_single_font.sh 11 18 regular latin_and_katakana
./utils/translations_and_fonts/generate_single_font.sh 9 16 regular latin_and_katakana

# Fonts for SMALL layout displays (with cyrillic)
./utils/translations_and_fonts/generate_single_font.sh 7 13 regular latin_and_cyrillic
./utils/translations_and_fonts/generate_single_font.sh 11 18 regular latin_and_cyrillic
./utils/translations_and_fonts/generate_single_font.sh 9 16 regular latin_and_cyrillic

# Fonts for SMALL layout displays (latin only)
./utils/translations_and_fonts/generate_single_font.sh 7 13 regular latin
./utils/translations_and_fonts/generate_single_font.sh 11 18 regular latin
./utils/translations_and_fonts/generate_single_font.sh 9 16 regular latin
