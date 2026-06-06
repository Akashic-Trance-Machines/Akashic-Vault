//
// cmoduleregistry.h
//
// Akashic Vault - Registry of available modules (generators / FX / MIDI FX).
// Copyright (C) 2026  The Akashic Trance Machines Team
//
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include "imodule.h"

// Factory function type: constructs a fresh module instance on the heap.
// (Construction happens at init / preset-load time, never on the audio path.)
typedef IModule *(*TModuleFactory) ();

// Static descriptor for one selectable module. The table of these is emitted by
// scripts/gen_menus.py from every module's menu.json into src/generated/.
struct TModuleEntry
{
	const char	*pId;		// stable id ("braids")
	const char	*pName;		// display name ("Braids")
	ModuleKind	 Kind;
	TModuleFactory	 pFactory;
};

// Enumerates modules so the UI selector can list and instantiate them by kind.
// Backed by the generated table; no allocation for enumeration.
class CModuleRegistry
{
public:
	// Provided by generated code (src/generated/modules_table.cpp).
	static const TModuleEntry	*Entries ();
	static unsigned			 Count ();

	// Enumerate by kind.
	static unsigned	CountOfKind (ModuleKind Kind);
	static const TModuleEntry *NthOfKind (ModuleKind Kind, unsigned n);

	// Look up + instantiate. Returns nullptr on unknown id.
	static const TModuleEntry *Find (const char *pId);
	static IModule		  *Create (const char *pId);
};
