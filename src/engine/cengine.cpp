//
// cengine.cpp
//
// Akashic Vault - Engine skeleton. Phase 0: structure + real-time-safe render
// loop. Generator/FX/MIDI-FX instancing is wired here as modules land.
// Copyright (C) 2026  The Akashic Trance Machines Team
//
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#include "cengine.h"
#include "cmoduleregistry.h"
#include <cstring>

CEngine::CEngine () {}
CEngine::~CEngine () {}

void CEngine::Init (unsigned nSampleRate, unsigned nBlock)
{
	m_nSampleRate = nSampleRate;
	m_nBlock = (nBlock <= MAX_BLOCK) ? nBlock : MAX_BLOCK;
	memset (m_MixL, 0, sizeof m_MixL);
	memset (m_MixR, 0, sizeof m_MixR);
}

// ── Module wiring (UI / preset context — may allocate) ───────────────────────

bool CEngine::SetGenerator (unsigned nSlot, const char *pModuleId)
{
	if (nSlot >= MAX_GENERATORS)
		return false;

	IModule *p = CModuleRegistry::Create (pModuleId);
	if (!p || p->Kind () != ModuleKind::Generator)
	{
		delete p;
		return false;
	}

	delete m_pGen[nSlot];
	m_pGen[nSlot] = static_cast<ISoundGenerator *> (p);
	m_pGen[nSlot]->Init (m_nSampleRate, m_nBlock);
	if (nSlot + 1 > m_nGenerators)
		m_nGenerators = nSlot + 1;
	return true;
}

bool CEngine::SetAudioFX (unsigned nFxSlot, const char *pModuleId)
{
	if (nFxSlot >= NUM_FX_SLOTS)
		return false;

	IModule *p = CModuleRegistry::Create (pModuleId);	// nullptr/"none" clears
	if (p && p->Kind () != ModuleKind::AudioFX)
	{
		delete p;
		return false;
	}

	delete m_pFX[nFxSlot];
	m_pFX[nFxSlot] = static_cast<IAudioFX *> (p);
	if (m_pFX[nFxSlot])
		m_pFX[nFxSlot]->Init (m_nSampleRate, m_nBlock);
	return true;
}

bool CEngine::AddMidiFX (const char *pModuleId)
{
	if (m_nMidiFX >= MAX_MIDI_FX)
		return false;

	IModule *p = CModuleRegistry::Create (pModuleId);
	if (!p || p->Kind () != ModuleKind::MidiFX)
	{
		delete p;
		return false;
	}

	m_pMidiFX[m_nMidiFX] = static_cast<IMidiFX *> (p);
	m_pMidiFX[m_nMidiFX]->Init (m_nSampleRate, m_nBlock);
	m_nMidiFX++;
	return true;
}

void CEngine::ClearMidiFX ()
{
	for (unsigned i = 0; i < m_nMidiFX; i++)
	{
		delete m_pMidiFX[i];
		m_pMidiFX[i] = nullptr;
	}
	m_nMidiFX = 0;
}

void CEngine::SetAudioFXDirect (unsigned nFxSlot, IAudioFX *pFX)
{
	if (nFxSlot >= NUM_FX_SLOTS || !pFX)
		return;
	// Don't delete — not owned. Caller keeps ownership.
	m_pFX[nFxSlot] = pFX;
}

void CEngine::SetGeneratorDirect (unsigned nSlot, ISoundGenerator *pGen)
{
	if (nSlot >= MAX_GENERATORS || !pGen)
		return;
	// Don't delete — not owned. Caller keeps ownership (e.g. stack/member).
	m_pGen[nSlot] = pGen;
	if (nSlot + 1 > m_nGenerators)
		m_nGenerators = nSlot + 1;
}

ISoundGenerator *CEngine::Generator (unsigned nSlot)
{
	return (nSlot < MAX_GENERATORS) ? m_pGen[nSlot] : nullptr;
}

IAudioFX *CEngine::AudioFX (unsigned nFxSlot)
{
	return (nFxSlot < NUM_FX_SLOTS) ? m_pFX[nFxSlot] : nullptr;
}

void CEngine::SetFXBypass (unsigned nFxSlot, bool bBypass)
{
	if (nFxSlot < NUM_FX_SLOTS)
		m_bFXBypass[nFxSlot] = bBypass;
}

// ── MIDI / clock (TODO: lock-free queue, channel routing) ────────────────────

void CEngine::PushMidi (const TMidiEvent &Event)
{
	// TODO Phase 1: enqueue into a single-producer/single-consumer ring drained
	// by DrainMidi() at the top of each block. For now, dispatch directly to the
	// generator(s) on the matching channel (acceptable until the queue lands).
	for (unsigned g = 0; g < m_nGenerators; g++)
		if (m_pGen[g] && m_GenChannel[g] == Event.nChannel)
			m_pGen[g]->HandleMidi (Event);
}

void CEngine::SetClockSource (ClockSource Source) { m_ClockSource = Source; }
void CEngine::SetTempo (float fBPM)
{
	m_fTempoBPM = fBPM;
	for (unsigned i = 0; i < m_nMidiFX; i++)
		m_pMidiFX[i]->SetTempo (fBPM);
}

void CEngine::DrainMidi (unsigned /*nFrames*/) { /* TODO Phase 1 */ }
void CEngine::AdvanceClock (unsigned /*nFrames*/) { /* TODO Phase 1 */ }

// ── Audio render — REAL-TIME. No heap/locks/logging below this line. ─────────

void CEngine::Process (float *pOutL, float *pOutR, unsigned nFrames)
{
	if (nFrames > m_nBlock)
		nFrames = m_nBlock;

	DrainMidi (nFrames);
	AdvanceClock (nFrames);

	// Sum active generators into the mix buffer.
	memset (m_MixL, 0, nFrames * sizeof (float));
	memset (m_MixR, 0, nFrames * sizeof (float));

	for (unsigned g = 0; g < m_nGenerators; g++)
	{
		if (!m_pGen[g])
			continue;

		float tmpL[MAX_BLOCK];
		float tmpR[MAX_BLOCK];
		m_pGen[g]->Process (tmpL, tmpR, nFrames);
		for (unsigned i = 0; i < nFrames; i++)
		{
			m_MixL[i] += tmpL[i];
			m_MixR[i] += tmpR[i];
		}
	}

	// Run the FX chain in order (skip empty / bypassed slots).
	for (unsigned s = 0; s < NUM_FX_SLOTS; s++)
		if (m_pFX[s] && !m_bFXBypass[s])
			m_pFX[s]->Process (m_MixL, m_MixR, nFrames);

	// Copy to the output. (I2S driver applies master volume / clamping.)
	memcpy (pOutL, m_MixL, nFrames * sizeof (float));
	memcpy (pOutR, m_MixR, nFrames * sizeof (float));
}
