# Akashic Vault — TODO

## Input

### Encoder acceleration
When twisting faster, increment in larger steps so full-range sweeps don't require many rotations.

- Measure inter-detent interval per encoder using `CTimer::GetTicks()`
- Map interval → multiplier: `<20ms → 10×`, `20–80ms → 3×`, `>80ms → 1×`
- Apply in `C4RowMenu::EncoderDelta()` before min/max clamp
- Skip acceleration for enum params (e.g. engine selector) — single-step only
- Location: `src/kernel.cpp` PollInput() + `src/ui/c4rowmenu.cpp` EncoderDelta()

### Encoder pulse-per-step — PEC11R-4020F-S0024
Currently hardcoded `STEPS_PER_DETENT = 3` in `src/kernel.cpp`.
The PEC11R-4020F-S0024 has **24 detents/rev** — verify the correct quadrature
transition count per detent on hardware and make it configurable via `av-vault.ini`
(`EncoderPulsePerStep=` is already in the config, just not read at runtime yet).

- Read `EncoderPulsePerStep` from `av-vault.ini` at boot via `CPropertiesFatFsFile`
- Pass to `PollInput()` instead of the compile-time constant
- Document: PEC11R-4020F-S0024 = 24 detents, 2 pulses/detent → `EncoderPulsePerStep=2`
  (adjust if hardware testing shows otherwise)
