//
// main.cpp
//
// Akashic Vault - Circle entry point.
// Copyright (C) 2026  The Akashic Trance Machines Team
//
// This file is part of Akashic Vault and is licensed under GPL-3.0. See ../LICENSE.
//
#include "kernel.h"
#include <circle/startup.h>

int main (void)
{
	// Circle calls main() after low-level init; cleanup happens via reboot/halt.
	// NOTE: CKernel is large (menu pool, generator voice memory, bank buffers,
	// engine mix buffers). It MUST be static (BSS), not a stack local — Circle's
	// startup stack is small and a stack-allocated CKernel overflows it, which
	// corrupts adjacent memory (heard as persistent audio crackle that survives
	// generator switches). static places it in BSS where there is ample room.
	static CKernel Kernel;
	if (!Kernel.Initialize ())
	{
		halt ();
		return EXIT_HALT;
	}

	TShutdownMode ShutdownMode = Kernel.Run ();

	switch (ShutdownMode)
	{
	case ShutdownReboot:
		reboot ();
		return EXIT_REBOOT;
	case ShutdownHalt:
	default:
		halt ();
		return EXIT_HALT;
	}
}
