//
// cmodrouter.h
//
// Akashic Vault — Mod sources: 2 LFOs + 2 cyclic envelopes.
// Updated from the main loop; the raw source values are pushed to the sound
// generator, which pulls them into its own parameters according to its route
// params (pull model — routes live with the SG, so swapping generators never
// leaves dangling routings).
//
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include "clfo.h"
#include "ccyclicenv.h"
#include <cstdint>

class ISoundGenerator;

class CModRouter
{
public:
	static constexpr unsigned NUM_LFOS = 2;
	static constexpr unsigned NUM_ENVS = 2;

	CModRouter () {}

	CLFO       &LFO (unsigned n)	{ return m_LFO[n < NUM_LFOS ? n : 0]; }
	CCyclicEnv &Env (unsigned n)	{ return m_Env[n < NUM_ENVS ? n : 0]; }

	// Advance all sources one tick and push their values to the active SG.
	// Call from the main loop with CTimer::GetClockTicks() (microseconds) and
	// the current BPM (used by sources in sync mode). pGen may be any
	// ISoundGenerator (or nullptr) — the router is generator-agnostic.
	void Update (uint32_t nNowUs, float fBPM, ISoundGenerator *pGen);

private:
	CLFO		m_LFO[NUM_LFOS];
	CCyclicEnv	m_Env[NUM_ENVS];
};
