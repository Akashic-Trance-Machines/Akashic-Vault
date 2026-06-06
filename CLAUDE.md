# CLAUDE.md — Akashic Vault

Guidance for AI agents (and humans) working in this repo. Read this fully before
making changes. Keep it up to date when conventions change.

---

## 1. What this project is

**Akashic Vault** is a bare-metal DSP **MIDI sound module** for Akashic Vault
hardware on Raspberry Pi 3/4. It runs on **Circle** (no Linux, no POSIX userland).
The build produces a single `kernel8.img` loaded by the RPi bootloader.

This is a **clean-from-scratch** codebase. AV-106/Dreamdexed are *reference only*
for hardware behaviour and the UI/UX — do not copy their source. The display layer
uses **LVGL** (Circle's `addon/lvgl`, v9.4.0) in monochrome mode, **not** u8g2.

Signal flow:

```
MIDI In (TRS + USB) ─▶ MIDI router ─▶ [MIDI FX] ─▶ Sound Generator ─▶ [Audio FX 1..3] ─▶ I2S out
                     └▶ MIDI Out / Thru
```

The user selects one sound generator, chains 2–3 audio FX and one or more MIDI FX,
and saves presets to the SD card. UI is the **4-row OLED menu** (1 nav encoder + 4
value encoders), inherited from Dreamdexed/AV-106.

**Read these before coding:**
`docs/HARDWARE_PLATFORM.md` (pins, OLED, encoders, audio/MIDI),
`docs/UI_4ROW.md` (the UI spec), `docs/SCHWUNG_NOTES.md` (what porting Schwung
means), `PLAN.md` (roadmap and locked decisions).

---

## 2. Golden rules (read first, they prevent crashes)

1. **No Linux/POSIX.** No `std::thread`, no `fork`, no raw filesystem paths — use
   Circle's FatFs for SD access. No dynamic plugin loading; modules are compiled in.
2. **The audio callback is real-time and sacred.** In the audio block path:
   **no heap allocation**, no locks, no logging, no SD/I2C, no blocking calls,
   no exceptions. Pre-allocate everything at init.
3. **No `CTimer`, I2C, or display calls during static/global construction** — the
   timer and buses are not initialized yet. Do this work in an `Initialize()` method
   called from the kernel, not in constructors of static objects.
4. **Never modify submodules in place.** Fork and repoint `.gitmodules`.
5. **Always `make clean` before a build**, and **always build in `/tmp`** (the repo
   path contains spaces). Verify the kernel size changed — identical size means your
   code wasn't actually included.
6. **`--gc-sections` is on.** Unreferenced code is stripped; new files only ship if
   something reachable from `main()` calls them.

---

## 3. Repository layout

| Path | Contents | Modify? |
|------|----------|:------:|
| `src/` | All Akashic Vault application code | ✅ |
| `src/modules/generators/<name>/` | Sound-generator modules — **each its own Git submodule** | ✅ (in its repo) |
| `src/modules/audiofx/<name>/` | Audio-FX modules — **each its own Git submodule** | ✅ (in its repo) |
| `src/modules/midifx/<name>/` | MIDI-FX modules — **each its own Git submodule** | ✅ (in its repo) |
| `src/generated/` | Codegen output from each module's `menu.json` (param tables, menu trees) | ⚠️ generated |
| `scripts/gen_menus.py` | Build-time `menu.json` → C generator | ✅ |
| `src/engine/` | `CEngine`, parameter model, voice/note routing | ✅ |
| `src/ui/` | 4-row UI, menus, screen layout, LVGL glue | ✅ |
| `src/ui/lv_port/` | LVGL display/input port: mono flush, encoder input | ✅ |
| `src/platform/` | App glue: MIDI in/out/thru, I2S audio, SD/config | ✅ |
| `circle-stdlib/` | Circle OS (submodule, **upstream smuehlst/circle-stdlib**) | ❌ |
| `circle-stdlib/libs/circle/` | Circle, **repointed to the ATM fork** (`Akashic-Trance-Machines/circle`) — carries the SSD1309 + MCP23017 drivers | ❌ |
| `.../addon/display/`, `.../addon/gpio/`, `.../addon/lvgl/` | `CSSD1309Display`, `CMCP23017`, `CLVGL` (v9.4.0) | ❌ |
| `CMSIS_5/` | ARM DSP intrinsics (submodule) | ❌ |
| `docs/` | Hardware/UI/architecture specs | ✅ |
| `sdcard/` | Generated SD content | ❌ gitignored |

Run `git submodule update --init --recursive` after cloning.

---

## 4. Build & deploy

### Toolchain
- **`aarch64-none-elf-gcc`** (ARM GNU bare-metal toolchain, v15.2+) — **not** the
  Linux `aarch64-linux-gnu` toolchain.
- macOS path: `/Applications/ArmGNUToolchain/15.2.rel1/aarch64-none-elf/bin`

### Workflow
```bash
export PATH="/Applications/ArmGNUToolchain/15.2.rel1/aarch64-none-elf/bin:$PATH"
export RPI=4                      # primary target; use RPI=3 for the RPi3 build

# Build from a /tmp staging copy (repo path has spaces)
rm -rf /tmp/AkashicVault_build
rsync -a --delete --exclude=".git" . /tmp/AkashicVault_build/
cd /tmp/AkashicVault_build/src && make clean && make -j$(sysctl -n hw.ncpu)

# Inspect the tail of the build for `undefined reference` — link errors can be
# swallowed by wrapper scripts. Always confirm a fresh kernel8.img was produced.
ls -la /tmp/AkashicVault_build/src/kernel8.img

# Deploy to the SD card volume
./deploy-sdcard.sh
```

### Build the matrix before merging
Build **both** `RPI=4` and `RPI=3` for any change to the engine, audio path, or
drivers — RPi3 is the headroom-constrained target.

---

## 5. Coding conventions

C++17 (C++20 features only if the toolchain confirms support). Match the existing
Dreamdexed/AV-106 style so ported code stays consistent.

### Formatting (`.clang-format`, enforced)
- **Tabs**, width 8. **Allman** braces (opening brace on its own line).
- `ColumnLimit: 0` (no hard wrap). Pointers right-aligned: `int *p`.
- Run `clang-format` before committing.

### Naming
| Element | Convention | Example |
|---------|-----------|---------|
| Classes | `C` + PascalCase | `CEngine`, `CSoundGenerator` |
| Interfaces | `I` + PascalCase | `ISoundGenerator`, `IAudioFX` |
| Structs (POD) | `T` + PascalCase | `TParamDesc`, `TMidiEvent` |
| Enums | PascalCase members | `ParamType::Continuous` |
| Members | `m_` + type hint | `m_pEngine`, `m_nVoices`, `m_bBypass` |
| Type hints | `p`=ptr, `n`=numeric, `b`=bool, `s`=string, `f`=float | `m_fCutoff` |
| Statics | `s_` prefix | `s_nLastTick` |
| Constants | `UPPER_SNAKE` or `constexpr` | `MAX_FX_SLOTS` |
| Header guard | `#pragma once` | — |

File names lowercase: `cengine.cpp` / `isoundgenerator.h`. Every source file starts
with a short header comment (filename, one-line purpose, copyright/license).

### Modern C++, used carefully
- Prefer `constexpr`, `enum class`, `std::clamp`, `std::array`, `gsl`-style spans
  over macros and raw C idioms — **except** in the audio loop, where you avoid
  anything that allocates or throws.
- No exceptions and no RTTI in the audio/IRQ path (and likely project-wide; confirm
  Circle build flags). Prefer return codes / `bool`.
- Avoid `std::function` and virtual dispatch *inside* the per-sample inner loop;
  resolve the module once per block, then run a tight loop.

---

## 6. Bare-metal & real-time constraints

- **RAM**: RPi3 = 1 GB, RPi4 = 1–8 GB. Kernel loads at `0x80000`. Budget audio
  buffers and voice state statically; never grow them at runtime.
- **No FPU in IRQ context** unless Circle enables it for that path — audio runs in a
  dedicated callback, not a raw IRQ; keep IRQ handlers integer-only.
- **Denormals**: flush-to-zero on the audio path (set FPCR FZ/DN, or guard feedback
  paths) to avoid CPU spikes in reverbs/filters.
- **Block processing**: process audio in blocks (`nFrames`), not sample-by-sample
  across module boundaries. One generator pass, then each FX pass, over the block.
- **No GPU**: all OLED rendering is CPU-side.
- **Timing**: use Circle's `CTimer`/`CScheduler` primitives; never busy-wait in the
  audio thread.

---

## 7. Audio engine & module interfaces

The stable interfaces in `src/engine/` are the project's backbone. Keep them small
and uniform — the UI, presets, and MIDI-learn all rely on the shared parameter model.

- `IModule`: identity (`Id()`, `Name()`), parameter enumeration
  (`NumParams()`, `ParamDesc(i)`, `GetParam(i)`, `SetParam(i, v)`), and
  `Serialize()/Deserialize()`.
- `ISoundGenerator : IModule`: `NoteOn/NoteOff/CC/PitchBend/...`,
  `Process(float* outL, float* outR, unsigned nFrames)`.
- `IAudioFX : IModule`: `Process(float* ioL, float* ioR, unsigned nFrames)`,
  bypass.
- `IMidiFX : IModule`: `Process(const TMidiEvent&, TMidiEvent* out, unsigned maxOut)`,
  clock/tempo aware.
- `CModuleRegistry`: id → factory, so the UI lists and instantiates modules.

**Modules live in their own repos.** Each generator/FX is a standalone Git
repository added as a **submodule** under `src/modules/<kind>/<name>/`. Its layout,
the `menu.json` schema, and the credits/license requirements are defined in
`docs/MODULE_FORMAT.md`. The upstream DSP core is vendored **unchanged** (original
license header preserved) inside the module repo's `core/`; the Akashic wrapper
adapts it to the interface.

**Menus are declared in JSON, compiled at build time.** Each module ships a
`menu.json` (params + menu/sub-page tree) as the human-readable source of truth.
`scripts/gen_menus.py` turns every `menu.json` into C (`TParamDesc[]` + menu trees +
registry entries) into `src/generated/` as a pre-build step — **no runtime JSON
parsing** (modules are compiled in). Edit `menu.json`, regenerate; never hand-edit
`src/generated/`. Param `id`s are stable (presets reference them).

---

## 8. UI rules (Circle CLVGL monochrome + 4-row)

The display is driven by **Circle's `CLVGL` addon** rendering onto
**`CSSD1309Display`** (a `CDisplay`-derived graphics driver in the ATM circle fork).
Do **not** hand-roll an LVGL display/flush port — Circle already does it correctly:

- `CSSD1309Display::GetDepth()` returns **1**, so `CLVGL::Initialize()` automatically
  sets `LV_COLOR_FORMAT_I1` (1-bit). `CLVGL::DisplayFlush()` skips the 8-byte I1
  palette and calls `CDisplay::SetArea()`, which packs into the panel's page
  framebuffer. The whole monochrome conversion is handled for you.
- Wiring (see `src/kernel.cpp`): construct `CSSD1309Display(&i2c, resetGPIO, addr)`,
  then `CLVGL gui(&display)`; call `display.Initialize()` then `gui.Initialize()`;
  build the UI with normal LVGL v9 calls on `lv_screen_active()`; call
  `gui.Update()` from the **main loop** (never an IRQ).
- LVGL config (`lv_conf.h`, mono theme, I1 support) comes from the **addon**, not
  this repo — don't add a competing `lv_conf.h`.
- Use the **v9 API** (`lv_display_*`, `lv_label_create`, …). The old LVGL OLED blog
  tutorial (`set_px_cb`/`lv_disp_drv_t`/`rounder_cb`) is v6/v7 — ignore it.

### The 4-row UX on LVGL
- `docs/UI_4ROW.md` and `docs/HARDWARE_PLATFORM.md` describe the **visual target**
  (pixel layout, scrollbar, action ▶ icons, fonts). Reproduce that look with LVGL;
  the pixel tables are the spec, LVGL is the implementation.
- Build the screen as four fixed row objects + a scrollbar object, or draw rows on
  an `lv_canvas` if you want pixel-exact control. Either is fine; match the spec.
- **Input mapping is custom, not standard LVGL group navigation.** The 4 value
  encoders map *directly* to the 4 visible rows (encoder N → row N), independent of
  focus — so route them yourself to the UI/parameter model rather than relying on an
  LVGL encoder group. The nav encoder drives scroll / back / enter. You may still
  feed the nav encoder through an `lv_indev` of type `LV_INDEV_TYPE_ENCODER` if it
  simplifies focus, but value-encoder→row mapping stays explicit.
- Encoder/button reads come from the MCP23017 driver (see hardware doc,
  `4RowNavMode=encoder`).

---

## 9. Configuration & presets

- Boot config lives in an INI on the SD card (`av-vault.ini` style), read via
  Circle's `CPropertiesFatFsFile`. Add a getter in `config.h/.cpp` and a default in
  the INI for each new property; prefix UI pin keys consistently (see hardware doc).
- **Presets**: a versioned snapshot of generator id + params, FX slots + params,
  MIDI FX, and global MIDI/routing. Always write a format-version byte in the header
  for forward compatibility. See `PLAN.md` open items for the chosen on-disk format.

---

## 10. Testing & debugging

- **No emulator** — verify on real hardware: build → deploy → check HDMI/serial
  kernel log + OLED + audio.
- **Host-side unit tests** are encouraged for pure DSP/parameter logic: compile the
  module core + wrapper with the host compiler (no Circle) and test buffers/params
  with deterministic input. Keep these in `tests/` and out of the kernel build.
- Logging via Circle (`LOGNOTE/LOGWARN/LOGERR/LOGPANIC`) — **never in the audio
  callback**.
- **Bisect boot crashes** incrementally: add one change, build, deploy, test;
  confirm kernel size changed each step.

### Common pitfalls
| Symptom | Cause | Fix |
|---------|-------|-----|
| Build "succeeds" but kernel unchanged | Missing `.c/.cpp` for an `.o` in Makefile | Verify sources exist; check kernel size |
| Display blank at boot | `CSSD1309Display`/`CLVGL` not initialized, or OLED reset GPIO/addr wrong | Check `Initialize()` return + reset pin (GPIO23) + I2C addr (0x3C) |
| Link error: `CSSD1309Display`/`CMCP23017` undefined | `libs/circle` not on the ATM fork branch with the drivers | Re-run `setup-submodules.sh` (sets `CIRCLE_BRANCH`) |
| Crash calling LVGL from IRQ | LVGL APIs aren't reentrant | Call `m_GUI.Update()` only from the main loop |
| Glitches/dropouts under load | Allocation/lock/log in audio path | Move work out of the callback; pre-allocate |
| Reverb/filter CPU spikes | Denormals | Enable flush-to-zero |
| Link error invisible | Wrapper script hides it | Read `make` tail for `undefined reference` |
| Your new module code never runs | `--gc-sections` stripped it | Ensure it's reachable from `main()` |

---

## 11. Adding a module (checklist)

1. Create the module's **own Git repo** following `docs/MODULE_FORMAT.md`: vendor
   the upstream core in `core/` unchanged (keep its license header), add the
   wrapper in `src/`, a `menu.json`, a `LICENSE`, and a `CREDITS.md`.
2. Add it as a **submodule** under `src/modules/<generators|audiofx|midifx>/<name>/`.
3. Write the wrapper implementing the matching interface (`ISoundGenerator` /
   `IAudioFX` / `IMidiFX`); map the core's parameters to the `menu.json` ids.
4. Regenerate tables: run `scripts/gen_menus.py` (or `make gen`) — this populates
   `src/generated/` and the `CModuleRegistry` entry. Don't hand-edit generated code.
5. Add the module's `.o` targets to the Makefile.
6. **Record the source in `THIRD_PARTY_LICENSES.md`** and confirm GPL-3.0
   compatibility.
7. Build `RPI=4` **and** `RPI=3`; confirm fresh kernel + size delta.
8. Deploy; verify sound/params on hardware; check CPU headroom on RPi3.
9. Add/extend a host-side DSP test if practical.

---

## 12. Git workflow

- Conventional commits: `feat:`, `fix:`, `refactor:`, `docs:`, `perf:`, `build:`.
- **Do not commit**: `*.o`, `*.d`, `*.elf`, `*.img`, `*.map`, `sdcard/`, `.DS_Store`,
  agent config dirs, temporary test harnesses.
- One logical change per commit; keep ported-core imports in their own commit
  separate from the wrapper so upstream updates stay reviewable.

---

## 13. Licensing & attribution (non-negotiable)

- **Akashic Vault is GPL-3.0** (`LICENSE`). New first-party files carry a GPL-3.0
  header.
- **Any copied or ported code must comply with its upstream license and credit the
  source.** Concretely: keep the original license text and per-file headers
  verbatim in the module's `core/`, fill in the module's `CREDITS.md` (author,
  URL, license, summary of changes), and add a row to the top-level
  `THIRD_PARTY_LICENSES.md`.
- **Verify GPL-3.0 compatibility before merging.** MIT/BSD/Apache-2.0 and GPL-3.0
  cores are fine; isolate or reimplement anything incompatible. When in doubt,
  stop and ask rather than vendoring.
