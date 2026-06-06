//
// iaudiofx.h
//
// Akashic Vault - Audio effect interface.
// Copyright (C) 2026  The Akashic Trance Machines Team
//
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include "imodule.h"

// An in-place stereo audio effect occupying one of the 3 FX slots. Processed in
// order after the sound generator: gen -> FX1 -> FX2 -> FX3 -> output.
class IAudioFX : public IModule
{
public:
	ModuleKind Kind () const override { return ModuleKind::AudioFX; }

	// Process the block IN PLACE. Real-time: no allocation/locks/logging.
	// When bypassed the engine skips this call entirely, so implementations
	// need not check bypass themselves.
	virtual void	Process (float *pIoL, float *pIoR, unsigned nFrames) = 0;

	// Tail length in frames (e.g. reverb/delay) so the engine can keep
	// processing silence after notes stop. 0 = no tail.
	virtual unsigned TailFrames () const { return 0; }
};
