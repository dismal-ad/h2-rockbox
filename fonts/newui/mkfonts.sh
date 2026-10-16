#!/bin/sh
# Regenerate the new UI's fonts from Nimbus Sans (URW, AGPL-3 with font
# exception, see LICENSE-NimbusSans.txt), a free Helvetica clone.
# Needs tools/convttf (make -C tools convttf; requires libfreetype-dev).
# The generated .fnt files are committed so normal builds don't need FreeType.
set -e
cd "$(dirname "$0")"
C=../../tools/convttf
# text: Latin, Greek, Cyrillic, general punctuation up to the single guillemets (›)
$C -p 14 -s 32 -l 8250 -o newui-14.fnt     NimbusSans-Regular.otf  # small: times, counts
$C -p 18 -s 32 -l 8250 -o newui-18.fnt     NimbusSans-Regular.otf  # secondary: artist, values
$C -p 20 -s 32 -l 8250 -o newui-20bold.fnt NimbusSans-Bold.otf     # rows, the UI font
$C -p 22 -s 32 -l 8250 -o newui-22bold.fnt NimbusSans-Bold.otf     # titles
# big letter / numbers: ASCII only
$C -p 40 -s 32 -l 126  -o newui-40bold.fnt NimbusSans-Bold.otf
