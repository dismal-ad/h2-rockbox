#!/bin/sh
# Regenerate the new UI's fonts from Source Sans 3 (SIL OFL 1.1, see OFL.txt).
# Needs tools/convttf (make -C tools convttf; requires libfreetype-dev).
# The generated .fnt files are committed so normal builds don't need FreeType.
set -e
cd "$(dirname "$0")"
C=../../tools/convttf
# text: Latin, Greek, Cyrillic, general punctuation up to the ellipsis
$C -p 12 -s 32 -l 8230 -o newui-12.fnt  SourceSans3-Regular.ttf
$C -p 15 -s 32 -l 8230 -o newui-15.fnt  SourceSans3-Regular.ttf
$C -p 18 -s 32 -l 8230 -o newui-18b.fnt SourceSans3-Semibold.ttf
# big letter / numbers: ASCII only
$C -p 40 -s 32 -l 126  -o newui-40b.fnt SourceSans3-Semibold.ttf
