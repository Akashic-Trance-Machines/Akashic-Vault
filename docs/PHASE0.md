# Phase 0 — Bootstrap & Bring-up

Goal: a minimal kernel that boots on **both RPi 3 and RPi 4** and proves every
driver + the LVGL display port, before any synth code exists. Build from scratch —
AV-106/Dreamdexed are reference only.

## One-time setup (on your Mac)

```bash
# Prereqs on macOS:
#   - ARM bare-metal toolchain on PATH (aarch64-none-elf-gcc, v15.2+)
#   - GNU getopt + bash 4+ (circle-stdlib's ./configure uses `getopt --long` and
#     `mapfile`; macOS's BSD getopt and bash 3.2 both fail). build.sh auto-detects
#     the Homebrew versions:
brew install gnu-getopt bash

# Add the Circle sources once:
./scripts/setup-submodules.sh   # git init + circle-stdlib + CMSIS_5, repoint libs/circle
```

(Offline alternative: copy them from a sibling project —
`rsync -a ../AV-106/circle-stdlib/ ./circle-stdlib/ && rsync -a ../AV-106/CMSIS_5/ ./CMSIS_5/`.)

## Build & deploy

```bash
./scripts/build.sh all          # -> build/kernel8.img  +  build/kernel8-rpi4.img
./scripts/deploy-sdcard.sh /Volumes/AKASHIC
```

`build.sh 3` or `build.sh 4` build a single target. Both kernels coexist on one SD
card (Circle picks `kernel8.img` on a Pi3, `kernel8-rpi4.img` on a Pi4).

## Bring-up milestones (verify each on hardware)

1. **Boot + log** — kernel boots on Pi3 and Pi4; serial/HDMI shows the
   `Akashic Vault <version> starting` banner.
2. **OLED via CLVGL** — `CSSD1309Display(&i2c, GPIO23, 0x3C)` →
   `CLVGL gui(&display)`; `Initialize()` both; draw a "hello" label on
   `lv_screen_active()`; `gui.Update()` in the loop. Circle handles the I1/mono
   conversion automatically (depth==1). *(Scaffolded in `kernel.cpp`.)*
3. **Input** — `CMCP23017` reads all 5 encoders (1 nav + 4 value) + clicks;
   log deltas. Verify quadrature direction and detent stepping (3/detent).
4. **MIDI in** — TRS UART @31250 and USB MIDI both deliver note/CC events into
   `CEngine::PushMidi`; log them.
5. **Audio out** — I2S sound device at 48 kHz / 256-frame blocks calls
   `CEngine::Process`; load a temporary sine-test generator and confirm a clean
   tone (both boards, no underruns — watch the Pi3 especially).

When all five pass, the platform is proven and Phase 1 (engine + first real
generator) begins.

## What's scaffolded now

| File | State |
|------|-------|
| `scripts/build.sh`, `deploy-sdcard.sh` | ✅ dual-target build/deploy |
| `scripts/gen_menus.py` | ✅ menu.json → C codegen (tested) |
| `src/engine/*.h`, `cengine.cpp`, `cmoduleregistry.cpp`, `isoundgenerator.cpp` | ✅ interfaces + engine skeleton (host-compiled clean) |
| `src/kernel.*`, `src/main.cpp` | ✅ milestone 2 wired (CSSD1309Display + CLVGL + CMCP23017); MIDI/I2S are TODO (milestones 4–5) |
| `src/Makefile` | ✅ links addon/display, addon/gpio, addon/lvgl (no custom LVGL port) |
| `config/config.txt`, `av-vault.ini` | ✅ boot + runtime config |

LVGL has **no** custom port in this repo — Circle's `CLVGL` addon renders onto
`CSSD1309Display` and does the monochrome (I1) conversion itself.

## Notes / things to confirm against the real toolchain

- `libs/circle` must be on the ATM-fork branch that contains **both** the SSD1309
  and MCP23017 drivers. They currently live on two separate feature branches; make
  an integration branch (`setup-submodules.sh` prints the exact commands) and set
  `CIRCLE_BRANCH` to it.
- The `src/Makefile` `LIBS` list (incl. `addon/display`, `addon/gpio`,
  `addon/lvgl`) should be sanity-checked against the vendored circle version.
- These projects build inside `/tmp` (the repo path has spaces); `build.sh`
  already stages there.
