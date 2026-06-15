//
// c4rowmenu.h
//
// Akashic Vault — 4-row menu model.
// Manages the menu tree (pages, rows), navigation state, and input dispatch.
// Does NOT do any rendering — see C4RowRenderer.
//
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#pragma once

#include <cstdint>
#include "../engine/imodule.h"

// ── Row types ─────────────────────────────────────────────────────────────────

enum class TMenuRowType : uint8_t
{
	Property,	// encoder adjusts value; optional click action
	MenuItem,	// click navigates to child page
	Action,		// click triggers a callback
	ReadOnly	// display-only (status info, etc.)
};

struct TMenuPage;	// forward

struct TMenuRow
{
	TMenuRowType	type;
	const char	*pLabel;
	bool		bHasAction;		// show ▶ indicator?

	// ── Property backed by an IModule param ───────────────────────────────
	IModule		*pModule;
	unsigned	 nParamIndex;

	// ── Free property (system params not backed by IModule) ───────────────
	// Used when pModule == nullptr and type == Property.
	// pfAdjust: called with ±1 per encoder detent.
	// pfGetStr: writes current value text into pBuf (max nMax chars).
	void		(*pfAdjust)(void *pFreeCtx, int nDelta);
	void		(*pfGetStr)(void *pFreeCtx, char *pBuf, unsigned nMax);
	void		*pFreeCtx;

	// ── ReadOnly static string (pModule==nullptr, no callbacks) ──────────
	const char	*pStaticValue;

	// ── MenuItem ──────────────────────────────────────────────────────────
	TMenuPage	*pChildPage;

	// ── Action / Property-with-click ─────────────────────────────────────
	void		(*pfAction)(void *pCtx);
	void		*pActionCtx;
};

// ── Menu page ─────────────────────────────────────────────────────────────────

// Must exceed the largest single module's parameter count (CloudSeed = 46).
static constexpr unsigned MAX_ROWS_PER_PAGE = 64;

struct TMenuPage
{
	const char	*pName;
	TMenuPage	*pParent;
	TMenuRow	*pRows[MAX_ROWS_PER_PAGE];
	unsigned	 nRows;
};

// ── Navigation state ─────────────────────────────────────────────────────────

class C4RowMenu
{
public:
	C4RowMenu ();

	// Set the root page. Call before any input.
	void		SetRoot (TMenuPage *pRoot);

	// ── Input handlers (called from C4RowUI) ─────────────────────────────
	void		NavUp ();		// scroll up
	void		NavDown ();		// scroll down
	void		NavBack ();		// go up one level
	void		GoToRoot ();		// jump to root from anywhere (long-press)
	void		NavigateToPage (TMenuPage *pPage);	// jump to any page
	void		EncoderDelta (unsigned nRow, int nDelta); // value encoder
	void		EncoderClick (unsigned nRow);		  // value encoder click

	// ── Accessors (used by renderer) ─────────────────────────────────────
	TMenuPage	*CurrentPage () const	{ return m_pCurrentPage; }
	unsigned	 ScrollIndex () const	{ return m_nScrollIndex; }

	// Returns visible row i (0–3), nullptr if the page has fewer rows.
	TMenuRow	*VisibleRow (unsigned i) const;

private:
	void		Navigate (TMenuPage *pPage);

private:
	TMenuPage	*m_pRoot;
	TMenuPage	*m_pCurrentPage;
	unsigned	 m_nScrollIndex;
};
