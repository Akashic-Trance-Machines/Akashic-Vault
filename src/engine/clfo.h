//
// clfo.h
//
// Akashic Vault — Simple LFO source for the mod router.
// Runs at main-loop update rate (typically ~1 kHz); accumulates phase using
// wall-clock microseconds from CTimer::GetClockTicks().
//
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

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
		m_nLastUs (0)
	{
	}

	void  SetRate  (float fHz)
	{
		m_fRate  = fHz  < 0.01f ? 0.01f : (fHz  > 20.0f ? 20.0f : fHz);
	}
	void  SetDepth (float fD)
	{
		m_fDepth = fD   < 0.0f  ? 0.0f  : (fD   > 1.0f  ? 1.0f  : fD);
	}
	void  SetShape (Shape s)   { m_Shape = s; }
	float GetRate  () const    { return m_fRate;  }
	float GetDepth () const    { return m_fDepth; }
	Shape GetShape () const    { return m_Shape;  }

	// Call once per main-loop tick.  Returns current output (-depth..+depth).
	float Update (uint32_t nNowUs)
	{
		if (m_nLastUs != 0)
		{
			float fDeltaS = (float)(uint32_t)(nNowUs - m_nLastUs) * 1e-6f;
			m_fPhase += m_fRate * fDeltaS;
			if (m_fPhase >= 1.0f)
				m_fPhase -= (float)(int)m_fPhase;	// wrap, keep fractional
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
	uint32_t	m_nLastUs;
};
