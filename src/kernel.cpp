//
// kernel.cpp
//
// Akashic Vault — Circle bare-metal kernel.
// Phase 0 complete. Phase 1: 4-row UI + Plaits generator.
//
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of Akashic Vault and is licensed under GPL-3.0. See ../LICENSE.
//
#include "kernel.h"
#include <circle/gpiopin.h>
#include <cstdio>
#include <cstring>

LOGMODULE ("kernel");

#define DRIVE			"SD:"

// Hardware (see docs/HARDWARE_PLATFORM.md).
#define OLED_I2C_ADDRESS	0x3C
#define OLED_RESET_GPIO		23
#define MCP_I2C_ADDRESS		0x20
#define MCP_RESET_GPIO		24

// ── Constructor ───────────────────────────────────────────────────────────────

CKernel::CKernel ()
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_USBHCI (&m_Interrupt, &m_Timer, TRUE),
	m_I2CMaster (CMachineInfo::Get ()->GetDevice (DeviceI2CMaster), TRUE),
	m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
	m_Serial (&m_Interrupt),
	m_Display (&m_I2CMaster, OLED_RESET_GPIO, OLED_I2C_ADDRESS),
	m_GUI (&m_Display),
	m_MCP (&m_I2CMaster, MCP_I2C_ADDRESS),
	m_I2SAudio (&m_Interrupt, 48000, MAX_BLOCK * 2),
	m_nPrevPortA (0xFF),
	m_nPrevPortB (0xFF),
	m_nEncAccum {0, 0, 0, 0, 0},
	m_nEncPos {0, 0, 0, 0, 0},
	m_nClickCount {0, 0, 0, 0, 0},
	m_bSerialOK (FALSE),
	m_nSerialBytes (0),
	m_nMidiParseStatus (0),
	m_nMidiParseData0 (0),
	m_nMidiParseIdx (0),
	m_nMidiQWrite (0),
	m_nMidiQRead (0),
	m_bUsbHCIInitialized (FALSE),
	m_bGUIReady (FALSE),
	m_bUsbMidiFound (FALSE),
	m_nDispMidi {0, 0, 0},
	m_nMidiCount (0),
	m_fBPM (120.0f),
	m_nClockSource (0),
	m_nExtClockLastUs (0),
	m_nExtClockDeltaUs (0),
	m_bExtClockValid (false),
	m_nVolume (80),
	m_nFXSlot {1, 0, 0},	// FX1=CloudSeed, FX2=None, FX3=None
	m_bNavHeld (FALSE),
	m_nNavHoldStart (0),
	m_bNavLongFired (FALSE),
	m_nMenuRowCount (0),
	m_nLastDetentTick {0, 0, 0, 0, 0}
{
	m_ActLED.Blink (5);
}

CKernel::~CKernel () {}

// ── Initialize ────────────────────────────────────────────────────────────────

boolean CKernel::Initialize ()
{
	boolean bOK = TRUE;

	// HDMI screen: debug/log output. Non-fatal — headless operation is normal.
	// (The earlier Pi4 "crash" attributed to this call was actually the
	// firmware never starting the kernel at all: missing bcm2711-rpi-4-b.dtb,
	// missing armstub8-rpi4.bin, and an outdated firmware pin. See
	// scripts/deploy-sdcard.sh and config/config.txt.)
	boolean bScreenOK = m_Screen.Initialize ();
	if (bScreenOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (!pTarget) pTarget = &m_Screen;
		m_Logger.Initialize (pTarget);
	}
	// ACT LED diagnostic blinks — 750 ms per blink so each group is easy to
	// count without HDMI.  Report the LAST group you see before it stops.
	// Constructor already did a fast burst.
	#define DIAG_BLINK(n) do { for (unsigned _i=0;_i<(n);_i++) { \
		m_ActLED.On(); CTimer::SimpleMsDelay(750); \
		m_ActLED.Off(); CTimer::SimpleMsDelay(400); } \
		CTimer::SimpleMsDelay(1200); } while(0)
	// DIAG_NOTE: log a message and pause 2 s so it's readable on HDMI.
	#define DIAG_NOTE(msg) do { LOGNOTE(msg); CTimer::SimpleMsDelay(2000); } while(0)

	// ── CRITICAL: Interrupt + Timer — only fatal failures ───────────────────────
	DIAG_NOTE (">> Interrupt init");
	if (bOK) bOK = m_Interrupt.Initialize ();
	DIAG_NOTE (">> Timer init");
	if (bOK) bOK = m_Timer.Initialize ();
	if (bOK) DIAG_BLINK (1);

	// ── USB host: skip entirely (hangs on Pi4 without USB device) ────────────
	DIAG_NOTE (">> USBHCI skipped");
	if (bOK) DIAG_BLINK (2);

	// ── I2C master ────────────────────────────────────────────────────────────
	DIAG_NOTE (">> I2C master init");
	if (bOK && !m_I2CMaster.Initialize ())
		LOGWARN ("I2C master init failed");
	if (bOK) DIAG_BLINK (3);

	// ── EMMC ──────────────────────────────────────────────────────────────────
	DIAG_NOTE (">> EMMC init");
	if (bOK && !m_EMMC.Initialize ())
		LOGWARN ("EMMC init failed");
	if (bOK) DIAG_BLINK (4);

	// ── TRS MIDI UART @31250 ─────────────────────────────────────────────────
	DIAG_NOTE (">> Serial init");
	m_bSerialOK = m_Serial.Initialize (31250);
	if (!m_bSerialOK)
		LOGWARN ("TRS MIDI UART init failed");

	DIAG_NOTE (">> SD mount");
	if (f_mount (&m_FileSystem, DRIVE, 1) != FR_OK)
		LOGWARN ("SD mount failed (presets/config unavailable)");

	LOGNOTE ("Akashic Vault " VERSION " starting");

	// ── OLED via CLVGL: non-fatal ─────────────────────────────────────────────
	DIAG_NOTE (">> OLED Display init");
	boolean bDisplayOK = m_Display.Initialize ();
	DIAG_NOTE (">> LVGL init");
	boolean bGUI_OK    = bDisplayOK && m_GUI.Initialize ();
	m_bGUIReady        = bGUI_OK;
	if (!bDisplayOK) LOGWARN ("OLED display init failed");
	if (bDisplayOK) DIAG_BLINK (5);

	if (bGUI_OK)
	{
		lv_obj_set_style_bg_color   (lv_screen_active (), lv_color_black (), LV_PART_MAIN);
		lv_obj_set_style_bg_opa     (lv_screen_active (), LV_OPA_COVER,     LV_PART_MAIN);
		lv_obj_set_style_text_color (lv_screen_active (), lv_color_white (), LV_PART_MAIN);
	}

	// ── MCP23017 (encoders + buttons): non-fatal ──────────────────────────────
	if (bOK)
	{
		CGPIOPin McpReset (MCP_RESET_GPIO, GPIOModeOutput);
		McpReset.Write (HIGH); m_Timer.MsDelay (10);
		McpReset.Write (LOW);  m_Timer.MsDelay (10);
		McpReset.Write (HIGH); m_Timer.MsDelay (100);
	}
	if (bOK && !m_MCP.Initialize ())
		LOGWARN ("MCP23017 init failed — encoders/buttons unavailable");
	if (bOK)
	{
		m_nPrevPortA = m_MCP.ReadPortA ();
		m_nPrevPortB = m_MCP.ReadPortB ();
	}

	// ── Engine + Plaits generator ─────────────────────────────────────────────
	DIAG_NOTE (">> Engine init");
	m_Engine.Init (48000, MAX_BLOCK);
	DIAG_NOTE (">> Plaits init");
	m_Plaits.Init (48000, MAX_BLOCK);
	m_Engine.SetGeneratorDirect (0, &m_Plaits);
	LOGNOTE ("Plaits loaded");

	// ── Audio FX init ─────────────────────────────────────────────────────────
	DIAG_NOTE (">> CloudSeed init");
	m_CloudSeed.Init (48000, MAX_BLOCK);
	DIAG_NOTE (">> YKChorus init");
	m_YKChorus.Init  (48000, MAX_BLOCK);
	m_nFXSlot[0] = 1;
	m_nFXSlot[1] = 0;
	m_nFXSlot[2] = 0;
	m_Engine.SetAudioFXDirect (0, &m_CloudSeed);
	LOGNOTE ("FX ready");

	// ── MIDI FX init ──────────────────────────────────────────────────────────
	DIAG_NOTE (">> Arp init");
	m_Arp.Init (48000, MAX_BLOCK);
	m_Engine.SetMidiFXDirect (0, &m_Arp);
	m_Engine.SetTempo (120.0f);
	LOGNOTE ("MIDI FX ready");

	// ── I2S audio: starts regardless of display/encoder state ─────────────────
	DIAG_NOTE (">> I2S audio start");
	m_I2SAudio.SetEngine (&m_Engine);
	if (bOK)
	{
		if (m_I2SAudio.Start ())
		{
			m_I2SAudio.SetVolume ((float) m_nVolume / 100.0f);

			// Audio ping: play middle-C for 1.5 s so we can confirm I2S
			// is producing output independently of MIDI input.
			TMidiEvent noteOn;
			noteOn.Type     = MidiType::NoteOn;
			noteOn.nChannel = 0;
			noteOn.nData1   = 60;	// middle C
			noteOn.nData2   = 100;
			m_Engine.PushMidi (noteOn);
			CTimer::SimpleMsDelay (1500);
			TMidiEvent noteOff;
			noteOff.Type     = MidiType::NoteOff;
			noteOff.nChannel = 0;
			noteOff.nData1   = 60;
			noteOff.nData2   = 0;
			m_Engine.PushMidi (noteOff);

			DIAG_BLINK (6);		// ── diag 6: I2S audio running
		}
		else
		{
			LOGWARN ("I2S audio start failed");
		}
	}

	// ── 4-row UI: only if OLED + LVGL initialised ────────────────────────────
	if (bGUI_OK)
	{
		DIAG_NOTE (">> BuildMenus");
		BuildMenus ();
		m_UI.Init (&m_PageOSRoot);
	}

	LOGNOTE (">> Initialize complete bOK=%d bGUI=%d", bOK ? 1 : 0, bGUI_OK ? 1 : 0);
	CTimer::SimpleMsDelay (2000);

	// If anything fatal failed, freeze the HDMI screen so the log stays visible.
	if (!bOK)
	{
		LOGERR ("BOOT FAILED — screen frozen for diagnosis");
		for (;;)
			;
	}

	return bOK;
}

// ── BuildMenus ────────────────────────────────────────────────────────────────
// Constructs the static menu tree pointing to m_Plaits params.
// Plaits menu.json pages:
//   Main  — engine, harmonics, timbre, morph     (params 0-3)
//   Tone  — decay, lpg_colour                    (params 4-5)
//   Mod   — fm_amt, timbre_mod, morph_mod        (params 6-8)

// ── Menu row factory helpers ──────────────────────────────────────────────────

TMenuRow *CKernel::AllocRow ()
{
	if (m_nMenuRowCount >= MAX_MENU_ROWS)
		return nullptr;
	TMenuRow *r = &m_MenuRows[m_nMenuRowCount++];
	memset (r, 0, sizeof (*r));
	return r;
}

void CKernel::MakeParamRow (TMenuPage *page, IModule *m, unsigned idx)
{
	TMenuRow *r = AllocRow (); if (!r) return;
	r->type        = TMenuRowType::Property;
	r->pLabel      = m->ParamDesc (idx).pLabel;
	r->pModule     = m;
	r->nParamIndex = idx;
	page->pRows[page->nRows++] = r;
}

void CKernel::MakeMenuRow (TMenuPage *page, const char *label, TMenuPage *child)
{
	TMenuRow *r = AllocRow (); if (!r) return;
	r->type       = TMenuRowType::MenuItem;
	r->pLabel     = label;
	r->bHasAction = true;
	r->pChildPage = child;
	page->pRows[page->nRows++] = r;
}

void CKernel::MakeFreeRow (TMenuPage *page, const char *label,
			    void (*pfAdj)(void*, int), void (*pfGet)(void*, char*, unsigned),
			    void *ctx)
{
	TMenuRow *r = AllocRow (); if (!r) return;
	r->type       = TMenuRowType::Property;
	r->pLabel     = label;
	r->pfAdjust   = pfAdj;
	r->pfGetStr   = pfGet;
	r->pFreeCtx   = ctx;
	page->pRows[page->nRows++] = r;
}

void CKernel::MakeReadOnlyRow (TMenuPage *page, const char *label, const char *value)
{
	TMenuRow *r = AllocRow (); if (!r) return;
	r->type         = TMenuRowType::ReadOnly;
	r->pLabel       = label;
	r->pStaticValue = value;
	page->pRows[page->nRows++] = r;
}

static void InitPage (TMenuPage *p, const char *name, TMenuPage *parent)
{
	memset (p, 0, sizeof (*p));
	p->pName   = name;
	p->pParent = parent;
}

// ── Volume callbacks ──────────────────────────────────────────────────────────

void CKernel::VolumeAdjust (void *pCtx, int nDelta)
{
	CKernel *pThis = static_cast<CKernel *> (pCtx);
	pThis->AdjustVolume (nDelta);
}

void CKernel::VolumeGetStr (void *pCtx, char *pBuf, unsigned nMax)
{
	CKernel *pThis = static_cast<CKernel *> (pCtx);
	snprintf (pBuf, nMax, "%u%%", pThis->m_nVolume);
}

void CKernel::AdjustVolume (int nDelta)
{
	int v = (int) m_nVolume + nDelta;
	if (v < 0)   v = 0;
	if (v > 100) v = 100;
	m_nVolume = (unsigned) v;
	m_I2SAudio.SetVolume ((float) m_nVolume / 100.0f);
}

// ── BPM + clock source callbacks ─────────────────────────────────────────────

void CKernel::BPMAdjust (void *pCtx, int nDelta)
{
	CKernel *pThis = static_cast<CKernel *> (pCtx);
	float fNew = pThis->m_fBPM + (float) nDelta;
	if (fNew < 20.0f)  fNew = 20.0f;
	if (fNew > 300.0f) fNew = 300.0f;
	pThis->m_fBPM = fNew;
	if (pThis->m_nClockSource == 0)		// only push if in internal mode
		pThis->m_Engine.SetTempo (pThis->m_fBPM);
}

void CKernel::BPMGetStr (void *pCtx, char *pBuf, unsigned nMax)
{
	CKernel *pThis = static_cast<CKernel *> (pCtx);
	snprintf (pBuf, nMax, "%.0f", pThis->m_fBPM);
}

void CKernel::ClockSrcAdjust (void *pCtx, int /*nDelta*/)
{
	// Toggle between Internal (0) and External MIDI (1).
	CKernel *pThis = static_cast<CKernel *> (pCtx);
	pThis->m_nClockSource = (pThis->m_nClockSource == 0) ? 1 : 0;
	if (pThis->m_nClockSource == 0)
	{
		pThis->m_Engine.SetClockSource (ClockSource::Internal);
		pThis->m_Engine.SetTempo (pThis->m_fBPM);
	}
	else
	{
		pThis->m_bExtClockValid   = false;
		pThis->m_nExtClockDeltaUs = 0;
		// Keep current BPM estimate running until first clock arrives.
	}
}

void CKernel::ClockSrcGetStr (void *pCtx, char *pBuf, unsigned nMax)
{
	CKernel *pThis = static_cast<CKernel *> (pCtx);
	snprintf (pBuf, nMax, "%s", pThis->m_nClockSource == 0 ? "Internal" : "Ext MIDI");
}

// Derive BPM from 0xF8 inter-pulse timing (24 clocks per quarter note).
void CKernel::HandleMidiClock ()
{
	uint32_t nNow = m_Timer.GetClockTicks ();
	if (m_bExtClockValid)
	{
		uint32_t nDelta = nNow - m_nExtClockLastUs;
		// Rolling 8-pulse average for stability.
		if (m_nExtClockDeltaUs == 0)
			m_nExtClockDeltaUs = nDelta;
		else
			m_nExtClockDeltaUs = (m_nExtClockDeltaUs * 7 + nDelta) / 8;

		// 24 MIDI clocks per quarter note.
		float fBPM = 60000000.0f / ((float) m_nExtClockDeltaUs * 24.0f);
		if (fBPM >= 20.0f && fBPM <= 300.0f)
		{
			m_fBPM = fBPM;
			m_Engine.SetTempo (fBPM);
		}
	}
	m_nExtClockLastUs = nNow;
	m_bExtClockValid  = true;
}

// ── Mod param callbacks ───────────────────────────────────────────────────────
//
// Sources 0-1 = LFO 0-1,  sources 2-3 = Env 0-1.
// LFO params: 0=Rate(Hz) 1=Shape 2=Depth 3=Target
// Env params: 0=Attack(ms) 1=Decay(ms) 2=Depth 3=Target

void CKernel::ModParamAdjust (void *pCtx, int nDelta)
{
	auto *c = static_cast<TModParamCtx *> (pCtx);
	CKernel *pK = c->pKernel;
	unsigned s  = c->nSrc;
	unsigned p  = c->nParam;

	if (s < 2)
	{
		// ── LFO ──────────────────────────────────────────────────────────
		// params: 0=Sync  1=Rate(Hz)/Division  2=Shape  3=Depth  4=Target
		CLFO &lfo = pK->m_ModRouter.LFO (s);
		switch (p)
		{
		case 0:	// Sync toggle
			lfo.SetSync (!lfo.GetSync ());
			break;
		case 1:
			if (lfo.GetSync ())
			{
				// Division — step through sync table
				int d = (int) lfo.GetDivision () + nDelta;
				int n = (int) kNumSyncDivisions;
				d = ((d % n) + n) % n;
				lfo.SetDivision ((unsigned) d);
			}
			else
			{
				// Free Hz — step 0.1 Hz
				lfo.SetRate (lfo.GetRate () + (float) nDelta * 0.1f);
			}
			break;
		case 2:	// Shape
		{
			int sh = (int) lfo.GetShape () + nDelta;
			int n  = (int) CLFO::Shape::NUM_SHAPES;
			sh = ((sh % n) + n) % n;
			lfo.SetShape ((CLFO::Shape) sh);
			break;
		}
		case 3:	// Depth — step 1%
			lfo.SetDepth (lfo.GetDepth () + (float) nDelta * 0.01f);
			break;
		case 4:	// Target
		{
			int t = (int) pK->m_ModRouter.GetLFOTarget (s) + nDelta;
			int n = (int) ModTarget::NUM_TARGETS;
			t = ((t % n) + n) % n;
			pK->m_ModRouter.SetLFOTarget (s, (ModTarget) t);
			break;
		}
		}
	}
	else
	{
		// ── Cyclic Envelope ───────────────────────────────────────────────
		// params: 0=Sync  1=Atk(ms)/Div  2=Dec(ms)/Div  3=Depth  4=Target
		unsigned e = s - 2;
		CCyclicEnv &env = pK->m_ModRouter.Env (e);
		switch (p)
		{
		case 0:	// Sync toggle
			env.SetSync (!env.GetSync ());
			break;
		case 1:
			if (env.GetSync ())
			{
				int d = (int) env.GetDivAtk () + nDelta;
				int n = (int) kNumSyncDivisions;
				d = ((d % n) + n) % n;
				env.SetDivAtk ((unsigned) d);
			}
			else
			{
				env.SetAttack (env.GetAttack () + (float) nDelta * 10.0f);
			}
			break;
		case 2:
			if (env.GetSync ())
			{
				int d = (int) env.GetDivDec () + nDelta;
				int n = (int) kNumSyncDivisions;
				d = ((d % n) + n) % n;
				env.SetDivDec ((unsigned) d);
			}
			else
			{
				env.SetDecay (env.GetDecay () + (float) nDelta * 10.0f);
			}
			break;
		case 3:	// Depth — step 1%
			env.SetDepth (env.GetDepth () + (float) nDelta * 0.01f);
			break;
		case 4:	// Target
		{
			int t = (int) pK->m_ModRouter.GetEnvTarget (e) + nDelta;
			int n = (int) ModTarget::NUM_TARGETS;
			t = ((t % n) + n) % n;
			pK->m_ModRouter.SetEnvTarget (e, (ModTarget) t);
			break;
		}
		}
	}
}

void CKernel::ModParamGetStr (void *pCtx, char *pBuf, unsigned nMax)
{
	auto *c = static_cast<TModParamCtx *> (pCtx);
	CKernel *pK = c->pKernel;
	unsigned s  = c->nSrc;
	unsigned p  = c->nParam;

	if (s < 2)
	{
		CLFO &lfo = pK->m_ModRouter.LFO (s);
		switch (p)
		{
		case 0: snprintf (pBuf, nMax, "%s", lfo.GetSync () ? "On" : "Off"); break;
		case 1:
			if (lfo.GetSync ())
				snprintf (pBuf, nMax, "%s", kSyncDivNames[lfo.GetDivision ()]);
			else
				snprintf (pBuf, nMax, "%.2f Hz", lfo.GetRate ());
			break;
		case 2: snprintf (pBuf, nMax, "%s", CLFO::ShapeNames[(unsigned) lfo.GetShape ()]); break;
		case 3: snprintf (pBuf, nMax, "%u%%", (unsigned)(lfo.GetDepth () * 100.0f + 0.5f)); break;
		case 4: snprintf (pBuf, nMax, "%s", kModTargetNames[(unsigned) pK->m_ModRouter.GetLFOTarget (s)]); break;
		}
	}
	else
	{
		unsigned e = s - 2;
		CCyclicEnv &env = pK->m_ModRouter.Env (e);
		switch (p)
		{
		case 0: snprintf (pBuf, nMax, "%s", env.GetSync () ? "On" : "Off"); break;
		case 1:
			if (env.GetSync ())
				snprintf (pBuf, nMax, "%s", kSyncDivNames[env.GetDivAtk ()]);
			else
				snprintf (pBuf, nMax, "%.0f ms", env.GetAttack ());
			break;
		case 2:
			if (env.GetSync ())
				snprintf (pBuf, nMax, "%s", kSyncDivNames[env.GetDivDec ()]);
			else
				snprintf (pBuf, nMax, "%.0f ms", env.GetDecay ());
			break;
		case 3: snprintf (pBuf, nMax, "%u%%", (unsigned)(env.GetDepth () * 100.0f + 0.5f)); break;
		case 4: snprintf (pBuf, nMax, "%s", kModTargetNames[(unsigned) pK->m_ModRouter.GetEnvTarget (e)]); break;
		}
	}
}

// ── FX slot selection ─────────────────────────────────────────────────────────

static const char *const s_FXNames[] = { "None", "CloudSeed", "YKChorus" };
static constexpr unsigned NUM_FX_ALGOS = 3;

void CKernel::ApplyFXSlot (unsigned nSlot)
{
	IAudioFX *pFX = nullptr;
	switch (m_nFXSlot[nSlot])
	{
		case 1: pFX = &m_CloudSeed; break;
		case 2: pFX = &m_YKChorus;  break;
		default: break;
	}
	m_Engine.SetAudioFXDirect (nSlot, pFX);
}

void CKernel::AdjustFXSlot (unsigned nSlot, int nDelta)
{
	unsigned &sel = m_nFXSlot[nSlot];
	if (nDelta > 0)
		sel = (sel + 1) % NUM_FX_ALGOS;
	else if (nDelta < 0)
		sel = (sel == 0) ? NUM_FX_ALGOS - 1 : sel - 1;
	ApplyFXSlot (nSlot);
}

const char *CKernel::GetFXSlotName (unsigned nSlot) const
{
	return s_FXNames[m_nFXSlot[nSlot] < NUM_FX_ALGOS ? m_nFXSlot[nSlot] : 0];
}

void CKernel::NavigateToFXParams (unsigned nSlot)
{
	switch (m_nFXSlot[nSlot])
	{
		case 1: m_UI.NavigateToPage (&m_PageCloudSeed); break;
		case 2: m_UI.NavigateToPage (&m_PageYKChorus);  break;
		default: break;
	}
}

void CKernel::FXSlotAdjust (void *pCtx, int nDelta)
{
	static_cast<TFXSlotCtx *> (pCtx)->pKernel->AdjustFXSlot (
		static_cast<TFXSlotCtx *> (pCtx)->nSlot, nDelta);
}

void CKernel::FXSlotGetStr (void *pCtx, char *pBuf, unsigned nMax)
{
	auto *c = static_cast<TFXSlotCtx *> (pCtx);
	snprintf (pBuf, nMax, "%s", c->pKernel->GetFXSlotName (c->nSlot));
}

void CKernel::FXSlotAction (void *pCtx)
{
	auto *c = static_cast<TFXSlotCtx *> (pCtx);
	c->pKernel->NavigateToFXParams (c->nSlot);
}

// ── BuildMenus ────────────────────────────────────────────────────────────────

void CKernel::BuildMenus ()
{
	m_nMenuRowCount = 0;

	// ── Tone sub-page ─────────────────────────────────────────────────────
	InitPage (&m_PageTone, "Tone", &m_PageSoundGen);
	MakeParamRow (&m_PageTone, &m_Plaits, 4);	// decay
	MakeParamRow (&m_PageTone, &m_Plaits, 5);	// lpg_colour

	// ── Mod sub-page ──────────────────────────────────────────────────────
	InitPage (&m_PageMod, "Mod", &m_PageSoundGen);
	MakeParamRow (&m_PageMod, &m_Plaits, 6);	// fm_amt
	MakeParamRow (&m_PageMod, &m_Plaits, 7);	// timbre_mod
	MakeParamRow (&m_PageMod, &m_Plaits, 8);	// morph_mod

	// ── Sound Generator page ──────────────────────────────────────────────
	InitPage (&m_PageSoundGen, "Sound Generator", &m_PageOSRoot);
	MakeReadOnlyRow (&m_PageSoundGen, "SG",         "Plaits");	// selector (v1 read-only)
	MakeParamRow    (&m_PageSoundGen, &m_Plaits, 0);		// engine
	MakeParamRow    (&m_PageSoundGen, &m_Plaits, 2);		// timbre
	MakeParamRow    (&m_PageSoundGen, &m_Plaits, 3);		// morph
	MakeParamRow    (&m_PageSoundGen, &m_Plaits, 1);		// harmonics
	MakeMenuRow     (&m_PageSoundGen, "Tone",    &m_PageTone);
	MakeMenuRow     (&m_PageSoundGen, "Mod",     &m_PageMod);

	// ── CloudSeed param page (all params, scrollable) ────────────────────
	InitPage (&m_PageCloudSeed, "CloudSeed", &m_PageFXChain);
	for (unsigned i = 0; i < m_CloudSeed.NumParams (); i++)
		MakeParamRow (&m_PageCloudSeed, &m_CloudSeed, i);

	// ── YKChorus param page ───────────────────────────────────────────────
	InitPage (&m_PageYKChorus, "YKChorus", &m_PageFXChain);
	for (unsigned i = 0; i < m_YKChorus.NumParams (); i++)
		MakeParamRow (&m_PageYKChorus, &m_YKChorus, i);

	// ── FX Chain page — 3 selector rows ──────────────────────────────────
	// Encoder cycles through None/CloudSeed/YKChorus and immediately wires
	// the engine. Encoder click navigates to that algorithm's param page.
	InitPage (&m_PageFXChain, "FX Chain", &m_PageOSRoot);
	static const char *s_FXSlotLabels[3] = { "FX1", "FX2", "FX3" };
	for (unsigned i = 0; i < 3; i++)
	{
		m_FXSlotCtx[i] = { this, i };
		TMenuRow *r = AllocRow (); if (!r) break;
		r->type       = TMenuRowType::Property;
		r->pLabel     = s_FXSlotLabels[i];
		r->bHasAction = true;
		r->pfAdjust   = FXSlotAdjust;
		r->pfGetStr   = FXSlotGetStr;
		r->pFreeCtx   = &m_FXSlotCtx[i];
		r->pfAction   = FXSlotAction;
		r->pActionCtx = &m_FXSlotCtx[i];
		m_PageFXChain.pRows[m_PageFXChain.nRows++] = r;
	}

	// ── Arpeggiator param page (all params, scrollable) ──────────────────
	InitPage (&m_PageArp, "Arpeggiator", &m_PageMidiFX);
	for (unsigned i = 0; i < m_Arp.NumParams (); i++)
		MakeParamRow (&m_PageArp, &m_Arp, i);

	// ── MIDI FX page ──────────────────────────────────────────────────────
	// "Arp" toggles the module on/off right here (param 0 = enable); diving
	// into "Arp Settings" reaches Mode/Octaves/Rate/Gate. Chord is a stub —
	// next on the Phase 5 porting list (Eucalypso / chord generator).
	InitPage (&m_PageMidiFX, "MIDI FX", &m_PageOSRoot);
	MakeParamRow    (&m_PageMidiFX, &m_Arp, 0);			// enable (On/Off)
	MakeMenuRow     (&m_PageMidiFX, "Arp Settings", &m_PageArp);
	MakeReadOnlyRow (&m_PageMidiFX, "Chord",        "Off");

	// ── Presets page (stubs) ──────────────────────────────────────────────
	InitPage (&m_PagePresets, "Presets", &m_PageOSRoot);
	MakeReadOnlyRow (&m_PagePresets, "Save", "-");
	MakeReadOnlyRow (&m_PagePresets, "Load", "-");

	// ── Settings page ─────────────────────────────────────────────────────
	InitPage (&m_PageSettings, "Settings", &m_PageOSRoot);
	MakeFreeRow  (&m_PageSettings, "BPM",       BPMAdjust,     BPMGetStr,     this);
	MakeFreeRow  (&m_PageSettings, "Clock Src", ClockSrcAdjust, ClockSrcGetStr, this);
	MakeReadOnlyRow (&m_PageSettings, "MIDI Ch",    "All");
	MakeReadOnlyRow (&m_PageSettings, "Pitchbend",  "+/-2");
	MakeReadOnlyRow (&m_PageSettings, "Version",    VERSION);

	// ── Mod router pages ──────────────────────────────────────────────────
	// LFO sub-pages (0=LFO1, 1=LFO2); Env sub-pages (0=Env1, 1=Env2).
	// Each has 4 rows: Rate/Atk, Shape/Dec, Depth, Target.
	// LFO pages: Sync / Rate (Hz or div) / Shape / Depth / Target  — 5 rows
	// Env pages: Sync / Attack (ms or div) / Decay (ms or div) / Depth / Target — 5 rows
	static const char *const s_LFOParamLabels[] = { "Sync", "Rate", "Shape", "Depth", "Target" };
	static const char *const s_EnvParamLabels[] = { "Sync", "Attack", "Decay", "Depth", "Target" };
	static const char *const s_LFOPageNames[]   = { "LFO 1", "LFO 2" };
	static const char *const s_EnvPageNames[]   = { "Env 1", "Env 2" };

	InitPage (&m_PageModMain, "Mod", &m_PageOSRoot);
	for (unsigned i = 0; i < 2; i++)
	{
		InitPage (&m_PageModLFO[i], s_LFOPageNames[i], &m_PageModMain);
		for (unsigned p = 0; p < 5; p++)
		{
			m_ModParamCtx[i][p] = { this, i, p };
			MakeFreeRow (&m_PageModLFO[i], s_LFOParamLabels[p],
				     ModParamAdjust, ModParamGetStr, &m_ModParamCtx[i][p]);
		}
		MakeMenuRow (&m_PageModMain, s_LFOPageNames[i], &m_PageModLFO[i]);
	}
	for (unsigned i = 0; i < 2; i++)
	{
		InitPage (&m_PageModEnv[i], s_EnvPageNames[i], &m_PageModMain);
		for (unsigned p = 0; p < 5; p++)
		{
			m_ModParamCtx[i + 2][p] = { this, i + 2, p };
			MakeFreeRow (&m_PageModEnv[i], s_EnvParamLabels[p],
				     ModParamAdjust, ModParamGetStr, &m_ModParamCtx[i + 2][p]);
		}
		MakeMenuRow (&m_PageModMain, s_EnvPageNames[i], &m_PageModEnv[i]);
	}

	// ── OS Root ───────────────────────────────────────────────────────────
	InitPage (&m_PageOSRoot, "AV-OS", nullptr);
	MakeFreeRow  (&m_PageOSRoot, "Volume",          VolumeAdjust, VolumeGetStr, this);
	MakeMenuRow  (&m_PageOSRoot, "Sound Generator", &m_PageSoundGen);
	MakeMenuRow  (&m_PageOSRoot, "FX Chain",        &m_PageFXChain);
	MakeMenuRow  (&m_PageOSRoot, "MIDI FX",         &m_PageMidiFX);
	MakeMenuRow  (&m_PageOSRoot, "Mod",             &m_PageModMain);
	MakeMenuRow  (&m_PageOSRoot, "Presets",         &m_PagePresets);
	MakeMenuRow  (&m_PageOSRoot, "Settings",        &m_PageSettings);
}

// ── Run ───────────────────────────────────────────────────────────────────────

TShutdownMode CKernel::Run ()
{
	LOGNOTE ("Compile time: " __DATE__ " " __TIME__);

	for (;;)
	{
		// USB host plug-and-play: only call after Initialize() succeeded.
		// On Pi4 without a USB device, CXHCIDevice::Initialize() blocks
		// indefinitely — we skip it entirely and rely on TRS MIDI instead.
		// m_bUsbHCIInitialized is set true if Initialize() was called and
		// returned; UpdatePlugAndPlay() is safe only in that case.
		if (m_bUsbHCIInitialized)
			m_USBHCI.UpdatePlugAndPlay ();

		PollMidi ();
		PollInput ();
		{
			uint32_t nNow = m_Timer.GetClockTicks ();
			m_Engine.Tick (nNow);				// drives clock-synced MIDI FX (arp)
			m_ModRouter.Update (nNow, m_fBPM, &m_Plaits);	// LFO/env → Plaits live mod
		}
		if (m_bGUIReady)
		{
			m_UI.Draw ();
			m_GUI.Update ();
		}
		m_Scheduler.Yield ();
	}

	return ShutdownHalt;
}

// ── PollInput — MCP23017 quadrature decode + button edge detection ────────────

static const int s_EncTable[16] =
{
	 0, -1,  1,  0,
	 1,  0,  0, -1,
	-1,  0,  0,  1,
	 0,  1, -1,  0
};

// PEC11R-4020F-S0024: 24 detents/rev, 2 quadrature transitions per detent.
#define STEPS_PER_DETENT	2

// Encoder acceleration: map inter-detent interval (CTimer ticks @ 100Hz)
// to a step multiplier for value encoders.
// Nav encoder is never accelerated — scroll jumps would feel wrong.
static int EncAccelMultiplier (unsigned nIntervalTicks)
{
	if (nIntervalTicks < 2)  return 10;	// < 20ms  — very fast
	if (nIntervalTicks < 5)  return 5;	//  20-50ms — fast
	if (nIntervalTicks < 15) return 2;	//  50-150ms — medium
	return 1;				// > 150ms  — slow / normal
}

void CKernel::PollInput ()
{
	u8 portA = m_MCP.ReadPortA ();
	u8 portB = m_MCP.ReadPortB ();

	// ── Value encoders (Port B) — with acceleration ───────────────────────────
	for (unsigned i = 0; i < 4; i++)
	{
		unsigned chA_cur  = (portB         >> (i * 2 + 1)) & 1;
		unsigned chB_cur  = (portB         >> (i * 2))     & 1;
		unsigned chA_prev = (m_nPrevPortB  >> (i * 2 + 1)) & 1;
		unsigned chB_prev = (m_nPrevPortB  >> (i * 2))     & 1;
		unsigned curr     = (chA_cur  << 1) | chB_cur;
		unsigned prev     = (chA_prev << 1) | chB_prev;
		int dir           = -s_EncTable[(prev << 2) | curr];	// reversed

		m_nEncAccum[i] += dir;
		int sign = 0;
		if (m_nEncAccum[i] >= STEPS_PER_DETENT)
		{
			sign = +1;
			m_nEncPos[i]++;
			m_nEncAccum[i] = 0;
		}
		else if (m_nEncAccum[i] <= -STEPS_PER_DETENT)
		{
			sign = -1;
			m_nEncPos[i]--;
			m_nEncAccum[i] = 0;
		}

		if (sign != 0)
		{
			unsigned now      = m_Timer.GetTicks ();
			unsigned interval = now - m_nLastDetentTick[i];
			m_nLastDetentTick[i] = now;
			int multiplier    = EncAccelMultiplier (interval);
			m_UI.EncoderDelta (i, sign * multiplier);
		}
	}

	// ── Nav encoder (Port A, GPA5=ChA, GPA6=ChB) — no acceleration ───────────
	{
		unsigned chA_cur  = (portA        >> 5) & 1;
		unsigned chB_cur  = (portA        >> 6) & 1;
		unsigned chA_prev = (m_nPrevPortA >> 5) & 1;
		unsigned chB_prev = (m_nPrevPortA >> 6) & 1;
		unsigned curr     = (chA_cur  << 1) | chB_cur;
		unsigned prev     = (chA_prev << 1) | chB_prev;
		int dir           = -s_EncTable[(prev << 2) | curr];	// reversed

		m_nEncAccum[4] += dir;
		if (m_nEncAccum[4] >= STEPS_PER_DETENT)
		{
			m_nEncPos[4]++;
			m_nEncAccum[4] = 0;
			m_UI.NavDown ();
		}
		else if (m_nEncAccum[4] <= -STEPS_PER_DETENT)
		{
			m_nEncPos[4]--;
			m_nEncAccum[4] = 0;
			m_UI.NavUp ();
		}
	}

	// ── Button edges (falling = press, active-low) ────────────────────────────
	u8 pressed = m_nPrevPortA & ~portA;

	// GPA3=Enc1 click, GPA2=Enc2, GPA1=Enc3, GPA0=Enc4
	for (unsigned i = 0; i < 4; i++)
	{
		if (pressed & (1u << (3 - i)))
		{
			m_nClickCount[i]++;
			m_UI.EncoderClick (i);
		}
	}
	// GPA4 = nav encoder click → short=BACK, long=OS root
	// Detect both falling edge (press) and rising edge (release).
	bool navBit      = !(portA & (1u << 4));		// true = held down
	bool navWasDown  = !(m_nPrevPortA & (1u << 4));

	if (navBit && !navWasDown)
	{
		// Falling edge — button just pressed.
		m_bNavHeld      = TRUE;
		m_nNavHoldStart = m_Timer.GetTicks ();
		m_bNavLongFired = FALSE;
	}
	else if (!navBit && navWasDown)
	{
		// Rising edge — button released.
		if (m_bNavHeld && !m_bNavLongFired)
		{
			// Short press → BACK.
			m_nClickCount[4]++;
			m_UI.NavBack ();
		}
		m_bNavHeld = FALSE;
	}
	else if (navBit && m_bNavHeld && !m_bNavLongFired)
	{
		// Still held — check for long press (500ms = 50 ticks at 100Hz).
		if (m_Timer.GetTicks () - m_nNavHoldStart >= 50)
		{
			m_bNavLongFired = TRUE;
			m_UI.GoToRoot ();
		}
	}

	m_nPrevPortA = portA;
	m_nPrevPortB = portB;
}

// ── PollMidi ──────────────────────────────────────────────────────────────────

static unsigned MidiMsgLen (u8 status)
{
	if (status < 0x80)  return 0;
	if (status < 0xC0)  return 3;
	if (status < 0xE0)  return 2;
	if (status < 0xF0)  return 3;
	switch (status)
	{
		case 0xF8: case 0xFA: case 0xFB: case 0xFC: return 1;
		default:   return 0;
	}
}

void CKernel::DispatchMidi (u8 status, u8 d1, u8 d2)
{
	TMidiEvent ev;
	ev.nData1     = d1;
	ev.nData2     = d2;
	ev.nTimeStamp = 0;

	// System-realtime messages (0xF8-0xFF) carry no channel nibble — the status
	// byte IS the full type.  Channel-voice / system-common use the upper nibble.
	if (status >= 0xF8)
	{
		ev.Type     = static_cast<MidiType> (status);
		ev.nChannel = 0;

		// MIDI clock: used for external BPM sync when clock source = External.
		if (status == 0xF8 && m_nClockSource == 1)
			HandleMidiClock ();
		// MIDI Start: reset the clock estimator so BPM derivation is fresh.
		if (status == 0xFA)
		{
			m_bExtClockValid   = false;
			m_nExtClockDeltaUs = 0;
		}
	}
	else
	{
		ev.Type     = static_cast<MidiType> (status & 0xF0);
		ev.nChannel = status & 0x0F;
	}

	m_Engine.PushMidi (ev);

	m_nDispMidi[0] = status;
	m_nDispMidi[1] = d1;
	m_nDispMidi[2] = d2;
	m_nMidiCount++;
}

void CKernel::PollMidi ()
{
	// ── Lazy USB MIDI device discovery ───────────────────────────────────────
	if (!m_bUsbMidiFound)
	{
		CUSBMIDIDevice *pDev = static_cast<CUSBMIDIDevice *> (
			CDeviceNameService::Get ()->GetDevice ("umidi1", FALSE));
		if (pDev)
		{
			pDev->RegisterPacketHandler (UsbMidiPacketHandler, this);
			m_bUsbMidiFound = TRUE;
			LOGNOTE ("USB MIDI connected");
		}
	}

	// ── Drain USB MIDI FIFO (all queued events, not just the latest) ─────────
	// The USB callback is SPSC-safe: it only advances m_nMidiQWrite and we
	// only advance m_nMidiQRead, so no lock is needed — just a memory barrier
	// to ensure we read packet bytes before the write-index update is visible.
	while (m_nMidiQRead != m_nMidiQWrite)
	{
		__sync_synchronize ();		// load-acquire: see packet bytes before index
		unsigned nR = m_nMidiQRead;
		DispatchMidi (m_MidiQueue[nR][0], m_MidiQueue[nR][1], m_MidiQueue[nR][2]);
		m_nMidiQRead = (nR + 1) & (MIDI_QUEUE_SIZE - 1);
	}

	// ── Drain TRS UART ring buffer ────────────────────────────────────────────
	if (!m_bSerialOK)
		return;

	u8 byte;
	int n;
	while ((n = m_Serial.Read (&byte, 1)) == 1)
	{
		m_nSerialBytes++;

		if (byte & 0x80)
		{
			m_nMidiParseStatus = byte;
			m_nMidiParseIdx    = 1;
			if (MidiMsgLen (byte) == 1)
				DispatchMidi (byte, 0, 0);
		}
		else if (m_nMidiParseStatus)
		{
			unsigned len = MidiMsgLen (m_nMidiParseStatus);
			if (m_nMidiParseIdx == 1)
			{
				m_nMidiParseData0 = byte;
				m_nMidiParseIdx   = 2;
				if (len == 2)
				{
					DispatchMidi (m_nMidiParseStatus, byte, 0);
					m_nMidiParseIdx = 1;
				}
			}
			else if (m_nMidiParseIdx == 2 && len == 3)
			{
				DispatchMidi (m_nMidiParseStatus, m_nMidiParseData0, byte);
				m_nMidiParseIdx = 1;
			}
		}
	}
}

// ── USB MIDI packet handler (USB interrupt context) ───────────────────────────

void CKernel::UsbMidiPacketHandler (unsigned /*nCable*/, u8 *pPacket,
				     unsigned nLength, unsigned /*nDevice*/,
				     void *pParam)
{
	if (nLength < 1 || !(pPacket[0] & 0x80))
		return;

	CKernel *pThis = static_cast<CKernel *> (pParam);

	unsigned nW    = pThis->m_nMidiQWrite;
	unsigned nNext = (nW + 1) & (MIDI_QUEUE_SIZE - 1);

	if (nNext == pThis->m_nMidiQRead)
		return;		// queue full — drop rather than corrupt

	pThis->m_MidiQueue[nW][0] = pPacket[0];
	pThis->m_MidiQueue[nW][1] = nLength > 1 ? pPacket[1] : 0;
	pThis->m_MidiQueue[nW][2] = nLength > 2 ? pPacket[2] : 0;
	__sync_synchronize ();		// store-release: ensure bytes visible before index
	pThis->m_nMidiQWrite = nNext;
}
