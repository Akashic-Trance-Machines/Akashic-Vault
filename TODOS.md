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
