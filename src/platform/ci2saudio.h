//
// ci2saudio.h
//
// Akashic Vault - I2S audio output device.
// Subclasses CI2SSoundBaseDevice; GetChunk() is the DMA-driven audio render
// entry point. Phase 0: generates a 440 Hz sine smoke-test tone.
// Phase 1+: calls CEngine::Process() to render the full signal chain.
//
// Rules: GetChunk() is called from IRQ/DMA context.
//        NO heap allocation, NO locks, NO logging, NO Circle I2C/SD calls.
//
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include <circle/sound/i2ssoundbasedevice.h>
#include <circle/interrupt.h>
#include <circle/types.h>

class CEngine;		// forward — avoid pulling in engine headers

class CI2SAudio : public CI2SSoundBaseDevice
{
public:
	/// \param pInterrupt  Pointer to the interrupt system object.
	/// \param nSampleRate Sample rate in Hz (48000).
	/// \param nChunkSize  Words per GetChunk() call; 2 × frame count
	///                    (512 → 256 stereo frames = MAX_BLOCK).
	CI2SAudio (CInterruptSystem *pInterrupt,
		   unsigned nSampleRate = 48000,
		   unsigned nChunkSize  = 512);

	~CI2SAudio () override = default;

	/// Wire the engine. Call before Start(). pEngine may be nullptr (→ sine test tone).
	void SetEngine (CEngine *pEngine) { m_pEngine = pEngine; }

	/// Master volume 0.0–1.0. Safe to call from main loop; read in DMA callback.
	void SetVolume (float fVolume) { m_fVolume = fVolume; }

protected:
	/// Called from DMA interrupt — fill pBuffer with nChunkSize words (L,R pairs).
	unsigned GetChunk (u32 *pBuffer, unsigned nChunkSize) override;

private:
	CEngine  *m_pEngine;
	unsigned  m_nSampleRate;
	volatile float m_fVolume;	// 0.0–1.0, applied to all output

	// Sine oscillator state (smoke-test, real-time safe).
	float	  m_fPhase;
	float	  m_fPhaseInc;	// = 2π × 440 / sampleRate
};
