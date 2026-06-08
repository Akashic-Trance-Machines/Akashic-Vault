//
// c4rowui.h
//
// Akashic Vault — 4-row UI controller.
// Owns the menu model and renderer; routes input from the kernel.
//
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include "c4rowmenu.h"
#include "c4rowrenderer.h"

class C4RowUI
{
public:
	C4RowUI ();

	// Call after CLVGL is initialized.
	void	Init (TMenuPage *pRootPage);

	// ── Input (called from kernel PollInput) ──────────────────────────────
	void	NavUp ();
	void	NavDown ();
	void	NavBack ();
	void	GoToRoot ();
	void	NavigateToPage (TMenuPage *pPage);
	void	EncoderDelta (unsigned nEncoder, int nDelta);	// 0-3 = enc1-4
	void	EncoderClick (unsigned nEncoder);

	// Call once per main-loop iteration, before gui.Update().
	void	Draw ();

private:
	C4RowMenu	m_Menu;
	C4RowRenderer	m_Renderer;
};
