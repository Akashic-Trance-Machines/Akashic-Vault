//
// imodule.h
//
// Akashic Vault - Base interface common to all modules
//                 (sound generators, audio FX, MIDI FX).
// Copyright (C) 2026  The Akashic Trance Machines Team
//
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include "tparamdesc.h"
#include <cstddef>
#include <cstdint>

enum class ModuleKind : uint8_t { Generator, AudioFX, MidiFX };

// Common surface for every module. The parameter model defined here is what the
// 4-row UI, the preset system, and MIDI-learn all build on — so it is uniform
// across module kinds and never allocates on the audio path.
//
// REAL-TIME RULES for implementers:
//   * Process*() runs in the audio block callback: no heap, no locks, no logging,
//     no blocking I/O, no exceptions. Pre-allocate in Init().
//   * SetParam() may be called from the UI thread; keep it lock-free and cheap
//     (store into a value that the audio loop reads atomically / per-block).
class IModule
{
public:
	virtual ~IModule () = default;

	// Identity (matches menu.json "id"/"name").
	virtual const char	*Id () const = 0;
	virtual const char	*Name () const = 0;
	virtual ModuleKind	 Kind () const = 0;

	// One-time setup. Called once before any Process*(); may allocate here.
	// nSampleRate in Hz (48000), nMaxBlock = largest block size in frames (256).
	virtual void	Init (unsigned nSampleRate, unsigned nMaxBlock) = 0;

	// Reset internal DSP state (e.g. on preset load), no reallocation.
	virtual void	Reset () = 0;

	// Parameter model. Descriptors come from generated tables (menu.json).
	virtual unsigned		NumParams () const = 0;
	virtual const TParamDesc	&ParamDesc (unsigned nIndex) const = 0;
	virtual TParamValue		GetParam (unsigned nIndex) const = 0;
	virtual void			SetParam (unsigned nIndex, TParamValue Value) = 0;

	// Look up a parameter index by its stable id; returns -1 if not found.
	virtual int	FindParam (const char *pId) const = 0;

	// Preset serialisation: write/read this module's parameter block.
	// Return bytes written / read; pass nullptr buffer to query required size.
	virtual size_t	Serialize (uint8_t *pBuffer, size_t nCapacity) const = 0;
	virtual size_t	Deserialize (const uint8_t *pBuffer, size_t nLength) = 0;
};
