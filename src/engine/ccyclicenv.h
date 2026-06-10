//
// ccyclicenv.h
//
// Akashic Vault — BPM-syncable looping AD envelope for the mod router.
// Attack and decay times can each be set as a free value (ms) or as a note
// division derived from the current BPM.  Output is unipolar: 0 .. depth.
//
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include "cmodsync.h"
#include <cstdint>

class CCyclicEnv
{
public:
	CCyclicEnv ()
	:	m_fAttackMs (200.0f),
		m_fDecayMs  (200.0f),
		m_fDepth    (0.5f),
		m_fPhase    (0.0f),
		m_bAttacking (true),
		m_bSync     (false),
		m_nDivAtk   (3),		// default: 1/4 note
		m_nDivDec   (3),
		m_nLastUs   (0)
	{
	}

	// Free-run times (used when Sync = Off)
	void  SetAttack (float fMs)
	{
		m_fAttackMs = fMs < 10.0f ? 10.0f : (fMs > 4000.0f ? 4000.0f : fMs);
	}
	void  SetDecay  (float fMs)
	{
		m_fDecayMs  = fMs < 10.0f ? 10.0f : (fMs > 4000.0f ? 4000.0f : fMs);
	}
	float GetAttack () const { return m_fAttackMs; }
	float GetDecay  () const { return m_fDecayMs;  }

	void  SetDepth (float fD)
	{
		m_fDepth = fD < 0.0f ? 0.0f : (fD > 1.0f ? 1.0f : fD);
	}
	float GetDepth () const { return m_fDepth; }

	// BPM sync — a single Sync flag governs both Attack and Decay.
	// Each has its own division selector.
	void     SetSync    (bool b)     { m_bSync = b; }
	bool     GetSync    () const     { return m_bSync; }

	void     SetDivAtk  (unsigned n) { m_nDivAtk = n < kNumSyncDivisions ? n : 0; }
	void     SetDivDec  (unsigned n) { m_nDivDec = n < kNumSyncDivisions ? n : 0; }
	unsigned GetDivAtk  () const     { return m_nDivAtk; }
	unsigned GetDivDec  () const     { return m_nDivDec; }

	// Advance one tick.  fBPM is used only when Sync = On.
	// Returns current unipolar output in (0 .. depth).
	float Update (uint32_t nNowUs, float fBPM)
	{
		float fAtkMs = m_bSync ? SyncDivToMs (m_nDivAtk, fBPM) : m_fAttackMs;
		float fDecMs = m_bSync ? SyncDivToMs (m_nDivDec, fBPM) : m_fDecayMs;

		if (m_nLastUs != 0)
		{
			float fDeltaMs = (float)(uint32_t)(nNowUs - m_nLastUs) * 1e-3f;
			if (m_bAttacking)
			{
				m_fPhase += fDeltaMs / fAtkMs;
				if (m_fPhase >= 1.0f) { m_fPhase = 1.0f; m_bAttacking = false; }
			}
			else
			{
				m_fPhase -= fDeltaMs / fDecMs;
				if (m_fPhase <= 0.0f) { m_fPhase = 0.0f; m_bAttacking = true;  }
			}
		}
		m_nLastUs = nNowUs;
		return m_fPhase * m_fDepth;
	}

	float GetCurrent () const { return m_fPhase * m_fDepth; }

private:
	float		m_fAttackMs;
	float		m_fDecayMs;
	float		m_fDepth;
	float		m_fPhase;
	bool		m_bAttacking;
	bool		m_bSync;
	unsigned	m_nDivAtk;
	unsigned	m_nDivDec;
	uint32_t	m_nLastUs;
};
