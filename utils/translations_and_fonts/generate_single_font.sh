#!/bin/bash

w=$1
h=$2
type=$3
charset_option=$4

png_dst=src/gui/res/fnt_png/
dst_name=font_${type}_${w}x${h}_${charset_option}

# Generate required characters for every font charset_option
python3 utils/translations_and_fonts/lang.py generate-required-chars src/lang/po .

# Calculate character count
full_character_count=$(cat "full-chars.txt" | tr -d ' \n' | wc -m)
latin_character_count=$(cat "latin-chars.txt" | tr -d ' \n' | wc -m)
digits_character_count=$(cat "digits-chars.txt" | tr -d ' \n' | wc -m)
latin_and_katakana_character_count=$(cat "latin-and-katakana-chars.txt" | tr -d ' \n' | wc -m)
latin_and_cyrillic_character_count=$(cat "latin-and-cyrillic-chars.txt" | tr -d ' \n' | wc -m)

# Increment by 1 to add "space" character
((full_character_count++))
((latin_character_count++))
((digits_character_count++))
((latin_and_katakana_character_count++))
((latin_and_cyrillic_character_count++))

# Caluclate rows of current font
if [[ "$charset_option" == "full" ]]; then
    char_rows=$(awk "BEGIN {print int(\"$full_character_count\" / 16 + 0.999999)}")
    required_chars=full-chars.txt
    ipp_path=src/guiapi/include/fnt-full-indices.ipp
elif [[ "$charset_option" == "latin" ]]; then
    char_rows=$(awk "BEGIN {print int(\"$latin_character_count\" / 16 + 0.999999)}")
    required_chars=latin-chars.txt
    ipp_path=src/guiapi/include/fnt-latin-indices.ipp
elif [[ "$charset_option" == "digits" ]]; then
    char_rows=$(awk "BEGIN {print int(\"$digits_character_count\" / 16 + 0.999999)}")
    required_chars=digits-chars.txt
    ipp_path=src/guiapi/include/fnt-digits-indices.ipp
elif [[ "$charset_option" == "latin_and_katakana" ]]; then
    char_rows=$(awk "BEGIN {print int(\"$latin_and_katakana_character_count\" / 16 + 0.999999)}")
    required_chars=latin-and-katakana-chars.txt
    ipp_path=src/guiapi/include/fnt-latin-and-katakana-indices.ipp
elif [[ "$charset_option" == "latin_and_cyrillic" ]]; then
    char_rows=$(awk "BEGIN {print int(\"$latin_and_cyrillic_character_count\" / 16 + 0.999999)}")
    required_chars=latin-and-cyrillic-chars.txt
    ipp_path=src/guiapi/include/fnt-latin-and-cyrillic-indices.ipp
else
    echo "Argument 'charset_option' ( digits / latin / latin_and_katakana / latin_and_cyrillic / full ) contains an unexpected value"
    rm -rf full-chars.txt latin-chards.txt digits-chars.txt latin-and-katakana.txt latin-and-cyrillic.txt
    exit 1
fi

# Generate font PNGs and index files (based on charset)
# Having all possible source (standard and katakana) for each font IS MANDATORY!
python3 utils/translations_and_fonts/font.py src/gui/res/fnt_src/*${type}_${w}x${h}.png src/gui/res/fnt_src/${w}x${h}px*_katakana.png src/gui/res/fnt_src/${w}x${h}px*_cyrillic.png ${charset_option} ${required_chars} ${w} ${h} ${png_dst}${dst_name}.png ${ipp_path}

# Check the return value of font.py
if [ $? -eq 0 ]; then
    echo "font.py script executed successfully..."
else
    echo "FAILED: Errors occured in \"font.py\" script"
    rm -rf full-chars.txt latin-chars.txt digits-chars.txt latin-and-katakana-chars.txt latin-and-cyrillic-chars.txt
    exit 1
fi
rm -rf full-chars.txt latin-chars.txt digits-chars.txt latin-and-katakana-chars.txt latin-and-cyrillic-chars.txt

# Generate binary from the font
python3 utils/translations_and_fonts/png2font.py \
 --source="${png_dst}${dst_name}.png" \
 --output="${dst_name}.bin" \
 --width=${w} \
 --height=${h} \
 --columns=16 \
 --rows=${char_rows}

# Generate C++ header from binary
python3 utils/translations_and_fonts/bin2cc.py ${dst_name}.bin src/gui/res/cc/${dst_name}.hpp ${type} ${w} ${h} FontCharacterSet::${charset_option}
rm -rf ${dst_name}.bin
