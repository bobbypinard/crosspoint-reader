#!/bin/bash
# Bakes the Rotunda theme chrome fonts (Inter, docs/rotunda/DESIGN.md "Type").
# Inter is subset to Latin-1 + common punctuation first so fontconvert's default
# intervals (Greek, Cyrillic, Vietnamese...) cannot pull extra glyphs into flash.
# Sizes are fontconvert points at 150 dpi: 7pt ~ 14px, 8pt ~ 17px, 11pt ~ 23px, 13pt ~ 27px.

set -e

cd "$(dirname "$0")"

SRC=../builtinFonts/source/Inter
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

UNICODES="U+0020-007E,U+00A0-00FF,U+0131,U+0152-0153,U+0160-0161,U+0178,U+017D-017E,U+2009,U+2013-2014,U+2018-201A,U+201C-201E,U+2020-2022,U+2026,U+2030,U+2032-2033,U+2039-203A,U+2212"
for weight in Medium SemiBold Bold; do
  pyftsubset "$SRC/Inter-$weight.ttf" --unicodes="$UNICODES" --layout-features='kern' --no-hinting \
    --output-file="$TMP/Inter-$weight.ttf"
done

convert() {
  local name=$1 size=$2 weight=$3
  python fontconvert.py "$name" "$size" "$TMP/Inter-$weight.ttf" > "../builtinFonts/$name.h"
  echo "Generated ../builtinFonts/$name.h"
}

convert rotunda_xs_regular 7 Medium
convert rotunda_xs_bold 7 Bold
convert rotunda_sm_regular 8 Medium
convert rotunda_sm_bold 8 Bold
convert rotunda_row_regular 11 SemiBold
convert rotunda_title_regular 13 Bold
