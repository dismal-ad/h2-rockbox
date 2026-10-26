#!/bin/sh
# Regenerate the new UI's fonts from Noto Sans CJK JP (SIL Open Font
# License 1.1, see LICENSE-NotoSansCJK.txt): Latin, Greek, Cyrillic and
# Chinese, Japanese and Korean (kana, Han in Japanese shapes, Hangul).
# Needs tools/convttf (make -C tools convttf; requires libfreetype-dev) and
# the Noto CJK fonts: Ubuntu/Debian package fonts-noto-cjk, or NOTO=<dir>
# with NotoSansCJK-Regular.ttc and NotoSansCJK-Bold.ttc.
# The generated .fnt files are committed so normal builds don't need FreeType.
#
# -t 0: the JP face of the collection. -TA/-TD trim the font's tall line
# spacing to what the glyphs use (accents, Han and Hangul still fit).
# -e: the two- and three-em dashes are left out; they are 2-3 times wider
# than any other glyph and every glyph cache entry is sized to the widest.
# Glyphs load from the card on demand, so the size of these files (several
# MB each) costs disk space, not RAM.
set -e
cd "$(dirname "$0")"
NOTO=${NOTO:-/usr/share/fonts/opentype/noto}
C="../../tools/convttf -t 0 -TA 10 -TD 20 -e 0x2E3A -e 0x2E3B"
# text: everything the font has in the Basic Multilingual Plane
$C -p 14 -s 32 -l 65535 -o newui-14.fnt     $NOTO/NotoSansCJK-Regular.ttc  # small: times, counts
$C -p 18 -s 32 -l 65535 -o newui-18.fnt     $NOTO/NotoSansCJK-Regular.ttc  # secondary: artist, values
$C -p 20 -s 32 -l 65535 -o newui-20bold.fnt $NOTO/NotoSansCJK-Bold.ttc     # rows, the UI font
$C -p 22 -s 32 -l 65535 -o newui-22bold.fnt $NOTO/NotoSansCJK-Bold.ttc     # titles
# big letter / numbers: ASCII only, which needs less room than Han
$C -TA 25 -TD 30 -p 40 -s 32 -l 126 -o newui-40bold.fnt $NOTO/NotoSansCJK-Bold.ttc
