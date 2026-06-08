# Akashic Vault — TODO

## Input

### ✅ Encoder acceleration — DONE
Inter-detent interval measured via `CTimer::GetTicks()` in `PollInput()`.
Multiplier: `<20ms → 10×`, `20-50ms → 5×`, `50-150ms → 2×`, `>150ms → 1×`.
Enum/Bool params clamped to ±1 in `C4RowMenu::EncoderDelta()`.
Nav encoder never accelerated (scroll jumps would feel wrong).

### ✅ Encoder pulse-per-step PEC11R-4020F-S0024 — DONE
`STEPS_PER_DETENT = 2` in `src/kernel.cpp`.
`EncoderPulsePerStep=2` in `config/av-vault.ini`.
Reading from ini at runtime is a future improvement.

## AV-Plaits

### Polyphony
Add N `plaits::Voice` instances + stmlib `VoiceAllocator` for proper polyphony.
Currently monophonic — only one note sounds at a time.

## Phase 5 — MIDI FX

### ✅ MIDI FX chain wired into CEngine — DONE
`CEngine::PushMidi`/`RunMidiFXChain`/`DispatchToGenerators`/`Tick` route
input → `IMidiFX[0].Process` → ... → generators (by channel). Allocation-free
fixed-size ping-pong scratch buffers (`MAX_CHAIN_EVENTS = 8`). Clock-synced FX
(arp/sequencer) are advanced from the main loop via `CEngine::Tick(nNowUs)`,
which derives a sample-clock estimate from `CTimer::GetClockTicks()`.

### ✅ AV-Arp arpeggiator — DONE
New original module (GPL-3.0, no upstream core — "Super Arp" wasn't portable to
bare metal) at https://github.com/Akashic-Trance-Machines/AV-Arp, submodule
`src/modules/midifx/arp`. Modes: Up/Down/Up-Down/Random/As-Played; 1-4 octave
range; 6 rates (1/4 .. 1/32, incl. triplets); gate length 10-100%. Wired as
slot 0 (`m_Arp` in `CKernel`), menu page under MIDI FX → "Arp Settings".
Transparent passthrough when disabled.

### ⏳ Eucalypso (Euclidean sequencer) — TODO
Second Phase 5 MIDI FX target per `PLAN.md`. Currently a UI stub
(`MakeReadOnlyRow (&m_PageMidiFX, "Chord", "Off")`). Needs its own module repo
(`AV-Eucalypso`?) following the AV-Arp pattern (likely original implementation —
check whether a portable Euclidean-rhythm core exists worth vendoring first).

### ⏳ Global MIDI/routing page — TODO
Per `PLAN.md` Phase 5: input channel(s), TRS vs USB source selection, Out/Thru
routing, clock source (internal/external). Not yet built.
