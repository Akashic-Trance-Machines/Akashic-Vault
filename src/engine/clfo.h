//
// clfo.h
//
// Akashic Vault — BPM-syncable LFO source for the mod router.
// Runs at main-loop update rate (~1 kHz); accumulates phase using wall-clock
// microseconds.  In sync mode the rate is derived from BPM + note division;
// in free mode it runs at the stored Hz value.
//
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include "cmodsync.h"
#include <cmath>
#include <cstdint>

class CLFO
{
public:
	enum class Shape : uint8_t
	{
		Sine, Triangle, Saw, Ramp, Square,
		NUM_SHAPES
	};

	static constexpr const char *const ShapeNames[] =
	{
		"Sine", "Tri", "Saw", "Ramp", "Square"
	};

	CLFO ()
	:	m_fRate (1.0f),
		m_fDepth (0.5f),
		m_fPhase (0.0f),
		m_Shape (Shape::Sine),
		m_bSync (false),
		m_nDivision (3),		// default: 1/4 note
		m_nLastUs (0)
	{
	}

	// Free-run rate (used when Sync = Off)
	void  SetRate  (float fHz)
	{
		m_fRate = fHz < 0.01f ? 0.01f : (fHz > 20.0f ? 20.0f : fHz);
	}
	float GetRate  () const { return m_fRate; }

	void  SetDepth (float fD)
	{
		m_fDepth = fD < 0.0f ? 0.0f : (fD > 1.0f ? 1.0f : fD);
	}
	float GetDepth () const { return m_fDepth; }

	void  SetShape (Shape s) { m_Shape = s; }
	Shape GetShape () const  { return m_Shape; }

	// BPM sync
	void     SetSync     (bool b)     { m_bSync = b; }
	bool     GetSync     () const     { return m_bSync; }
	void     SetDivision (unsigned n) { m_nDivision = n < kNumSyncDivisions ? n : 0; }
	unsigned GetDivision () const     { return m_nDivision; }

	// Advance one tick.  fBPM is used only when Sync = On.
	// Returns current output in (-depth .. +depth).
	float Update (uint32_t nNowUs, float fBPM)
	{
		float fRate = m_bSync ? SyncDivToHz (m_nDivision, fBPM) : m_fRate;
		if (m_nLastUs != 0)
		{
			float fDeltaS = (float)(uint32_t)(nNowUs - m_nLastUs) * 1e-6f;
			m_fPhase += fRate * fDeltaS;
			if (m_fPhase >= 1.0f)
				m_fPhase -= (float)(int)m_fPhase;
		}
		m_nLastUs = nNowUs;
		return _value ();
	}

	float GetCurrent () const { return _value (); }

private:
	float _value () const
	{
		float t = m_fPhase;
		float v;
		switch (m_Shape)
		{
		case Shape::Triangle:
			v = (t < 0.5f) ? (4.0f * t - 1.0f) : (3.0f - 4.0f * t);
			break;
		case Shape::Saw:
			v = 2.0f * t - 1.0f;
			break;
		case Shape::Ramp:
			v = 1.0f - 2.0f * t;
			break;
		case Shape::Square:
			v = (t < 0.5f) ? 1.0f : -1.0f;
			break;
		case Shape::Sine:
		default:
			v = sinf (2.0f * 3.14159265f * t);
			break;
		}
		return v * m_fDepth;
	}

	float		m_fRate;
	float		m_fDepth;
	float		m_fPhase;
	Shape		m_Shape;
	bool		m_bSync;
	unsigned	m_nDivision;
	uint32_t	m_nLastUs;
};
