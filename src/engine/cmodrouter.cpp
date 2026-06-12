//
// cmodrouter.cpp
//
// Akashic Vault — Mod sources implementation.
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#include "cmodrouter.h"
#include "../modules/generators/plaits/src/plaits_generator.h"

void CModRouter::Update (uint32_t nNowUs, float fBPM, CPlaitsGenerator *pPlaits)
{
	float fLFO1 = m_LFO[0].Update (nNowUs, fBPM);	// bipolar ±depth
	float fLFO2 = m_LFO[1].Update (nNowUs, fBPM);
	float fEnv1 = m_Env[0].Update (nNowUs, fBPM);	// unipolar 0..depth
	float fEnv2 = m_Env[1].Update (nNowUs, fBPM);

	if (pPlaits)
		pPlaits->SetModSources (fLFO1, fLFO2, fEnv1, fEnv2);
}
