//
// ccyclicenv.h
//
// Akashic Vault — Looping (cyclic / AD looper) envelope source for the mod
// router.  Outputs a unipolar signal 0..depth:  ramps up over attack_ms,
// ramps back down over decay_ms, then immediately loops.
//
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

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
		m_nLastUs   (0)
	{
	}

	void  SetAttack (float fMs)
	{
		m_fAttackMs = fMs < 10.0f ? 10.0f : (fMs > 4000.0f ? 4000.0f : fMs);
	}
	void  SetDecay  (float fMs)
	{
		m_fDecayMs  = fMs < 10.0f ? 10.0f : (fMs > 4000.0f ? 4000.0f : fMs);
	}
	void  SetDepth  (float fD)
	{
		m_fDepth    = fD  < 0.0f  ? 0.0f  : (fD  > 1.0f    ? 1.0f    : fD);
	}
	float GetAttack () const { return m_fAttackMs; }
	float GetDecay  () const { return m_fDecayMs;  }
	float GetDepth  () const { return m_fDepth;    }

	// Returns current unipolar output (0..depth).
	float Update (uint32_t nNowUs)
	{
		if (m_nLastUs != 0)
		{
			float fDeltaMs = (float)(uint32_t)(nNowUs - m_nLastUs) * 1e-3f;
			if (m_bAttacking)
			{
				m_fPhase += fDeltaMs / m_fAttackMs;
				if (m_fPhase >= 1.0f) { m_fPhase = 1.0f; m_bAttacking = false; }
			}
			else
			{
				m_fPhase -= fDeltaMs / m_fDecayMs;
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
	uint32_t	m_nLastUs;
};
