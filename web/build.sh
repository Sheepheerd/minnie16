#!/usr/bin/env bash
# Builds the web frontend into build/web (or the directory given as $1).
# Needs emcc on the PATH, `nix develop` provides it.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$root/build/web}"

mkdir -p "$out/programs"

emcc -O2 -std=c11 -Wall -Wextra -I"$root/src" \
  "$root/src/cpu.c" \
  "$root/src/bus.c" \
  "$root/src/decoder.c" \
  "$root/src/simulator.c" \
  "$root/src/debug.c" \
  "$root/src/mmio.c" \
  "$root/src/web.c" \
  -sMODULARIZE=1 \
  -sEXPORT_NAME=createMinnie \
  -sENVIRONMENT=web,node \
  -sFORCE_FILESYSTEM=1 \
  -sEXPORTED_RUNTIME_METHODS=FS,HEAPU8,HEAPU16,UTF8ToString \
  -o "$out/minnie.js"

cp "$root/web/index.html" "$root/web/style.css" "$root/web/app.js" \
  "$root/web/asm.js" "$out/"
cp "$root"/programs/*.asm "$out/programs/"

# The page reads this to fill in the examples menu
(
  cd "$out/programs"
  printf '['
  sep=''
  for f in *.asm; do
    printf '%s"%s"' "$sep" "$f"
    sep=','
  done
  printf ']\n'
) > "$out/programs/index.json"

echo "Built $out"
