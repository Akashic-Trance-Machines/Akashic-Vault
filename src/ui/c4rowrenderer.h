//
// c4rowrenderer.h
//
// Akashic Vault — LVGL renderer for the 4-row UI.
// Pixel-accurate layout per docs/UI_4ROW.md:
//   x 0-1:   2px scrollbar
//   x 3-121: label + value text
//   x 122-127: 6px action icon (▶)
// Each row is 16px tall; 4 rows × 16px = 64px = full display height.
//
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include <lvgl/lvgl.h>
#include "c4rowmenu.h"

class C4RowRenderer
{
public:
	C4RowRenderer ();

	// Call after CLVGL is initialized. Builds all LVGL objects once.
	void	Init ();

	// Rebuild LVGL object states from current menu state.
	// Call this once per main-loop iteration (before gui.Update()).
	void	Draw (const C4RowMenu &menu);

private:
	// Format a row's label+value into pBuf (at most nMax chars).
	void	FormatRow (const TMenuRow *pRow, char *pBuf, unsigned nMax) const;

	// Update scrollbar size and position.
	void	UpdateScrollbar (unsigned nTotal, unsigned nScrollIndex);

private:
	// ── LVGL objects (created in Init, never destroyed) ──────────────────
	lv_obj_t	*m_pScrollbar;		// white rect, x=0
	lv_obj_t	*m_pRowLabel[4];	// text labels
	lv_obj_t	*m_pActionBox[4];	// white 6×15 box
	lv_obj_t	*m_pActionArrow[4];	// ">" label on action box
};
