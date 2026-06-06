//
// isoundgenerator.cpp
//
// Akashic Vault - Default MIDI dispatch for sound generators.
// Copyright (C) 2026  The Akashic Trance Machines Team
//
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#include "isoundgenerator.h"

void ISoundGenerator::HandleMidi (const TMidiEvent &Event)
{
	switch (Event.Type)
	{
	case MidiType::NoteOn:
		if (Event.nData2 == 0)		// running-status note-off
			NoteOff (Event.nData1, 0);
		else
			NoteOn (Event.nData1, Event.nData2);
		break;

	case MidiType::NoteOff:
		NoteOff (Event.nData1, Event.nData2);
		break;

	case MidiType::ControlChange:
		if (Event.nData1 == 123)	// All Notes Off
			AllNotesOff ();
		else
			ControlChange (Event.nData1, Event.nData2);
		break;

	case MidiType::PitchBend:
		PitchBend (Event.PitchBend14 ());
		break;

	case MidiType::ChannelPressure:
		ChannelPressure (Event.nData1);
		break;

	default:
		break;			// ignore other types here
	}
}
