//
// kernel.h
//
// Akashic Vault - Circle bare-metal kernel. Phase 0 bring-up: owns the hardware
// (I2C, SSD1309 OLED via CLVGL, MCP23017 input, MIDI, I2S audio) + the engine.
// Copyright (C) 2026  The Akashic Trance Machines Team
//
// This file is part of Akashic Vault and is licensed under GPL-3.0. See ../LICENSE.
//
#pragma once

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/types.h>
#include <circle/i2cmaster.h>
#include <circle/sched/scheduler.h>
#include <circle/usb/usbhcidevice.h>
#include <SDCard/emmc.h>
#include <fatfs/ff.h>

// From the ATM circle fork (libs/circle): graphics OLED driver + MCP23017 +
// the LVGL addon that renders onto any CDisplay.
#include <display/ssd1309display.h>
#include <gpio/mcp23017.h>
#include <lvgl/lvgl.h>
#include <circle/usb/usbmidi.h>

#include "engine/cengine.h"
#include "platform/ci2saudio.h"
#include "ui/c4rowui.h"
#include "modules/generators/plaits/src/plaits_generator.h"
#include "modules/audiofx/cloudseed/src/cloudseed_fx.h"
#include "modules/audiofx/ykchorus/src/ykchorus_fx.h"

enum TShutdownMode { ShutdownNone, ShutdownHalt, ShutdownReboot };

class CKernel
{
public:
	CKernel ();
	~CKernel ();

	boolean Initialize ();
	TShutdownMode Run ();

private:
	void	PollInput ();		// MCP23017 encoders/buttons -> m_UI + engine
	void	PollMidi ();		// TRS UART + USB MIDI -> m_Engine.PushMidi
	void	DispatchMidi (u8 status, u8 d1, u8 d2);	// build TMidiEvent, push to engine
	void	BuildMenus ();		// construct static menu tree
	TMenuRow *AllocRow ();		// allocate next row from static pool

	// Menu row factory methods
	void	MakeParamRow    (TMenuPage *page, IModule *m, unsigned idx);
	void	MakeMenuRow     (TMenuPage *page, const char *label, TMenuPage *child);
	void	MakeFreeRow     (TMenuPage *page, const char *label,
				 void (*pfAdj)(void*, int),
				 void (*pfGet)(void*, char*, unsigned),
				 void *ctx);
	void	MakeReadOnlyRow (TMenuPage *page, const char *label, const char *value);

	// USB MIDI: packet handler (static, called from USB interrupt context).
	static void UsbMidiPacketHandler (unsigned nCable, u8 *pPacket,
					  unsigned nLength, unsigned nDevice,
					  void *pParam);

private:
	// Constructed in this order (Circle convention).
	CActLED			m_ActLED;
	CKernelOptions		m_Options;
	CDeviceNameService	m_DeviceNameService;
	CScreenDevice		m_Screen;	// HDMI kernel log (debug)
	CExceptionHandler	m_ExceptionHandler;
	CInterruptSystem	m_Interrupt;
	CTimer			m_Timer;
	CLogger			m_Logger;
	CScheduler		m_Scheduler;
	CUSBHCIDevice		m_USBHCI;	// USB MIDI host
	CI2CMaster		m_I2CMaster;	// OLED + MCP23017
	CEMMCDevice		m_EMMC;		// SD card
	FATFS			m_FileSystem;
	CSerialDevice		m_Serial;	// TRS MIDI UART (31250 baud, interrupt-driven)

	// SSD1309 OLED (I2C) -> CLVGL renders the UI onto it. GetDepth()==1, so
	// CLVGL auto-selects LV_COLOR_FORMAT_I1 and CSSD1309Display::SetArea packs
	// into the panel's page layout. No custom flush needed.
	CSSD1309Display		m_Display;
	CLVGL			m_GUI;

	CMCP23017		m_MCP;		// 16 inputs: 5 encoders + clicks
	CEngine			m_Engine;
	CI2SAudio		m_I2SAudio;	// I2S DMA output (48kHz/256-frame blocks)

	// ── Generators ────────────────────────────────────────────────────────
	CPlaitsGenerator	m_Plaits;

	// ── Audio FX instances ────────────────────────────────────────────────
	CCloudSeedFX		m_CloudSeed;
	CYKChorusFX		m_YKChorus;

	// ── 4-row UI ──────────────────────────────────────────────────────────
	C4RowUI			m_UI;

	// ── Nav long-press state ─────────────────────────────────────────────────
	boolean		m_bNavHeld;		// nav encoder is currently pressed
	unsigned	m_nNavHoldStart;	// CTimer tick when pressed
	boolean		m_bNavLongFired;	// long press already triggered this hold

	// ── Input decode state ────────────────────────────────────────────────────

	u8			m_nPrevPortA;
	u8			m_nPrevPortB;
	int			m_nEncAccum[5];		// [0-3]=value, [4]=nav
	int			m_nEncPos[5];		// running detent position
	unsigned		m_nClickCount[5];	// [0-3]=enc1-4 clicks, [4]=back
	unsigned		m_nLastDetentTick[5];	// CTimer tick of last detent per encoder (for acceleration)

	// ── FX slot selection ────────────────────────────────────────────────────
	// 0 = None, 1 = CloudSeed, 2 = YKChorus
	unsigned		m_nFXSlot[3];
	void		ApplyFXSlot   (unsigned nSlot);
	void		AdjustFXSlot  (unsigned nSlot, int nDelta);
	const char	*GetFXSlotName (unsigned nSlot) const;
	void		NavigateToFXParams (unsigned nSlot);

	struct TFXSlotCtx { CKernel *pKernel; unsigned nSlot; };
	TFXSlotCtx		m_FXSlotCtx[3];
	static void	FXSlotAdjust (void *pCtx, int nDelta);
	static void	FXSlotGetStr (void *pCtx, char *pBuf, unsigned nMax);
	static void	FXSlotAction (void *pCtx);

	// ── Master volume (0–100, applied in CI2SAudio) ──────────────────────────
	unsigned	m_nVolume;	// 0–100
	void		AdjustVolume (int nDelta);
	unsigned	GetVolume () const { return m_nVolume; }

	// Volume free-property callbacks (static, passed as pFreeCtx=this)
	static void	VolumeAdjust (void *pCtx, int nDelta);
	static void	VolumeGetStr (void *pCtx, char *pBuf, unsigned nMax);

	// ── Static menu storage (built once in BuildMenus) ───────────────────────
	TMenuPage	m_PageOSRoot;
	TMenuPage	m_PageSoundGen;
	TMenuPage	m_PageTone;
	TMenuPage	m_PageMod;
	TMenuPage	m_PageFXChain;
	TMenuPage	m_PageYKChorus;
	TMenuPage	m_PageCloudSeed;	// FX slot 0 param page
	TMenuPage	m_PageMidiFX;
	TMenuPage	m_PagePresets;
	TMenuPage	m_PageSettings;

	static constexpr unsigned MAX_MENU_ROWS = 96;
	TMenuRow	m_MenuRows[MAX_MENU_ROWS];
	unsigned	m_nMenuRowCount;

	// ── MIDI state ────────────────────────────────────────────────────────────
	boolean			m_bSerialOK;		// TRS serial init result
	unsigned		m_nSerialBytes;		// raw bytes received (diagnostic)

	// TRS parser state (main-loop only — no IRQ parser needed with Read()).
	u8			m_nMidiParseStatus;
	u8			m_nMidiParseData0;
	u8			m_nMidiParseIdx;	// 1 = waiting d1, 2 = waiting d2

	// USB MIDI: incoming packet flag (set from USB IRQ, cleared in main loop).
	volatile boolean	m_bMidiPending;
	volatile u8		m_nLastMidi[3];		// [0]=status [1]=d1 [2]=d2

	// USB MIDI device (found lazily in PollMidi).
	boolean			m_bUsbMidiFound;

	// Display: last MIDI event for UpdateDisplay().
	u8			m_nDispMidi[3];
	unsigned		m_nMidiCount;
};
