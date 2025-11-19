# GENERATING NEW FONTS TUTORIAL
## NOTICE:
 * arguments in "" are placeholders *others* are to be copy-pasted
 * helping scripts and programs are run from the root directory of the project
 * Language names are not translated, so non-ascii characters contained in these names are not in *.po files. They are added in the lang.py script (line:242)

# AUTOMATION
This pipeline can be fully automated with a bash script for a single font case:
The script expects:
1. Preparation steps are all done
2. Source pngs are named * / \{type}_\{w}x\{h}.png e.g. "15px-LiberationMono-regular_9x16.png" and non latin alphabet source pngs are named \{w}x\{h}px_*\{type}_\{katakana/cyrillic}.png e.g. "9x16px-LiberationMono-regular_cyrillic.png"
3. Script is run from root directory of project

```bash
w           - pixel width of given font
h           - pixel height of given font
type        - type of font used ("bold" / "regular")
fnt_charset - set of characters ("full" / "digits" / "latin" / "latin_and_katakana" / "latin_and_cyrillic")

./utils/translation_and_fonts/generate_single_fonts.sh
```

FOR GENERATION OF ALL FONTS AT ONCE USE:
```bash
./utils/translation_and_fonts/generate_all_fonts.sh
```

# Manual execution
1. It is necessary to aquire list of all required symbols that are currently being used by the printer. Symbols can be generated via utils/translations_and_fonts/lang.py script
```bash
mode           - Script is actually capable of doing multiple things -h will show you the
input-dir      - Given argument should contain pre-generated files required; the directory needs to contain po files with translations
output-dir     - Multiple files will be generated there,  full-chars.txt/latin-chars.txt/digits-chars.txt/latin-and-katakana-chars.txt/latin-and-cyrillic-chars.txt containing necessary characters
```

2. New pngs with all symbols are required.
 * !!! Symbols not found in the source pngs will be replaced with black squares at the end of the generated pngs in the following steps
 * !!! Source png needs to be of RGB type

 * Save source pngs to src/gui/res/fnt_src

## Generating
No we have to select, what character set option we want to generate. There are 3 possibilities:
1. "full"               - contains full standard ASCII (32-127) + all needed non-ascii characters + katakana + cyrillic alphabet
2. "latin"              - contains full standard ASCII (32-127) + all needed non-ascii characters
3. "digits"             - contains only digits (0-9) + '%' + '?' + '.' + ' ' + '-'
4. "latin_and_katakana" - contains full standard ASCII (32-127) + all needed non-ascii characters + katakana
5. "latin_and_cyrillic" - contains full standard ASCII (32-127) + all needed non-ascii characters + cyrillic alphabet

1. Run utils/translations_and_fonts/font.py. This script generates C++ header files directly from source font images.
```bash
--width             - character width in pixels
--height            - character height in pixels
--charset           - character set option (full, latin, digits, latin_and_katakana, latin_and_cyrillic)
                      Automatically determines required chars file and IPP path
--type              - font type (regular, bold, ...)
--src-latin         - path to source PNG with Latin characters; must be RGB mode; "src/gui/res/fnt_src/{name}"
--src-katakana      - path to source PNG with Katakana characters; must be RGB mode; "src/gui/res/fnt_src/{name}"
--src-cyrillic      - path to source PNG with Cyrillic characters; must be RGB mode; "src/gui/res/fnt_src/{name}"
--output            - destination C++ header file path; "src/gui/res/cc/{name}.hpp"
```

2. Redo step 1 for all fonts that are to be changed
3. At last change includes in src/gui/fonts.cpp to the ones just added and cleanup unused ones
