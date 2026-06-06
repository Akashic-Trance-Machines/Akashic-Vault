//
// imidifx.h
//
// Akashic Vault - MIDI effect interface.
// Copyright (C) 2026  The Akashic Trance Machines Team
//
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include "imodule.h"
#include "tmidievent.h"

// A MIDI processor (arpeggiator, Euclidean sequencer, chord, etc.) that sits
// between MIDI input and the sound generator. Each input event may produce zero
// or more output events. Clock-aware modules also receive Tick().
class IMidiFX : public IModule
{
public:
	ModuleKind Kind () const override { return ModuleKind::MidiFX; }

	// Transform one input event into 0..nMaxOut output events written to pOut.
	// Returns the number of events written. Real-time-ish: called from the
	// MIDI/UI context, not the audio inner loop, but must not block.
	virtual unsigned Process (const TMidiEvent &In,
				  TMidiEvent *pOut, unsigned nMaxOut) = 0;

	// Advance internal timing. Called at the engine's clock resolution
	// (e.g. 24 PPQN ticks) with the current sample timestamp. Modules that
	// emit notes on the clock write them via the same pOut pattern on Tick().
	virtual unsigned Tick (uint32_t nTimeStamp,
			       TMidiEvent *pOut, unsigned nMaxOut)
	{
		(void)nTimeStamp; (void)pOut; (void)nMaxOut; return 0;
	}

	// Tempo update (BPM) for sync. Default: ignore.
	virtual void	SetTempo (float fBPM) { (void)fBPM; }
};
