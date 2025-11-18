#!/bin/bash

w=$1
h=$2
type=$3
charset=$4

# Generate required characters for every font charset
python3 utils/translations_and_fonts/lang.py generate-required-chars src/lang/po .

# Generate C++ header with font data
python3 utils/translations_and_fonts/font.py \
 --width=${w} \
 --height=${h} \
 --type=${type} \
 --charset=${charset} \
 --src-latin src/gui/res/fnt_src/*${type}_${w}x${h}.png \
 --src-katakana src/gui/res/fnt_src/${w}x${h}px*_katakana.png \
 --src-cyrillic src/gui/res/fnt_src/${w}x${h}px*_cyrillic.png \
 --output=src/gui/res/cc/font_${type}_${w}x${h}_${charset}.hpp

rm -rf full-chars.txt latin-chars.txt digits-chars.txt latin-and-katakana-chars.txt latin-and-cyrillic-chars.txt
