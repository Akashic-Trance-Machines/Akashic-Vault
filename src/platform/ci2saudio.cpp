//
// ci2saudio.cpp
//
// Akashic Vault - I2S audio output device.
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#include "ci2saudio.h"
#include "../engine/cengine.h"

#include <cmath>

// Scale factor for 24-bit signed samples in a 32-bit I2S word.
static constexpr float s_fScale = 8388607.0f;	// 2^23 - 1

CI2SAudio::CI2SAudio (CInterruptSystem *pInterrupt,
		      unsigned nSampleRate,
		      unsigned nChunkSize)
:	CI2SSoundBaseDevice (pInterrupt, nSampleRate, nChunkSize),
	m_pEngine (nullptr),
	m_nSampleRate (nSampleRate),
	m_fVolume (1.0f),
	m_fPhase (0.0f),
	m_fPhaseInc (2.0f * (float) M_PI * 440.0f / (float) nSampleRate)
{
}

unsigned CI2SAudio::GetChunk (u32 *pBuffer, unsigned nChunkSize)
{
	// nChunkSize words; each stereo frame = 2 words (L then R).
	unsigned nFrames = nChunkSize / 2;

	if (m_pEngine)
	{
		// ── Production path: render engine signal chain ───────────────
		// Scratch buffers live on the stack — keep nFrames ≤ MAX_BLOCK.
		float outL[256], outR[256];
		m_pEngine->Process (outL, outR, nFrames);

		for (unsigned i = 0; i < nFrames; i++)
		{
			// Flush denormals and clamp to [-1, 1].
			float l = outL[i];
			float r = outR[i];
			if (l >  1.0f) l =  1.0f;
			if (l < -1.0f) l = -1.0f;
			if (r >  1.0f) r =  1.0f;
			if (r < -1.0f) r = -1.0f;

			pBuffer[i * 2]     = (u32)(s32)(l * s_fScale * m_fVolume);
			pBuffer[i * 2 + 1] = (u32)(s32)(r * s_fScale * m_fVolume);
		}
	}
	else
	{
		// ── Smoke-test path: 440 Hz sine on both channels ─────────────
		for (unsigned i = 0; i < nFrames; i++)
		{
			float sample = sinf (m_fPhase) * 0.5f;	// 50% amplitude

			m_fPhase += m_fPhaseInc;
			if (m_fPhase >= 2.0f * (float) M_PI)
				m_fPhase -= 2.0f * (float) M_PI;

			s32 nSample = (s32)(sample * s_fScale);
			pBuffer[i * 2]     = (u32)(s32)(sample * s_fScale * m_fVolume);
			pBuffer[i * 2 + 1] = (u32)(s32)(sample * s_fScale * m_fVolume);
		}
	}

	return nChunkSize;
}
