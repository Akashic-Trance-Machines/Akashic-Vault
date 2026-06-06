//
// cmoduleregistry.cpp
//
// Akashic Vault - Registry helpers built on the generated module table.
// (Entries()/Count() are emitted into src/generated/modules_table.cpp.)
// Copyright (C) 2026  The Akashic Trance Machines Team
//
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#include "cmoduleregistry.h"
#include <cstring>

unsigned CModuleRegistry::CountOfKind (ModuleKind Kind)
{
	unsigned n = 0;
	const TModuleEntry *e = Entries ();
	for (unsigned i = 0; i < Count (); i++)
		if (e[i].Kind == Kind)
			n++;
	return n;
}

const TModuleEntry *CModuleRegistry::NthOfKind (ModuleKind Kind, unsigned n)
{
	const TModuleEntry *e = Entries ();
	for (unsigned i = 0; i < Count (); i++)
		if (e[i].Kind == Kind && n-- == 0)
			return &e[i];
	return nullptr;
}

const TModuleEntry *CModuleRegistry::Find (const char *pId)
{
	if (!pId)
		return nullptr;
	const TModuleEntry *e = Entries ();
	for (unsigned i = 0; i < Count (); i++)
		if (strcmp (e[i].pId, pId) == 0)
			return &e[i];
	return nullptr;
}

IModule *CModuleRegistry::Create (const char *pId)
{
	const TModuleEntry *pEntry = Find (pId);
	return (pEntry && pEntry->pFactory) ? pEntry->pFactory () : nullptr;
}
