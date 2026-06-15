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
#include "engine/cmodrouter.h"
#include "platform/ci2saudio.h"
#include "ui/c4rowui.h"
#include "modules/generators/plaits/src/plaits_generator.h"
#include "modules/generators/dexed/src/dexed_generator.h"
#include "modules/audiofx/cloudseed/src/cloudseed_fx.h"
#include "modules/audiofx/ykchorus/src/ykchorus_fx.h"
#include "modules/midifx/arp/src/arp_midifx.h"

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
	CDexedGenerator		m_Dexed;

	// SG registry — the active generator is m_pSG[m_nActiveSG], wired into
	// engine slot 0 and fed by the mod router. Pages are hand-built per SG;
	// the "SG" selector row cycles the active one, rewires the engine, and
	// jumps to that SG's page. Bump NUM_SG and register the new instance +
	// page to add a generator.
	static constexpr unsigned NUM_SG = 2;
	ISoundGenerator	*m_pSG[NUM_SG];		// registered generators
	TMenuPage	*m_pSGPage[NUM_SG];	// each SG's top page (set in BuildMenus)
	unsigned	 m_nActiveSG;
	ISoundGenerator	*ActiveSG ()		{ return m_pSG[m_nActiveSG]; }
	void		 SelectSG (unsigned nIdx);
	static void	 SGSelectAdjust (void *pCtx, int nDelta);
	static void	 SGSelectGetStr (void *pCtx, char *pBuf, unsigned nMax);

	// ── Dexed bank/patch browser — reads DX7 .syx banks from SD:/dexed ────
	static constexpr unsigned MAX_DEXED_BANKS = 32;
	char		m_DexedBankName[MAX_DEXED_BANKS][64];
	unsigned	m_nDexedBankCount;
	int		m_nDexedBank;		// index into m_DexedBankName, -1 = none
	uint8_t		m_DexedBankBuf[CDexedGenerator::BANK_SYSEX_LEN];
	void		ScanDexedBanks ();
	bool		LoadDexedBank (unsigned nIdx);
	static void	DexedBankAdjust  (void *pCtx, int nDelta);
	static void	DexedBankGetStr  (void *pCtx, char *pBuf, unsigned nMax);
	static void	DexedPatchAdjust (void *pCtx, int nDelta);
	static void	DexedPatchGetStr (void *pCtx, char *pBuf, unsigned nMax);

	// ── Audio FX instances ────────────────────────────────────────────────
	CCloudSeedFX		m_CloudSeed;
	CYKChorusFX		m_YKChorus;

	// ── MIDI FX instances ─────────────────────────────────────────────────
	CArpMidiFX		m_Arp;

	// ── Mod router (2 LFOs + 2 cyclic envelopes → active SG) ──────────────
	CModRouter		m_ModRouter;

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
	TMenuPage	m_PageSoundGen;		// Plaits main page (m_pSGPage[0])
	TMenuPage	m_PageTone;
	TMenuPage	m_PageMod;		// Sound Gen → Mod (FM/timbre/morph amounts)
	TMenuPage	m_PageDexed;		// Dexed main page (m_pSGPage[1])
	TMenuPage	m_PageDexedOps;		// Dexed → Operators (per-op level + on/off)
	TMenuPage	m_PageFXChain;
	TMenuPage	m_PageYKChorus;
	TMenuPage	m_PageCloudSeed;	// FX slot 0 param page
	TMenuPage	m_PageMidiFX;
	TMenuPage	m_PageArp;		// Arpeggiator param page
	TMenuPage	m_PagePresets;
	TMenuPage	m_PageSettings;
	// ── Mod router pages (top-level "Mod" item) ───────────────────────────
	TMenuPage	m_PageModMain;		// LFO1 / LFO2 / Env1 / Env2
	TMenuPage	m_PageModLFO[2];	// Rate, Shape, Depth, Target
	TMenuPage	m_PageModEnv[2];	// Attack, Decay, Depth, Target

	static constexpr unsigned MAX_MENU_ROWS = 160;
	TMenuRow	m_MenuRows[MAX_MENU_ROWS];
	unsigned	m_nMenuRowCount;

	// ── Mod source UI contexts ────────────────────────────────────────────────
	// sources: 0-1 = LFO 0-1,  2-3 = Env 0-1
	// params:  LFO: 0=Sync 1=Rate/Div 2=Shape 3=Depth
	//          Env: 0=Sync 1=Atk ms/Div 2=Dec ms/Div 3=Depth
	// (Routing lives in the SG's own params — see Sound Generator → Mod.)
	struct TModParamCtx { CKernel *pKernel; unsigned nSrc; unsigned nParam; };
	TModParamCtx	m_ModParamCtx[4][4];
	static void	ModParamAdjust (void *pCtx, int nDelta);
	static void	ModParamGetStr (void *pCtx, char *pBuf, unsigned nMax);

	// ── BPM + clock source ────────────────────────────────────────────────────
	float		m_fBPM;			// internal BPM (20–300)
	unsigned	m_nClockSource;		// 0=Internal, 1=External MIDI
	uint32_t	m_nExtClockLastUs;	// wall-clock time of last 0xF8 received
	uint32_t	m_nExtClockDeltaUs;	// rolling-average inter-clock interval (µs)
	bool		m_bExtClockValid;	// first pulse seen yet?
	void		HandleMidiClock ();
	static void	BPMAdjust     (void *pCtx, int nDelta);
	static void	BPMGetStr     (void *pCtx, char *pBuf, unsigned nMax);
	static void	ClockSrcAdjust (void *pCtx, int nDelta);
	static void	ClockSrcGetStr (void *pCtx, char *pBuf, unsigned nMax);

	// ── MIDI state ────────────────────────────────────────────────────────────
	boolean			m_bSerialOK;		// TRS serial init result
	unsigned		m_nSerialBytes;		// raw bytes received (diagnostic)

	// TRS parser state (main-loop only — no IRQ parser needed with Read()).
	u8			m_nMidiParseStatus;
	u8			m_nMidiParseData0;
	u8			m_nMidiParseIdx;	// 1 = waiting d1, 2 = waiting d2

	// USB MIDI: SPSC ring buffer (USB callback writes, PollMidi reads).
	// Power-of-two size so wrap is a cheap bitwise AND.
	static constexpr unsigned MIDI_QUEUE_SIZE = 32;
	u8		 m_MidiQueue[MIDI_QUEUE_SIZE][3];	// [n][0]=status [n][1]=d1 [n][2]=d2
	volatile unsigned m_nMidiQWrite;	// head — written only by USB callback
	volatile unsigned m_nMidiQRead;		// tail — written only by PollMidi

	// USB host: deferred init (USBHCI hangs at boot on Pi4 without USB
	// devices attached — we try Initialize() once from the main loop instead).
	boolean			m_bUsbHCIInitialized;

	// Set true when OLED + LVGL initialised successfully; guards GUI calls.
	boolean			m_bGUIReady;

	// USB MIDI device (found lazily in PollMidi).
	boolean			m_bUsbMidiFound;

	// Display: last MIDI event for UpdateDisplay().
	u8			m_nDispMidi[3];
	unsigned		m_nMidiCount;
};
