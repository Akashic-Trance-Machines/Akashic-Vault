//
// cengine.h
//
// Akashic Vault - Audio/MIDI engine: owns the active signal chain and runs the
//                 block render. Built to host N generators internally (multi-
//                 timbral hook); v1 UI exposes a single generator.
// Copyright (C) 2026  The Akashic Trance Machines Team
//
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include "isoundgenerator.h"
#include "iaudiofx.h"
#include "imidifx.h"
#include "tmidievent.h"

// Compile-time limits (tune in PLAN.md). Static sizing keeps the audio path
// allocation-free.
static constexpr unsigned MAX_GENERATORS = 8;	// multi-timbral hook
static constexpr unsigned NUM_FX_SLOTS	 = 3;	// confirmed: 3 audio FX slots
static constexpr unsigned MAX_MIDI_FX	 = 4;
static constexpr unsigned MAX_BLOCK	 = 256;	// confirmed: 48 kHz / 256 frames

enum class ClockSource : uint8_t { Internal, ExternalTRS, ExternalUSB };

class CEngine
{
public:
	CEngine ();
	~CEngine ();

	// Called once at boot. nSampleRate = 48000, nBlock <= MAX_BLOCK.
	void	Init (unsigned nSampleRate, unsigned nBlock);

	// ── Module wiring (UI / preset-load context, NOT the audio path) ──────────
	// v1: index 0 is the single active generator. Multi-timbral uses 1..N.
	bool	SetGenerator (unsigned nSlot, const char *pModuleId);
	// Direct injection — for pre-constructed instances (not owned by engine).
	void	SetGeneratorDirect (unsigned nSlot, ISoundGenerator *pGen);
	bool	SetAudioFX (unsigned nFxSlot, const char *pModuleId);	// 0..2
	void	SetAudioFXDirect (unsigned nFxSlot, IAudioFX *pFX);	// direct injection
	bool	AddMidiFX (const char *pModuleId);
	void	ClearMidiFX ();

	ISoundGenerator	*Generator (unsigned nSlot);
	IAudioFX	*AudioFX (unsigned nFxSlot);
	void		 SetFXBypass (unsigned nFxSlot, bool bBypass);

	// ── MIDI input (from TRS + USB routers) ──────────────────────────────────
	// Pushed events are run through the MIDI FX chain, then dispatched to the
	// generator(s) by channel. Lock-free queue read by the audio block.
	void	PushMidi (const TMidiEvent &Event);

	// ── Clock ─────────────────────────────────────────────────────────────────
	void	SetClockSource (ClockSource Source);
	void	SetTempo (float fBPM);

	// ── Audio render ──────────────────────────────────────────────────────────
	// THE real-time entry point, called from the I2S callback. Renders nFrames
	// of interleaved-by-pointer stereo. No allocation/locks/logging in here.
	void	Process (float *pOutL, float *pOutR, unsigned nFrames);

private:
	void	DrainMidi (unsigned nFrames);	// apply queued MIDI for this block
	void	AdvanceClock (unsigned nFrames);

private:
	unsigned	m_nSampleRate = 48000;
	unsigned	m_nBlock      = MAX_BLOCK;

	ISoundGenerator	*m_pGen[MAX_GENERATORS]   = {};
	uint8_t		 m_GenChannel[MAX_GENERATORS] = {};	// MIDI ch per gen
	unsigned	 m_nGenerators = 0;

	IAudioFX	*m_pFX[NUM_FX_SLOTS] = {};
	bool		 m_bFXBypass[NUM_FX_SLOTS] = {};

	IMidiFX		*m_pMidiFX[MAX_MIDI_FX] = {};
	unsigned	 m_nMidiFX = 0;

	ClockSource	 m_ClockSource = ClockSource::Internal;
	float		 m_fTempoBPM   = 120.0f;

	// Scratch mix buffers (static, no heap on the audio path).
	float		 m_MixL[MAX_BLOCK];
	float		 m_MixR[MAX_BLOCK];
};
