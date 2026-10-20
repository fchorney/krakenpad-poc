#!/bin/sh
# Build and run the host check of firmware/shared/panel_config.h, as C (the
# panel's language) and as C++ (the master's).
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

cc  -std=c11   -Wall -Wextra -Werror -I"$here/.." "$here/test_panel_config.c" -o "$out/test_c"
c++ -std=c++17 -Wall -Wextra -Werror -I"$here/.." -x c++ "$here/test_panel_config.c" -o "$out/test_cpp"

echo "C:";   "$out/test_c"
echo "C++:"; "$out/test_cpp"
