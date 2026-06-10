//
// cmodsync.h
//
// Akashic Vault — Shared BPM-sync division table for the mod router.
// Included by clfo.h and ccyclicenv.h.
//
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

// Divisions expressed as a fraction of one whole note.
// Period (seconds) = kDivValues[n] * 240.0f / BPM
//                  = kDivValues[n] * 4 beats * (60s / BPM)
//
// Examples at 120 BPM:
//   1/16 → 0.125s  (8 Hz)
//   1/8  → 0.25s   (4 Hz)
//   1/4  → 0.5s    (2 Hz)   ← quarter note = 1 beat
//   1/2  → 1.0s    (1 Hz)
//   1/1  → 2.0s    (0.5 Hz)
//   2/1  → 4.0s    (0.25 Hz)
static constexpr unsigned kNumSyncDivisions = 10;

static constexpr float kSyncDivValues[kNumSyncDivisions] =
{
	1.0f / 32,	// 1/32  — thirty-second note
	1.0f / 16,	// 1/16  — sixteenth note
	1.0f / 8,	// 1/8   — eighth note
	1.0f / 4,	// 1/4   — quarter note  (1 beat)
	3.0f / 8,	// 3/8   — dotted quarter
	1.0f / 2,	// 1/2   — half note     (2 beats)
	3.0f / 4,	// 3/4   — dotted half
	1.0f,		// 1/1   — whole note    (4 beats)
	2.0f,		// 2/1   — two bars
	4.0f,		// 4/1   — four bars
};

static constexpr const char *const kSyncDivNames[kNumSyncDivisions] =
{
	"1/32", "1/16", "1/8", "1/4", "3/8.", "1/2", "3/4.", "1/1", "2/1", "4/1"
};

// Convert a division index + BPM to a cycle rate in Hz.
inline float SyncDivToHz (unsigned nDiv, float fBPM)
{
	if (nDiv >= kNumSyncDivisions) nDiv = 3;	// fallback: 1/4
	float fPeriodS = kSyncDivValues[nDiv] * 240.0f / fBPM;
	float fHz = 1.0f / fPeriodS;
	return fHz > 20.0f ? 20.0f : fHz;		// cap at 20 Hz
}

// Convert a division index + BPM to a period in milliseconds.
inline float SyncDivToMs (unsigned nDiv, float fBPM)
{
	if (nDiv >= kNumSyncDivisions) nDiv = 3;
	return kSyncDivValues[nDiv] * 240000.0f / fBPM;
}
