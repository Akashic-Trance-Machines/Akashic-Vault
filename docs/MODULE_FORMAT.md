# Akashic Vault — Module Format

Every sound generator, audio FX, and MIDI FX is an independent **Git repository**,
included in the firmware as a **submodule** under `src/modules/<kind>/<name>/`
(`kind` = `generators` | `audiofx` | `midifx`). This keeps third-party DSP cores,
their licenses, and their credits self-contained and independently versioned.

## Module repo layout

```
schwung-<name>/                 (or akashic-<name>/)
├── core/                       Vendored upstream DSP source (unchanged)
│   └── LICENSE                 Upstream license, kept verbatim
├── src/
│   ├── <name>.cpp/.h           Akashic wrapper: implements ISoundGenerator /
│   │                           IAudioFX / IMidiFX, maps params to the core
├── menu.json                   Parameters + menu structure (source of truth)
├── CREDITS.md                  Who wrote the core, links, what was changed
├── LICENSE                     This module's license
└── README.md
```

## `menu.json`

Human-readable declaration of the module's identity, parameters, and menu tree.
It is the **single source of truth** for the UI. At build time a generator turns
it into C parameter tables (`TParamDesc[]`) and the menu tree (no runtime JSON
parsing — modules are compiled in).

### Schema (informal)

```jsonc
{
  "id": "braids",                 // unique, stable, lowercase
  "name": "Braids",               // display name
  "kind": "generator",            // generator | audiofx | midifx
  "version": 1,                   // menu.json schema version for this module
  "credits": "Mutable Instruments (Émilie Gillet); port: …",
  "license": "MIT",               // SPDX id of the core's license

  "params": [
    {
      "id": "timbre",             // stable id, used in presets
      "label": "Timbre",          // shown on the OLED row
      "type": "int",              // int | float | enum | bool
      "min": 0, "max": 127, "default": 64,
      "step": 1,
      "display": "raw"            // raw | percent | db | hz | note | semitones | ms
    },
    {
      "id": "shape",
      "label": "Shape",
      "type": "enum",
      "default": 0,
      "options": ["CSAW", "Morph", "Saw Square", "Sine Triangle", "Buzz"]
    },
    {
      "id": "color",
      "label": "Color",
      "type": "float",
      "min": 0.0, "max": 1.0, "default": 0.5,
      "display": "percent"
    }
  ],

  "menu": [
    { "page": "Main", "items": ["shape", "timbre", "color"] },
    { "page": "Envelope", "items": ["attack", "decay", "sustain", "release"] }
  ]
}
```

### Notes
- `id`s (module and param) are **stable** — presets reference them, so renaming a
  label is safe but renaming an id breaks saved presets.
- The 4-row UI renders `menu` pages; the **selector pattern** (see `UI_4ROW.md`) is
  expressed by putting a selector param first on a page.
- `display` controls how the value is formatted on the OLED (e.g. `db` → "-6 dB").
- Keep `params` flat and reference them by id from `menu` pages; this lets a param
  appear on more than one page and keeps preset serialization simple.

## Build-time codegen (recommended)

A script (e.g. `scripts/gen_menus.py`) walks every module's `menu.json` and emits
generated C (param tables + menu trees + `CModuleRegistry` entries). Run it as a
pre-build step in the Makefile. Benefits: humans edit JSON, the kernel ships
type-checked tables with zero parsing overhead, and adding a module is "add
submodule + regenerate".

## Licensing & attribution (required)

- Keep the upstream `LICENSE` and original file headers in `core/` **verbatim**.
- Fill in `CREDITS.md`: original author(s), project URL, license, and a summary of
  any changes made for the port.
- Add the module to the top-level `THIRD_PARTY_LICENSES.md`.
- Confirm GPL-3.0 compatibility before merging (the firmware as a whole is GPL-3.0).
