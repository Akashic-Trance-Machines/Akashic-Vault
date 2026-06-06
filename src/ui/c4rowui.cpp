//
// c4rowui.cpp
//
// Akashic Vault — 4-row UI controller.
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#include "c4rowui.h"

C4RowUI::C4RowUI () {}

void C4RowUI::Init (TMenuPage *pRootPage)
{
	m_Menu.SetRoot (pRootPage);
	m_Renderer.Init ();
}

void C4RowUI::NavUp ()             { m_Menu.NavUp (); }
void C4RowUI::NavDown ()           { m_Menu.NavDown (); }
void C4RowUI::NavBack ()           { m_Menu.NavBack (); }
void C4RowUI::GoToRoot ()          { m_Menu.GoToRoot (); }

void C4RowUI::EncoderDelta (unsigned nEncoder, int nDelta)
{
	m_Menu.EncoderDelta (nEncoder, nDelta);
}

void C4RowUI::EncoderClick (unsigned nEncoder)
{
	m_Menu.EncoderClick (nEncoder);
}

void C4RowUI::Draw ()
{
	m_Renderer.Draw (m_Menu);
}
