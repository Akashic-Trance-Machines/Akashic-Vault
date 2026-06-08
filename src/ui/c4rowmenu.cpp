//
// c4rowmenu.cpp
//
// Akashic Vault — 4-row menu model implementation.
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0.
// See ../../LICENSE.
//
#include "c4rowmenu.h"

C4RowMenu::C4RowMenu ()
:	m_pRoot (nullptr),
	m_pCurrentPage (nullptr),
	m_nScrollIndex (0)
{
}

void C4RowMenu::SetRoot (TMenuPage *pRoot)
{
	m_pRoot         = pRoot;
	m_pCurrentPage  = pRoot;
	m_nScrollIndex  = 0;
}

void C4RowMenu::Navigate (TMenuPage *pPage)
{
	m_pCurrentPage = pPage;
	m_nScrollIndex = 0;
}

TMenuRow *C4RowMenu::VisibleRow (unsigned i) const
{
	if (!m_pCurrentPage)
		return nullptr;
	unsigned idx = m_nScrollIndex + i;
	if (idx >= m_pCurrentPage->nRows)
		return nullptr;
	return m_pCurrentPage->pRows[idx];
}

void C4RowMenu::NavUp ()
{
	if (m_nScrollIndex > 0)
		m_nScrollIndex--;
}

void C4RowMenu::NavDown ()
{
	if (!m_pCurrentPage)
		return;
	unsigned maxScroll = (m_pCurrentPage->nRows > 4)
		? m_pCurrentPage->nRows - 4 : 0;
	if (m_nScrollIndex < maxScroll)
		m_nScrollIndex++;
}

void C4RowMenu::NavBack ()
{
	if (!m_pCurrentPage || !m_pCurrentPage->pParent)
		return;			// already at root — do nothing
	Navigate (m_pCurrentPage->pParent);
}

void C4RowMenu::GoToRoot ()
{
	if (m_pRoot)
		Navigate (m_pRoot);
}

void C4RowMenu::NavigateToPage (TMenuPage *pPage)
{
	if (pPage)
		Navigate (pPage);
}

void C4RowMenu::EncoderDelta (unsigned nRow, int nDelta)
{
	TMenuRow *pRow = VisibleRow (nRow);
	if (!pRow || pRow->type != TMenuRowType::Property)
		return;

	if (pRow->pModule)
	{
		// Module-backed property.
		const TParamDesc &desc = pRow->pModule->ParamDesc (pRow->nParamIndex);
		TParamValue val = pRow->pModule->GetParam (pRow->nParamIndex);

		float step;
		int   delta = nDelta;

		switch (desc.Type)
		{
		case ParamType::Enum:
		case ParamType::Bool:
			// No acceleration for discrete types — always single step.
			delta = (nDelta > 0) ? 1 : -1;
			step  = 1.0f;
			break;
		case ParamType::Int:
			step = 1.0f;
			break;
		default: // Float
			step = desc.fStep;
			break;
		}

		float newVal = val.f + (float) delta * step;
		if (newVal < desc.fMin) newVal = desc.fMin;
		if (newVal > desc.fMax) newVal = desc.fMax;
		pRow->pModule->SetParam (pRow->nParamIndex, {newVal});
	}
	else if (pRow->pfAdjust)
	{
		// Free property (e.g. master volume).
		pRow->pfAdjust (pRow->pFreeCtx, nDelta);
	}
}

void C4RowMenu::EncoderClick (unsigned nRow)
{
	TMenuRow *pRow = VisibleRow (nRow);
	if (!pRow)
		return;

	switch (pRow->type)
	{
	case TMenuRowType::MenuItem:
		if (pRow->pChildPage)
			Navigate (pRow->pChildPage);
		break;

	case TMenuRowType::Action:
		if (pRow->pfAction)
			pRow->pfAction (pRow->pActionCtx);
		break;

	case TMenuRowType::Property:
		// Property with action — trigger optional click callback.
		if (pRow->bHasAction && pRow->pfAction)
			pRow->pfAction (pRow->pActionCtx);
		break;

	default:
		break;
	}
}
