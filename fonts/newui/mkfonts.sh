#!/bin/sh
# Regenerate the new UI's fonts from Source Sans 3 (SIL OFL 1.1, see OFL.txt).
# Needs tools/convttf (make -C tools convttf; requires libfreetype-dev).
# The generated .fnt files are committed so normal builds don't need FreeType.
set -e
cd "$(dirname "$0")"
C=../../tools/convttf
# text: Latin, Greek, Cyrillic, general punctuation up to the single guillemets (›)
$C -p 13 -s 32 -l 8250 -o newui-13.fnt  SourceSans3-Regular.ttf   # small: times, counts
$C -p 17 -s 32 -l 8250 -o newui-17.fnt  SourceSans3-Regular.ttf   # secondary: artist, values
$C -p 17 -s 32 -l 8250 -o newui-17b.fnt SourceSans3-Semibold.ttf  # rows, the UI font
$C -p 20 -s 32 -l 8250 -o newui-20b.fnt SourceSans3-Semibold.ttf  # titles
# big letter / numbers: ASCII only
$C -p 40 -s 32 -l 126  -o newui-40b.fnt SourceSans3-Semibold.ttf
