//
// cmodrouter.cpp
//
// Akashic Vault — Mod router implementation.
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#include "cmodrouter.h"
#include "../modules/generators/plaits/src/plaits_generator.h"

CModRouter::CModRouter ()
{
	for (unsigned i = 0; i < NUM_LFOS; i++) m_LFOTarget[i] = ModTarget::None;
	for (unsigned i = 0; i < NUM_ENVS; i++) m_EnvTarget[i] = ModTarget::None;
}

void CModRouter::Update (uint32_t nNowUs, float fBPM, CPlaitsGenerator *pPlaits)
{
	// Advance all sources and accumulate per-target values.
	float fMod[kNumModTargets] = {};

	for (unsigned i = 0; i < NUM_LFOS; i++)
	{
		float v = m_LFO[i].Update (nNowUs, fBPM);
		unsigned t = (unsigned) m_LFOTarget[i];
		if (t > 0 && t < kNumModTargets)
			fMod[t] += v;
	}
	for (unsigned i = 0; i < NUM_ENVS; i++)
	{
		float v = m_Env[i].Update (nNowUs, fBPM);
		unsigned t = (unsigned) m_EnvTarget[i];
		if (t > 0 && t < kNumModTargets)
			fMod[t] += v;
	}

	if (!pPlaits) return;

	// Check whether any target actually has modulation.
	bool bAny = false;
	for (unsigned i = 1; i < kNumModTargets; i++)
		if (fMod[i] != 0.0f) { bAny = true; break; }

	if (!bAny)
	{
		pPlaits->ClearLiveModulations ();
		return;
	}

	pPlaits->SetLiveModulations (
		fMod[(unsigned) ModTarget::Timbre],
		fMod[(unsigned) ModTarget::Morph],
		fMod[(unsigned) ModTarget::Harmonics],
		fMod[(unsigned) ModTarget::FMAmt],
		fMod[(unsigned) ModTarget::LPGColour]
	);
}
