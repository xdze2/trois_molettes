#!/bin/bash
# Usage: img2web.sh [-s WIDTHx] [-r] <input_image>
# Outputs a web-optimized version alongside the original, with _web suffix.
# Options:
#   -s WIDTHx   resize width (default: 400x)
#   -r          remove original after conversion
set -e

size="400x"
remove=0
while getopts "s:r" opt; do
    case $opt in
        s) size="$OPTARG" ;;
        r) remove=1 ;;
        *) echo "Usage: img2web.sh [-s WIDTHx] [-r] <input_image>" >&2; exit 1 ;;
    esac
done
shift $((OPTIND - 1))

input="$1"
output="${input%.*}_web.${input##*.}"

convert "$input" -resize "$size" -colors 64 -dither Riemersma "$output"
echo "$output"

[ "$remove" -eq 1 ] && rm "$input"
