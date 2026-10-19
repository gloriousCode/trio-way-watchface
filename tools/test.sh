#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/tests
node --test tests/*.test.js
for name in settings digits; do
  cc -std=c11 -Wall -Wextra -Werror -Itests/include -Isrc/c \
    "tests/${name}_test.c" "src/c/mdbl_${name}.c" -o "build/tests/${name}_test_plain"
  "build/tests/${name}_test_plain"
done
for name in geometry hybrid; do
  cc -std=c11 -Wall -Wextra -Werror -Itests/include -Isrc/c \
    "tests/${name}_test.c" "src/c/mdbl_${name}.c" src/c/mdbl_complication_layout.c -lm -o "build/tests/${name}_test_plain"
  "build/tests/${name}_test_plain"
done
for platform in EMERY GABBRO; do
  cc -std=c11 -Wall -Wextra -Werror -DPBL_PLATFORM_$platform -Itests/include -Isrc/c \
    tests/numbered_test.c src/c/mdbl_numbered.c src/c/mdbl_hybrid.c src/c/mdbl_complication_layout.c -lm \
    -o "build/tests/numbered_${platform}_test_plain"
  "build/tests/numbered_${platform}_test_plain"
done
for platform in EMERY GABBRO; do
  cc -std=c11 -Wall -Wextra -Werror -DPBL_PLATFORM_$platform -Itests/include -Isrc/c \
    tests/ticks_test.c src/c/mdbl_ticks.c -lm -o "build/tests/ticks_${platform}_test_plain"
  "build/tests/ticks_${platform}_test_plain"
done
cc -std=c11 -Wall -Wextra -Werror -Itests/include -Isrc/c tests/complication_layout_test.c \
  src/c/mdbl_complication_layout.c src/c/mdbl_geometry.c src/c/mdbl_hybrid.c src/c/mdbl_numbered.c src/c/mdbl_ticks.c \
  -lm -o build/tests/complication_layout_test_plain
build/tests/complication_layout_test_plain
cc -std=c11 -Wall -Wextra -Werror -Itests/include -Isrc/c tests/texture_test.c \
  src/c/mdbl_texture.c -lm -o build/tests/texture_test_plain
build/tests/texture_test_plain
python3 tests/emulator_test.py
python3 tests/capture_matrix_test.py
asset_python=${PEBBLE_ASSET_PYTHON:-python3}
if [ -z "${PEBBLE_ASSET_PYTHON:-}" ] && ! "$asset_python" -c 'import PIL' 2>/dev/null; then
  pebble_python="$HOME/.local/share/uv/tools/pebble-tool/bin/python"
  if [ -x "$pebble_python" ]; then
    asset_python=$pebble_python
  fi
fi
"$asset_python" tests/digit_assets_test.py
