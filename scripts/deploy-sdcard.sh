#!/usr/bin/env bash
#
# deploy-sdcard.sh — Deploy Akashic Vault to an SD card
#
#   ./scripts/deploy-sdcard.sh [/Volumes/AKASHIC]
#
# Copies BOTH kernels (kernel8.img for Pi3, kernel8-rpi4.img for Pi4) plus the
# Circle-compatible boot firmware and config, so a single card boots on either
# board. Build first with: ./scripts/build.sh all
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

SDCARD="${1:-/Volumes/AKASHIC}"
BUILD_DIR="$ROOT/build"
BOOTFILES_DIR="/tmp/rpi-bootfiles-circle"

# Firmware commit THIS Circle version is pinned to — keep in sync with
# circle-stdlib/libs/circle/boot/Makefile (FIRMWARE ?= ...). The old
# AV-106-era commit (bc7f439c) does not boot Circle 51 on a Pi4.
CIRCLE_FW_COMMIT="0641c5bf25d185d5bf25d8dcafd1da6dc8fdbf68"
RPI_FW="https://github.com/raspberrypi/firmware/raw/$CIRCLE_FW_COMMIT/boot"

# RPi4 additionally needs start4.elf/fixup4.dat AND bcm2711-rpi-4-b.dtb —
# without the device tree the Pi4 firmware never starts a bare-metal 64-bit
# kernel (it hangs at the rainbow splash screen).
BOOT_FILES=(bootcode.bin start.elf fixup.dat start_cd.elf fixup_cd.dat \
            start4.elf fixup4.dat start4cd.elf fixup4cd.dat \
            bcm2711-rpi-4-b.dtb)

echo "Checking Circle-compatible firmware (commit ${CIRCLE_FW_COMMIT:0:8})..."
mkdir -p "$BOOTFILES_DIR"
for f in "${BOOT_FILES[@]}"; do
  if [ ! -f "$BOOTFILES_DIR/$f" ]; then
    echo "  Downloading $f ..."
    curl -fsSL -o "$BOOTFILES_DIR/$f" "$RPI_FW/$f" || echo "  (skipped $f)"
  fi
done

if [ ! -d "$SDCARD" ]; then
  echo "ERROR: SD card not found at $SDCARD"
  echo "  Mount it, or pass the path: ./scripts/deploy-sdcard.sh /Volumes/NAME"
  exit 1
fi

echo "Deploying to $SDCARD ..."

# RPi3 firmware auto-loads armstub8.bin if present; Circle doesn't use one.
[ -f "$SDCARD/armstub8.bin" ] && { echo "  Removing stale armstub8.bin"; rm -f "$SDCARD/armstub8.bin"; }

# Boot firmware
for f in "${BOOT_FILES[@]}"; do
  [ -f "$BOOTFILES_DIR/$f" ] && rsync -a "$BOOTFILES_DIR/$f" "$SDCARD/"
done

# Kernels (whichever were built)
[ -f "$BUILD_DIR/kernel8.img" ]      && rsync -a "$BUILD_DIR/kernel8.img"      "$SDCARD/"
[ -f "$BUILD_DIR/kernel8-rpi4.img" ] && rsync -a "$BUILD_DIR/kernel8-rpi4.img" "$SDCARD/"

# Config + Circle ARM stub for Pi4 (GIC setup; built from circle/boot/armstub)
rsync -a "$ROOT/config/config.txt"           "$SDCARD/"
rsync -a "$ROOT/config/av-vault.ini"         "$SDCARD/"
rsync -a "$ROOT/config/armstub8-rpi4.bin"    "$SDCARD/"

sync
echo ""
echo "SD card contents:"
ls -lh "$SDCARD/"
echo ""
echo "=== Deployment complete (Pi3 + Pi4 on one card) ==="
