#!/bin/sh
# Render every harness .yml to out/ as SVG + PNG + HTML + BOM TSV.
set -e
cd "$(dirname "$0")"
mkdir -p out
for f in *.yml; do
    [ -e "$f" ] || continue
    echo "==> $f"
    wireviz -f hpst -o out "$f"
done
echo "Done. Open out/*.html"
