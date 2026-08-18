#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
WORK_DIR=${WORD2VEC_WORK_DIR:-"$ROOT_DIR/.word2vec-work"}
WORD2VEC_REPOSITORY=${WORD2VEC_REPOSITORY:-https://github.com/tmikolov/word2vec.git}
TEXT8_URL=${TEXT8_URL:-http://mattmahoney.net/dc/text8.zip}
DIMENSION=${WORD2VEC_DIMENSION:-50}
VOCAB_SIZE=${WORD2VEC_VOCAB_SIZE:-4096}
THREADS=${WORD2VEC_THREADS:-2}
MODEL_BIN="$WORK_DIR/vectors.bin"

if [ "$DIMENSION" -ne 50 ] || [ "$VOCAB_SIZE" -gt 4096 ] || [ "$VOCAB_SIZE" -lt 50 ]; then
    echo "Use 50 dimensions and a vocabulary between 50 and 4096 for the 1 MB target." >&2
    exit 2
fi

mkdir -p "$WORK_DIR"

if [ ! -x "$WORK_DIR/word2vec" ]; then
    if [ ! -d "$WORK_DIR/upstream/.git" ]; then
        git clone --depth 1 "$WORD2VEC_REPOSITORY" "$WORK_DIR/upstream"
    fi
    make -C "$WORK_DIR/upstream" word2vec \
        CFLAGS="-lm -pthread -O3 -march=native -Wall -funroll-loops -Wno-unused-result -Dfgetc_unlocked=getc_unlocked"
    cp "$WORK_DIR/upstream/word2vec" "$WORK_DIR/word2vec"
fi

if [ ! -f "$WORK_DIR/text8" ]; then
    if [ ! -f "$WORK_DIR/text8.zip" ]; then
        curl -L --fail --retry 3 "$TEXT8_URL" -o "$WORK_DIR/text8.zip"
    fi
    unzip -p "$WORK_DIR/text8.zip" text8 > "$WORK_DIR/text8"
fi

if [ "${FORCE_WORD2VEC_TRAIN:-0}" = "1" ] || [ ! -f "$MODEL_BIN" ]; then
    "$WORK_DIR/word2vec" \
        -train "$WORK_DIR/text8" \
        -output "$MODEL_BIN" \
        -size "$DIMENSION" \
        -window 5 \
        -sample 1e-4 \
        -negative 5 \
        -hs 0 \
        -cbow 1 \
        -min-count 5 \
        -iter 3 \
        -threads "$THREADS" \
        -binary 1
fi

python3 "$ROOT_DIR/tools/extract_word2vec.py" \
    --input "$MODEL_BIN" \
    --output "$ROOT_DIR/src/model_data.h" \
    --vocab-size "$VOCAB_SIZE" \
    --dim "$DIMENSION" \
    --max-word-length 23

echo "Model source: $MODEL_BIN"
echo "Runtime table: $ROOT_DIR/src/model_data.h"
