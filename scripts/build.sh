#!/usr/bin/env bash
#
# build.sh — Build Akashic Vault for RPi 3 and/or RPi 4
#
#   ./scripts/build.sh 3      # build for Raspberry Pi 3   -> build/kernel8.img
#   ./scripts/build.sh 4      # build for Raspberry Pi 4   -> build/kernel8-rpi4.img
#   ./scripts/build.sh all    # build both (default)
#
# Circle names the kernel per target, so the two outputs have different names
# (kernel8.img vs kernel8-rpi4.img) and can sit on the SAME SD card — it will
# boot correctly on either a Pi 3 or a Pi 4. See scripts/deploy-sdcard.sh.
#
# Prereqs (on your Mac):
#   - ARM bare-metal toolchain on PATH (aarch64-none-elf-gcc, v15.2+)
#   - submodules present:  git submodule update --init --recursive
#
set -euo pipefail

# ── Locate project root (this script lives in <root>/scripts) ────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT="AkashicVault"

# ── Toolchain ────────────────────────────────────────────────────────────────
# Override by exporting TOOLCHAIN_BIN before calling, e.g.
#   export TOOLCHAIN_BIN=/Applications/ArmGNUToolchain/15.2.rel1/aarch64-none-elf/bin
: "${TOOLCHAIN_BIN:=/Applications/ArmGNUToolchain/15.2.rel1/aarch64-none-elf/bin}"
if [ -d "$TOOLCHAIN_BIN" ]; then
  export PATH="$TOOLCHAIN_BIN:$PATH"
fi
if ! command -v aarch64-none-elf-gcc >/dev/null 2>&1; then
  echo "ERROR: aarch64-none-elf-gcc not found on PATH."
  echo "       Set TOOLCHAIN_BIN to your ARM bare-metal toolchain bin/ dir."
  exit 1
fi

TARGET="${1:-all}"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

# ── Ensure GNU getopt (circle-stdlib's ./configure uses `getopt --long …`) ───
# macOS ships BSD getopt, which ignores --long, so --prefix never registers and
# the build fails with: 'Invalid toolchain prefix ... TOOLPREFIX is arm-none-eabi-'.
# GNU "enhanced" getopt returns exit code 4 to `--test`; BSD getopt does not.
# NOTE: that 4 must be captured (|| rc=$?) so `set -e` doesn't kill us here.
_is_gnu_getopt() {
  local g="$1" rc=0
  "$g" --test >/dev/null 2>&1 || rc=$?
  [ "$rc" -eq 4 ]
}
ensure_gnu_getopt() {
  if _is_gnu_getopt getopt; then
    echo "getopt: $(command -v getopt) (GNU enhanced)"
    return 0
  fi
  for d in /opt/homebrew/opt/gnu-getopt/bin /usr/local/opt/gnu-getopt/bin; do
    if [ -x "$d/getopt" ] && _is_gnu_getopt "$d/getopt"; then
      export PATH="$d:$PATH"
      echo "Using GNU getopt from $d"
      return 0
    fi
  done
  cat <<'EOF' >&2
ERROR: GNU getopt is required (circle-stdlib's ./configure uses getopt --long,
       which macOS's built-in BSD getopt does not support). Install it:

    brew install gnu-getopt

Then re-run ./scripts/build.sh — it picks up Homebrew's gnu-getopt automatically.
EOF
  exit 1
}
echo "== Akashic Vault build (target: $TARGET, jobs: $JOBS) =="
ensure_gnu_getopt

# ── Ensure bash 4+ (circle-stdlib's ./configure uses `mapfile`, a bash-4 builtin;
# macOS ships bash 3.2 as /bin/bash). We run configure through a modern bash. ──
BASH4=""
ensure_bash4() {
  for b in bash /opt/homebrew/bin/bash /usr/local/bin/bash; do
    local ver
    ver="$("$b" -c 'echo ${BASH_VERSINFO[0]}' 2>/dev/null || echo 0)"
    if [ "${ver:-0}" -ge 4 ] 2>/dev/null; then
      BASH4="$("$b" -c 'command -v bash' 2>/dev/null || echo "$b")"
      echo "bash for configure: $BASH4 (v$ver)"
      return 0
    fi
  done
  cat <<'EOF' >&2
ERROR: bash 4+ is required (circle-stdlib's ./configure uses `mapfile`, which
       macOS's built-in bash 3.2 lacks). Install a newer bash:

    brew install bash

Then re-run ./scripts/build.sh — it picks up Homebrew's bash automatically.
EOF
  exit 1
}
ensure_bash4

# ── Preflight: dependencies must be present before we stage/build ────────────
preflight() {
  local missing=0
  for dep in circle-stdlib CMSIS_5; do
    if [ ! -e "$ROOT/$dep/Makefile" ] && [ ! -e "$ROOT/$dep/configure" ] \
       && [ -z "$(ls -A "$ROOT/$dep" 2>/dev/null)" ]; then
      echo "ERROR: '$dep/' is missing or empty in the project."
      missing=1
    fi
  done
  if [ "$missing" = "1" ]; then
    cat <<EOF

The Circle OS sources aren't in this project yet. Get them one of two ways:

  (A) Git submodules (recommended — needs network + access to the ATM fork):
        ./scripts/setup-submodules.sh

  (B) Reuse the copy already checked out in a sibling project (offline):
        rsync -a "../AV-106/circle-stdlib/" "$ROOT/circle-stdlib/"
        rsync -a "../AV-106/CMSIS_5/"       "$ROOT/CMSIS_5/"

Then re-run: ./scripts/build.sh $TARGET
EOF
    exit 1
  fi
}
preflight

build_one() {
  local RPI="$1"
  echo ""
  echo "============================================================"
  echo "  Building Akashic Vault for Raspberry Pi ${RPI}  (AArch64)"
  echo "============================================================"

  # Stage to /tmp because the repo path contains spaces (breaks some tooling).
  local STAGE="/tmp/${PROJECT}_build_rpi${RPI}"
  rm -rf "$STAGE"
  mkdir -p "$STAGE"
  # NOTE: "/build/" is anchored to the project root so we only skip Akashic
  # Vault's own output dir — NOT nested build/ dirs like
  # circle-stdlib/build/circle-newlib (a tracked placeholder configure cd's into).
  rsync -a --delete --exclude=".git" --exclude="/build/" "$ROOT"/ "$STAGE"/

  local TOOLCHAIN_PREFIX="aarch64-none-elf-"

  # System options shared with the AV-106/Dreamdexed builds.
  local OPTIONS="-o SAVE_VFP_REGS_ON_IRQ -o REALTIME -o SCREEN_DMA_BURST_LENGTH=1 -o ARM_ALLOW_MULTI_CORE"
  if [ "$RPI" = "3" ]; then
    OPTIONS="$OPTIONS -o USE_SDHOST"     # RPi3 SD/Wi-Fi
  fi

  # ── 1. Build circle-stdlib for this target ─────────────────────────────────
  pushd "$STAGE/circle-stdlib" >/dev/null
    make mrproper || true
    # Run configure via bash 4+ (shebang is /bin/bash = 3.2 on macOS; needs mapfile).
    "$BASH4" ./configure -r "$RPI" --prefix "$TOOLCHAIN_PREFIX" $OPTIONS -o KERNEL_MAX_SIZE=0x400000
    make -j"$JOBS"
    # Circle addon libraries we depend on
    for addon in display gpio Properties sensor; do
      if [ -d "libs/circle/addon/$addon" ]; then
        ( cd "libs/circle/addon/$addon" && make clean || true && make -j"$JOBS" )
      fi
    done
    # LVGL addon (monochrome UI)
    if [ -d "libs/circle/addon/lvgl" ]; then
      ( cd "libs/circle/addon/lvgl" && make clean || true && make -j"$JOBS" )
    fi
  popd >/dev/null

  # ── 2. Generate menu tables from each module's menu.json ───────────────────
  python3 "$STAGE/scripts/gen_menus.py" --root "$STAGE" || {
    echo "WARN: gen_menus.py failed or no modules yet — continuing."; }

  # ── 3. Build the application ───────────────────────────────────────────────
  pushd "$STAGE/src" >/dev/null
    make clean
    rm -rf ./gcc-* || true
    make -j"$JOBS" RASPPI="$RPI" AARCH=64
  popd >/dev/null

  # ── 4. Collect the kernel image ────────────────────────────────────────────
  mkdir -p "$ROOT/build"
  local IMG
  if [ "$RPI" = "4" ]; then IMG="kernel8-rpi4.img"; else IMG="kernel8.img"; fi
  if [ ! -f "$STAGE/src/$IMG" ]; then
    echo "ERROR: expected $IMG was not produced for RPi${RPI}."
    echo "       Check the make output above for 'undefined reference' link errors."
    exit 1
  fi
  cp "$STAGE/src/$IMG" "$ROOT/build/$IMG"
  echo "  -> build/$IMG  ($(du -h "$ROOT/build/$IMG" | cut -f1))"
}

case "$TARGET" in
  3)   build_one 3 ;;
  4)   build_one 4 ;;
  all) build_one 3; build_one 4 ;;
  *)   echo "Usage: $0 <3|4|all>"; exit 1 ;;
esac

echo ""
echo "Done. Images in build/:"
ls -lh "$ROOT/build/"*.img 2>/dev/null || true
