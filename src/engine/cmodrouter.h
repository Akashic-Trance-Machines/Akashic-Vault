//
// cmodrouter.h
//
// Akashic Vault — Mod router: 2 LFOs + 2 cyclic envelopes routed to Plaits
// parameters.  Updated from the main loop; applies offset modulation on top
// of the preset values via CPlaitsGenerator::SetLiveModulations().
//
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include "clfo.h"
#include "ccyclicenv.h"
#include <cstdint>

// Modulation targets (maps to Plaits patch fields).
enum class ModTarget : uint8_t
{
	None      = 0,
	Timbre,
	Morph,
	Harmonics,
	FMAmt,
	LPGColour,
	NUM_TARGETS
};

static constexpr unsigned kNumModTargets = (unsigned) ModTarget::NUM_TARGETS;

static constexpr const char *const kModTargetNames[] =
{
	"None", "Timbre", "Morph", "Harmonics", "FM Amt", "LPG Col"
};

class CPlaitsGenerator;

class CModRouter
{
public:
	static constexpr unsigned NUM_LFOS = 2;
	static constexpr unsigned NUM_ENVS = 2;

	CModRouter ();

	CLFO       &LFO (unsigned n)       { return m_LFO[n < NUM_LFOS ? n : 0]; }
	CCyclicEnv &Env (unsigned n)       { return m_Env[n < NUM_ENVS ? n : 0]; }

	ModTarget  GetLFOTarget (unsigned n) const { return n < NUM_LFOS ? m_LFOTarget[n] : ModTarget::None; }
	ModTarget  GetEnvTarget (unsigned n) const { return n < NUM_ENVS ? m_EnvTarget[n] : ModTarget::None; }
	void       SetLFOTarget (unsigned n, ModTarget t) { if (n < NUM_LFOS) m_LFOTarget[n] = t; }
	void       SetEnvTarget (unsigned n, ModTarget t) { if (n < NUM_ENVS) m_EnvTarget[n] = t; }

	// Advance all sources one tick and push mod values to Plaits.
	// Call from the main loop with CTimer::GetClockTicks() (microseconds) and
	// the current BPM (used by sources in sync mode).
	void Update (uint32_t nNowUs, float fBPM, CPlaitsGenerator *pPlaits);

private:
	CLFO		m_LFO[NUM_LFOS];
	CCyclicEnv	m_Env[NUM_ENVS];
	ModTarget	m_LFOTarget[NUM_LFOS];
	ModTarget	m_EnvTarget[NUM_ENVS];
};
