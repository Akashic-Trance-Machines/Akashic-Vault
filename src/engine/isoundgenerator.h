//
// isoundgenerator.h
//
// Akashic Vault - Sound generator interface.
// Copyright (C) 2026  The Akashic Trance Machines Team
//
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include "imodule.h"
#include "tmidievent.h"

// A polyphonic (or mono) sound source. Driven entirely by incoming MIDI — there
// is no on-board keybed. The engine feeds it note/CC events, then asks it to
// render a block of stereo float audio.
class ISoundGenerator : public IModule
{
public:
	ModuleKind Kind () const override { return ModuleKind::Generator; }

	// MIDI input. Called outside the render, before Process() for the block.
	virtual void	NoteOn (uint8_t nNote, uint8_t nVelocity) = 0;
	virtual void	NoteOff (uint8_t nNote, uint8_t nVelocity) = 0;
	virtual void	ControlChange (uint8_t nCC, uint8_t nValue) = 0;
	virtual void	PitchBend (int nValue14) = 0;		// -8192..+8191
	virtual void	ChannelPressure (uint8_t nValue) = 0;
	virtual void	AllNotesOff () = 0;

	// Convenience dispatcher (default routes the common event types above).
	virtual void	HandleMidi (const TMidiEvent &Event);

	// Render nFrames of stereo audio. ADDITIVE is false: generators WRITE
	// (overwrite) their output. Real-time: no allocation/locks/logging.
	virtual void	Process (float *pOutL, float *pOutR, unsigned nFrames) = 0;

	// Optional: number of voices currently sounding (for UI/status). 0 if N/A.
	virtual unsigned ActiveVoices () const { return 0; }
};
