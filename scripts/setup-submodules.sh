#!/usr/bin/env bash
#
# setup-submodules.sh — one-time: add circle-stdlib + CMSIS_5 as git submodules.
#
# Run this ONCE on a machine with network access and access to the ATM fork:
#   ./scripts/setup-submodules.sh
#
# Idempotent: safe to re-run. After it finishes, build with:
#   ./scripts/build.sh all
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

# URLs (also recorded in .gitmodules).
# circle-stdlib = public upstream (smuehlst). It pulls Circle into libs/circle.
CIRCLE_URL="https://github.com/smuehlst/circle-stdlib.git"
CMSIS_URL="https://github.com/ARM-software/CMSIS_5.git"

# The SSD1309 + MCP23017 drivers live in the ATM *circle* fork. circle-stdlib's
# libs/circle is repointed there, on a branch that contains BOTH drivers.
# Override via env:  CIRCLE_FORK=... CIRCLE_BRANCH=... ./scripts/setup-submodules.sh
CIRCLE_FORK="${CIRCLE_FORK:-https://github.com/Akashic-Trance-Machines/circle.git}"
CIRCLE_BRANCH="${CIRCLE_BRANCH:-akashic-vault}"

[ -d .git ] || { echo "Initialising git repo..."; git init -q; }

add_sub() {
  local url="$1" path="$2"
  if [ -e "$path/.git" ] || [ -f "$path/Makefile" ] || [ -f "$path/configure" ]; then
    echo "  $path already present — skipping add."
    return
  fi
  # Clean up any partial state left by a previous failed add/clone.
  git rm -f --cached "$path" 2>/dev/null || true
  rm -rf ".git/modules/$path" 2>/dev/null || true
  [ -d "$path" ] && rm -rf "$path"          # empty/partial dir from a failed clone
  echo "  Adding submodule $path ($url) ..."
  git submodule add --force "$url" "$path"
}

add_sub "$CIRCLE_URL" circle-stdlib
add_sub "$CMSIS_URL"  CMSIS_5

echo "Fetching submodule contents (recursive)..."
git submodule update --init --recursive

# ── Repoint libs/circle to the ATM fork branch with the SSD1309 + MCP23017 drivers ──
CIRCLE_DIR="circle-stdlib/libs/circle"
if [ -d "$CIRCLE_DIR" ]; then
  echo "Repointing $CIRCLE_DIR -> $CIRCLE_FORK ($CIRCLE_BRANCH) ..."
  (
    cd "$CIRCLE_DIR"
    git remote get-url atm >/dev/null 2>&1 || git remote add atm "$CIRCLE_FORK"
    git remote set-url atm "$CIRCLE_FORK"
    if git fetch atm "$CIRCLE_BRANCH" 2>/dev/null; then
      git checkout -B "$CIRCLE_BRANCH" "atm/$CIRCLE_BRANCH"
      echo "  libs/circle now on $CIRCLE_BRANCH (drivers present)."
    else
      cat <<EOF
  WARNING: branch '$CIRCLE_BRANCH' not found on the fork ($CIRCLE_FORK).
  The SSD1309 + MCP23017 drivers live on two separate feature branches
  (feature/ssd1309-display, feature/mcp23017-driver). Create an integration
  branch that contains BOTH and push it to the fork, then re-run this script.

  Easiest: do it in your standalone fork clone (where 'origin' is the fork and
  both branches already exist) — NOT here in libs/circle:

      cd /path/to/your/circle        # the fork clone, origin = $CIRCLE_FORK
      git fetch origin
      git checkout -b $CIRCLE_BRANCH origin/feature/ssd1309-display
      git merge --no-edit origin/feature/mcp23017-driver
      git push origin $CIRCLE_BRANCH

  (Verified clean merge: addon/display + addon/gpio don't overlap.)
EOF
    fi
  )
fi

echo ""
echo "Done. Next:  ./scripts/build.sh all"
