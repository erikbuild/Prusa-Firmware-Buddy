#!/usr/bin/env python3
"""Signature Oak icon parity report.

Signature Oak is a limited-edition Core One variant with a brass UI theme.
Brass icons in src/gui/res/png_brass/ are OVERRIDES of standard icons whose
orange pixels are replaced with brass. Only standard icons that contain
orange pixels need a brass counterpart.

This script lists standard icons that have orange pixels but no brass
override. The output is informational; the script never fails the pipeline.
"""

from pathlib import Path

from PIL import Image

# Prusa orange: RGB(248, 101, 27)
ORANGE_RGB = (248, 101, 27)
COLOR_TOLERANCE = 30

PNG_DIR = Path('src/gui/res/png')
BRASS_DIR = Path('src/gui/res/png_brass')

# Icons that intentionally do not require a brass variant, even if they contain
# orange pixels in the standard theme. Add filenames here to exclude them from
# the parity check.
NO_BRASS_REQUIRED: set[str] = {
    # Cleaner is only on INDX and won't be on Signature Oak,
    # so we can ignore these icons and not make brass variants.
    'cleaner_calibration_x.png',
    'cleaner_calibration_y.png',
    'cleaner_calibration_z.png',
}


def is_orange(r, g, b):
    return (abs(r - ORANGE_RGB[0]) < COLOR_TOLERANCE
            and abs(g - ORANGE_RGB[1]) < COLOR_TOLERANCE
            and abs(b - ORANGE_RGB[2]) < COLOR_TOLERANCE)


def png_has_orange(path):
    img = Image.open(path).convert('RGBA')
    for r, g, b, a in img.getdata():
        if a < 10:
            continue
        if is_orange(r, g, b):
            return True
    return False


def main():
    standard_icons = sorted(p.name for p in PNG_DIR.glob('*.png'))
    brass_icons = set(p.name for p in BRASS_DIR.glob('*.png'))

    missing = [
        icon for icon in standard_icons if png_has_orange(PNG_DIR / icon)
        and icon not in brass_icons and icon not in NO_BRASS_REQUIRED
    ]

    if missing:
        print(f'\ufeffSignature Oak icon parity: {len(missing)} icon(s) '
              f'with orange pixels but no brass override in png_brass/ 🙏')
        print('For each icon below, either request a brass variant from the '
              'designer, or add the filename to the NO_BRASS_REQUIRED set in '
              'utils/generate-icon-parity-report.py if the icon will not '
              'appear on Signature Oak:')
        for icon in missing:
            print(f'* `{icon}`')
    else:
        print('\ufeffSignature Oak icon parity: '
              'all orange icons have brass overrides 💪')


if __name__ == '__main__':
    main()
