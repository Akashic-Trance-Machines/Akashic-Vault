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
	CKernel Kernel;
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
