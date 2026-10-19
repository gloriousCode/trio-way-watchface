#!/bin/sh
# Build with an existing SDK when pebble-tool's SDK virtualenv is unavailable.
set -eu
cd "$(dirname "$0")/.."
sdk_root=${PEBBLE_SDK_ROOT:-"$HOME/Library/Application Support/Pebble SDK/SDKs/current"}
sdk_python=${PEBBLE_SDK_PYTHON:-"$HOME/.local/share/uv/tools/pebble-tool/bin/python"}
export PATH="$sdk_root/toolchain/arm-none-eabi/bin:$PATH"
export NODE_PATH="$sdk_root/node_modules"
export NOCLIMB=1
exec "$sdk_python" "$sdk_root/sdk-core/pebble/waf" configure build
