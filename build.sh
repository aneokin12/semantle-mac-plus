#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

if [ ! -f "$ROOT_DIR/src/model_data.h" ]; then
    echo "Missing src/model_data.h; run ./tools/train_word2vec.sh first." >&2
    exit 2
fi

if [ -n "${RETRO68_TOOLCHAIN:-}" ]; then
    TOOLCHAIN_FILE=$RETRO68_TOOLCHAIN
elif [ -n "${RETRO68_PREFIX:-}" ]; then
    TOOLCHAIN_FILE=$RETRO68_PREFIX/m68k-apple-macos/cmake/retro68.toolchain.cmake
else
    echo "Set RETRO68_TOOLCHAIN or RETRO68_PREFIX to a Retro68 m68k installation." >&2
    echo "Example: nix develop github:autc04/Retro68#m68k" >&2
    exit 2
fi

if [ ! -f "$TOOLCHAIN_FILE" ]; then
    echo "Retro68 toolchain file not found: $TOOLCHAIN_FILE" >&2
    exit 2
fi

cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"
cmake --build "$ROOT_DIR/build" --parallel

echo "Classic Macintosh outputs are in $ROOT_DIR/build"
