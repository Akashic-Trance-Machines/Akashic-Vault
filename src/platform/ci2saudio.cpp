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
#include <circle/timer.h>

#include <cmath>
#include <cstdint>

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
	m_fPhaseInc (2.0f * (float) M_PI * 440.0f / (float) nSampleRate),
	m_nMaxRenderUs (0),
	m_nPeakX1000 (0),
	m_nNanCount (0)
{
}

unsigned CI2SAudio::GetChunk (u32 *pBuffer, unsigned nChunkSize)
{
	// Enable flush-to-zero (FZ) before any FP work in this block.
	// CLAUDE.md §6 requires this on the audio path: reverbs and filters with
	// long decay tails generate denormals that stall the FPU and cause dropouts.
	// Must be re-applied each chunk because Circle restores FPCR around
	// exception/IRQ context switches.
#ifdef __aarch64__
	{
		uint64_t fpcr;
		__asm__ volatile ("mrs %0, fpcr" : "=r" (fpcr));
		fpcr |= (1ULL << 24);	// FZ — flush denormals to zero
		__asm__ volatile ("msr fpcr, %0" : : "r" (fpcr));
	}
#endif

	// nChunkSize words; each stereo frame = 2 words (L then R).
	unsigned nFrames = nChunkSize / 2;

	if (m_pEngine)
	{
		// ── Production path: render engine signal chain ───────────────
		// Scratch buffers live on the stack — keep nFrames ≤ MAX_BLOCK.
		float outL[256], outR[256];

		// Diagnostic: time the render (µs) to compare against the block
		// deadline (nFrames/sampleRate). No logging here — main loop reads it.
		const unsigned nStartUs = CTimer::GetClockTicks ();
		m_pEngine->Process (outL, outR, nFrames);
		const unsigned nRenderUs = CTimer::GetClockTicks () - nStartUs;
		if (nRenderUs > m_nMaxRenderUs) m_nMaxRenderUs = nRenderUs;

		float fPeak = 0.0f;
		for (unsigned i = 0; i < nFrames; i++)
		{
			float al = outL[i] < 0.0f ? -outL[i] : outL[i];
			float ar = outR[i] < 0.0f ? -outR[i] : outR[i];
			if (al > fPeak) fPeak = al;
			if (ar > fPeak) fPeak = ar;
		}
		{
			unsigned nPk = (unsigned) (fPeak * 1000.0f);
			if (nPk > m_nPeakX1000) m_nPeakX1000 = nPk;
		}

		for (unsigned i = 0; i < nFrames; i++)
		{
			// Flush NaN/Inf to SILENCE (not full-scale): a generator
			// emitting NaN otherwise becomes a loud crackle here while
			// reading as "quiet" on the peak meter (fabs(NaN) casts to 0).
			// Count them so the diagnostic can confirm/localise the source.
			float l = outL[i];
			float r = outR[i];
			if (l != l) { l = 0.0f; m_nNanCount++; }
			else if (l < -1.0f) l = -1.0f; else if (l > 1.0f) l = 1.0f;
			if (r != r) { r = 0.0f; m_nNanCount++; }
			else if (r < -1.0f) r = -1.0f; else if (r > 1.0f) r = 1.0f;

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
