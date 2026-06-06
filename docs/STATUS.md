# Akashic Vault — Project Status

> Handoff brief / current state. Read this first, then `CLAUDE.md` (conventions)
> and `PLAN.md` (roadmap). Last updated: Phase 0 complete.

## What this is

Bare-metal DSP **MIDI sound module** on Circle (RPi 3/4): pick a sound generator,
chain up to 3 audio FX + MIDI FX, save presets to SD. 128×64 OLED + 5 encoders
(1 nav + 4 value), TRS + USB MIDI in, I2S audio out. UI = the 4-row paradigm from
Dreamdexed/AV-106, rendered with LVGL.

## Locked decisions

- **OS:** Circle bare-metal (no Linux). Built from scratch (AV-106/Dreamdexed are
  reference only — no source copied).
- **Schwung:** port the portable C/C++ **DSP cores**, not the binaries (Schwung is
  a Linux/Ableton-Move framework). See `docs/SCHWUNG_NOTES.md`.
- **Hardware:** RPi4 primary, RPi3 supported. One SD card boots both
  (`kernel8.img` + `kernel8-rpi4.img`).
- **Voice model (v1):** single active generator → 3-slot audio-FX chain; `CEngine`
  is built to host N generators (multi-timbral hook) but the v1 UI exposes one.
- **Audio:** 48 kHz, stereo float, 256-frame blocks.
- **Presets:** versioned binary blob + human-readable text index.
- **MIDI clock:** internal or external (TRS/USB), selectable.
- **UI toolkit:** Circle's `CLVGL` addon rendering onto `CSSD1309Display`
  (auto monochrome I1 — no custom flush in this repo).
- **Modules:** each generator/FX/MIDI-FX is its **own git repo** (submodule under
  `src/modules/<kind>/<name>/`) with a `menu.json` that codegens param tables.
- **License:** GPL-3.0. Ported code keeps its license + credit
  (`THIRD_PARTY_LICENSES.md`, per-module `CREDITS.md`).

## Drivers (in the ATM circle fork, not this repo)

`CSSD1309Display` (`addon/display`) and `CMCP23017` (`addon/gpio`) live in
`Akashic-Trance-Machines/circle` on the **`akashic-vault`** branch (rebased onto
Circle Step51). `circle-stdlib`'s `libs/circle` is repointed there via
`setup-submodules.sh`.

## Done

- Repo scaffold, engine interfaces, `gen_menus.py`, build/deploy scripts, docs.
- **Circle ATM fork** rebased onto Step51; pushed to origin. Both driver commits
  (SSD1309 + MCP23017) cherry-picked cleanly.
- **Phase 0 — all 5 milestones verified on RPi3:**
  1. Boot + HDMI log banner ✅
  2. SSD1309 OLED via CLVGL — dark theme (black bg / white text) ✅
  3. MCP23017 — 4 value encoders + nav encoder (3 steps/detent) + clicks ✅
  4. MIDI in — TRS UART @31250 (IRQ-buffered `Read()`) + USB MIDI hot-plug ✅
  5. I2S audio — clean 440 Hz sine, no dropouts (`src/platform/ci2saudio.h/.cpp`) ✅

## Current state

**Phase 0 complete. Starting Phase 1.**

`kernel.cpp` is in smoke-test mode:
- OLED shows encoder positions, nav, MIDI events, serial/USB status.
- I2S plays 440 Hz sine — engine not yet wired to audio (`CI2SAudio::SetEngine()`
  not called; sine fallback active in `GetChunk()`).
- MIDI events received and pushed to `CEngine::PushMidi()` (engine queues them
  but no generator is loaded to render them yet).

## Immediate next actions

1. **First generator** — port **Braids/Plaits** or **Dexed** behind
   `ISoundGenerator` as a submodule under `src/modules/generators/<name>/`.
   Then call `m_I2SAudio.SetEngine(&m_Engine)` in `kernel.cpp`.
2. **4-row UI** — replace debug OLED with the real menu renderer
   (`docs/UI_4ROW.md`): scrollbar, 4 rows, encoder-to-row mapping,
   menu tree navigation.
3. **RPi4 hardware test** — `./scripts/build.sh 4`, deploy, verify all 5
   milestones on Pi4.

## macOS build gotchas (already handled in build.sh)

- Needs `brew install gnu-getopt bash` (BSD getopt + bash 3.2 break circle-stdlib's
  `configure`). `build.sh` auto-detects the Homebrew versions.
- Repo path has spaces → `build.sh` stages to `/tmp` before building.
- ARM toolchain expected at `/Applications/ArmGNUToolchain/15.2.rel1/...`; override
  with `TOOLCHAIN_BIN=...`.

## Key files

| Path | What |
|------|------|
| `CLAUDE.md` | Conventions, build rules, golden rules |
| `PLAN.md` | Phased roadmap + locked decisions |
| `docs/PHASE0.md` | Bring-up milestones (all done) |
| `docs/UI_4ROW.md` | 4-row UI spec — next to implement |
| `docs/MODULE_FORMAT.md` | Module repo layout + `menu.json` schema |
| `src/kernel.cpp` | Main kernel — smoke-test mode, all drivers wired |
| `src/platform/ci2saudio.h/.cpp` | I2S DMA audio device (subclasses Circle) |
| `src/engine/*` | Module interfaces + engine skeleton |
| `scripts/build.sh` | Dual-target build (RPi3/4) |
| `scripts/setup-submodules.sh` | circle-stdlib + CMSIS + libs/circle repoint |
