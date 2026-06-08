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
	m_bMidiPending (FALSE),
	m_nLastMidi {0, 0, 0},
	m_bUsbMidiFound (FALSE),
	m_nDispMidi {0, 0, 0},
	m_nMidiCount (0),
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

	if (bOK) bOK = m_Screen.Initialize ();
	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (!pTarget) pTarget = &m_Screen;
		bOK = m_Logger.Initialize (pTarget);
	}
	if (bOK) bOK = m_Interrupt.Initialize ();
	if (bOK) bOK = m_Timer.Initialize ();
	if (bOK) bOK = m_USBHCI.Initialize ();
	if (bOK) bOK = m_I2CMaster.Initialize ();
	if (bOK) bOK = m_EMMC.Initialize ();

	// ── TRS MIDI UART @31250 (interrupt-buffered; Read() in PollMidi) ─────────
	m_bSerialOK = m_Serial.Initialize (31250);
	if (!m_bSerialOK)
		LOGWARN ("TRS MIDI UART init failed");

	if (bOK && f_mount (&m_FileSystem, DRIVE, 1) != FR_OK)
		LOGWARN ("SD mount failed (presets/config unavailable)");

	LOGNOTE ("Akashic Vault " VERSION " starting");

	// ── OLED via CLVGL ────────────────────────────────────────────────────────
	if (bOK) bOK = m_Display.Initialize ();
	if (bOK) bOK = m_GUI.Initialize ();

	if (bOK)
	{
		lv_obj_set_style_bg_color   (lv_screen_active (), lv_color_black (), LV_PART_MAIN);
		lv_obj_set_style_bg_opa     (lv_screen_active (), LV_OPA_COVER,     LV_PART_MAIN);
		lv_obj_set_style_text_color (lv_screen_active (), lv_color_white (), LV_PART_MAIN);
	}

	// ── MCP23017 (encoders + buttons) ─────────────────────────────────────────
	{
		CGPIOPin McpReset (MCP_RESET_GPIO, GPIOModeOutput);
		McpReset.Write (HIGH); m_Timer.MsDelay (10);
		McpReset.Write (LOW);  m_Timer.MsDelay (10);
		McpReset.Write (HIGH); m_Timer.MsDelay (100);
	}
	if (bOK) bOK = m_MCP.Initialize ();
	if (bOK)
	{
		m_nPrevPortA = m_MCP.ReadPortA ();
		m_nPrevPortB = m_MCP.ReadPortB ();
	}

	// ── Engine + Plaits generator ─────────────────────────────────────────────
	m_Engine.Init (48000, MAX_BLOCK);
	m_Plaits.Init (48000, MAX_BLOCK);
	m_Engine.SetGeneratorDirect (0, &m_Plaits);
	LOGNOTE ("Plaits loaded — 24 engines ready");

	// ── Audio FX init (FX slot 0 = CloudSeed by default) ─────────────────────
	m_CloudSeed.Init (48000, MAX_BLOCK);
	m_YKChorus.Init  (48000, MAX_BLOCK);
	m_nFXSlot[0] = 1;	// CloudSeed
	m_nFXSlot[1] = 0;	// None
	m_nFXSlot[2] = 0;	// None
	m_Engine.SetAudioFXDirect (0, &m_CloudSeed);
	LOGNOTE ("FX ready: CloudSeed + YKChorus");

	// ── I2S audio ─────────────────────────────────────────────────────────────
	m_I2SAudio.SetEngine (&m_Engine);
	if (bOK && !m_I2SAudio.Start ())
		LOGWARN ("I2S audio start failed");

	// ── 4-row UI ──────────────────────────────────────────────────────────────
	if (bOK)
	{
		BuildMenus ();
		m_I2SAudio.SetVolume ((float) m_nVolume / 100.0f);
		m_UI.Init (&m_PageOSRoot);
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

	// ── MIDI FX page (stubs) ──────────────────────────────────────────────
	InitPage (&m_PageMidiFX, "MIDI FX", &m_PageOSRoot);
	MakeReadOnlyRow (&m_PageMidiFX, "Arpeggiator", "Off");
	MakeReadOnlyRow (&m_PageMidiFX, "Chord",        "Off");

	// ── Presets page (stubs) ──────────────────────────────────────────────
	InitPage (&m_PagePresets, "Presets", &m_PageOSRoot);
	MakeReadOnlyRow (&m_PagePresets, "Save", "-");
	MakeReadOnlyRow (&m_PagePresets, "Load", "-");

	// ── Settings page ─────────────────────────────────────────────────────
	InitPage (&m_PageSettings, "Settings", &m_PageOSRoot);
	MakeReadOnlyRow (&m_PageSettings, "MIDI Ch",    "All");
	MakeReadOnlyRow (&m_PageSettings, "Pitchbend",  "+/-2");
	MakeReadOnlyRow (&m_PageSettings, "Version",    VERSION);

	// ── OS Root ───────────────────────────────────────────────────────────
	InitPage (&m_PageOSRoot, "AV-OS", nullptr);
	MakeFreeRow  (&m_PageOSRoot, "Volume",          VolumeAdjust, VolumeGetStr, this);
	MakeMenuRow  (&m_PageOSRoot, "Sound Generator", &m_PageSoundGen);
	MakeMenuRow  (&m_PageOSRoot, "FX Chain",        &m_PageFXChain);
	MakeMenuRow  (&m_PageOSRoot, "MIDI FX",         &m_PageMidiFX);
	MakeMenuRow  (&m_PageOSRoot, "Presets",         &m_PagePresets);
	MakeMenuRow  (&m_PageOSRoot, "Settings",        &m_PageSettings);
}

// ── Run ───────────────────────────────────────────────────────────────────────

TShutdownMode CKernel::Run ()
{
	LOGNOTE ("Compile time: " __DATE__ " " __TIME__);

	for (;;)
	{
		m_USBHCI.UpdatePlugAndPlay ();

		PollMidi ();
		PollInput ();
		m_UI.Draw ();

		m_GUI.Update ();
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
	ev.Type       = static_cast<MidiType> (status & 0xF0);
	ev.nChannel   = status & 0x0F;
	ev.nData1     = d1;
	ev.nData2     = d2;
	ev.nTimeStamp = 0;
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

	// ── Drain USB MIDI pending event ─────────────────────────────────────────
	if (m_bMidiPending)
	{
		m_bMidiPending = FALSE;
		DispatchMidi (m_nLastMidi[0], m_nLastMidi[1], m_nLastMidi[2]);
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
	pThis->m_nLastMidi[0] = pPacket[0];
	pThis->m_nLastMidi[1] = nLength > 1 ? pPacket[1] : 0;
	pThis->m_nLastMidi[2] = nLength > 2 ? pPacket[2] : 0;
	pThis->m_bMidiPending  = TRUE;
}
