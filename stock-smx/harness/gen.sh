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
for f in *.dot; do
    [ -e "$f" ] || continue
    echo "==> $f"
    dot -Tsvg "$f" -o "out/${f%.dot}.svg"
    dot -Tpng -Gdpi=100 "$f" -o "out/${f%.dot}.png"
done
echo "Done. Open out/*.html"
