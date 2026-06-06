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

# Firmware commit Circle was tested against (matches AV-106/Dreamdexed).
CIRCLE_FW_COMMIT="bc7f439c234e19371115e07b57c366df59cc1bc7"
RPI_FW="https://github.com/raspberrypi/firmware/raw/$CIRCLE_FW_COMMIT/boot"

# RPi4 also needs start4.elf / fixup4.dat in addition to the Pi3 set.
BOOT_FILES=(bootcode.bin start.elf fixup.dat start_cd.elf fixup_cd.dat \
            start4.elf fixup4.dat start4cd.elf fixup4cd.dat)

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

# Config
rsync -a "$ROOT/config/config.txt"    "$SDCARD/"
rsync -a "$ROOT/config/av-vault.ini"  "$SDCARD/"

sync
echo ""
echo "SD card contents:"
ls -lh "$SDCARD/"
echo ""
echo "=== Deployment complete (Pi3 + Pi4 on one card) ==="
