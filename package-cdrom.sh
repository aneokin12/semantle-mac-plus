#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR=${1:-"$ROOT_DIR/build"}
OUTPUT_DIR=${2:-"$ROOT_DIR/dist"}

APP="$BUILD_DIR/SemantlePlus.APPL"
if [ ! -f "$APP" ]; then
    echo "Expected $APP. Run ./build.sh first." >&2
    echo "The Retro68 .dsk output remains usable with an emulator." >&2
    exit 2
fi

mkdir -p "$OUTPUT_DIR"
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/semantle-plus-cd.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

IMAGE="$TMP_DIR/SemantlePlus.dmg"
MOUNT_POINT="$TMP_DIR/mount"
mkdir -p "$MOUNT_POINT"

HFORMAT=${HFORMAT:-$(command -v hformat 2>/dev/null || true)}
HCOPY=${HCOPY:-$(command -v hcopy 2>/dev/null || true)}
HUMOUNT=${HUMOUNT:-$(command -v humount 2>/dev/null || true)}

if [ -n "$HFORMAT" ] && [ -n "$HCOPY" ] && command -v hdiutil >/dev/null 2>&1; then
    # Build an Apple Partition Map with hdiutil, then replace its HFS+
    # partition with a genuine classic HFS volume made by hfsutils. The
    # resulting raw .cdr remains burnable while System 3 sees HFS.
    APM_IMAGE="$TMP_DIR/SemantlePlus-APM.dmg"
    APM_MASTER="$TMP_DIR/SemantlePlus-APM.cdr"
    PARTITION_BLOCKS=40896
    hdiutil create -ov -layout SPUD -fs HFS+ -size 20m \
        -volname "TEMPORARY" "$APM_IMAGE" >/dev/null
    hdiutil convert -ov -format UDTO -o "$TMP_DIR/SemantlePlus-APM" \
        "$APM_IMAGE" >/dev/null

    RAW_IMAGE="$TMP_DIR/SemantlePlus-CD.hfs"
    dd if=/dev/zero of="$RAW_IMAGE" bs=512 count="$PARTITION_BLOCKS" >/dev/null 2>&1
    "$HFORMAT" -l "SEMANTLE PLUS" "$RAW_IMAGE" 0 >/dev/null
    "$HCOPY" -m "$BUILD_DIR/SemantlePlus.bin" : >/dev/null
    if [ -n "$HUMOUNT" ]; then
        "$HUMOUNT" "$RAW_IMAGE" >/dev/null 2>&1 || true
    fi
    dd if="$RAW_IMAGE" of="$APM_MASTER" bs=512 seek=64 conv=notrunc \
        >/dev/null 2>&1
    cp "$APM_MASTER" "$OUTPUT_DIR/SemantlePlus.cdr"
    echo "Wrote $OUTPUT_DIR/SemantlePlus.cdr (classic HFS inside an Apple Partition Map)"
    echo "Burn that image at a low speed and use a System 3-compatible SCSI CD-ROM driver."
    exit 0
fi

# Older macOS releases can create and mount legacy HFS directly. Current
# releases reject it, which is why the hfsutils/APM path above is preferred.
if command -v hdiutil >/dev/null 2>&1 && hdiutil create -ov -layout SPUD -fs HFS -size 20m \
    -volname "SEMANTLE PLUS" "$IMAGE" >/dev/null 2>&1; then
    ATTACH_OUTPUT=$(hdiutil attach -nobrowse -owners on -mountpoint "$MOUNT_POINT" "$IMAGE")
    DEVICE=$(printf '%s\n' "$ATTACH_OUTPUT" | tail -1 | awk '{print $1}')
    ditto --rsrc "$APP" "$MOUNT_POINT/Semantle Plus"
    hdiutil detach "$DEVICE" >/dev/null
    hdiutil convert -ov -format UDTO -o "$OUTPUT_DIR/SemantlePlus" "$IMAGE" >/dev/null
    echo "Wrote $OUTPUT_DIR/SemantlePlus.cdr"
    echo "Burn that image at a low speed and use a System 3-compatible SCSI CD-ROM driver."
    exit 0
fi

echo "Could not create a classic HFS CD image." >&2
echo "Run this script inside: nix develop github:autc04/Retro68#m68k" >&2
exit 2
