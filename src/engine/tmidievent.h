//
// tmidievent.h
//
// Akashic Vault - Normalised MIDI event passed through the engine.
// Copyright (C) 2026  The Akashic Trance Machines Team
//
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include <cstdint>

enum class MidiType : uint8_t
{
	NoteOff		= 0x80,
	NoteOn		= 0x90,
	PolyAftertouch	= 0xA0,
	ControlChange	= 0xB0,
	ProgramChange	= 0xC0,
	ChannelPressure	= 0xD0,
	PitchBend	= 0xE0,
	// Realtime / clock (channel ignored)
	Clock		= 0xF8,
	Start		= 0xFA,
	Continue	= 0xFB,
	Stop		= 0xFC,
};

// A single channel-voice or realtime MIDI message in a compact form.
// Sources: TRS MIDI In (UART), USB MIDI In. See CEngine for routing.
struct TMidiEvent
{
	MidiType	Type;
	uint8_t		nChannel;	// 0..15
	uint8_t		nData1;		// note / cc number / program / bend LSB
	uint8_t		nData2;		// velocity / cc value / bend MSB
	uint32_t	nTimeStamp;	// sample-clock timestamp (for jitter-free timing)

	// Convenience accessors.
	int	PitchBend14() const	{ return ((int)nData2 << 7 | nData1) - 8192; } // -8192..+8191
};
