# Schwung — Architecture Analysis for Akashic Vault

> What "run the same or forked Schwung modules" actually means on Akashic Vault
> hardware. Read this before committing to an OS architecture.

## What Schwung is

Schwung (formerly "Move Everything") is an **unofficial framework for the Ableton
Move**. The Move runs **embedded Linux**. Schwung installs over SSH and adds a
"Shadow UI" signal-chain host that runs *alongside* Move's stock firmware. Modules
are installed from a Module Store and are mostly:

- C/C++ DSP cores (ports of existing engines) compiled as **native Linux binaries**, plus
- a **JavaScript module API** for UI/parameter glue, plus
- in some cases Linux-only runtimes (ffmpeg, yt-dlp, neural-net inference, RNBO,
  CLAP plugin hosting, networking/AirPlay).

Repo languages: ~72% C, ~19% JavaScript, ~5% C++, ~4% Shell. License MIT, but
individual modules bundle third-party engines under **varied licenses (incl. GPL)**.

## The core tension

| | Schwung / Ableton Move | AV-106 / Dreamdexed (current ATM stack) |
|---|---|---|
| OS | Embedded **Linux** | **Circle** bare-metal (no Linux, no POSIX) |
| Modules | Native binaries + JS API, dynamically loaded | Compiled into one `kernel8.img` |
| Deps | ffmpeg, yt-dlp, CLAP, RNBO, networking | newlib only; no dynamic loading |
| Input | Pad grid + jog + knobs | OLED + 5 encoders, MIDI-driven |

**Schwung module binaries will not run unmodified on bare-metal Circle.** They
assume a Linux process model, filesystem, dynamic linking, and (for several
modules) a network stack and external tools.

## What *is* portable

The thing worth reusing is the **DSP engine source** inside each sound/FX module —
most are clean, portable C/C++ that compile anywhere. Dreamdexed already proves
this in this very workspace: it ported **Dexed** (a Schwung sound generator) and
**CloudSeed** (a Schwung FX) into Circle by hand.

Sound generators whose cores are realistically portable to Circle:

- **Dexed** (FM) — already ported in Dreamdexed
- **OB-Xd** (virtual analog Oberheim)
- **Braids / Plaits** (Mutable Instruments macro oscillators)
- **Hera** (Juno-60), **HUSH ONE** (SH-101), **NuSaw**, **RaffoSynth** (Moog)
- **Chiptune** (NES/Game Boy)
- **FluidLite** (SF2) / **sfizz** (SFZ) — heavier, RAM/SD dependent

Portable FX cores: **CloudSeed** (done), tape delay, PSX verb, Junologue chorus,
gate, vocoder, utility, etc.

Modules that are **not** realistic on bare-metal Circle: Webstream, Radio Garden,
AirPlay, Stems, Time Stretch, NAM (neural), CLAP host, RNBO runner — these need
Linux + heavy/networked runtimes.

## Two viable paths (decide in PLAN)

1. **Circle bare-metal + ATM module framework** (continuation of AV-106).
   Port each Schwung DSP core behind a common `ISoundGenerator` / `IAudioFX` /
   `IMidiFX` interface. Pros: lowest latency, instant boot, matches existing stack
   and hardware drivers. Cons: every engine ported by hand; no JS-API or
   network/Linux modules; no dynamic plugin loading.

2. **Minimal Linux base** (e.g. Pi OS Lite / buildroot, optionally adapting
   Schwung itself). Pros: far closer to running Schwung modules as-is, dynamic
   loading possible. Cons: boot time, real-time audio tuning (PREEMPT_RT),
   bigger surface area, and the whole Dreamdexed UI/driver stack would need a
   Linux re-implementation.

Recommendation to discuss: **Path 1** for the core product (it leverages
everything already built and the no-Linux real-time guarantees), treating Schwung
as a *source of portable DSP cores and a reference UX*, not a binary runtime.
