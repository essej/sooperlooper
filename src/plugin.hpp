/*
** Copyright (C) 2004 Jesse Chappell <jesse@essej.net>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/

#ifndef __sooperlooper_plugin_hpp__
#define __sooperlooper_plugin_hpp__

#include "ladspa.h"

// TODO, move the whole looping core into a class


namespace SooperLooper {

enum ControlPort {
	TriggerThreshold = 0,
	DryLevel,
	WetLevel,
	Feedback,
	Rate,
	ScratchPosition,
	Multi,
	TapDelayTrigger,
	UseFeedbackPlay,
	Quantize,
	Round,
	RedoTap,
	Sync,
	UseRate,
	FadeSamples,
	TempoInput,
	PlaybackSync,
	EighthPerCycleLoop,
	UseSafetyFeedback,
	InputLatency,
	OutputLatency,
	TriggerLatency,
	MuteQuantized,
	OverdubQuantized,
	SyncOffsetSamples,
	RoundIntegerTempo,
	LASTCONTROLPORT,
	RelativeSync // not an enum
};

enum OutputPort {
	State = 26,
	LoopLength,
	LoopPosition,
	CycleLength,
	LoopFreeMemory,
	LoopMemory,
	Waiting,
	TrueRate,
	NextState,
	LASTPORT
};

enum AudioPort {
	AudioInputPort=35,
	AudioOutputPort,
	SyncInputPort,
	SyncOutputPort,
	PORT_COUNT // must be last
};

enum {
	QUANT_OFF=0,
	QUANT_CYCLE,
	QUANT_8TH,
	QUANT_LOOP
};

enum LooperState
{
	LooperStateUnknown = -1,
	LooperStateOff = 0,
	LooperStateWaitStart,
	LooperStateRecording,
	LooperStateWaitStop,
	LooperStatePlaying,
	LooperStateOverdubbing,
	LooperStateMultiplying,
	LooperStateInserting,
	LooperStateReplacing,
	LooperStateDelay,
	LooperStateMuted,
	LooperStateScratching,
	LooperStateOneShot,
	LooperStateSubstitute,
	LooperStatePaused,
	LooperStateOffMuted = 20
};

	
};

/*****************************************************************************/

// defines all a loop needs to know to cycle properly in memory
// one of these will prefix the actual loop data in our buffer memory
typedef struct _LoopChunk {

	/* pointers in buffer memory. */
	//LADSPA_Data * pLoopStart;
	//LADSPA_Data * pLoopStop;    
	
	// index into main sample buffer
	unsigned long lLoopStart;
	//unsigned long lLoopStop;    
	unsigned long lLoopLength;
	
	// adjustment needed in the case of multiply/insert
	unsigned long lStartAdj;
	unsigned long lEndAdj;
	unsigned long lInsPos; // used only by INSERT mode
	unsigned long lRemLen; // used only by INSERT mode
	
	// markers needed for frontfilling and backfilling
	unsigned long lMarkL;
	unsigned long lMarkH;    
	unsigned long lMarkEndL;
	unsigned long lMarkEndH;        

	unsigned long lSyncOffset; // used for rel sync
	long lSyncPos; // used for retriggering
	long lOrigSyncPos;
	
	int firsttime;
	int frontfill;
	int backfill;
	int valid;
	int mult_out; // used for multi-increase
	
	unsigned long lCycles;
	unsigned long lCycleLength;
	
	// current position is double to support alternative rates easier
	double dCurrPos;
	
	// used when doing frontfills
	LADSPA_Data dOrigFeedback;
	
	// the loop where we should be frontfilled and backfilled from
	struct _LoopChunk* srcloop;
	
	struct _LoopChunk* next;
	struct _LoopChunk* prev;
	
} LoopChunk;


/* Instance data */
typedef struct {
    
	LADSPA_Data fSampleRate;

	/* the sample memory */
	//LADSPA_Data * pfSampleBuf;
	LADSPA_Data * pSampleBuf;
    
	unsigned int lLoopIndex;
	unsigned int lChannelIndex;
	
	/* Buffer size, IS necessarily a power of two. */
	unsigned long lBufferSize;
	unsigned long lBufferSizeMask;

	LADSPA_Data * pInputBuf;
	unsigned long lInputBufSize;
	unsigned long lInputBufMask;
	unsigned long lInputBufReadPos;
	unsigned long lInputBufWritePos;
	long lFramesUntilInput; // used for input latency compensation
	long lFramesUntilFilled; // used to fill the gaps right after a record
	
	// the loopchunk pool
	LoopChunk * pLoopChunks;
	LoopChunk * lastLoopChunk;
	unsigned long lLoopChunkCount;
	
	LADSPA_Data fTotalSecs;	
	
	/* the current state of the sampler */
	int state;

	int nextState;

	bool wasMuted;
	double dPausedPos;

	bool safetyFeedback;
	
	int waitingForSync;
	bool recSyncEnded;
	
	long lLastMultiCtrl;

	// initial location of params
	LADSPA_Data fQuantizeMode;
	LADSPA_Data fRoundMode;    
	LADSPA_Data fRedoTapMode;
	LADSPA_Data fSyncMode;
	LADSPA_Data fMuteQuantized;
	LADSPA_Data fOverdubQuantized;
	
	bool bReplaceQuantized;
    
	// used only when in DELAY mode
	int bHoldMode;

	bool donePlaySync;
	bool rounding;
	
	unsigned long lTapTrigSamples;

	LADSPA_Data fLastOverTrig;    
	unsigned long lOverTrigSamples;    

	unsigned long lRampSamples;
    
	LADSPA_Data fCurrRate;
	LADSPA_Data fNextCurrRate;

	LADSPA_Data fLastScratchVal;
	unsigned long lScratchSamples;
	LADSPA_Data fCurrScratchRate;
	LADSPA_Data fLastRateSwitch;
	int bRateCtrlActive;
    
	LADSPA_Data fLastTapCtrl;
	int bPreTap;
    
	// linked list of loop chunks
	LoopChunk * headLoopChunk;
	LoopChunk * tailLoopChunk;    
	unsigned int lHeadLoopChunk;
	unsigned int lTailLoopChunk;
    
	LADSPA_Data fWetCurr;
	LADSPA_Data fWetTarget;

	LADSPA_Data fDryCurr;
	LADSPA_Data fDryTarget;

	LADSPA_Data fRateCurr;
	LADSPA_Data fRateTarget;

	LADSPA_Data fScratchPosCurr;
	LADSPA_Data fScratchPosTarget;

	LADSPA_Data fFeedbackCurr;
	LADSPA_Data fFeedbackTarget;

	LADSPA_Data fLoopFadeAtten;
	LADSPA_Data fLoopFadeDelta;

	LADSPA_Data fLoopSrcFadeAtten;
	LADSPA_Data fLoopSrcFadeDelta;

	LADSPA_Data fPlayFadeAtten;
	LADSPA_Data fPlayFadeDelta;

	LADSPA_Data fFeedFadeAtten;
	LADSPA_Data fFeedFadeDelta;

	LADSPA_Data fFeedSrcFadeAtten;
	LADSPA_Data fFeedSrcFadeDelta;

	LADSPA_Data fLoopXfadeTime;

	unsigned int lSamplesSinceSync;
	
	
	/* Ports:
	   ------ */

    

	LADSPA_Data * pfWet;
    
	LADSPA_Data * pfDry;

    
	/* Feedback 0 for none, 1 for infinite */
	LADSPA_Data * pfFeedback;

	/* Trigger level for record and stop record */
	LADSPA_Data * pfTrigThresh;


	/* The rate of loop playback, if RateSwitch is on */
	LADSPA_Data * pfRate;

	/* The destination position in the loop to scratch to. 0 is the start */
	/*  and 1.0 is the end of the loop.  Only active if RateSwitch is on */
	LADSPA_Data * pfScratchPos;

	/* The multicontrol port.  Each value from (0-127) has a
	 * meaning.  This is considered a momentary control, thus
	 * ANY change to a value within the value range is only
	 * noticed at the moment it changes from something different.
	 *  If you want to do two identical values in a row, you must change
	 * the value to something outside our range for a cycle before using
	 * the real value again.
	 */
	LADSPA_Data * pfMultiCtrl;

	LADSPA_Data * pfUseFeedbackPlay;
    
	/* changes on this control signal with more than TAP_THRESH_SAMP samples
	 * between them (to handle settle time) is treated as a a TAP Delay trigger
	 */
	LADSPA_Data *pfTapCtrl;

	/* non zero here toggle quantize and round mode
	 *  WARNING: the plugin may set this value internally... cause I want
	 *  it controllable (via mute mode)
	 */
	LADSPA_Data *pfQuantMode;
	LADSPA_Data *pfRoundMode;    
	LADSPA_Data *pfSyncMode;    
	LADSPA_Data *pfPlaybackSyncMode;    
	LADSPA_Data *pfRateCtrlActive;
	LADSPA_Data *pfMuteQuantized;
	LADSPA_Data *pfOverdubQuantized;
	LADSPA_Data *pfSyncOffsetSamples;
	LADSPA_Data *pfRoundIntegerTempo;

	LADSPA_Data *pfXfadeSamples;

	LADSPA_Data *pfTempo;
	LADSPA_Data *pfEighthPerCycle;
	LADSPA_Data *pfUseSafetyFeedback;

	LADSPA_Data *pfInputLatency;
	LADSPA_Data *pfOutputLatency;
	LADSPA_Data *pfTriggerLatency;

	
	/* if non zero, the redo command is treated like a tap trigger */
	LADSPA_Data *pfRedoTapMode;
    
	/* Input audio port data location. */
	LADSPA_Data * pfInput;
    
	/* Output audio port data location. */
	LADSPA_Data * pfOutput;

	LADSPA_Data * pfSyncInput;
	LADSPA_Data * pfSyncOutput;

    
	/* Control outputs */

	LADSPA_Data * pfStateOut;
	LADSPA_Data * pfLoopLength;
	LADSPA_Data * pfLoopPos;        
	LADSPA_Data * pfCycleLength;

	/* how many seconds of loop memory free and total */
	LADSPA_Data * pfSecsFree;
	LADSPA_Data * pfSecsTotal;    

	LADSPA_Data * pfWaiting;    
	LADSPA_Data * pfRateOutput;
	LADSPA_Data * pfNextStateOut;    
	
} SooperLooperI;



// reads loop audio into buffer, up to frames length, starting from loop_offset.  if fewer frames are
// available returns amount read.  if 0 is returned loop is done.
extern unsigned long sl_read_current_loop_audio (LADSPA_Handle instance, float * buf, unsigned long frames, unsigned long loop_offset);

// override current samples since sync
extern void sl_set_samples_since_sync (LADSPA_Handle instance, unsigned long frames);

extern void sl_set_replace_quantized (LADSPA_Handle instance, bool value);
extern bool sl_get_replace_quantized (LADSPA_Handle instance);
extern void sl_set_loop_index (LADSPA_Handle instance, unsigned int index, unsigned int chan);

extern bool sl_has_loop (const LADSPA_Handle instance);

#endif
