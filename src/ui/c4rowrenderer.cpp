//
// c4rowrenderer.cpp
//
// Akashic Vault — LVGL renderer for the 4-row UI.
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#include "c4rowrenderer.h"
#include <cstdio>
#include <cstring>

// ── Layout constants (see docs/UI_4ROW.md) ────────────────────────────────────

static constexpr int DISPLAY_W     = 128;
static constexpr int DISPLAY_H     = 64;
static constexpr int ROW_H         = 16;
static constexpr int SCROLLBAR_W   = 2;
static constexpr int TEXT_X        = 3;
static constexpr int TEXT_W        = 119;	// x 3..121
static constexpr int ACTION_X      = 122;
static constexpr int ACTION_W      = 6;

// Per-row text baselines (y measured from top-of-display).
// The baseline is the bottom of the text glyph (ascent from top = ~13px).
static const int ROW_Y[4] = { 0, 16, 32, 48 };  // top of each row

// ── Constructor / Init ────────────────────────────────────────────────────────

C4RowRenderer::C4RowRenderer ()
{
	for (unsigned i = 0; i < 4; i++)
	{
		m_pRowLabel[i]   = nullptr;
		m_pActionBox[i]  = nullptr;
		m_pActionArrow[i]= nullptr;
	}
	m_pScrollbar = nullptr;
}

void C4RowRenderer::Init ()
{
	lv_obj_t *scr = lv_screen_active ();

	// Zero screen padding so lv_obj_set_pos() maps directly to pixels.
	lv_obj_set_style_pad_all (scr, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width (scr, 0, LV_PART_MAIN);

	// ── Scrollbar ─────────────────────────────────────────────────────────
	m_pScrollbar = lv_obj_create (scr);
	lv_obj_set_size (m_pScrollbar, SCROLLBAR_W, DISPLAY_H);
	lv_obj_set_pos  (m_pScrollbar, 0, 0);
	lv_obj_set_style_bg_color  (m_pScrollbar, lv_color_white (), LV_PART_MAIN);
	lv_obj_set_style_bg_opa    (m_pScrollbar, LV_OPA_COVER,     LV_PART_MAIN);
	lv_obj_set_style_border_width (m_pScrollbar, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all   (m_pScrollbar, 0, LV_PART_MAIN);
	lv_obj_set_style_radius    (m_pScrollbar, 0, LV_PART_MAIN);

	for (unsigned i = 0; i < 4; i++)
	{
		int rowY = ROW_Y[i];

		// ── Row text label ─────────────────────────────────────────────
		m_pRowLabel[i] = lv_label_create (scr);
		lv_obj_set_pos  (m_pRowLabel[i], TEXT_X, rowY);
		lv_obj_set_size (m_pRowLabel[i], TEXT_W, ROW_H);
		lv_label_set_long_mode (m_pRowLabel[i], LV_LABEL_LONG_CLIP);
		lv_obj_set_style_text_color  (m_pRowLabel[i], lv_color_white (), LV_PART_MAIN);
		// Opaque black bg (not transparent): I1 mode needs an explicit bg
		// fill before drawing glyph pixels, otherwise text is invisible.
		lv_obj_set_style_bg_color    (m_pRowLabel[i], lv_color_black (), LV_PART_MAIN);
		lv_obj_set_style_bg_opa      (m_pRowLabel[i], LV_OPA_COVER,     LV_PART_MAIN);
		lv_obj_set_style_border_width(m_pRowLabel[i], 0,                LV_PART_MAIN);
		lv_obj_set_style_pad_all     (m_pRowLabel[i], 0,                LV_PART_MAIN);
		lv_obj_set_style_radius      (m_pRowLabel[i], 0,                LV_PART_MAIN);
		lv_label_set_text (m_pRowLabel[i], "");

		// ── Action icon: white box ─────────────────────────────────────
		m_pActionBox[i] = lv_obj_create (scr);
		lv_obj_set_size (m_pActionBox[i], ACTION_W, ROW_H - 1);
		lv_obj_set_pos  (m_pActionBox[i], ACTION_X, rowY + 1);
		lv_obj_set_style_bg_color  (m_pActionBox[i], lv_color_white (), LV_PART_MAIN);
		lv_obj_set_style_bg_opa    (m_pActionBox[i], LV_OPA_COVER,     LV_PART_MAIN);
		lv_obj_set_style_border_width (m_pActionBox[i], 0,             LV_PART_MAIN);
		lv_obj_set_style_pad_all   (m_pActionBox[i], 0,                LV_PART_MAIN);
		lv_obj_set_style_radius    (m_pActionBox[i], 0,                LV_PART_MAIN);
		lv_obj_add_flag (m_pActionBox[i], LV_OBJ_FLAG_HIDDEN);

		// ── Action icon: dark arrow on white box ───────────────────────
		m_pActionArrow[i] = lv_label_create (m_pActionBox[i]);
		lv_label_set_text (m_pActionArrow[i], ">");
		lv_obj_set_style_text_color (m_pActionArrow[i], lv_color_black (), LV_PART_MAIN);
		lv_obj_set_style_bg_opa     (m_pActionArrow[i], LV_OPA_TRANSP,    LV_PART_MAIN);
		lv_obj_set_style_pad_all    (m_pActionArrow[i], 0,                LV_PART_MAIN);
		lv_obj_align (m_pActionArrow[i], LV_ALIGN_CENTER, 0, 0);
	}
}

// ── Draw ──────────────────────────────────────────────────────────────────────

void C4RowRenderer::Draw (const C4RowMenu &menu)
{
	const TMenuPage *pPage = menu.CurrentPage ();
	unsigned nTotal       = pPage ? pPage->nRows : 0;
	unsigned scrollIdx    = menu.ScrollIndex ();

	// Scrollbar
	UpdateScrollbar (nTotal, scrollIdx);

	// 4 rows
	char buf[40];
	for (unsigned i = 0; i < 4; i++)
	{
		const TMenuRow *pRow = menu.VisibleRow (i);

		if (!pRow)
		{
			lv_label_set_text (m_pRowLabel[i], "");
			lv_obj_add_flag (m_pActionBox[i], LV_OBJ_FLAG_HIDDEN);
			continue;
		}

		FormatRow (pRow, buf, sizeof (buf));
		lv_label_set_text (m_pRowLabel[i], buf);

		if (pRow->bHasAction)
			lv_obj_clear_flag (m_pActionBox[i], LV_OBJ_FLAG_HIDDEN);
		else
			lv_obj_add_flag   (m_pActionBox[i], LV_OBJ_FLAG_HIDDEN);
	}
}

// ── Private helpers ───────────────────────────────────────────────────────────

void C4RowRenderer::UpdateScrollbar (unsigned nTotal, unsigned nScrollIndex)
{
	int barH, barY;

	if (nTotal <= 4)
	{
		barH = DISPLAY_H;
		barY = 0;
	}
	else
	{
		barH = (4 * DISPLAY_H) / (int) nTotal;
		if (barH < 4) barH = 4;
		barY = (int) nScrollIndex * (DISPLAY_H - barH) / (int) (nTotal - 4);
	}

	lv_obj_set_size (m_pScrollbar, SCROLLBAR_W, barH);
	lv_obj_set_pos  (m_pScrollbar, 0, barY);
}

void C4RowRenderer::FormatRow (const TMenuRow *pRow, char *pBuf, unsigned nMax) const
{
	if (!pRow)
	{
		pBuf[0] = '\0';
		return;
	}

	// No value for non-property rows.
	if (pRow->type == TMenuRowType::MenuItem ||
	    pRow->type == TMenuRowType::Action)
	{
		snprintf (pBuf, nMax, "%s", pRow->pLabel);
		return;
	}

	// ReadOnly with static string.
	if (pRow->type == TMenuRowType::ReadOnly && !pRow->pModule)
	{
		snprintf (pBuf, nMax, "%s: %s",
			pRow->pLabel,
			pRow->pStaticValue ? pRow->pStaticValue : "-");
		return;
	}

	// Free property (system param, no module).
	if (!pRow->pModule && pRow->pfGetStr)
	{
		char valBuf[24];
		pRow->pfGetStr (pRow->pFreeCtx, valBuf, sizeof (valBuf));
		snprintf (pBuf, nMax, "%s: %s", pRow->pLabel, valBuf);
		return;
	}

	// Property with no module and no callback — just show label.
	if (!pRow->pModule)
	{
		snprintf (pBuf, nMax, "%s", pRow->pLabel);
		return;
	}

	const TParamDesc &desc = pRow->pModule->ParamDesc (pRow->nParamIndex);
	TParamValue val        = pRow->pModule->GetParam   (pRow->nParamIndex);

	char valBuf[20];
	switch (desc.Type)
	{
	case ParamType::Enum:
		if (desc.ppOptions && val.AsInt () < (int) desc.nOptions)
			snprintf (valBuf, sizeof (valBuf), "%s",
				desc.ppOptions[val.AsInt ()]);
		else
			snprintf (valBuf, sizeof (valBuf), "%d", val.AsInt ());
		break;

	case ParamType::Bool:
		snprintf (valBuf, sizeof (valBuf), "%s",
			val.AsBool () ? "On" : "Off");
		break;

	case ParamType::Int:
		switch (desc.Display)
		{
		case ParamDisplay::Decibels:
			snprintf (valBuf, sizeof (valBuf), "%d dB", val.AsInt ());
			break;
		case ParamDisplay::Milliseconds:
			snprintf (valBuf, sizeof (valBuf), "%d ms", val.AsInt ());
			break;
		case ParamDisplay::OnOff:
			snprintf (valBuf, sizeof (valBuf), "%s",
				val.AsInt () ? "On" : "Off");
			break;
		default:
			snprintf (valBuf, sizeof (valBuf), "%d", val.AsInt ());
			break;
		}
		break;

	case ParamType::Float:
		switch (desc.Display)
		{
		case ParamDisplay::Percent:
			// Round away from zero so bipolar values display symmetrically.
			snprintf (valBuf, sizeof (valBuf), "%d%%",
				(int) (val.f * 100.0f + (val.f >= 0.0f ? 0.5f : -0.5f)));
			break;
		case ParamDisplay::Decibels:
			snprintf (valBuf, sizeof (valBuf), "%.1f dB", (double) val.f);
			break;
		case ParamDisplay::Hertz:
			snprintf (valBuf, sizeof (valBuf), "%.0f Hz", (double) val.f);
			break;
		case ParamDisplay::Milliseconds:
			snprintf (valBuf, sizeof (valBuf), "%.0f ms", (double) val.f);
			break;
		default:
			snprintf (valBuf, sizeof (valBuf), "%.2f", (double) val.f);
			break;
		}
		break;

	default:
		snprintf (valBuf, sizeof (valBuf), "?");
		break;
	}

	snprintf (pBuf, nMax, "%s: %s", desc.pLabel, valBuf);
}
