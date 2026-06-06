//
// tparamdesc.h
//
// Akashic Vault - Parameter descriptor (shared param model).
// Copyright (C) 2026  The Akashic Trance Machines Team
//
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include <cstdint>

// How a parameter's value is interpreted / displayed on the OLED.
enum class ParamType : uint8_t
{
	Int,	// integer steps in [min..max]
	Float,	// continuous in [fmin..fmax]
	Enum,	// index into a string option table
	Bool	// 0 / 1
};

// How the value is rendered to text for the 4-row UI.
enum class ParamDisplay : uint8_t
{
	Raw,		// "64"
	Percent,	// "50%"
	Decibels,	// "-6 dB"
	Hertz,		// "440 Hz"
	Milliseconds,	// "120 ms"
	Note,		// "C3"
	Semitones,	// "+7"
	OnOff		// "On" / "Off"
};

// Static description of one module parameter. Generated from menu.json.
// POD, trivially copyable, no ownership — option strings are static literals.
struct TParamDesc
{
	const char	*pId;		// stable id, referenced by presets (e.g. "cutoff")
	const char	*pLabel;	// OLED label (e.g. "Cutoff")
	ParamType	 Type;
	ParamDisplay	 Display;

	// Numeric range (used for Int/Float; Enum uses [0 .. nOptions-1]).
	float		 fMin;
	float		 fMax;
	float		 fDefault;
	float		 fStep;		// encoder increment (Int/Float)

	// Enum options (Type == Enum), else nullptr / 0.
	const char *const *ppOptions;
	uint16_t	 nOptions;
};

// A module parameter value at runtime. Kept as float; Int/Enum/Bool round.
struct TParamValue
{
	float f;

	int	AsInt() const		{ return (int)(f + (f >= 0 ? 0.5f : -0.5f)); }
	bool	AsBool() const		{ return f >= 0.5f; }
};
