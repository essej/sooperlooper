/* SooperLooper.c  :  
   Copyright (C) 2002-2005 Jesse Chappell <jesse@essej.net>

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
// ------------------------------------------------------------------------


   This is still based on the original LADSPA plugin, yeah it's a mess.

*/
   
/*****************************************************************************/

#include <climits>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cfloat>
#include <iostream>

using namespace std;

/*****************************************************************************/

#include "ladspa.h"

#include "plugin.hpp"
#include "utils.hpp"

#include "event.hpp"

using namespace SooperLooper;


/*****************************************************************************/
//#define LOOPDEBUG

#ifdef LOOPDEBUG
#define DBG(x) x
#else
#define DBG(x)
#endif

#define VERSION "1.6"

/* The maximum sample memory  (in seconds). */

#ifndef SAMPLE_MEMORY
#define SAMPLE_MEMORY 80.0
#endif

#define XFADE_SAMPLES 128

// settle time for tap trigger (trigger if two changes
// happen within at least X samples)
//#define TRIG_SETTLE  4410
#define TRIG_SETTLE  2205  

// another thing that shouldn't be hardcoded
#define MAX_LOOPS 512


#define SAFETY_FEEDBACK 0.96f

/*****************************************************************************/


/* States of the sampler */

#define STATE_OFF        0
#define STATE_TRIG_START 1
#define STATE_RECORD     2
#define STATE_TRIG_STOP  3
#define STATE_PLAY       4
#define STATE_OVERDUB    5
#define STATE_MULTIPLY   6
#define STATE_INSERT     7
#define STATE_REPLACE    8
#define STATE_DELAY      9
#define STATE_MUTE       10
#define STATE_SCRATCH    11
#define STATE_ONESHOT    12
#define STATE_SUBSTITUTE  13
#define STATE_PAUSED     14
#define STATE_UNDO_ALL	15
#define STATE_TRIGGER_PLAY 16
#define STATE_UNDO		17
#define STATE_REDO		18
#define STATE_REDO_ALL	19
#define STATE_OFF_MUTE	20

/* 1s digit of
 * Multicontroller parameter functions */

#define MULTI_UNDO       0

#define MULTI_REDO       1   // posibbly MULTI_DELAY if toggled, or REDO_TOG if muted

#define MULTI_REPLACE    2   // QUANT_TOG in mute mode

#define MULTI_REVERSE    3   // ROUND_TOG in mute mode

#define MULTI_SCRATCH    4
#define MULTI_RECORD     5
#define MULTI_OVERDUB    6
#define MULTI_MULTIPLY   7
#define MULTI_INSERT     8
#define MULTI_MUTE       9

// extra features, cannot be picked directly
#define MULTI_DELAY      10   // only 
#define MULTI_REDO_TOG   11   // only when in mute mode
#define MULTI_QUANT_TOG  12   // only when in mute mode
#define MULTI_ROUND_TOG  13   // only when in mute mode

#define MULTI_ONESHOT    14
#define MULTI_TRIGGER    15
#define MULTI_SUBSTITUTE 16
#define MULTI_UNDO_ALL   17
#define MULTI_REDO_ALL   18

#define MULTI_MUTE_ON    19
#define MULTI_MUTE_OFF   20
#define MULTI_PAUSE      21
#define MULTI_PAUSE_ON      22
#define MULTI_PAUSE_OFF      23

#define MULTI_SET_SYNC_POS   Event::SET_SYNC_POS
#define MULTI_RESET_SYNC_POS Event::RESET_SYNC_POS
#define MULTI_MUTE_TRIGGER   Event::MUTE_TRIGGER
#define MULTI_RECORD_OR_OVERDUB Event::RECORD_OR_OVERDUB
#define MULTI_RECORD_OVERDUB_END Event::RECORD_OVERDUB_END_SOLO
#define MULTI_UNDO_TWICE     Event::UNDO_TWICE


/*****************************************************************************/

#define LIMIT_BETWEEN_0_AND_1(x)          \
	f_clamp (x, 0.0f, 1.0f)

#define LIMIT_BETWEEN_NEG1_AND_1(x)          \
	f_clamp (x, -1.0f, 1.0f)


#define MAX(x,y) \
	f_max (x, y)



// reads loop audio into buffer, up to frames length, starting from loop_offset.  if fewer frames are
// available returns amount read.  if 0 is returned loop is done.
unsigned long
sl_read_current_loop_audio (LADSPA_Handle instance, float * buf, unsigned long frames, unsigned long loop_offset)
{
	SooperLooperI * pLS = (SooperLooperI *)instance;

	if (!pLS || !buf) return 0;

	LoopChunk * loop = pLS->headLoopChunk;
	if (!loop) return 0;
	if (loop_offset > loop->lLoopLength) return 0;
	
	// adjust for sync pos, so that a loop_offset of 0 actually means start from the syncpos
	unsigned long adj_offset  = (loop_offset + (loop->lLoopLength - loop->lSyncPos)) % loop->lLoopLength;
	unsigned long frames_left = loop->lLoopLength - loop_offset;
	unsigned long startpos = (loop->lLoopStart + adj_offset) & pLS->lBufferSizeMask;
	unsigned long first_chunk;
	unsigned long second_chunk=0;

	if (adj_offset > (loop->lLoopLength - loop->lSyncPos)) {
		// between sync and end of loop mem, clamp frames
		frames = std::min(loop->lLoopLength-adj_offset, frames);
	}

	if (frames > frames_left) {
		frames = frames_left;
	}
	
	if ( startpos > ((startpos + frames) & pLS->lBufferSizeMask)) {
		// crosses buffer boundary, 2 chunks needed
		first_chunk = pLS->lBufferSize - startpos;
		second_chunk = frames - first_chunk;
	}
	else {
		first_chunk = frames;
		second_chunk = 0;
	}


	// read first chunk
	memcpy ((char *)buf, (char *) &pLS->pSampleBuf[startpos], first_chunk * sizeof(LADSPA_Data));

	if (second_chunk) {
		memcpy ((char *) (buf + first_chunk), (char *) pLS->pSampleBuf, second_chunk * sizeof(LADSPA_Data));
	}

	return frames;
}

void
sl_set_samples_since_sync (LADSPA_Handle instance, unsigned long frames)
{
	SooperLooperI * pLS = (SooperLooperI *)instance;

	if (!pLS) return;

	pLS->lSamplesSinceSync = frames;
}

void
sl_set_replace_quantized (LADSPA_Handle instance, bool value)
{
	SooperLooperI * pLS = (SooperLooperI *)instance;

	if (!pLS) return;
	pLS->bReplaceQuantized = value;
}

void
sl_set_loop_index (LADSPA_Handle instance, unsigned int value, unsigned int chan)
{
	SooperLooperI * pLS = (SooperLooperI *)instance;
	
	if (!pLS) return;
	pLS->lLoopIndex = value;
	pLS->lChannelIndex = chan;
	DBG(fprintf(stderr, "set loop: %u  chan: %u\n", pLS->lLoopIndex, pLS->lChannelIndex));
}

bool
sl_get_replace_quantized (LADSPA_Handle instance)
{
	SooperLooperI * pLS = (SooperLooperI *)instance;
	if (!pLS) return false;
	return pLS->bReplaceQuantized;
}


bool sl_has_loop (const LADSPA_Handle instance)
{
	const SooperLooperI * pLS = (const SooperLooperI *)instance;
	if (!pLS) return false;
        return pLS->headLoopChunk != 0;
}

static bool invalidateTails (SooperLooperI * pLS, unsigned long bufstart, unsigned long buflen, LoopChunk * currloop)
{
	LoopChunk * tailLoop = pLS->tailLoopChunk;
	LoopChunk * tmploop;
	
	if (!tailLoop) {
		return false;
	}
	
	while (tailLoop  && tailLoop != currloop && 
	       (((bufstart + buflen) < pLS->lBufferSize &&
		 (tailLoop->lLoopStart >= bufstart && tailLoop->lLoopStart < (bufstart + buflen)))
		||
		((bufstart + buflen) >= pLS->lBufferSize &&
		 (tailLoop->lLoopStart < ((bufstart + buflen) & pLS->lBufferSizeMask)
		  || tailLoop->lLoopStart >= bufstart))))
	{
		
		// this invalidates a loop
		if (tailLoop->valid) {
			DBG(fprintf(stderr, "%u:%u  invalidating %08x\n", pLS->lLoopIndex, pLS->lChannelIndex, (unsigned) tailLoop));
			tailLoop->valid = 0;
			if (tailLoop->next) {
				tailLoop->next->prev = 0;
			}
			tmploop = tailLoop->next;
			//tailLoop->next = 0;
			tailLoop = tmploop;
		}
		else {
			// this really shouldn't happen
			DBG(fprintf(stderr, "%u:%u  we've hit the last tail this shoudlnt happen\n",pLS->lLoopIndex, pLS->lChannelIndex));
			tailLoop = 0;
		}

		// tailLoop = (tailLoop == pLS->lastLoopChunk) ? pLS->pLoopChunks: tailLoop + 1;
		pLS->tailLoopChunk = tailLoop;
	}

// 	if (!tailLoop || tailLoop == currloop) {
// 		DBG(fprintf(stderr, "we've hit nothing or ourselves, returning false\n"));
// 		return false;
// 	}
	if (pLS->tailLoopChunk == NULL) {
		// we invalidated the last loop
		pLS->tailLoopChunk = currloop;
	}
	
	return true;
}

static LoopChunk * ensureLoopSpace(SooperLooperI* pLS, LoopChunk *loop, unsigned long morelength, LoopChunk * pendsrc)
{
	// TODO: check to see if we'll require more space than the buffer allows
	
	if (!loop) {
		loop = (pLS->headLoopChunk == pLS->lastLoopChunk) ? pLS->pLoopChunks: pLS->headLoopChunk + 1;
		loop->lLoopStart = (pLS->headLoopChunk->lLoopStart + pLS->headLoopChunk->lLoopLength) & pLS->lBufferSizeMask;
		loop->lLoopLength = 0;
		loop->lCycleLength = 0;
		loop->lCycles = 0;
		loop->dCurrPos = 0;
		loop->frontfill = 0;
		loop->backfill = 0;
		loop->valid = 1;
		loop->mult_out = 0;
		loop->lSyncOffset = 0;
		loop->lSyncPos = 0;
		loop->lOrigSyncPos = 0;
		
		loop->next = NULL;
		loop->prev = pLS->headLoopChunk;
		if (loop->prev) {
			loop->prev->next = loop;
			// do this to help out the overdub fail case
			loop->lCycles = loop->prev->lCycles;
			loop->lCycleLength = loop->prev->lCycleLength;
			loop->lLoopLength = loop->prev->lLoopLength;
			loop->dCurrPos = loop->prev->dCurrPos;
		}
		loop->srcloop = pendsrc;

		pLS->headLoopChunk = loop;
	}
	else {

		// check to see if we'll require more space than the buffer allows
		if (loop->lLoopLength + morelength > pLS->lBufferSize) {
			DBG(fprintf(stderr, "%u:%u  requesting more space than exists in buffer!\n",pLS->lLoopIndex, pLS->lChannelIndex));
			return NULL;
		}
	}
	
	// invalidate any tails in this area
	invalidateTails (pLS, loop->lLoopStart, loop->lLoopLength + morelength, loop);

	return loop;
}


// creates a new loop chunk and puts it on the head of the list
// returns the new chunk
static LoopChunk * pushNewLoopChunk(SooperLooperI* pLS, unsigned long initLength, LoopChunk * pendsrc)
{
   LoopChunk * loop;   

   if (pLS->headLoopChunk) {
      // use the next spot in memory

      loop = ensureLoopSpace (pLS, NULL, initLength, pendsrc);

      if (!loop) {
	      return NULL;
      }
      

      // we are the new head
      pLS->headLoopChunk = loop;
      
   }
   else {
      // first loop on the list!
      loop = pLS->pLoopChunks;
      loop->next = loop->prev = NULL;
      pLS->headLoopChunk = pLS->tailLoopChunk = loop;
      loop->lLoopStart = 0;
      loop->valid = 1;
   }
   

   DBG(fprintf(stderr, "%u:%u  New head is %08x   start: %lu\n",pLS->lLoopIndex, pLS->lChannelIndex, (unsigned)loop, loop->lLoopStart);)

   
   return loop;
}


// pop the head off and free it
static void popHeadLoop(SooperLooperI *pLS, bool forceClear)
{
   LoopChunk *dead;
   dead = pLS->headLoopChunk;

   if (dead && dead->prev) {
      // leave the next where is is for redo
      //dead->prev->next = NULL;
	   pLS->headLoopChunk = dead->prev;
	   if (!pLS->headLoopChunk->prev) {
		   pLS->tailLoopChunk = pLS->headLoopChunk; 
	   }
   }
   else if (dead && (!dead->next || forceClear)) {
	   // only clear the loop if this was the only loop in history
	   pLS->headLoopChunk = NULL;
	   // pLS->tailLoopChunk is still valid to support redo
	   // from nothing
   }
}

// clear all LoopChunks (undoAll , can still redo them back)
static void clearLoopChunks(SooperLooperI *pLS)
{
   
   pLS->headLoopChunk = NULL;
}

static void undoLoop(SooperLooperI *pLS, bool forceClear)
{
   LoopChunk *loop = pLS->headLoopChunk;
   LoopChunk *prevloop;
   
   if (!loop) return;
	
   prevloop = loop->prev;

   //if (prevloop && prevloop == loop->srcloop) {
      // if the previous was the source of the one we're undoing
      // pass the dCurrPos along, otherwise leave it be.
   // nevermind, ALWAYS pass it along, what the hell
   if (prevloop) {
	   prevloop->dCurrPos = fmod(loop->dCurrPos+loop->lStartAdj, prevloop->lLoopLength);
   }
   
   popHeadLoop(pLS, forceClear);
   DBG(fprintf(stderr, "%u:%u  Undoing last loop %08x: new head is %08x\n" , pLS->lLoopIndex, pLS->lChannelIndex, (unsigned)loop,
	       (unsigned)pLS->headLoopChunk);)
}


static void redoLoop(SooperLooperI *pLS)
{
   LoopChunk *loop = NULL;
   LoopChunk *nextloop = NULL;

   if (pLS->headLoopChunk) {
      loop = pLS->headLoopChunk;
      nextloop = loop->next;
   }
   else if (pLS->tailLoopChunk) {
      // we've undone everything, use the tail
      loop = NULL;
      nextloop = pLS->tailLoopChunk;
   }

   if (nextloop) {
      
	   //if (loop && loop == nextloop->srcloop) {
	   // if the next is using us as a source
	   // pass the dCurrPos along, otherwise leave it be.
	   // nevermind, ALWAYS pass it along
	   if (loop) {
		   nextloop->dCurrPos = fmod(loop->dCurrPos+loop->lStartAdj, nextloop->lLoopLength);
	   }
	   else {
		   // start at loop beginning
		   nextloop->dCurrPos = nextloop->lLoopLength - nextloop->lSyncPos;
	   }
	   
	   pLS->headLoopChunk = nextloop;
	   
	   DBG(fprintf(stderr, "%u:%u  Redoing last loop %08x: new head is %08x\n", pLS->lLoopIndex, pLS->lChannelIndex, (unsigned)loop,
		       (unsigned)pLS->headLoopChunk);)

   }
}



/*****************************************************************************/

/* Construct a new plugin instance. */
LADSPA_Handle 
instantiateSooperLooper(const LADSPA_Descriptor * Descriptor,
			unsigned long             SampleRate)
{

   SooperLooperI * pLS;
   char * sampmem;
   
   // important note: using calloc to zero all data
   pLS = (SooperLooperI *) calloc(1, sizeof(SooperLooperI));
   
   if (pLS == NULL) 
      return NULL;

   pLS->pSampleBuf = NULL;
   pLS->pLoopChunks = NULL;
   pLS->pInputBuf = NULL;
   
   pLS->fSampleRate = (LADSPA_Data)SampleRate;

   pLS->fTotalSecs = SAMPLE_MEMORY;
   
   // HACK for the moment!
   sampmem = getenv("SL_SAMPLE_TIME");
   if (sampmem != NULL) {
	   if (sscanf(sampmem, "%f", &pLS->fTotalSecs) != 1) {
		   pLS->fTotalSecs = SAMPLE_MEMORY;
	   }
	   // printf ("Got sample mem: %f\n", pLS->fTotalSecs);
   }
   
   // we do include the LoopChunk structures in the Buf, so we really
   // get a little less the SAMPLE_MEMORY seconds
   //pLS->lBufferSize = (unsigned long)((LADSPA_Data)SampleRate * pLS->fTotalSecs * sizeof(LADSPA_Data));
   pLS->lBufferSize = (unsigned long) pow (2.0, ceil (log2 ((LADSPA_Data)SampleRate * pLS->fTotalSecs)));
   pLS->fTotalSecs = pLS->lBufferSize / (float) SampleRate;
   pLS->lBufferSizeMask = pLS->lBufferSize - 1;
   
   // not using calloc to force touching all memory ahead of time 
   // this could be bad if you try to allocate too much for your system
   // well, we are using calloc again... so sad
   pLS->pSampleBuf = (LADSPA_Data *) calloc(pLS->lBufferSize,  sizeof(LADSPA_Data));
   if (pLS->pSampleBuf == NULL) {
	   goto cleanup;
   }
   // we'll warm up up to 50 secs worth of the loop mem as a tradeoff to the low-mem mac people
	// Removed because this causes heavy cpu load on first record!!!
   //memset (pLS->pSampleBuf, 0, min(pLS->lBufferSize, (unsigned long) (SampleRate * 50) ) * sizeof(LADSPA_Data));

   pLS->lLoopChunkCount = MAX_LOOPS;

   // not using calloc to force touching all memory ahead of time -- PSYCHE!
   pLS->pLoopChunks = (LoopChunk *) calloc(pLS->lLoopChunkCount,  sizeof(LoopChunk));
   if (pLS->pLoopChunks == NULL) {
	   goto cleanup;
   }
   memset (pLS->pLoopChunks, 0, pLS->lLoopChunkCount * sizeof(LoopChunk));

   pLS->lastLoopChunk = pLS->pLoopChunks + pLS->lLoopChunkCount - 1;

   // this is the input buffer to handle input latency.  32k max samples of input latency
   pLS->lInputBufSize = 32768;
   pLS->lInputBufMask = pLS->lInputBufSize - 1;
   pLS->pInputBuf = (LADSPA_Data *) calloc(pLS->lInputBufSize, sizeof(LADSPA_Data));
   pLS->lInputBufWritePos = 0;
   pLS->lInputBufReadPos = 0;
   
   /* just one for now */
   //pLS->lLoopStart = 0;
   //pLS->lLoopStop = 0;   
   //pLS->lCurrPos = 0;

   pLS->state = STATE_OFF;
   pLS->wasMuted = false;
   pLS->recSyncEnded = false;
   
   DBG(fprintf(stderr,"%u:%u  instantiated with buffersize: %lu\n", pLS->lLoopIndex, pLS->lChannelIndex, pLS->lBufferSize));

   
   pLS->pfQuantMode = &pLS->fQuantizeMode;
   pLS->pfRoundMode = &pLS->fRoundMode;
   pLS->pfRedoTapMode = &pLS->fRedoTapMode;
   
   //cerr << "INSTANTIATE: " << pLS << endl;
   
   return pLS;

cleanup:

   if (pLS->pSampleBuf) {
	   free (pLS->pSampleBuf);
   }
   if (pLS->pLoopChunks) {
	   free (pLS->pLoopChunks);
   }
   return NULL;
   
}

/*****************************************************************************/

/* Throw away a simple delay line. */
void 
cleanupSooperLooper(LADSPA_Handle Instance) {
	
	SooperLooperI * pLS;
	
	pLS = (SooperLooperI *)Instance;
	
	if (pLS->pLoopChunks) {
		free (pLS->pLoopChunks);
	}
	
	if (pLS->pSampleBuf) {
		free (pLS->pSampleBuf);
	}
	
	//cerr << "******* cleanup SL instance" << endl;
	
	
	free(pLS);
}


/*****************************************************************************/

/* Initialise and activate a plugin instance. */
void
activateSooperLooper(LADSPA_Handle Instance) {

  SooperLooperI * pLS;
  pLS = (SooperLooperI *)Instance;

  //cerr << "Activate: " << pLS << endl;
	 
  pLS->lLastMultiCtrl = -1;

  pLS->lScratchSamples = 0;
  pLS->lTapTrigSamples = 0;
  pLS->lRampSamples = 0;
  pLS->bPreTap = 1; // first tap init
  pLS->fLastScratchVal = 0.0f;
  pLS->fLastTapCtrl = -1;
  pLS->fCurrRate = 1.0f;
  pLS->fNextCurrRate = 0.0f;
  pLS->fQuantizeMode = 0;
  pLS->fRoundMode = 0;  
  pLS->bHoldMode = 0;
  pLS->fSyncMode = 0;
  pLS->fMuteQuantized = 0;
  pLS->fOverdubQuantized = 0;
  pLS->bReplaceQuantized = true;
  pLS->fRedoTapMode = 1;
  pLS->bRateCtrlActive = (int) *pLS->pfRateCtrlActive;

  pLS->waitingForSync = 0;
  pLS->donePlaySync = false;
  pLS->rounding = false;
  pLS->safetyFeedback = true;
  
  pLS->fWetCurr = pLS->fWetTarget = *pLS->pfWet;
  pLS->fDryCurr = pLS->fDryTarget = *pLS->pfDry;
  pLS->fFeedbackCurr = pLS->fFeedbackTarget = *pLS->pfFeedback;
  pLS->fRateCurr = pLS->fRateTarget = *pLS->pfRate;
  pLS->fScratchPosCurr = pLS->fScratchPosTarget = *pLS->pfScratchPos;

  pLS->fLoopFadeDelta = 0.0f;
  pLS->fLoopFadeAtten = 0.0f;
  pLS->fLoopSrcFadeDelta = 0.0f;
  pLS->fLoopSrcFadeAtten = 0.0f;
  pLS->fFeedSrcFadeDelta = 0.0f;
  pLS->fFeedSrcFadeAtten = 1.0f;
  pLS->fPlayFadeDelta = 0.0f;
  pLS->fPlayFadeAtten = 0.0f;
  pLS->fFeedFadeDelta = 0.0f;
  pLS->fFeedFadeAtten = 1.0f;

  // todo make this a port, for now 2ms
  //pLS->fLoopXfadeSamples = 0.002 * pLS->fSampleRate;
  
  pLS->state = STATE_OFF;
  pLS->nextState = -1;
  pLS->wasMuted = false;
  pLS->recSyncEnded = false;

  pLS->lSamplesSinceSync = 0;

  pLS->lInputBufWritePos = 0;
  pLS->lFramesUntilInput = 0;
  pLS->lFramesUntilFilled = 0;
  memset (pLS->pInputBuf, 0, pLS->lInputBufSize * sizeof(LADSPA_Data));
  
  clearLoopChunks(pLS);


  if (pLS->pfSecsTotal) {
     *pLS->pfSecsTotal = (LADSPA_Data) pLS->fTotalSecs;
  }

  if (pLS->pfSecsFree) {
	  *pLS->pfSecsFree = pLS->lBufferSize / pLS->fSampleRate;
  }
  
  //fprintf(stderr,"activated\n");  
}

/*****************************************************************************/

/* Connect a port to a data location. */
void 
connectPortToSooperLooper(LADSPA_Handle Instance,
			     unsigned long Port,
			     LADSPA_Data * DataLocation)
{
   
   SooperLooperI * pLS;

   //fprintf(stderr,"connectPortTo %d  %08x\n", Port, DataLocation);  

   
   pLS = (SooperLooperI *)Instance;
   switch (Port) {
      case DryLevel:
	 pLS->pfDry = DataLocation;
	 break;
      case WetLevel:
	 pLS->pfWet = DataLocation;
	 break;

      case Feedback:
	 pLS->pfFeedback = DataLocation;
	 break;
      case TriggerThreshold:
	 pLS->pfTrigThresh = DataLocation;
	 break;
      case Rate:
	 pLS->pfRate = DataLocation;
	 break;
      case ScratchPosition:
	 pLS->pfScratchPos = DataLocation;
	 break;
      case Multi:
	 pLS->pfMultiCtrl = DataLocation;
	 break;
      case TapDelayTrigger:
	 pLS->pfTapCtrl = DataLocation;
	 break;
      case UseFeedbackPlay:
	 pLS->pfUseFeedbackPlay = DataLocation;
	 break;
      case Quantize:
	 pLS->pfQuantMode = DataLocation;
	 break;
      case Sync:
	 pLS->pfSyncMode = DataLocation;
	 break;
      case PlaybackSync:
	 pLS->pfPlaybackSyncMode = DataLocation;
	 break;
      case UseRate:
	 pLS->pfRateCtrlActive = DataLocation;
	 break;
      case FadeSamples:
	 pLS->pfXfadeSamples = DataLocation;
	 break;
      case TempoInput:
	 pLS->pfTempo = DataLocation;
	 break;
      case EighthPerCycleLoop:
	 pLS->pfEighthPerCycle = DataLocation;
	 break;
      case Round:
	 pLS->pfRoundMode = DataLocation;
	 break;
      case RedoTap:
	 pLS->pfRedoTapMode = DataLocation;
	 break;
      case UseSafetyFeedback:
	 pLS->pfUseSafetyFeedback = DataLocation;
	 break;
      case OutputLatency:
	 pLS->pfOutputLatency = DataLocation;
	 break;
      case InputLatency:
	 pLS->pfInputLatency = DataLocation;
	 break;
      case TriggerLatency:
	 pLS->pfTriggerLatency = DataLocation;
	 break;
      case MuteQuantized:
	 pLS->pfMuteQuantized = DataLocation;
	 break;
      case OverdubQuantized:
	 pLS->pfOverdubQuantized = DataLocation;
	 break;
      case SyncOffsetSamples:
	 pLS->pfSyncOffsetSamples = DataLocation;
	 break;
      case RoundIntegerTempo:
	 pLS->pfRoundIntegerTempo = DataLocation;
	 break;
	 
      case AudioInputPort:
	 pLS->pfInput = DataLocation;
	 break;
      case AudioOutputPort:
	 pLS->pfOutput = DataLocation;
	 break;
      case SyncInputPort:
	 pLS->pfSyncInput = DataLocation;
	 break;
      case SyncOutputPort:
	 pLS->pfSyncOutput = DataLocation;
	 break;

      case State:
	 pLS->pfStateOut = DataLocation;
	 break;
      case LoopLength:
	 pLS->pfLoopLength = DataLocation;
	 break;
      case LoopPosition:
	 pLS->pfLoopPos = DataLocation;
	 break;
      case CycleLength:
	 pLS->pfCycleLength = DataLocation;
	 break;
      case LoopFreeMemory:
	 pLS->pfSecsFree = DataLocation;
	 break;
      case LoopMemory:
	 pLS->pfSecsTotal= DataLocation;
	 break;
      case Waiting:
	 pLS->pfWaiting= DataLocation;
	 break;
      case TrueRate:
	 pLS->pfRateOutput= DataLocation;
	 break;
      case NextState:
	 pLS->pfNextStateOut = DataLocation;
	 break;

   }
}



static inline void fillLoops(SooperLooperI *pLS, LoopChunk *mloop, unsigned long lCurrPos, bool leavemarks)
{
   LoopChunk *loop=NULL, *nloop, *srcloop;
   
   // descend to the oldest valid unfilled loop
   for (nloop=mloop; nloop; nloop = nloop->srcloop)
   {
      if (nloop->valid && (nloop->frontfill || nloop->backfill)) {
	 loop = nloop;
	 continue;
      }
      
      break;
   }

   // everything is filled!
   if (!loop) return;

   // do filling from earliest to latest
   for (; loop; loop=loop->next)
   {
      srcloop = loop->srcloop;

      // leavemarks is a special hack
      if (leavemarks || (loop->frontfill && lCurrPos<=loop->lMarkH && lCurrPos>=loop->lMarkL))
      {
	      if (!srcloop->valid) {
		      // if src is not valid, fill with silence
		      pLS->pSampleBuf[(loop->lLoopStart + lCurrPos) & pLS->lBufferSizeMask] = 0.0f;
		      //DBG(fprintf(stderr, "srcloop invalid\n"));
	      }
	      else {
		      // we need to finish off a previous
		      pLS->pSampleBuf[(loop->lLoopStart + lCurrPos) & pLS->lBufferSizeMask] = 
			      pLS->pSampleBuf[(srcloop->lLoopStart + (lCurrPos % srcloop->lLoopLength)) & pLS->lBufferSizeMask];
	      }

	      if (!leavemarks) {
		      // move the right mark according to rate
		      if (pLS->fCurrRate > 0) {
			      loop->lMarkL = lCurrPos;
		      }
		      else {
			      loop->lMarkH = lCurrPos;
		      }
		      
		      // ASSUMPTION: our overdub rate is +/- 1 only
		      if (loop->lMarkL == loop->lMarkH) {
			      // now we take the input from ourself
			      DBG(fprintf(stderr,"%u:%u  front segment filled for %08x for %08x in at %lu\n", pLS->lLoopIndex, pLS->lChannelIndex,
					  (unsigned)loop, (unsigned) srcloop, loop->lMarkL););
			      loop->frontfill = 0;
			      loop->lMarkL = loop->lMarkH = LONG_MAX;
		      }
	      }
      }
      else if (loop->backfill && lCurrPos<=loop->lMarkEndH && lCurrPos>=loop->lMarkEndL)		
      {

	      if (!srcloop->valid) {
		      // if src is not valid, fill with silence
		      pLS->pSampleBuf[(loop->lLoopStart + lCurrPos) & pLS->lBufferSizeMask] = 0.0f;
		      //DBG(fprintf(stderr, "srcloop invalid\n"));
	      }
	      else {
		      // we need to finish off a previous
		      pLS->pSampleBuf[(loop->lLoopStart + lCurrPos) & pLS->lBufferSizeMask] =
			      pLS->pSampleBuf[(srcloop->lLoopStart +
				       ((lCurrPos  + loop->lStartAdj - loop->lEndAdj) % srcloop->lLoopLength)) & pLS->lBufferSizeMask];
	      }

	      if (!leavemarks) {
		      // move the right mark according to rate
		      if (pLS->fCurrRate > 0) {
			      loop->lMarkEndL = lCurrPos;
		      }
		      else {
			      loop->lMarkEndH = lCurrPos;
			      
		      }
		      // ASSUMPTION: our overdub rate is +/- 1 only
		      if (loop->lMarkEndL == loop->lMarkEndH) {
			      // now we take the input from ourself
			      DBG(fprintf(stderr,"%u:%u  back segment filled in for %08x from %08x at %lu\n", pLS->lLoopIndex, pLS->lChannelIndex,
					  (unsigned)loop, (unsigned)srcloop, loop->lMarkEndL););
			      loop->backfill = 0;
			      loop->lMarkEndL = loop->lMarkEndH = LONG_MAX;
		      }
	      }
      }
      
      if (mloop == loop) break;
   }

}


static LoopChunk* transitionToNext(SooperLooperI *pLS, LoopChunk *loop, int nextstate);


static LoopChunk* beginMultiply(SooperLooperI *pLS, LoopChunk *loop)
{
   LoopChunk * srcloop = loop;
   int xfadeSamples = (int) (*pLS->pfXfadeSamples);
   if (xfadeSamples < 1) xfadeSamples = 1;
   int prevstate = pLS->state;
   
   // first check if this is a multi-increase
   if (loop && loop->mult_out == (int)loop->lCycles && loop->backfill) {
	   //fprintf(stderr, "got a first multi-increase\n");
	   // increase by one cycle length

	   loop->mult_out += 1;
	   pLS->state = STATE_MULTIPLY;
	   
	   pLS->fLoopFadeDelta = 1.0f / xfadeSamples;
	   pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
	   
	   pLS->nextState = STATE_PLAY;

	   return loop;
   }
   else {
	   // make new loop chunk
	   loop = pushNewLoopChunk(pLS, loop->lCycleLength, loop);
   }
	   
   if (loop) {
      // if there is no source bail
      if (!loop->prev) {
	      // lets just back out and revalidate the passed in loop
	      loop->next = NULL;

	      DBG(fprintf(stderr, "begin mult: there is no source\n"));
	      
	      loop = srcloop;
	      pLS->headLoopChunk = pLS->tailLoopChunk = loop;
	      loop->next = NULL;
	      loop->prev = NULL;
	      loop->valid = 1;

	      // no mult op
	      return NULL;
	      // this is already set if we have no srcloop anymore
      }
      else {
	      loop->srcloop = srcloop = loop->prev;
      }

      int prevstate = pLS->state;

      pLS->state = STATE_MULTIPLY;

      if (prevstate == STATE_TRIG_STOP) {
	      // coming out of record we can actually 
	      // set the loopfadeatten to 0 without harm
	      // since we're fading out the loopsrcfade
	      pLS->fLoopFadeAtten = 0.0f;
      }

//      if (*pLS->pfQuantMode == 0.0f) {
	      // we'll do this later if we are quantizing
	      pLS->fLoopFadeDelta = 1.0f / xfadeSamples;
	      pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
//      }

      //pLS->fPlayFadeDelta = -1.0f / xfadeSamples;
      long lInputLatency = (long) (*pLS->pfInputLatency);
      long lOutputLatency = (long) (*pLS->pfOutputLatency);
      long lTriggerLatency = (long) (*pLS->pfTriggerLatency);
      pLS->lFramesUntilInput = (long) lInputLatency - lTriggerLatency;

      // start out with the single cycle as our length as a marker
      loop->lLoopLength = srcloop->lCycleLength;
      //loop->lLoopLength = srcloop->lLoopLength;

      // start out at same pos
      loop->dCurrPos = srcloop->dCurrPos;
      loop->lCycles = 1; // start with 1 by default
      //loop->lCycles = srcloop->lCycles; 

      loop->lCycleLength = srcloop->lCycleLength;

      loop->lSyncPos = srcloop->lSyncPos;
      loop->lOrigSyncPos = srcloop->lOrigSyncPos;
      loop->lSyncOffset = srcloop->lSyncOffset;
		       
      // force rate to +1
      pLS->fCurrRate = 1.0;
      loop->frontfill = 1;
      loop->backfill = 0;
      loop->firsttime = 1;
      loop->lStartAdj = 0;
      loop->lEndAdj = 0;
      pLS->nextState = -1;
      loop->dOrigFeedback = LIMIT_BETWEEN_0_AND_1(*pLS->pfFeedback);
		       
      // handle the case where the src loop
      // is already multiplied
      if (srcloop->lCycles > 1) {
	      // we effectively remove the first cycles from our new one
	      if (*pLS->pfQuantMode == QUANT_CYCLE) {
		      //loop->lStartAdj = ((int)floor(fabs((srcloop->dCurrPos-1) / srcloop->lCycleLength))
		      //		 + 1) * srcloop->lCycleLength; 
		      loop->lStartAdj = ((int)floorf(fabs((srcloop->dCurrPos) / srcloop->lCycleLength))) * srcloop->lCycleLength; 

		      loop->frontfill = 0; // no need.
		      DBG(fprintf(stderr, "%u:%u  start adj = %lu\n", pLS->lLoopIndex, pLS->lChannelIndex, loop->lStartAdj));
	      }
	      else {
		      // whatever cycle we are in will become the first
		      loop->lStartAdj = ((int)floorf(fabs((srcloop->dCurrPos) / srcloop->lCycleLength))) * srcloop->lCycleLength; 

	      }

	      // adjust dCurrPos by start adj.
	      // we handle this properly in the processing section
	      loop->dCurrPos = loop->dCurrPos - loop->lStartAdj;
	      
	      // start with 1 because we could end up with none!
	      // which will be subtracted at the end
	      loop->lCycles = 1;
	      loop->lLoopLength = srcloop->lCycleLength;
	      //loop->lLoopLength = 0;
	      DBG(fprintf(stderr,"%u:%u  Quantize ignoring first %d cycles.  Orig length %lu\n", pLS->lLoopIndex, pLS->lChannelIndex,
			  ((int)floor(srcloop->dCurrPos / srcloop->lCycleLength)+ 1),
			  srcloop->lLoopLength));
			  
      }

      double rCurrPos = srcloop->dCurrPos;

      if (prevstate != STATE_RECORD && prevstate != STATE_TRIG_STOP) {
	      rCurrPos -= lInputLatency - lOutputLatency;
	      
	      if (rCurrPos < 0) {
		      rCurrPos += loop->lLoopLength;
	      }
	      rCurrPos = fmod (rCurrPos, loop->lLoopLength);
      }
      
      if (loop->dCurrPos > 0) {
		  loop->lMarkL = 0;
		  //loop->lMarkH = (unsigned long) rCurrPos - 1;
		  //loop->lMarkH = (unsigned long) max (rCurrPos - (lOutputLatency + lInputLatency), 1.0) - 1;
		  long markh = (long) fmod (rCurrPos - (lOutputLatency + lInputLatency), loop->lLoopLength) - 1;
		  //cerr << "Low MARK H: " << markh << endl;
		  if (markh < 0) markh = 0;
		  loop->lMarkH = markh;
		  if (loop->lMarkH == 0) {
			  loop->frontfill = 0;
			  loop->lMarkL = loop->lMarkH = LONG_MAX;
		  }
      }
      else {
	      DBG(cerr << "no frontfill, dcurrpos: " << loop->dCurrPos << endl);
		  // no need to frontfill
		  loop->frontfill = 0;
		  loop->lMarkL = loop->lMarkH = LONG_MAX;
      }
      
      loop->lMarkEndL = loop->lMarkEndH = LONG_MAX;

      
      DBG(fprintf(stderr,"%u:%u  Mark at L:%lu  h:%lu\n", pLS->lLoopIndex, pLS->lChannelIndex,loop->lMarkL, loop->lMarkH);
	      fprintf(stderr,"%u:%u  EndMark at L:%lu  h:%lu\n", pLS->lLoopIndex, pLS->lChannelIndex,loop->lMarkEndL, loop->lMarkEndH);
		  fprintf(stderr,"%u:%u  Entering MULTIPLY state  with cyclecount=%lu   curpos=%g   looplen=%lu\n", pLS->lLoopIndex, pLS->lChannelIndex, loop->lCycles, loop->dCurrPos, loop->lLoopLength));

      if (prevstate == STATE_RECORD || prevstate == STATE_TRIG_STOP) {
	      // input always immediately available
	      pLS->lFramesUntilInput = 0;
	      // rest is handled elsewhere
	      pLS->fLoopSrcFadeDelta = -1.0f / xfadeSamples;
	      pLS->fFeedSrcFadeDelta = 1.0f / xfadeSamples;
      }

   }

   return loop;

}


// encapsulates what happens when a multiply is ended
static LoopChunk * endMultiply(SooperLooperI *pLS, LoopChunk *loop, int nextstate)
{
   LoopChunk *srcloop;
   int xfadeSamples = (int) (*pLS->pfXfadeSamples);
   if (xfadeSamples < 1) xfadeSamples = 1;
   
   srcloop = loop->srcloop;
		    
   if (*pLS->pfQuantMode != 0 && srcloop->lCycles > 1 && loop->lCycles<1)
   {
      DBG(fprintf(stderr,"%u:%u  Zero length loop now: at %d!\n", pLS->lLoopIndex, pLS->lChannelIndex, (int)loop->dCurrPos));
      DBG(fprintf(stderr,"%u:%u  Entering %d from MULTIPLY\n", pLS->lLoopIndex, pLS->lChannelIndex, nextstate));
      pLS->state = nextstate;

      loop->backfill = 0;
      loop->lLoopLength = 0;
   }
   else
   {
      if (*pLS->pfQuantMode != 0 && srcloop->lCycles > 1) {
	 // subtract a cycle right off?
	 //loop->lCycles -= 1;

      }

      
      long lInputLatency = (long) (*pLS->pfInputLatency);
      long lOutputLatency = (long) (*pLS->pfOutputLatency);
      pLS->lFramesUntilFilled = lInputLatency + lOutputLatency; //  to take care of leftover filling
      
      if (*pLS->pfRoundMode == 0) {
	 // calculate loop length
	 loop->lLoopLength = loop->lCycles * loop->lCycleLength;
	 loop->backfill = 1;

	 // adjust curr position
	 //loop->dCurrPos -= loop->lStartAdj;
	 loop->lMarkEndL = (unsigned long) loop->dCurrPos;
	 loop->lMarkEndH = loop->lLoopLength - 1;

	 DBG(fprintf(stderr,"%u:%u  Entering %d from MULTIPLY. Length %lu.  %lu cycles\n", pLS->lLoopIndex, pLS->lChannelIndex,nextstate,
		 loop->lLoopLength, loop->lCycles)); 
	 DBG(fprintf(stderr,"%u:%u  EndMark at L:%lu  h:%lu\n", pLS->lLoopIndex, pLS->lChannelIndex,loop->lMarkEndL, loop->lMarkEndH));

   
	 pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
	 pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
	 
	 loop = transitionToNext(pLS, loop, nextstate);

      }
      else {
	 // in round mode we need to wait it out
	 // and keep recording till the end
	 DBG(fprintf(stderr,"%u:%u  Finishing MULTIPLY rounded\n", pLS->lLoopIndex, pLS->lChannelIndex)); 
	 loop->lMarkEndL = (unsigned long)loop->dCurrPos;
//	 loop->lMarkEndH = loop->lLoopLength + loop->lStartAdj - 1;
	 loop->lMarkEndH = loop->lLoopLength - 1;
	 pLS->nextState = nextstate;
	 pLS->rounding = true;
      }
   }

   
   return loop;
}


static LoopChunk * beginInsert(SooperLooperI *pLS, LoopChunk *loop)
{
   LoopChunk *srcloop = loop;
   int xfadeSamples = (int) (*pLS->pfXfadeSamples);
   if (xfadeSamples < 1) xfadeSamples = 1;
   int prevstate = pLS->state;
   
   // try to get a new one with at least 1 cycle more length
   loop = pushNewLoopChunk(pLS, loop->lLoopLength + loop->lCycleLength, loop);

   if (loop) {
      DBG(fprintf(stderr,"%u:%u  Entering INSERT state\n", pLS->lLoopIndex, pLS->lChannelIndex));

      if (!loop->prev) {
	      // lets just back out and revalidate the passed in loop
	      loop->next = NULL;

	      DBG(fprintf(stderr, "begin ins: there is no source\n"));
	      
	      loop = srcloop;
	      pLS->headLoopChunk = pLS->tailLoopChunk = loop;
	      loop->next = NULL;
	      loop->prev = NULL;
	      loop->valid = 1;

	      // no mult op
	      return NULL;
	      // this is already set if we have no srcloop anymore
      }
      else {
	      loop->srcloop = srcloop = loop->prev;
      }

      pLS->state = STATE_INSERT;

      if (*pLS->pfQuantMode == 0.0f) {
	      // we'll do this later if we are quantizing
	      pLS->fLoopFadeDelta = 1.0f / xfadeSamples;
	      pLS->fFeedFadeDelta = -1.0f / xfadeSamples;
      }

      pLS->fPlayFadeDelta = -1.0f / xfadeSamples;

      long lInputLatency = (long) (*pLS->pfInputLatency);
      long lOutputLatency = (long) (*pLS->pfOutputLatency);
      long lTriggerLatency = (long) (*pLS->pfTriggerLatency);
      
      pLS->lFramesUntilInput = (long) lInputLatency - lTriggerLatency;
      
      // start out with the single cycle extra as our length
      loop->lLoopLength = srcloop->lLoopLength + srcloop->lCycleLength;
      loop->dCurrPos = srcloop->dCurrPos;
      loop->lCycles = srcloop->lCycles + 1; // start with src by default
      loop->lCycleLength = srcloop->lCycleLength;
      loop->dOrigFeedback = LIMIT_BETWEEN_0_AND_1(*pLS->pfFeedback);
		       
      // force rate to +1
      pLS->fCurrRate = 1.0;
      loop->frontfill = 1;
      loop->backfill = 0;
      loop->firsttime = 1;
      loop->lStartAdj = 0;
      loop->lEndAdj = 0;
		       
      if (*pLS->pfQuantMode == QUANT_CYCLE) {
	 // the next cycle boundary
	 loop->lInsPos =
	    ((int)floor(fabs(srcloop->dCurrPos-1) / srcloop->lCycleLength)
	     + 1) * srcloop->lCycleLength; 
      }
      else {
	 loop->lInsPos = (unsigned long) loop->dCurrPos;
      }


      if (*pLS->pfRoundMode != 0) {
	      loop->lRemLen = (unsigned long) (srcloop->lLoopLength - loop->lInsPos);
      }
      else {
	      loop->lRemLen = (unsigned long) (srcloop->lLoopLength - loop->dCurrPos);
      }
      pLS->nextState = -1;

      double rCurrPos = srcloop->dCurrPos;

      if (prevstate != STATE_RECORD && prevstate != STATE_TRIG_STOP) {
	      //rCurrPos -= lInputLatency + lOutputLatency;
      
	      //if (rCurrPos < 0) {
	      //	      rCurrPos += loop->lLoopLength;
	      //}
	      //rCurrPos = fmod (rCurrPos, loop->lLoopLength);
      }
      
      if (loop->dCurrPos > 0) {
	      long markh = (long) fmod (rCurrPos - (lOutputLatency + lInputLatency), loop->lLoopLength) - 1;
	      if (markh < 0) markh = 0;
	      loop->lMarkL = 0;
	      loop->lMarkH = (unsigned long) markh;
      }
      else {
	      loop->frontfill = 0; 
	      loop->lMarkL = loop->lMarkH = LONG_MAX;
      }
      
      loop->lMarkEndL = loop->lMarkEndH = LONG_MAX;
		       
      DBG(fprintf(stderr, "%u:%u  InsPos=%lu  RemLen=%lu\n", pLS->lLoopIndex, pLS->lChannelIndex, loop->lInsPos, loop->lRemLen));
      DBG(fprintf(stderr,"%u:%u  Total cycles now=%lu\n", pLS->lLoopIndex, pLS->lChannelIndex, loop->lCycles));
   }

   return loop;

}

// encapsulates what happens when a insert is ended
static LoopChunk * endInsert(SooperLooperI *pLS, LoopChunk *loop, int nextstate)
{
   LoopChunk *srcloop;

   int xfadeSamples = (int) (*pLS->pfXfadeSamples);
   if (xfadeSamples < 1) xfadeSamples = 1;
   
   srcloop = loop->srcloop;

   // we wait it out in insert until the end of the loop
   // in round mode we need to wait it out
   // and keep recording till the end

   // the end adjust is the difference in length between us and our source
   // now set our end adjustment for later backfill
   loop->lEndAdj = loop->lLoopLength - srcloop->lLoopLength;

   long lInputLatency = (long) (*pLS->pfInputLatency);
   long lOutputLatency = (long) (*pLS->pfOutputLatency);
   pLS->lFramesUntilFilled = lInputLatency + lOutputLatency; //  to take care of leftover filling
   
   loop->lMarkEndL = (unsigned long)loop->dCurrPos;
   loop->lMarkEndH = loop->lLoopLength - loop->lRemLen;

   DBG(fprintf(stderr,"%u:%u  Finishing INSERT... lMarkEndL=%lu  lMarkEndH=%lu  ll=%lu  rl=%lu\n", pLS->lLoopIndex, pLS->lChannelIndex,
	       loop->lMarkEndL, loop->lMarkEndH, loop->lLoopLength, loop->lRemLen)); 

   pLS->nextState = nextstate;

   if (*pLS->pfRoundMode == 0.0f) {
	   pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
	   pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
   }

   pLS->rounding = true;
   
   return loop;
   
}

static LoopChunk * beginOverdub(SooperLooperI *pLS, LoopChunk *loop)
{
   LoopChunk * srcloop = loop;
   int xfadeSamples = (int) (*pLS->pfXfadeSamples);
   if (xfadeSamples < 1) xfadeSamples = 1;

   // make new loop chunk
   //loop = pushNewLoopChunk(pLS, loop->lLoopLength, loop);
   loop = pushNewLoopChunk(pLS, 0, loop);
   if (loop) {
	   int prevstate = pLS->state;

      pLS->state = STATE_OVERDUB;
      // always the same length as previous loop

      if (prevstate == STATE_TRIG_STOP) {
	      // coming out of record we can actually 
	      // set the loopfadeatten to 0 without harm
	      // since we're fading out the loopsrcfade
	      pLS->fLoopFadeAtten = 0.0f;
      }

      pLS->fLoopFadeDelta = 1.0f / xfadeSamples;
      long lInputLatency = (long) (*pLS->pfInputLatency);
      long lOutputLatency = (long) (*pLS->pfOutputLatency);
      long lTriggerLatency = (long) (*pLS->pfTriggerLatency);

      pLS->fLoopSrcFadeDelta = -1.0f / xfadeSamples;
      pLS->fFeedSrcFadeDelta = 1.0f / xfadeSamples;

      pLS->lFramesUntilInput = (long) lInputLatency - lTriggerLatency;
      pLS->lFramesUntilFilled = lOutputLatency + lInputLatency;
      DBG(cerr << pLS->lLoopIndex << ":" << pLS->lChannelIndex << "   frames until input: " << pLS->lFramesUntilInput << "  xfade samps: " << xfadeSamples << endl);
				      
      if (!loop->prev) {
	      // then we are overwriting our own source
	      // lets just back out and revalidate the passed in loop
	      loop->next = NULL;

	      DBG(fprintf(stderr, "%u:%u  OVERdub using self\n", pLS->lLoopIndex, pLS->lChannelIndex));
	      
	      loop = srcloop;
	      pLS->headLoopChunk = pLS->tailLoopChunk = loop;
	      loop->next = NULL;
	      loop->prev = NULL;
	      loop->valid = 1;
	      loop->srcloop = loop; // !!!

	      pLS->nextState = -1;

	      return loop;
	      // this is already set if we have no srcloop anymore
      }
      else {
	      loop->srcloop = srcloop = loop->prev;
      }

      loop->lCycles = srcloop->lCycles;
      loop->lCycleLength = srcloop->lCycleLength;
      loop->lLoopLength = srcloop->lLoopLength;
      loop->dCurrPos = srcloop->dCurrPos;
      loop->lSyncPos = srcloop->lSyncPos;
      loop->lOrigSyncPos = srcloop->lOrigSyncPos;
      loop->lSyncOffset = srcloop->lSyncOffset;
    
      loop->lStartAdj = 0;
      loop->lEndAdj = 0;
      pLS->nextState = -1;
      
      loop->dOrigFeedback = LIMIT_BETWEEN_0_AND_1(*pLS->pfFeedback);

      //double rCurrPos = loop->dCurrPos - lInputLatency - lOutputLatency;
      double rCurrPos = loop->dCurrPos;
      if (rCurrPos < 0) {
	      rCurrPos += loop->lLoopLength;
      }
      rCurrPos = fmod (rCurrPos, loop->lLoopLength);
      
      if (rCurrPos > 0) 
	 loop->frontfill = 1;
      else
	 loop->frontfill = 0;

      loop->backfill = 1;
      // logically we need to fill in the cycle up to the
      // srcloop's current position.
      // we let the overdub loop itself do this when it gets around to it
      
      if (pLS->fCurrRate < 0) {
	 pLS->fCurrRate = -1.0;
	 // negative rate
	 // need to fill in between these values 
	 loop->lMarkL = (unsigned long) rCurrPos + 1;		       
	 loop->lMarkH = loop->lLoopLength - 1;
	 loop->lMarkEndL = 0;
	 //loop->lMarkEndH = (unsigned long) rCurrPos;
	 loop->lMarkEndH = (unsigned long) fmod (rCurrPos + (lOutputLatency + lInputLatency), loop->lLoopLength);

	 if (loop->lMarkEndH < loop->lMarkL) {
		 DBG(cerr << pLS->lLoopIndex << ":" << pLS->lChannelIndex << "  Border line rev" << endl);
		 loop->lMarkL = loop->lMarkEndH;
		 loop->lMarkEndH = (unsigned long) rCurrPos + 1;
	 }

      } else {
	 pLS->fCurrRate = 1.0;
	 loop->lMarkL = 0;
	 //loop->lMarkH = (unsigned long) fmod (rCurrPos - (lOutputLatency + lInputLatency), loop->lLoopLength)) - 1;
	 long markh = (long) fmod (rCurrPos - (lOutputLatency + lInputLatency), loop->lLoopLength) - 1;
	 if (markh < 0) markh = 0;
	 loop->lMarkH = (unsigned long) markh;
	 loop->lMarkEndL = (unsigned long) rCurrPos;
	 loop->lMarkEndH = loop->lLoopLength - 1;

	 if (loop->lMarkH > loop->lMarkEndL) {
		 DBG(cerr << "Border line" << endl);
		 loop->lMarkEndL = loop->lMarkH;
		 loop->lMarkH = (unsigned long) rCurrPos;
	 }
      }
      
      DBG(fprintf(stderr,"%u:%u  Mark at L:%lu  h:%lu\n", pLS->lLoopIndex, pLS->lChannelIndex,loop->lMarkL, loop->lMarkH));
      DBG(fprintf(stderr,"%u:%u  EndMark at L:%lu  h:%lu\n", pLS->lLoopIndex, pLS->lChannelIndex,loop->lMarkEndL, loop->lMarkEndH));
      DBG(fprintf(stderr,"%u:%u  Entering OVERDUB/repl/subs state: srcloop is %08x\n", pLS->lLoopIndex, pLS->lChannelIndex, (unsigned)srcloop));
   }

   return loop;
}

static LoopChunk * beginSubstitute(SooperLooperI *pLS, LoopChunk *loop)
{
	LoopChunk * tloop = beginOverdub(pLS, loop);
	int xfadeSamples = (int) (*pLS->pfXfadeSamples);
	if (xfadeSamples < 1) xfadeSamples = 1;

	if (tloop) {
		pLS->state = STATE_SUBSTITUTE;
		pLS->fFeedFadeDelta = -1.0f / xfadeSamples;
		pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
	}

	return tloop;
}

static LoopChunk * beginReplace(SooperLooperI *pLS, LoopChunk *loop)
{
	LoopChunk * tloop = beginOverdub(pLS, loop);
	int xfadeSamples = (int) (*pLS->pfXfadeSamples);
	if (xfadeSamples < 1) xfadeSamples = 1;

	if (tloop) {
		pLS->state = STATE_REPLACE;
		pLS->fPlayFadeDelta = -1.0f / xfadeSamples;
		pLS->fFeedFadeDelta = -1.0f / xfadeSamples;
	}

	return tloop;
}


static LoopChunk * transitionToNext(SooperLooperI *pLS, LoopChunk *loop, int nextstate)
{
   LoopChunk * newloop = loop;

   int xfadeSamples = (int) (*pLS->pfXfadeSamples);
   if (xfadeSamples < 1) xfadeSamples = 1;

   unsigned int eighthSamples = 0;
   unsigned int syncSamples = 0;
   if (*pLS->pfTempo > 0.0f) {
	   eighthSamples = (unsigned int) (pLS->fSampleRate * 30.0 / *pLS->pfTempo);
	   
	   if (*pLS->pfQuantMode == QUANT_CYCLE || *pLS->pfQuantMode == QUANT_LOOP) {
		   syncSamples = (unsigned int) (eighthSamples * *pLS->pfEighthPerCycle / 2);
	   }
	   else if (*pLS->pfQuantMode == QUANT_8TH) {
		   syncSamples = (eighthSamples / 2);
	   }
   }
   

   
   switch(nextstate)
   {
      case STATE_PLAY:
	      pLS->fLoopFadeDelta = -1.0f / (xfadeSamples);
	      pLS->fLoopSrcFadeDelta = -1.0f / xfadeSamples;
	      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
	      pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
	      pLS->fFeedSrcFadeDelta = 1.0f / xfadeSamples;
	      pLS->wasMuted = false;
	      if (pLS->state == STATE_PAUSED && loop) {
		      // set current loop position to paused position
		      loop->dCurrPos = pLS->dPausedPos;
		      DBG(fprintf(stderr, "%u:%u  t2n setting loop pos to pause pos: %g\n", pLS->lLoopIndex, pLS->lChannelIndex, pLS->dPausedPos));
	      }
	      break;
      case STATE_MUTE:
      case STATE_PAUSED:
	      pLS->fLoopFadeDelta = -1.0f / (xfadeSamples);
	      pLS->fPlayFadeDelta = -1.0f / xfadeSamples;
	      pLS->fLoopSrcFadeDelta = -1.0f / xfadeSamples;
	      pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
	      pLS->fFeedSrcFadeDelta = 1.0f / xfadeSamples;
	      pLS->wasMuted = true;
	      if (nextstate == STATE_PAUSED && loop) {
		      pLS->dPausedPos = loop->dCurrPos;
		      pLS->wasMuted = false;
	      }
	      break;

      case STATE_OVERDUB:
	 newloop = beginOverdub(pLS, loop);
	 break;

      case STATE_REPLACE:
	 newloop = beginReplace(pLS, loop);
	 break;

      case STATE_SUBSTITUTE:
	 newloop = beginSubstitute(pLS, loop);
	 break;
	 
      case STATE_INSERT:
	 newloop = beginInsert(pLS, loop);
	 if (!newloop) {
		 nextstate = -1;
	 }
	 break;

      case STATE_MULTIPLY:
	 newloop = beginMultiply(pLS, loop);
	 if (!newloop) {
		 nextstate = -1;
	 }
	 break;

      case STATE_TRIGGER_PLAY:
	      pLS->fLoopFadeDelta = -1.0f / (xfadeSamples);
	      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
	      pLS->fLoopSrcFadeDelta = -1.0f / xfadeSamples;
	      pLS->fFeedSrcFadeDelta = 1.0f / xfadeSamples;
	      if (loop) {
		      pLS->state = STATE_PLAY;
		      nextstate = STATE_PLAY;
		      pLS->wasMuted = false;
		      if (pLS->fCurrRate > 0) {

			      DBG(fprintf(stderr, "%u:%u  trigger play : pre loop pos: %g\n", pLS->lLoopIndex, pLS->lChannelIndex, loop->dCurrPos));

			      loop->dCurrPos = (double) loop->lLoopLength - loop->lSyncPos;
			      DBG(fprintf(stderr, "%u:%u  trigger play %08x: syncpos: %ld\n", pLS->lLoopIndex, pLS->lChannelIndex, (unsigned)pLS, loop->lSyncPos));

			      //loop->dCurrPos = fmod(loop->dCurrPos, loop->lLoopLength); // wrapx

			      if (*pLS->pfSyncMode >= 1.0f && pLS->lSamplesSinceSync < eighthSamples) {
				      // if rel sync is on, shift it forward to account for not being quantized
				      loop->dCurrPos += pLS->lSamplesSinceSync; // jlc
				      DBG(fprintf(stderr, "%u:%u  after adding %u  pos = %g\n", pLS->lLoopIndex, pLS->lChannelIndex, pLS->lSamplesSinceSync, loop->dCurrPos));
			      }
		      }
		      else {
			      loop->dCurrPos = (loop->lLoopLength - loop->lSyncPos) - 1;
			      if (*pLS->pfSyncMode >= 1.0f && pLS->lSamplesSinceSync < eighthSamples) {
				      // if rel sync is on, shift it forward to account for not being quantized
				      loop->dCurrPos -= pLS->lSamplesSinceSync; // jlc
				      DBG(fprintf(stderr, "%u:%u  after sub %u  pos = %g\n", pLS->lLoopIndex, pLS->lChannelIndex, pLS->lSamplesSinceSync, loop->dCurrPos));
			      }
		      }


	      }
	      break;
      case STATE_ONESHOT:
	      // play the loop one_shot mode
	      pLS->fLoopFadeDelta = -1.0f / (xfadeSamples);
	      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
	      pLS->fLoopSrcFadeDelta = -1.0f / xfadeSamples;
	      pLS->fFeedSrcFadeDelta = 1.0f / xfadeSamples;
	      if (loop) {
		      DBG(fprintf(stderr,"%u:%u  Starting ONESHOT state\n", pLS->lLoopIndex, pLS->lChannelIndex));
		      pLS->state = STATE_ONESHOT;
		      if (pLS->fCurrRate > 0) {
			      loop->dCurrPos = (loop->lLoopLength - loop->lSyncPos) + *pLS->pfSyncOffsetSamples;

			      if (*pLS->pfSyncMode == 2.0f && pLS->lSamplesSinceSync < eighthSamples) {
				      // if rel sync is on, shift it forward to account for not being quantized
				      loop->dCurrPos += pLS->lSamplesSinceSync; // jlc
				      DBG(fprintf(stderr, "%u:%u  after adding %u  pos = %g\n", pLS->lLoopIndex, pLS->lChannelIndex, pLS->lSamplesSinceSync, loop->dCurrPos));
			      }
		      }
		      else {
			      loop->dCurrPos = (loop->lLoopLength - loop->lSyncPos) - 1;
			      if (*pLS->pfSyncMode == 2.0f && pLS->lSamplesSinceSync < eighthSamples) {
				      // if rel sync is on, shift it forward to account for not being quantized
				      loop->dCurrPos -= pLS->lSamplesSinceSync; // jlc
				      DBG(fprintf(stderr, "%u:%u  after sub %u  pos = %g\n", pLS->lLoopIndex, pLS->lChannelIndex, pLS->lSamplesSinceSync, loop->dCurrPos));
			      }
		      }


		      // wrap
		      loop->dCurrPos = fmod(loop->dCurrPos, loop->lLoopLength);

		      DBG(fprintf(stderr,"%u:%u  Starting ONESHOT state, currpos now: %g\n", pLS->lLoopIndex, pLS->lChannelIndex, loop->dCurrPos));
	      }
	      break;
   }

   if (nextstate != -1) {
	   if (loop) {
		   DBG(fprintf(stderr,"%u:%u  Entering state %d from %d at %g\n", pLS->lLoopIndex, pLS->lChannelIndex, nextstate, pLS->state, loop->dCurrPos));
	   }
	   pLS->state = nextstate;

   }
   else {
      DBG(fprintf(stderr,"%u:%u  Next state is -1?? Why?\n", pLS->lLoopIndex, pLS->lChannelIndex));
      pLS->state = STATE_PLAY;
      pLS->wasMuted = false;
   }

   // never wait for input when transitioning
   pLS->lFramesUntilInput = 0;
		      

   return newloop;
}


/*****************************************************************************/

/* Run the sampler  for a block of SampleCount samples. */
void 
runSooperLooper(LADSPA_Handle Instance,
	       unsigned long SampleCount)
{

  LADSPA_Data * pfBuffer;
  LADSPA_Data * pfInput;
  LADSPA_Data * pfOutput;
  LADSPA_Data * pfSyncInput;
  LADSPA_Data * pfSyncOutput;
  LADSPA_Data * pfInputLatencyBuf;
  LADSPA_Data fDry=1.0f, fWet=1.0f, tmpWet;
  LADSPA_Data dryDelta=0.0f, wetDelta=0.0f, dryTarget=1.0f, wetTarget=1.0f;
  LADSPA_Data fInputSample;
  LADSPA_Data fOutputSample;

  LADSPA_Data fRate = 1.0f;
  LADSPA_Data fScratchPos = 0.0f;
  //LADSPA_Data rateDelta=0.0f, rateTarget=1.0f;
  LADSPA_Data scratchDelta=0.0f, scratchTarget=0.0f;
  LADSPA_Data fTrigThresh = 0.0f;
  LADSPA_Data fTempo;
  unsigned int eighthSamples = 1;
  unsigned int syncSamples = 0;
  unsigned int eighthPerCycle = 8;
  
  int lMultiCtrl=-1;  
  bool useFeedbackPlay = false;
  LADSPA_Data fTapTrig = 0.0f;
  
  LADSPA_Data fFeedback = 1.0f;
  LADSPA_Data feedbackDelta=0.0f, feedbackTarget=1.0f;
  unsigned int lCurrPos = 0;
  unsigned int xCurrPos = 0;
  unsigned int lpCurrPos = 0;  
  LADSPA_Data *pLoopSample, *spLoopSample, *rLoopSample, *rpLoopSample, *xLoopSample;
  long slCurrPos;
  double rCurrPos;
  double rpCurrPos;
  double dDummy;
  int firsttime, backfill;
  int useDelay = 0;
  int prevstate;
  
  float fPosRatio;
  int xfadeSamples = XFADE_SAMPLES;
  
  SooperLooperI * pLS;
  LoopChunk *loop, *srcloop=0;
  LoopChunk *lastloop, *prevloop, *nextloop;
  
  LADSPA_Data fSyncMode = 0.0f;
  LADSPA_Data fQuantizeMode = 0.0f;
  LADSPA_Data fPlaybackSyncMode = 0.0f;
  LADSPA_Data fMuteQuantized = 0.0f;
  LADSPA_Data fOverdubQuantized = 0.0f;
  bool        bReplaceQuantized = true;
  LADSPA_Data fSyncOffsetSamples = 0.0f;
  bool bRoundIntegerTempo = false;

  unsigned long lSampleIndex;

  LADSPA_Data fSafetyFeedback;
  
  pLS = (SooperLooperI *)Instance;

  if (!pLS || !pLS->pfInput || !pLS->pfOutput) {
     // something is badly wrong!!!
     return;
  }
  
  pfInput = pLS->pfInput;
  pfOutput = pLS->pfOutput;
  pfBuffer = (LADSPA_Data *)pLS->pSampleBuf;
  pfSyncOutput = pLS->pfSyncOutput;
  pfSyncInput = pLS->pfSyncInput;
  pfInputLatencyBuf = (LADSPA_Data *) pLS->pInputBuf;

  xfadeSamples = (int) (*pLS->pfXfadeSamples);
  if (xfadeSamples < 1) xfadeSamples = 1;
  
  fTrigThresh = *pLS->pfTrigThresh;

  pLS->bRateCtrlActive = (int) *pLS->pfRateCtrlActive;

  fSyncMode = *pLS->pfSyncMode;

  fPlaybackSyncMode = *pLS->pfPlaybackSyncMode;

  fQuantizeMode = *pLS->pfQuantMode;

  fMuteQuantized = *pLS->pfMuteQuantized;
  fOverdubQuantized = *pLS->pfOverdubQuantized;
  bReplaceQuantized = pLS->bReplaceQuantized;
  fSyncOffsetSamples = *pLS->pfSyncOffsetSamples;

  bRoundIntegerTempo = *pLS->pfRoundIntegerTempo;

  eighthPerCycle = (unsigned int) *pLS->pfEighthPerCycle;

  fSafetyFeedback = (*pLS->pfUseSafetyFeedback != 0.0f) ? SAFETY_FEEDBACK : 1.0f;
  
  fTempo = *pLS->pfTempo;
  if (fTempo > 0.0f) {
	  eighthSamples = (unsigned int) (pLS->fSampleRate * 30.0 / fTempo);

	  if (fQuantizeMode == QUANT_CYCLE || fQuantizeMode == QUANT_LOOP) {
		  syncSamples = (unsigned int) (eighthSamples * eighthPerCycle / 2);
	  }
	  else if (fQuantizeMode == QUANT_8TH) {
		  syncSamples = (eighthSamples / 2);
	  }

  }
  
  
  lMultiCtrl = (int) *(pLS->pfMultiCtrl);

  useFeedbackPlay = (bool) *(pLS->pfUseFeedbackPlay);
  
  fTapTrig = *(pLS->pfTapCtrl);

  
  if (lMultiCtrl == pLS->lLastMultiCtrl)
  {
     // ignore it, we must have a change to trigger an action
     // and it must be the right 10s.
     lMultiCtrl = -1;
  }
  else {
          DBG(fprintf(stderr, "Multi change from %d to %d\n", pLS->lLastMultiCtrl, lMultiCtrl));
     pLS->lLastMultiCtrl = lMultiCtrl;
  }

  // force use delay
  if (lMultiCtrl == MULTI_REDO && *pLS->pfRedoTapMode != 0)
  {
     useDelay = 1;
  }
  else if (lMultiCtrl == MULTI_MUTE_ON) {
	  if (pLS->state != STATE_MUTE) {
		  // lets mute it
		  lMultiCtrl = MULTI_MUTE;
	  } else {
		  // already muted, do nothing
		  lMultiCtrl = -1;
	  }
  }
  else if (lMultiCtrl == MULTI_MUTE_OFF) {
	  if (pLS->state == STATE_MUTE) {
		  // lets unmute it
		  lMultiCtrl = MULTI_MUTE;
	  } else {
		  // already not muted, do nothing
		  lMultiCtrl = -1;
	  }
  }
  else if (lMultiCtrl == MULTI_MUTE_TRIGGER) {
	  if (pLS->state != STATE_MUTE) {
		  // lets mute it
		  lMultiCtrl = MULTI_MUTE;
	  } else {
		  // already muted, Trigger
		  lMultiCtrl = MULTI_TRIGGER;
	  }
  }
  else if (lMultiCtrl == MULTI_RECORD_OR_OVERDUB) {
	  if (!pLS->headLoopChunk || pLS->state == STATE_OFF || pLS->state == STATE_OFF_MUTE || pLS->state == STATE_RECORD 
	      || pLS->state == STATE_TRIG_START || pLS->state == STATE_TRIG_STOP 
	      || pLS->state == STATE_DELAY) {
		  // we record
		  lMultiCtrl = MULTI_RECORD;
	  } else {
		  // otherwise overdub
		  lMultiCtrl = MULTI_OVERDUB;
	  }
  }
  else if (lMultiCtrl == MULTI_RECORD_OVERDUB_END) {
	  if (!pLS->headLoopChunk || pLS->state == STATE_OFF || pLS->state == STATE_OFF_MUTE
	      || pLS->state == STATE_DELAY) {
		  // we record
		  lMultiCtrl = MULTI_RECORD;
	  } else {
		  // otherwise overdub
		  lMultiCtrl = MULTI_OVERDUB;
	  }
  }
  else if (lMultiCtrl == MULTI_PAUSE_ON) {
	  if (pLS->state != STATE_PAUSED) {
		  // lets pause it
		  lMultiCtrl = MULTI_PAUSE;
	  } else {
		  // already paused, do nothing
		  lMultiCtrl = -1;
	  }
  }
  else if (lMultiCtrl == MULTI_PAUSE_OFF) {
	  if (pLS->state == STATE_PAUSED) {
		  // lets unpause it
		  lMultiCtrl = MULTI_PAUSE;
	  } else {
		  // already not paused, do nothing
		  lMultiCtrl = -1;
	  }
  }
  
  if (fTapTrig == pLS->fLastTapCtrl) {
     // ignore it, we must have a change to trigger a tap
     
  } else if (pLS->lTapTrigSamples >= TRIG_SETTLE) {
     // signal to below to trigger the delay tap command
     if (pLS->bPreTap) {
	// ignore the first time
	pLS->bPreTap = 0;
     }
     else {
	DBG(fprintf(stderr, "%u:%u  Tap triggered\n", pLS->lLoopIndex, pLS->lChannelIndex));
	lMultiCtrl = MULTI_REDO;
	useDelay = 1;
     }
  }
  pLS->fLastTapCtrl = fTapTrig;
 
  //fRateSwitch = *(pLS->pfRateSwitch);


  if (pLS->pfScratchPos) {
	  scratchTarget = LIMIT_BETWEEN_0_AND_1(*(pLS->pfScratchPos));
	  fScratchPos =  LIMIT_BETWEEN_0_AND_1(pLS->fScratchPosCurr);
	  scratchDelta = (scratchTarget - fScratchPos) / MAX(1, (SampleCount - 1));
  }
  
  // the rate switch is ON if it is below 1 but not 0
  // rate is 1 if rate switch is off
  //if (fRateSwitch > 1.0 || fRateSwitch==0.0) {
  //  fRate = 1.0;
  //}
  //else {
     //fprintf(stderr, "rateswitch is 1.0: %f!\n", fRate);
  //}

  wetTarget = LIMIT_BETWEEN_0_AND_1(*(pLS->pfWet));
  fWet =  LIMIT_BETWEEN_0_AND_1(pLS->fWetCurr);
  wetDelta = (wetTarget - fWet) / MAX(1, (SampleCount - 1));
  
// 	  wetTarget += wetDelta;
// 	  fWet = fWet * 0.1f + wetTarget * 0.9f;
  
  dryTarget = LIMIT_BETWEEN_0_AND_1(*(pLS->pfDry));
  fDry =  LIMIT_BETWEEN_0_AND_1(pLS->fDryCurr);
  dryDelta = (dryTarget - fDry) / MAX(1, (SampleCount - 1));

  feedbackTarget = LIMIT_BETWEEN_0_AND_1(*(pLS->pfFeedback));
  fFeedback =  LIMIT_BETWEEN_0_AND_1(pLS->fFeedbackCurr);
  feedbackDelta = (feedbackTarget - fFeedback) / MAX(1, (SampleCount - 1));
  
  // probably against the rules, but I'm doing it anyway
  *pLS->pfFeedback = feedbackTarget;


  loop = pLS->headLoopChunk;

  // clear sync out
  if (fSyncMode == 0.0f || pfSyncInput != pfSyncOutput) {
	  memset(pfSyncOutput, 0, SampleCount * sizeof(LADSPA_Data));
  }
  

  unsigned long lInputLatency = (unsigned long) (*pLS->pfInputLatency);
  unsigned long lOutputLatency = (unsigned long) (*pLS->pfOutputLatency);

  
  // copy input signal to input latency buffer
  unsigned long lbuf_wpos = pLS->lInputBufWritePos;
  for (unsigned long n=0; n < SampleCount; ++n) {
	  pfInputLatencyBuf[lbuf_wpos]  = pfInput[n];
	  lbuf_wpos = (lbuf_wpos+1) & pLS->lInputBufMask;
  }

  // calculate initial offset for reading for input

  unsigned long lInputReadPos = (lInputLatency <= pLS->lInputBufWritePos)
	  ? (pLS->lInputBufWritePos - lInputLatency)
	  : (pLS->lInputBufSize - (lInputLatency - pLS->lInputBufWritePos)) ;
  
	  
  // transitions due to control triggering
  
  if (lMultiCtrl >= 0 && lMultiCtrl <= 127)
  {
          DBG(fprintf(stderr, "Multictrl val is %ld\n", lMultiCtrl));

     //lMultiCtrl = lMultiCtrl;

     // change the value if necessary

     if (lMultiCtrl==MULTI_REDO && useDelay) {
	lMultiCtrl = MULTI_DELAY;
     }
     
     switch(lMultiCtrl)
     {
	case MULTI_RECORD:
	{
	   
	   switch(pLS->state) {
	      case STATE_RECORD:
		      
		      if (fSyncMode == 0.0f && (fTrigThresh==0.0f) && !bRoundIntegerTempo) {
			      // skip trig stop
			      pLS->state = STATE_PLAY;
			      pLS->wasMuted = false;
			      pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
			      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
			      pLS->fLoopSrcFadeDelta = -1.0f / xfadeSamples;
			      pLS->fFeedSrcFadeDelta = 1.0f / xfadeSamples;
			      DBG(fprintf(stderr,"%u:%u  from rec Entering PLAY state loop len: %lu\n", pLS->lLoopIndex, pLS->lChannelIndex, loop->lLoopLength));

			      // then send out a sync here for any slaves
			      pfSyncOutput[0] = 2.0f;
			      
			      if (loop) {
				      // we need to increment loop position by output latency (+ IL ?)
				      loop->dCurrPos = fmod(loop->dCurrPos + ( lOutputLatency + lInputLatency) * fRate, loop->lLoopLength);
				      pLS->lFramesUntilFilled = lOutputLatency + lInputLatency;
				      DBG(fprintf(stderr,"%u:%u  from rec Entering PLAY state at %g\n", pLS->lLoopIndex, pLS->lChannelIndex, loop->dCurrPos));
			      }

		      }
		      else {
			      pLS->state = STATE_TRIG_STOP;
			      pLS->nextState = STATE_PLAY;
			      DBG(fprintf(stderr,"%u:%u  Entering TRIG_STOP state\n", pLS->lLoopIndex, pLS->lChannelIndex));
		      }
		      break;

	      case STATE_MULTIPLY:
		 // special ending for multiply
		 // the loop ends immediately and the cycle length changes
		 // and #cycles becomes 1
		 if (loop) {
		    loop->backfill = 0;
		    loop->lLoopLength = (unsigned long) (loop->dCurrPos - loop->lStartAdj);
		    loop->lCycleLength = loop->lLoopLength;
		    loop->lCycles = 1;

		    pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
		    pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
		    pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
		    pLS->fLoopSrcFadeDelta = -1.0f / xfadeSamples;
		    pLS->fFeedSrcFadeDelta = 1.0f / xfadeSamples;
			      
		    pLS->state = STATE_PLAY;
		    pLS->wasMuted = false;
		    DBG(fprintf(stderr,"%u:%u  Entering PLAY state after Multiply NEW loop\n", pLS->lLoopIndex, pLS->lChannelIndex));

		    // we need to increment loop position by output latency (+ IL ?)
		    loop->dCurrPos = loop->dCurrPos + ( lOutputLatency + lInputLatency) * fRate;
		    pLS->lFramesUntilFilled = lOutputLatency + lInputLatency;
		    
		 }
		 break;

	      case STATE_INSERT:
		 // special ending for insert
		 // the loop ends immediately and the cycle length changes
		 // and #cycles becomes 1
		 if (loop) {
		    // how much loop was remaining when the insert started
		    loop->lEndAdj = loop->lRemLen;
		    loop->lLoopLength = (unsigned long) (loop->dCurrPos + loop->lRemLen);

		    loop->backfill = 1;
		    loop->lMarkEndL = (unsigned long) loop->dCurrPos;
		    loop->lMarkEndH = loop->lLoopLength - 1;
		    loop->lCycleLength = loop->lLoopLength;
		    loop->lCycles = 1;

		    pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
		    pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
		    pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
		    pLS->fLoopSrcFadeDelta = -1.0f / xfadeSamples;
		    pLS->fFeedSrcFadeDelta = 1.0f / xfadeSamples;
			      
		    pLS->state = STATE_PLAY;
		    pLS->wasMuted = false;
		    DBG(fprintf(stderr,"%u:%u  Entering PLAY state after Multiply NEW loop\n", pLS->lLoopIndex, pLS->lChannelIndex));

		    // we need to increment loop position by output latency (+ IL ?)
		    loop->dCurrPos = loop->dCurrPos + ( lOutputLatency + lInputLatency) * fRate;
		    pLS->lFramesUntilFilled = lOutputLatency + lInputLatency;
		    
		 }
		 break;
		 
	      case STATE_DELAY:
		 // goes back to play mode
		 // pop the delay loop off....
		 if (loop) {
		    undoLoop(pLS, false);
		 }
		 
		 pLS->state = STATE_TRIG_START;
		 DBG(fprintf(stderr,"%u:%u  Entering TRIG_START state from DELAY\n", pLS->lLoopIndex, pLS->lChannelIndex));
		 break;
		 
	      default:
		 pLS->state = STATE_TRIG_START;
		 DBG(fprintf(stderr,"%u:%u  Entering TRIG_START state\n", pLS->lLoopIndex, pLS->lChannelIndex));
	   }
	} break;

	case MULTI_OVERDUB:
	{
	   switch(pLS->state) {
	      case STATE_OVERDUB:
		      // don't sync overdub ops unless asked to
		      if ((fOverdubQuantized == 0.0f) || (fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF)) {

			      pLS->state = STATE_PLAY;
			      pLS->wasMuted = false;
			      DBG(fprintf(stderr,"%u:%u  Entering PLAY state\n", pLS->lLoopIndex, pLS->lChannelIndex));
			      pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
			      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
			      pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
			      // then send out a sync here for any slaves
			      pfSyncOutput[0] = 1.0f;
		      }
		      else {
			      pLS->nextState = STATE_PLAY;
			      pLS->waitingForSync = 1;
		      }
		      break;
	      case STATE_MULTIPLY:
		 if (loop) {
		    loop = endMultiply(pLS, loop, STATE_OVERDUB);
		 }
		 break;
		 
	      case STATE_INSERT:
		 if (loop) {
		    loop = endInsert(pLS, loop, STATE_OVERDUB);
		 }
		 break;
		 
		 
	   case STATE_RECORD:
	   case STATE_TRIG_STOP:
		   // lets sync on ending record with overdub
		   if (fSyncMode == 0.0f && !bRoundIntegerTempo) {
			   if (loop) {

				   loop = beginOverdub(pLS, loop);
				   if (loop)
					   srcloop = loop->srcloop;
				   else
					   srcloop = NULL;

				   // we need to increment loop position by output latency (+ IL ?)
				   loop->dCurrPos = loop->dCurrPos + ( lOutputLatency + lInputLatency) * fRate;
				   DBG(fprintf(stderr,"%u:%u  from rec Entering overdub state at %g\n", pLS->lLoopIndex, pLS->lChannelIndex, loop->dCurrPos));
				   pLS->lFramesUntilFilled = lOutputLatency + lInputLatency;

				   // input always immediately available
				   pLS->lFramesUntilInput = 0;

				   pLS->fLoopSrcFadeDelta = -1.0f / xfadeSamples;
				   pLS->fFeedSrcFadeDelta = 1.0f / xfadeSamples;

				   // then send out a sync here for any slaves
				   //pfSyncOutput[0] = 1.0f;
			   }
		   } else {
			   DBG(fprintf(stderr, "%u:%u  starting syncwait for overdub:  %f\n", pLS->lLoopIndex, pLS->lChannelIndex, fSyncMode));
			   pLS->state = STATE_TRIG_STOP;
			   pLS->nextState = STATE_OVERDUB;
			   pLS->waitingForSync = 1;
		   }
		   
		   break;
	   case STATE_DELAY:
		   // goes back to overdub mode
		   // pop the delay loop off....
		   if (loop) {
			   undoLoop(pLS, false);
			   loop = pLS->headLoopChunk;
		   }
		   // continue through to default

	   default:
		   if ((fOverdubQuantized == 0.0f) || (fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF)) {

			   if (loop) {
				   loop = beginOverdub(pLS, loop);
				   if (loop)
					   srcloop = loop->srcloop;
				   else
					   srcloop = NULL;
			   }
			   // then send out a sync here for any slaves
			   pfSyncOutput[0] = 1.0f;
		   }
		   else {
			   if (pLS->state == STATE_RECORD) {
				   pLS->state = STATE_TRIG_STOP;
			   }
			   DBG(fprintf(stderr, "%u:%u  starting syncwait for overdub\n", pLS->lLoopIndex, pLS->lChannelIndex));
			   pLS->nextState = STATE_OVERDUB;
			   pLS->waitingForSync = 1;
		   }
	   }
	} break;

	case MULTI_MULTIPLY:
	{
	   switch(pLS->state) {
	      case STATE_MULTIPLY:
		 // set mark
		      if (fSyncMode == 0.0f) {
			      if (loop) {
				      if (loop->mult_out > 0) {
					      // we're in a multiincrease
					      loop->mult_out += 1;
					      //fprintf(stderr, "mult out added for %d\n", loop->mult_out);
				      }
				      else {
					      loop->mult_out = loop->lCycles;
					      //fprintf(stderr, "mult out initiated\n");
					      loop = endMultiply(pLS, loop, STATE_PLAY);
				      }
			      }

			      // put a sync marker at the beginning here
			      // mostly for slave purposes, it might screw up others
			      pfSyncOutput[0] = 1.0f;
			      
		      } else {
			      if (loop) {
				      if (loop->mult_out > 0) {
					      // we're in a multiincrease
					      loop->mult_out += 1;
					      //fprintf(stderr, "mult out added for %d\n", loop->mult_out);
				      }
				      else {
					      loop->mult_out = loop->lCycles;
					      //fprintf(stderr, "mult out initiated\n");
					      // loop = endMultiply(pLS, loop, STATE_PLAY);
					      
					      DBG(fprintf(stderr, "%u:%u  waiting for sync multi end\n", pLS->lLoopIndex, pLS->lChannelIndex));
					      pLS->nextState = STATE_PLAY;
					      pLS->waitingForSync = 1;
				      }
			      }
		      }
		      break;

	      case STATE_INSERT:
		 if (loop) {
		    loop = endInsert(pLS, loop, STATE_MULTIPLY);
		 }
		 break;
		 
	      case STATE_DELAY:
		 // goes back to play mode
		 // pop the delay loop off....
		 if (loop) {
		    undoLoop(pLS, false);
		    loop = pLS->headLoopChunk;
		 }
		 // continue through to default

	      default:
		      if (fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF
			  && !(pLS->state == STATE_RECORD && bRoundIntegerTempo)) {
			      if (loop) {
				      prevstate = pLS->state;

				      loop = beginMultiply(pLS, loop);
				      if (loop)
					      srcloop = loop->srcloop;
				      else
					      srcloop = NULL;
				      
				      if (prevstate == STATE_RECORD || prevstate == STATE_TRIG_STOP) {
					      // input always immediately available
					      pLS->lFramesUntilInput = 0;
					      // we need to increment loop position by output latency (+ IL ?)
					      loop->dCurrPos = loop->dCurrPos + ( lOutputLatency + lInputLatency) * fRate;
					      DBG(fprintf(stderr,"%u:%u  from rec Entering multiply state at %g\n", pLS->lLoopIndex, pLS->lChannelIndex, loop->dCurrPos));
					      pLS->lFramesUntilFilled = lOutputLatency + lInputLatency;

					      pLS->fLoopSrcFadeDelta = -1.0f / xfadeSamples;
					      pLS->fFeedSrcFadeDelta = 1.0f / xfadeSamples;
				      }
				      
			      }

			      // then send out a sync here for any slaves
			      pfSyncOutput[0] = 1.0f;
		      } else {
			      if (pLS->state == STATE_RECORD) {
				      pLS->state = STATE_TRIG_STOP;
			      }
			      
			      DBG(fprintf(stderr, "%u:%u  starting syncwait for multiply\n", pLS->lLoopIndex, pLS->lChannelIndex));
			      pLS->nextState = STATE_MULTIPLY;
			      pLS->waitingForSync = 1;
		      }
	   }
	} break;

	case MULTI_INSERT:
	{

	   switch(pLS->state) {
	      case STATE_INSERT:
		 if (loop) {
		    loop = endInsert(pLS, loop, STATE_PLAY);
		 }
		 break;
	      case STATE_MUTE:
	      case STATE_ONESHOT:
		 // play the loop one_shot mode
		 if (loop) {
		    DBG(fprintf(stderr,"Starting ONESHOT state\n"));
		    pLS->state = STATE_ONESHOT;
		    if (pLS->fCurrRate > 0)
			    loop->dCurrPos = (loop->lLoopLength - loop->lSyncPos) ;
		    else
			    loop->dCurrPos = (loop->lLoopLength - loop->lSyncPos) - 1;

		    // wrap
		    loop->dCurrPos = fmod(loop->dCurrPos, loop->lLoopLength);

		 }
		 break;
	      case STATE_MULTIPLY:
		 if (loop) {
		    loop = endMultiply(pLS, loop, STATE_INSERT);
		 }
		 break;

	      case STATE_DELAY:
		 // goes back to play mode
		 // pop the delay loop off....
		 if (loop) {
		    undoLoop(pLS, false);
		    loop = pLS->headLoopChunk;
		 }
		 // continue through to default

	      default:
		 // make new loop chunk
		      if (fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF
			  && !(pLS->state == STATE_RECORD && bRoundIntegerTempo)) 
		      {
			      if (loop)
			      {
				      prevstate = pLS->state;
				      
				      loop = beginInsert(pLS, loop);
				      if (loop) srcloop = loop->srcloop;
				      else srcloop = NULL;

				      if (prevstate == STATE_RECORD || prevstate == STATE_TRIG_STOP) {
					      // input always immediately available
					      pLS->lFramesUntilInput = 0;
					      // we need to increment loop position by output latency (+ IL ?)
					      loop->dCurrPos = loop->dCurrPos + ( lOutputLatency + lInputLatency) * fRate;
					      DBG(fprintf(stderr,"from rec Entering insert state at %g\n", loop->dCurrPos));
					      pLS->fLoopSrcFadeDelta = -1.0f / xfadeSamples;
					      pLS->fFeedSrcFadeDelta = 1.0f / xfadeSamples;					      
					      pLS->lFramesUntilFilled = lOutputLatency + lInputLatency;
					      
				      }

			      }
			      // then send out a sync here for any slaves
			      pfSyncOutput[0] = 1.0f;
		      } else {
			      if (pLS->state == STATE_RECORD) {
				      pLS->state = STATE_TRIG_STOP;
			      }

			      DBG(fprintf(stderr, "starting syncwait for insert\n"));
			      pLS->nextState = STATE_INSERT;
			      pLS->waitingForSync = 1;
		      }
	   }

	} break;

	case MULTI_REPLACE: // also MULTI_TOG_REDO in mute mode
	{
	   
	   switch(pLS->state) {
	      case STATE_REPLACE:
		      if ((bReplaceQuantized == 0.0f) || fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF) {
			      pLS->state = STATE_PLAY;
			      pLS->wasMuted = false;
			      DBG(fprintf(stderr,"%u:%u  Entering PLAY state\n", pLS->lLoopIndex, pLS->lChannelIndex));
			      pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
			      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
			      pLS->fFeedFadeDelta = 1.0f / xfadeSamples;

			      // then send out a sync here for any slaves
			      pfSyncOutput[0] = 1.0f;
		      } else {
			      pLS->nextState = STATE_PLAY;
			      pLS->waitingForSync = 1;
		      }
		      break;


	      case STATE_MULTIPLY:
		 if (loop) {
		    loop = endMultiply(pLS, loop, STATE_REPLACE);
		 }
		 break;

	      case STATE_DELAY:
		 // this toggles HOLD mode
		 pLS->bHoldMode = !pLS->bHoldMode;
		 DBG(fprintf(stderr, "HoldMode is now %d\n", pLS->bHoldMode));
		 break;

	      case STATE_INSERT:
		 if (loop) {
		    loop = endInsert(pLS, loop, STATE_REPLACE);
		 }
		 break;
		 
	      default:
		      if ((bReplaceQuantized == 0.0f) || fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF
			  && !(pLS->state == STATE_RECORD && bRoundIntegerTempo)) {
			      if (loop)
			      {
				      prevstate = pLS->state;
				      
				      DBG(fprintf(stderr, "starting replace immediately\n"));
				      loop = beginReplace(pLS, loop);
				      if (loop) srcloop = loop->srcloop;
				      else srcloop = NULL;

				      if (prevstate == STATE_RECORD || prevstate == STATE_TRIG_STOP) {
					      // input always immediately available
					      pLS->lFramesUntilInput = 0;
					      // we need to increment loop position by output latency (+ IL ?)
					      loop->dCurrPos = loop->dCurrPos + ( lOutputLatency + lInputLatency) * fRate;
					      DBG(fprintf(stderr,"from rec Entering multiply state at %g\n", loop->dCurrPos));
					      pLS->fLoopSrcFadeDelta = -1.0f / xfadeSamples;
					      pLS->fFeedSrcFadeDelta = 1.0f / xfadeSamples;
					      pLS->lFramesUntilFilled = lOutputLatency + lInputLatency;
				      }
			      }

			      // then send out a sync here for any slaves
			      pfSyncOutput[0] = 1.0f;
		      }
		      else {
			      if (pLS->state == STATE_RECORD) {
				      pLS->state = STATE_TRIG_STOP;
			      }
			      DBG(fprintf(stderr, "starting syncwait for replace\n"));
			      pLS->nextState = STATE_REPLACE;
			      pLS->waitingForSync = 1;
		      }
	   }

	} break;

	case MULTI_SUBSTITUTE: // also MULTI_TOG_REDO in mute mode
	{
	   
	   switch(pLS->state) {
	      case STATE_SUBSTITUTE:
		      if ((bReplaceQuantized == 0.0f) || fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF) {
			      pLS->state = STATE_PLAY;
			      pLS->wasMuted = false;
			      DBG(fprintf(stderr,"Entering PLAY state\n"));
			      pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
			      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
			      pLS->fFeedFadeDelta = 1.0f / xfadeSamples;

			      // then send out a sync here for any slaves
			      pfSyncOutput[0] = 1.0f;
			      
		      } else {
			      pLS->nextState = STATE_PLAY;
			      pLS->waitingForSync = 1;
		      }
		      break;


	      case STATE_MULTIPLY:
		 if (loop) {
		    loop = endMultiply(pLS, loop, STATE_SUBSTITUTE);
		 }
		 break;

	      case STATE_INSERT:
		 if (loop) {
		    loop = endInsert(pLS, loop, STATE_SUBSTITUTE);
		 }
		 break;
		 
	      default:
		      if ((bReplaceQuantized == 0.0f) || fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF
			  && !(pLS->state == STATE_RECORD && bRoundIntegerTempo)) {
			      if (loop)
			      {
				      prevstate = pLS->state;

				      DBG(fprintf(stderr, "starting subst immediately\n"));
				      loop = beginSubstitute(pLS, loop);
				      if (loop) srcloop = loop->srcloop;
				      else srcloop = NULL;

				      if (prevstate == STATE_RECORD || prevstate == STATE_TRIG_STOP) {
					      // input always immediately available
					      pLS->lFramesUntilInput = 0;
					      // we need to increment loop position by output latency (+ IL ?)
					      loop->dCurrPos = loop->dCurrPos + ( lOutputLatency + lInputLatency) * fRate;
					      DBG(fprintf(stderr,"from rec Entering multiply state at %g\n", loop->dCurrPos));
					      pLS->fLoopSrcFadeDelta = -1.0f / xfadeSamples;
					      pLS->fFeedSrcFadeDelta = 1.0f / xfadeSamples;
					      pLS->lFramesUntilFilled = lOutputLatency + lInputLatency;
				      }
			      }

			      if (fQuantizeMode == QUANT_OFF) {
				      // then send out a sync here for any slaves
				      pfSyncOutput[0] = 1.0f;
			      }
		      }
		      else {
			      if (pLS->state == STATE_RECORD) {
				      pLS->state = STATE_TRIG_STOP;
			      }
			      DBG(fprintf(stderr, "starting syncwait for substitute\n"));
			      pLS->nextState = STATE_SUBSTITUTE;
			      pLS->waitingForSync = 1;
		      }
	   }

	} break;
	
	case MULTI_MUTE:
	case MULTI_PAUSE:
	{

	   switch(pLS->state) {

	       case STATE_OFF_MUTE:
	         pLS->state = STATE_OFF;
	         pLS->wasMuted = false;
	         break;
	       case STATE_OFF:
	         pLS->state = STATE_OFF_MUTE;
	         pLS->wasMuted = true;
	         break;
	       case STATE_MUTE:
		      // reset for audio ramp
		      //pLS->lRampSamples = xfadeSamples;
	       case STATE_ONESHOT:
	       case STATE_PAUSED:
	       // this enters play mode but from the continuous position
		       if (loop && ((fMuteQuantized == 0.0f) || (fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF) || pLS->state == STATE_PAUSED)) {
			       if (pLS->state == STATE_MUTE && lMultiCtrl == MULTI_PAUSE) {
				       pLS->state = STATE_PAUSED;
				       pLS->wasMuted = true;
				       DBG(fprintf(stderr,"%u:%u  Entering PAUSED state from mute\n", pLS->lLoopIndex, pLS->lChannelIndex));
				       pLS->dPausedPos = loop->dCurrPos;
			       }
			       else if (pLS->state == STATE_PAUSED && lMultiCtrl == MULTI_MUTE) {
				       pLS->state = STATE_MUTE;
				       pLS->wasMuted = true;
				       DBG(fprintf(stderr,"%u:%u  Entering MUTED state from pause\n", pLS->lLoopIndex, pLS->lChannelIndex));
				       pLS->dPausedPos = loop->dCurrPos;
			       }
			       else {
				       pLS->state = STATE_PLAY;
				       pLS->wasMuted = false;
				       DBG(fprintf(stderr,"%u:%u  Entering PLAY state continuous\n", pLS->lLoopIndex, pLS->lChannelIndex));
				       pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
				       
				       if (lMultiCtrl == MULTI_PAUSE && loop) {
					       // set current loop position to paused position
					       loop->dCurrPos = pLS->dPausedPos;
					       DBG(fprintf(stderr, "%u:%u  setting loop pos to pause pos: %g\n", pLS->lLoopIndex, pLS->lChannelIndex, pLS->dPausedPos));
				       }
				       
				       // then send out a sync here for any slaves
				       pfSyncOutput[0] = 1.0f;
			       }
		       }
		       else if (loop) {
			       // wait for sync
			       if (pLS->state == STATE_RECORD) {
				       pLS->state = STATE_TRIG_STOP;
			       }
			       DBG(fprintf(stderr, "%u:%u  starting syncwait for play from mute\n", pLS->lLoopIndex, pLS->lChannelIndex));
			       if (pLS->state == STATE_PAUSED && lMultiCtrl == MULTI_MUTE) {
				       pLS->nextState = STATE_MUTE;
			       }
			       else {
				       pLS->nextState = STATE_PLAY;
			       }
			       pLS->waitingForSync = 1;	 
		       }
		 
		 break;

	      case STATE_MULTIPLY:
		 if (loop) {
		    loop = endMultiply(pLS, loop, lMultiCtrl == MULTI_MUTE ? STATE_MUTE: STATE_PAUSED);
		 }
		 break;


	      case STATE_INSERT:
		 if (loop) {
		    loop = endInsert(pLS, loop, lMultiCtrl == MULTI_MUTE ? STATE_MUTE: STATE_PAUSED);
		 }
		 break;

		 
	      case STATE_DELAY:
		 // pop the delay loop off....
		 if (loop) {
		    undoLoop(pLS, false);
		    loop = pLS->headLoopChunk;
		 }
		 // continue through to default

	      default:
		   if (loop && ((fMuteQuantized == 0.0f) || (fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF))
			  && !(pLS->state == STATE_RECORD && bRoundIntegerTempo))
		   {
		     if (pLS->state == STATE_RECORD || pLS->state == STATE_TRIG_STOP) {
			 // we need to increment loop position by output latency (+ IL ?)
			 loop->dCurrPos = loop->dCurrPos + ( lOutputLatency + lInputLatency) * fRate;
			 DBG(fprintf(stderr,"%u:%u  from rec Entering mute state at %g\n", pLS->lLoopIndex, pLS->lChannelIndex, loop->dCurrPos));
			 pLS->lFramesUntilFilled = lOutputLatency + lInputLatency;
		     }
		 
	
		     pLS->state = (lMultiCtrl==MULTI_MUTE ? STATE_MUTE: STATE_PAUSED);
		     DBG(fprintf(stderr,"%u:%u   Entering MUTE/pause state\n", pLS->lLoopIndex, pLS->lChannelIndex));
		     // reset for audio ramp
		     //pLS->lRampSamples = xfadeSamples;
		     pLS->fPlayFadeDelta = -1.0f / xfadeSamples;
		     
		     pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
		     pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
		     pLS->wasMuted = true;
		     
		     if (lMultiCtrl == MULTI_PAUSE) {
			     pLS->dPausedPos = loop->dCurrPos;
			     pLS->wasMuted = false;
		     }

		      // then send out a sync here for any slaves
		      pfSyncOutput[0] = 1.0f;
		 }
		   else if (loop) {
			 // wait for sync
			 if (pLS->state == STATE_RECORD) {
				 pLS->state = STATE_TRIG_STOP;
			 }
			 DBG(fprintf(stderr, "%u:%u  starting syncwait for mute\n", pLS->lLoopIndex, pLS->lChannelIndex));
			 pLS->nextState = lMultiCtrl==MULTI_MUTE ? STATE_MUTE: STATE_PAUSED;
			 pLS->waitingForSync = 1;	 
		 }
	   }

	} break;

	case MULTI_DELAY:
	{
	   
	   switch(pLS->state) {
	      case STATE_DELAY:
		 // this is new tap
		 // calculate new length
		 if (loop) {
		    loop->lLoopLength = pLS->lTapTrigSamples;
		    loop->lCycleLength = loop->lLoopLength;

		    
		    if (loop->lLoopLength > (pLS->lBufferSize)) {
		       // ignore this tap, it's too long
		       // just treat as first
		       loop->lLoopLength = 0;
		       loop->lCycleLength = 0;
		       DBG(fprintf(stderr, "Long tap delay ignored....\n"));
		    }
		    
		    pLS->lTapTrigSamples = 0;

		    loop->dCurrPos = 0;
		    // need to clean the whole thing the first time

		    loop->backfill = 1;
		    loop->lMarkEndL = 0;
		    loop->lMarkEndH = loop->lLoopLength - 1;
		 
		    DBG(fprintf(stderr,"New delay length of %g secs\n",
				loop->lLoopLength / pLS->fSampleRate));
		 }
		 break;


		 
	      case STATE_REPLACE:
	      case STATE_SUBSTITUTE:
	      case STATE_RECORD:		 
	      case STATE_INSERT:
	      case STATE_OVERDUB:
	      case STATE_MULTIPLY:
		 // nothing happens
		 break;
		 
	      default:


		 loop = pushNewLoopChunk(pLS, 0, NULL);
		 if (loop)
		 {
		    pLS->state = STATE_DELAY;
		    loop->srcloop = NULL;
		    
		    // initial delay length 0 until next tap
		    loop->lLoopLength = 0;
		    
		    
		    pLS->bHoldMode = 0;
		    pLS->lTapTrigSamples = 0;
		    
		    
		    DBG(fprintf(stderr,"Entering DELAY state: waiting for second tap...\n"));
		 }
	   }
	} break;

	case MULTI_SCRATCH:
	{
	   switch(pLS->state) {
	      case STATE_SCRATCH:
		 pLS->state = STATE_PLAY;
		 pLS->wasMuted = false;
		 DBG(fprintf(stderr,"Entering PLAY state\n"));
		 break;

	      case STATE_MUTE:
	      case STATE_ONESHOT:		 
		 // this restarts the loop from beginnine
		 pLS->state = STATE_PLAY;
		 pLS->wasMuted = false;
		 if (loop) {
			 loop->dCurrPos = (loop->lLoopLength - loop->lSyncPos) + fSyncOffsetSamples;
			 // wrap
			 loop->dCurrPos = fmod(loop->dCurrPos, loop->lLoopLength);
		 }
		 DBG(fprintf(stderr,"Entering PLAY state from top\n"));
		 break;

		 
	      case STATE_PLAY:
		 if (loop && !loop->frontfill && !loop->backfill)
		 {
		    // good to go, not filling in a loop beginning
		    pLS->state = STATE_SCRATCH;
		    DBG(fprintf(stderr,"Entering SCRATCH state\n"));
		 }
		 break;

	      case STATE_RECORD:
	      case STATE_TRIG_STOP:
		 // THIS puts it into reverse and does a ONE_SHOT after TRIG_STOP
		 if (loop) {
		    fRate = pLS->fCurrRate = -1.0f;
		    pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
		    pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
		    pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
		    pLS->state = STATE_ONESHOT;
		    if (pLS->fCurrRate > 0)
			    loop->dCurrPos = (loop->lLoopLength -  loop->lSyncPos) + fSyncOffsetSamples;
		    else
			    loop->dCurrPos = (loop->lLoopLength - loop->lSyncPos) - 1 - fSyncOffsetSamples;

		    // we need to increment loop position by output latency (+ IL ?)
		    loop->dCurrPos = loop->dCurrPos + ( lOutputLatency + lInputLatency) * fRate;
		    DBG(fprintf(stderr,"from rec Entering REV ONCE state at %g\n", loop->dCurrPos));
		    pLS->lFramesUntilFilled = lOutputLatency + lInputLatency;
		    
		    // wrap
		    loop->dCurrPos = fmod(loop->dCurrPos, loop->lLoopLength);

		    DBG(fprintf(stderr,"Enter reversed ONESHOT state\n"));
		 }
		 break;
		 
	      case STATE_OVERDUB:
	      case STATE_MULTIPLY:
	      case STATE_REPLACE:
	      case STATE_SUBSTITUTE:
	      case STATE_INSERT:
	      case STATE_DELAY:
		 // nothing happens
		 break;
		 
	      default:
		 pLS->state = STATE_SCRATCH;
		 DBG(fprintf(stderr,"Entering SCRATCH state\n"));
	   }

	} break;

	case MULTI_UNDO:
	case MULTI_UNDO_TWICE:
	{


	   switch(pLS->state) {
	      case STATE_PLAY:
	      case STATE_RECORD:
	      case STATE_OVERDUB:
	      case STATE_MULTIPLY:
	      case STATE_INSERT:
	      case STATE_TRIG_START:
	      case STATE_TRIG_STOP:		 
	      case STATE_REPLACE:
	      case STATE_SUBSTITUTE:
	      case STATE_DELAY:
	      case STATE_MUTE:
	      case STATE_PAUSED:
			   
			   // cancel whatever mode, back to play mode (or mute)
			   pLS->state = pLS->wasMuted ? STATE_MUTE : STATE_PLAY;
		      
		      if (pLS->waitingForSync) {
			      // don't undo loop
			      // just return to play (or mute)
			      pLS->waitingForSync = 0;
			      pLS->nextState = -1;
		      }
		      else if (pLS->fNextCurrRate != 0.0f) {
			      // undo pending reverse
			      pLS->fNextCurrRate = 0.0f;
		      }

					if (pLS->state == STATE_MUTE) {
						// undo ONE)
						if (loop->prev) {
							undoLoop(pLS, false);;
						} else {
							pLS->state = STATE_UNDO_ALL;
						}
					} else {
						if (loop->prev) {
							pLS->state = STATE_UNDO;
							pLS->nextState = STATE_PLAY;
						} else {
							pLS->state = STATE_UNDO_ALL;
						}

					}
			   
			   pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
			   pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
			   
			   pLS->fPlayFadeAtten = 0.0f;
			   pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
			   
			   if (pLS->state == STATE_MUTE) {
				   pLS->fPlayFadeDelta = 0.0f;
			   } else if (pLS->state == STATE_UNDO_ALL) {
				   pLS->fPlayFadeAtten = 1.0f;
				   pLS->fPlayFadeDelta = -1.0f / xfadeSamples;
			   }

		      DBG(fprintf(stderr,"%u:%u  Undoing and reentering PLAY state from UNDO\n", pLS->lLoopIndex, pLS->lChannelIndex));
		      break;
	   }
	   
	} break;

	case MULTI_UNDO_ALL:
	{
		if (pLS->state != STATE_OFF && pLS->state != STATE_OFF_MUTE) {
			DBG(fprintf(stderr,"%u:%u  UNDO all loops\n", pLS->lLoopIndex, pLS->lChannelIndex));
			pLS->fPlayFadeDelta = -1.0f / xfadeSamples;
			
			pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
			pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
			
			pLS->state = pLS->wasMuted ? STATE_MUTE : STATE_PLAY;
			
			if (loop && loop->lLoopLength) {
				pLS->state = STATE_UNDO_ALL;
				
				pLS->fLoopFadeDelta = -1.0f / (xfadeSamples);
				pLS->fPlayFadeDelta = -1.0f / xfadeSamples;// fade out for undo all
				pLS->wasMuted = true;
			}			
		}
	} break;

	case MULTI_REDO_ALL:
	{
		if (pLS->state == STATE_OFF) {
			pLS->state = STATE_PLAY;
			pLS->wasMuted = false;
		} else if (pLS->state == STATE_OFF_MUTE) {
			pLS->state = STATE_MUTE;
		} 
		//else {
		//	pLS->state = pLS->wasMuted ? STATE_MUTE : STATE_PLAY;
		//}
		
		if (!loop || pLS->state == STATE_MUTE) {
			// redo ALL)
			lastloop = pLS->headLoopChunk;
			redoLoop(pLS);
			 
			while (pLS->headLoopChunk != lastloop) {
			lastloop = pLS->headLoopChunk;
			redoLoop(pLS);
			}
			if (!pLS->headLoopChunk) {
				pLS->state = STATE_OFF_MUTE;
			}
		} else {
			if (loop->next) {
				pLS->state = STATE_REDO_ALL;
				pLS->nextState = STATE_PLAY;
			}
		}
		
		pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
		pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
		
		pLS->fPlayFadeAtten = 0.0f;
		pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
		
		if (pLS->state == STATE_MUTE) {
			pLS->fPlayFadeDelta = 0.0f;
		}

		DBG(fprintf(stderr,"REDO all loops\n"));

	} break;
	
	case MULTI_REDO:
	{
	   switch (pLS->state) {
		   case STATE_PLAY:
		   case STATE_RECORD:
		   case STATE_TRIG_START:
		   case STATE_TRIG_STOP:		 
		   case STATE_OVERDUB:
		   case STATE_MULTIPLY:
		   case STATE_INSERT:
		   case STATE_REPLACE:
		   case STATE_SUBSTITUTE:
		   case STATE_MUTE:
		   case STATE_PAUSED:
		   case STATE_OFF:
		   case STATE_OFF_MUTE:
			   
			   if (pLS->state == STATE_OFF) {
				   pLS->state = STATE_PLAY;
				   pLS->wasMuted = false;
			   } else if(pLS->state == STATE_OFF_MUTE) {
				   pLS->state = STATE_MUTE;
				   //pLS->state = pLS->wasMuted ? STATE_MUTE : STATE_PLAY;
			   }
			   
			   if (!loop || pLS->state == STATE_MUTE) {
				   // we don't need a fadeout
				   redoLoop(pLS);
				   if (!pLS->headLoopChunk) {
					   pLS->state = STATE_OFF_MUTE;
				   }
			   } else {
				   // we need a x-fade
				   if (loop->next) {
					   pLS->state = STATE_REDO;
					   pLS->nextState = STATE_PLAY;
					   pLS->fPlayFadeAtten = 0.0f;
				   }
			   }
			   
			   pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
			   pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
			   			   
			   if (pLS->state == STATE_MUTE) {
				   // we don't need a fade in
				   pLS->fPlayFadeDelta = 0.0f;
			   } else {
				   pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
			   }

			break;
	   }

	} break;

	
	case MULTI_REVERSE: // ALSO MULTI_ROUND toggle in Mute mode
	{
	   switch(pLS->state) {
	      case STATE_RECORD:
	      case STATE_TRIG_STOP:
		 // ends the record operation NOW
		 // and starts playing in reverse
		      fRate = pLS->fCurrRate *= -1.0f;
		      pLS->state = STATE_PLAY;
		      pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
		      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
		      pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
		      pLS->wasMuted = false;
		      // we need to increment loop position by output latency (+ IL ?)
		      loop->dCurrPos = loop->dCurrPos + ( lOutputLatency + lInputLatency) * fRate;
		      DBG(fprintf(stderr,"from rec Entering ONCE state at %g\n", loop->dCurrPos));
		      pLS->lFramesUntilFilled = lOutputLatency + lInputLatency;
		      
		      DBG(fprintf(stderr,"Entering PLAY state by reversing\n"));
		 break;
	       case STATE_MULTIPLY:
	       case STATE_INSERT:
		  // ignore for now
		 break;

	      case STATE_SCRATCH:
		 // reverse button toggles rate control active when in scratch mode
		 //pLS->bRateCtrlActive = ! pLS->bRateCtrlActive;
		 //DBG(fprintf(stderr, "Rate control active state now %d\n", pLS->bRateCtrlActive));
		 break;
		 
	      default:

		 if (pLS->pfQuantMode && *pLS->pfQuantMode != 0) {
		    pLS->fNextCurrRate = pLS->fCurrRate * -1;
		    DBG(fprintf(stderr,"Reversing Rate quantized\n"));
		 }
		 else {
		    // simply negate the current rate
		    fRate = pLS->fCurrRate *= -1.0f;
		    DBG(fprintf(stderr,"Reversing Rate\n"));
		 }
	   }		 
	} break;

	case MULTI_ONESHOT:
	{
		switch(pLS->state) {
		case STATE_MULTIPLY:
			if (loop) {
				loop = endMultiply(pLS, loop, STATE_ONESHOT);
			}
			break;
			
		case STATE_INSERT:
			if (loop) {
				loop = endInsert(pLS, loop, STATE_ONESHOT);
			}
			break;
			
		default:
			// play the loop one_shot mode
			if (loop) {
				if (((fSyncMode == 0.0f) || (fSyncMode >= 1.0f && pLS->lSamplesSinceSync < eighthSamples)) // give it some slack on relsync
				    && !(pLS->state == STATE_RECORD && bRoundIntegerTempo)) {
					DBG(fprintf(stderr,"Starting ONESHOT state\n"));
					pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
					pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
					pLS->fFeedFadeDelta = 1.0f / xfadeSamples;

				      prevstate = pLS->state;
				      
				      pLS->state = STATE_ONESHOT;

				      if (pLS->fCurrRate > 0)
					      loop->dCurrPos = (loop->lLoopLength - loop->lSyncPos)  + fSyncOffsetSamples;
				      else
					      loop->dCurrPos = (loop->lLoopLength - loop->lSyncPos) - 1 - fSyncOffsetSamples;
				      
				      // wrap
				      loop->dCurrPos = fmod(loop->dCurrPos, loop->lLoopLength);

				      DBG(fprintf(stderr,"Starting ONESHOT state at   %g\n", loop->dCurrPos));

				      if (prevstate == STATE_RECORD || prevstate == STATE_TRIG_STOP) {
					      // we need to increment loop position by output latency (+ IL ?)
					      loop->dCurrPos = loop->dCurrPos + ( lOutputLatency + lInputLatency) * fRate;
					      DBG(fprintf(stderr,"from rec Entering ONCE state at %g\n", loop->dCurrPos));
					      pLS->lFramesUntilFilled = lOutputLatency + lInputLatency;
				      }
				      
				      // then send out a sync here for any slaves
				      pfSyncOutput[0] = 2.0f;
				      
				}
				else {
					if (pLS->state == STATE_RECORD) {
						pLS->state = STATE_TRIG_STOP;
					}
					DBG(fprintf(stderr, "starting syncwait for ONESHOT\n"));
					pLS->nextState = STATE_ONESHOT;
					pLS->waitingForSync = 1;
				}
			}
		      break;
		}
	} break;

	case MULTI_TRIGGER:
	{
		switch(pLS->state) {
		case STATE_MULTIPLY:
			if (loop) {
				loop = endMultiply(pLS, loop, STATE_PLAY);
			}
			break;
			
		case STATE_INSERT:
			if (loop) {
				loop = endInsert(pLS, loop, STATE_PLAY);
			}
			break;
			
		default:
			// play the loop from the start
			if (loop) {
				if ((fSyncMode == 0.0f || (fSyncMode >= 1.0f && pLS->lSamplesSinceSync < eighthSamples)) // give it some slack 
				    && !(pLS->state == STATE_RECORD && bRoundIntegerTempo)) 
				{

					prevstate = pLS->state;
					
					transitionToNext (pLS, loop, STATE_TRIGGER_PLAY);

					if (prevstate == STATE_RECORD || prevstate == STATE_TRIG_STOP) {
						// we need to increment loop position by output latency (+ IL ?)
						loop->dCurrPos = loop->dCurrPos + ( lOutputLatency + lInputLatency) * fRate;
						DBG(fprintf(stderr,"from rec Entering ONCE state at %g\n", loop->dCurrPos));
						pLS->lFramesUntilFilled = lOutputLatency + lInputLatency;
					}
					
					// then send out a sync here for any slaves
					pfSyncOutput[0] = 2.0f;
					
				}
				else {
					if (pLS->state == STATE_RECORD) {
						pLS->state = STATE_TRIG_STOP;
					}
					DBG(fprintf(stderr, "starting syncwait for trigger\n"));
					pLS->nextState = STATE_TRIGGER_PLAY;
					pLS->waitingForSync = 1;
				}
			}
		      break;
		}
	} break;

	case MULTI_SET_SYNC_POS:
	{
		// set current loop's sync offset to the current pos
		if (loop) {
			loop->lSyncPos = (unsigned long) (loop->lLoopLength - loop->dCurrPos);
		}

	} break;

	case MULTI_RESET_SYNC_POS:
	{
		// set current loop's sync offset to the current pos
		if (loop) {
			loop->lSyncPos = loop->lOrigSyncPos;
		}

	} break;

	case MULTI_QUANT_TOG:
	{
	   // toggle quantize
	   // changing input param!!! Not strictly allowed!
	   if (pLS->pfQuantMode) {
	      if (*pLS->pfQuantMode == 0)
		 *pLS->pfQuantMode = (LADSPA_Data) QUANT_CYCLE;
	      else
		 *pLS->pfQuantMode = (LADSPA_Data) QUANT_OFF;
	   
	      DBG(fprintf(stderr,"Quantize mode is now %g\n", *pLS->pfQuantMode));
	   }
	} break;

	case MULTI_ROUND_TOG:
	{
	   if (pLS->pfRoundMode) {
	      // toggle roundmode
	      // changing input param!!!   Not strictly allowed!
	      if (*pLS->pfRoundMode == 0)
		 *pLS->pfRoundMode = 1.0f;
	      else
		 *pLS->pfRoundMode = 0;

	      DBG(fprintf(stderr,"Round mode is now %g\n", *pLS->pfRoundMode));
	   }
	} break;

	
	
     }		 

  }
  

  fRate = pLS->fCurrRate;
  
  //fprintf(stderr,"before play\n");  
  //fprintf(stderr, "fRateSwitch: %f\n", fRateSwitch);

  // the run loop
  
  lSampleIndex = 0;

  while (lSampleIndex < SampleCount)
  {
     loop = pLS->headLoopChunk;

     switch(pLS->state)
     {
			 
	case STATE_TRIG_START:
	{
	   //fprintf(stderr,"in trigstart\n");
	   
	   // we are looking for the threshold to actually
	   // start the recording on (while still playing dry signal)
	   
	   for (;lSampleIndex < SampleCount;
		lSampleIndex++)
	   {
	      fWet += wetDelta;
              fDry += dryDelta;
	      fFeedback += feedbackDelta;
	      fScratchPos += scratchDelta;

	      // TODO: need to possibly wait IL-TL before actually starting
	      
	      fInputSample = pfInput[lSampleIndex];
	      if ((fSyncMode == 0.0f && ((fInputSample > fTrigThresh) || (fTrigThresh==0.0f)))
			  || (fSyncMode == 2.0f) // relative sync offset mode 
			  || (fSyncMode > 0.0f && pfSyncInput[lSampleIndex] != 0.0f))
	      {
			  
			  loop = pushNewLoopChunk(pLS, 0, NULL);
			  if (loop) {
				  DBG(fprintf(stderr,"%u:%u  Entering RECORD state: syncmode=%g\n", pLS->lLoopIndex, pLS->lChannelIndex, fSyncMode));
				  
				  pLS->state = STATE_RECORD;
				  // force rate to be 1.0
				  fRate = pLS->fCurrRate = 1.0f;
				  
				  loop->lLoopLength = 0;
				  loop->lStartAdj = 0;
				  loop->lEndAdj = 0;
				  loop->dCurrPos = 0.0;
				  loop->firsttime = 0;
				  loop->lMarkL = loop->lMarkEndL = LONG_MAX;
				  loop->frontfill = loop->backfill = 0;
				  loop->lCycles = 1; // at first just one		 
				  loop->srcloop = NULL;
				  pLS->nextState = -1;
				  loop->dOrigFeedback = fFeedback;
				  
				  if (fSyncMode == 2.0f) {
					  // use sync offset
					  loop->lSyncOffset = pLS->lSamplesSinceSync;
					  // this needs to be adjusted for latency
					  //loop->lOrigSyncPos = loop->lSyncPos = pLS->lSamplesSinceSync + (unsigned long) (( lOutputLatency + lInputLatency) * fRate); 
					  if (pLS->lSamplesSinceSync < syncSamples/2) {
						  //loop->lOrigSyncPos = loop->lSyncPos = pLS->lSamplesSinceSync + (unsigned long) (( lOutputLatency + lInputLatency) * fRate); 
						  loop->lOrigSyncPos = loop->lSyncPos = pLS->lSamplesSinceSync; 
					  }
					  else {
						  //loop->lOrigSyncPos = loop->lSyncPos = (pLS->lSamplesSinceSync - syncSamples) + (unsigned long) (( lOutputLatency + lInputLatency) * fRate); 
						  loop->lOrigSyncPos = loop->lSyncPos = (pLS->lSamplesSinceSync - syncSamples); 
					  }
					  
					  DBG(cerr <<  pLS->lLoopIndex << ":" << pLS->lChannelIndex << "   sync offset is: " << loop->lSyncOffset << "  syncpos: " << loop->lSyncPos <<endl);
				  }
				  else {
					  loop->lSyncOffset = 0;
					  loop->lOrigSyncPos = loop->lSyncPos = 0;
				  }
				  // cause input-to-loop fade in
				  pLS->fLoopFadeAtten = 0.0f;
				  pLS->fLoopFadeDelta = 1.0f / xfadeSamples;
				  pLS->fPlayFadeDelta = -1.0f / xfadeSamples;
				  
				  // only place this goes up
				  pLS->fLoopSrcFadeDelta = 1.0f / xfadeSamples;
				  // and this goes down
				  pLS->fFeedSrcFadeDelta = -1.0f / xfadeSamples;
				  
			  }
			  else {
				  DBG(fprintf(stderr, "out of memory! back to PLAY mode\n"));
				  pLS->state = STATE_PLAY;
				  pLS->wasMuted = false;
			  }

			  break;
	      }

	      pfOutput[lSampleIndex] = fDry * fInputSample;
	   }
     
	} break;
	
	case STATE_RECORD:
	{
	   // play the input out while recording it.
	   //fprintf(stderr,"in record\n");

	   if ((loop = ensureLoopSpace (pLS, loop, SampleCount - lSampleIndex, NULL)) == NULL) {
		   DBG(fprintf(stderr, "%u:%u  Entering PLAY state -- END of memory! %08x\n", pLS->lLoopIndex, pLS->lChannelIndex,
			       (unsigned) (pLS->pSampleBuf + pLS->lBufferSize) ));
		   pLS->state = STATE_PLAY;
		   pLS->wasMuted = false;
		   goto passthrough;
   	   }
		
	   for (;lSampleIndex < SampleCount;
		lSampleIndex++)
	   {
	      fWet += wetDelta;
		  fDry += dryDelta;
	      fFeedback += feedbackDelta;
	      fScratchPos += scratchDelta;

	      pLS->fLoopFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopFadeAtten + pLS->fLoopFadeDelta);
	      pLS->fLoopSrcFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopSrcFadeAtten + pLS->fLoopSrcFadeDelta);
	      pLS->fFeedSrcFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fFeedSrcFadeAtten + pLS->fFeedSrcFadeDelta);
	      pLS->fPlayFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fPlayFadeAtten + pLS->fPlayFadeDelta);
	      
// 	      if (pLS->waitingForSync && (fSyncMode == 0.0 || pfSyncInput[lSampleIndex] != 0.0))
// 	      {
// 		      DBG(fprintf(stderr,"Finishing synced record\n"));
// 		      pLS->state = pLS->nextState;
// 		      pLS->nextState = -1;
// 		      pLS->waitingForSync = 0;
// 		      break;
// 	      }

	      if (fSyncMode != 0.0f) {
		      pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];
	      }

	      if (fSyncMode >= 1.0f) {
		      pLS->lSamplesSinceSync++;

		      if (pfSyncInput[lSampleIndex] > 1.5f) {
			      pLS->lSamplesSinceSync = 0;
			      DBG(cerr << pLS->lLoopIndex << ":" << pLS->lChannelIndex <<" rec reseting sync: " << pLS << " at " << loop->dCurrPos << endl);
		      }
	      }
	      
	      // wrap at the proper loop end
	      lCurrPos = (unsigned int) lrint(loop->dCurrPos);
	      pLoopSample = & pLS->pSampleBuf[(loop->lLoopStart + lCurrPos) & pLS->lBufferSizeMask];
		      
// 	      if ((char *)(lCurrPos + loop->pLoopStart) >= (pLS->pSampleBuf + pLS->lBufferSize)) {
// 		 // stop the recording RIGHT NOW
// 		 // we don't support loop crossing the end of memory
// 		 // it's easier.
// 		 DBG(fprintf(stderr, "Entering PLAY state -- END of memory! %08x\n",
// 			     (unsigned) (pLS->pSampleBuf + pLS->lBufferSize) ));
// 		 pLS->state = STATE_PLAY;
// 		 break;
// 	      }
		   

	      if (lCurrPos == 0) {
		      pfSyncOutput[lSampleIndex] = 1.0f;
	      }
		   
	      fInputSample = pfInput[lSampleIndex];
	      
	      *pLoopSample = pLS->fLoopFadeAtten * fInputSample;
	      
	      // increment according to current rate
	      loop->dCurrPos = loop->dCurrPos + fRate;
	      
	      
	      pfOutput[lSampleIndex] = fDry * fInputSample;
	   }

	   // update loop values (in case we get stopped by an event)
	   lCurrPos = (unsigned int) lrint(loop->dCurrPos);
	   loop->lLoopLength = (unsigned long) lCurrPos;
	   loop->lCycleLength = loop->lLoopLength;

	} break;

	case STATE_TRIG_STOP:
	{
	   //fprintf(stderr,"in trigstop\n");	   
	   // play the input out.  Keep recording until we go
	   // above the threshold, then go into next state.

	   loop = ensureLoopSpace (pLS, loop, SampleCount - lSampleIndex, NULL);

	   bool roundTempoDone  = false;
	   unsigned int roundedSamples = 0;

	   if (bRoundIntegerTempo) {
		   // logic to figure out if it is time to stop with a rounded tempo
		   float eighths = *pLS->pfEighthPerCycle;
		   double tempo = (30.0 * eighths * pLS->fSampleRate / loop->dCurrPos);
		   while (tempo > 1.0f && (tempo < 60.0f || tempo > 240.0f)) {
			   eighths *= (tempo < 60.0f) ? 2.0f : 0.5f;
			   eighths = max(1.0f, eighths);
			   tempo = (30.0 * eighths * pLS->fSampleRate / loop->dCurrPos);
			   DBG(cerr << "eights adjusted to: " << eighths << "  tempo: " << tempo << endl;)
			   if (eighths == 1.0f) break;
		   } 
		   
		   roundedSamples = (unsigned int) (pLS->fSampleRate * 30.0 * eighths/ floor(tempo));
		   //bool roundTempoDone = bRoundIntegerTempo && abs((long)lCurrPos - (long)roundedSamples) < 1;
		   DBG(cerr << pLS->lLoopIndex<< ":" <<  pLS->lChannelIndex <<  "rounded samples now: " << roundedSamples << "  eighths: " << eighths << " tempo: " << tempo << " currpos: " << loop->dCurrPos << endl;)
	   }
		
	   for (;lSampleIndex < SampleCount;
		lSampleIndex++)
	   {
	      fWet += wetDelta;
              fDry += dryDelta;
	      fFeedback += feedbackDelta;
	      fScratchPos += scratchDelta;
	      pLS->fLoopFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopFadeAtten + pLS->fLoopFadeDelta);
	      pLS->fLoopSrcFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopFadeAtten + pLS->fLoopSrcFadeDelta);
	      pLS->fFeedSrcFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fFeedSrcFadeAtten + pLS->fFeedSrcFadeDelta);
	      pLS->fPlayFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fPlayFadeAtten + pLS->fPlayFadeDelta);
		   
	      lCurrPos = (unsigned int) loop->dCurrPos;
	      pLoopSample = & pLS->pSampleBuf[(loop->lLoopStart + lCurrPos) & pLS->lBufferSizeMask];
	      
	      fInputSample = pfInput[lSampleIndex];
	      
	      
// 	      if ((fSyncMode == 0.0f && ((fInputSample > fTrigThresh) || (fTrigThresh==0.0)))
// 		  || (fSyncMode > 0.0f && pfSyncInput[lSampleIndex] != 0.0))

	      roundTempoDone = bRoundIntegerTempo && lCurrPos == roundedSamples;
	      
	      // exit immediately if syncmode is off, or we have a sync
	      if ((fSyncMode == 0.0f && (!bRoundIntegerTempo || roundTempoDone))
 		  || (fSyncMode == 1.0f && (pfSyncInput[lSampleIndex] != 0.0f))
		  || (fSyncMode == 2.0f && pLS->lSamplesSinceSync == loop->lSyncOffset))
	      {
		      DBG(fprintf(stderr,"%u:%u   Entering %d state at %u\n", pLS->lLoopIndex, pLS->lChannelIndex, pLS->nextState, lCurrPos));
		 //pLS->state = pLS->nextState;
		 // reset for audio ramp
		 //pLS->lRampSamples = xfadeSamples;
		 //loop->dCurrPos = 0.0f;
		      DBG(cerr << pLS->lLoopIndex << ":" << pLS->lChannelIndex << "Round tempo: " << bRoundIntegerTempo << "  roundedSamples: " << roundedSamples << endl);

		      if (roundTempoDone) {
			      // force sync for slaves... not sure if this is good
			      pfSyncOutput[lSampleIndex] = 2.0f;
		      }

		      if (fSyncMode == 2.0f) {
			      //cerr << "ending recstop sync2d: " << lCurrPos << endl;
			      pLS->recSyncEnded = true;
		      }
		      else {
			      //cerr << "ending recstop sync1: " << lCurrPos << endl;
		      }

		      // update current info before transition
		      loop->lLoopLength = (unsigned long) lCurrPos;
		      loop->lCycleLength = loop->lLoopLength;
		      loop->lCycles = 1;

		      DBG(fprintf(stderr,"%u:%u  transitioning to %d  at %g with length: %lu\n", pLS->lLoopIndex, pLS->lChannelIndex, pLS->nextState, loop->dCurrPos, loop->lLoopLength));
		      

		      loop = transitionToNext (pLS, loop, pLS->nextState);
		      pLS->waitingForSync = 0;
		      //jlc trig
		      // we need to increment loop position by output latency (+ IL ?)
		      loop->dCurrPos = loop->dCurrPos + ( lOutputLatency + lInputLatency ) * fRate;
		      pLS->lFramesUntilFilled = lOutputLatency + lInputLatency;
		      
		      pLS->fLoopSrcFadeDelta = -1.0f / xfadeSamples;
		      
		      break;
	      }

	      if (fSyncMode != 0.0f) {
		      pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];
	      }

	      if (fSyncMode >= 1.0f) {
		      pLS->lSamplesSinceSync++;

		      if (pfSyncInput[lSampleIndex] > 1.5f) {
			      DBG(cerr <<  pLS->lLoopIndex << ":" << pLS->lChannelIndex << " TS reseting sync at pos: " << loop->dCurrPos << "  had: " << pLS->lSamplesSinceSync << " sync offset: " << loop->lSyncOffset << endl);
			      pLS->lSamplesSinceSync = 0;

		      }
	      }
	      
	      
	      *(pLoopSample) = pLS->fLoopFadeAtten * fInputSample;
	      
	      // increment according to current rate
	      loop->dCurrPos = loop->dCurrPos + fRate;


// 	      if ((char *)(loop->pLoopStart + (unsigned int)loop->dCurrPos)
// 		  > (pLS->pSampleBuf + pLS->lBufferSize)) {
// 		 // out of space! give up for now!
// 		 // undo!
// 		 pLS->state = STATE_PLAY;
// 		 //undoLoop(pLS);
// 		 DBG(fprintf(stderr,"Record Stopped early! Out of memory!\n"));
// 		 loop->dCurrPos = 0.0f;
// 		 break;
// 	      }

	      
	      pfOutput[lSampleIndex] = fDry * fInputSample;

	      
	   }

	   // update loop values (in case we get stopped by an event)
	   if (loop) {
		   loop->lLoopLength = (unsigned long) lCurrPos;
		   loop->lCycleLength = loop->lLoopLength;
		   loop->lCycles = 1;
	   }
	

   
	} break;


	
	case STATE_OVERDUB:
	case STATE_REPLACE:
	case STATE_SUBSTITUTE:
	{
	   if (loop && loop->srcloop && loop->lLoopLength)
	   {
	      srcloop = loop->srcloop;
		   
	      for (;lSampleIndex < SampleCount;
		   lSampleIndex++)
	      {
	         fWet += wetDelta;
                 fDry += dryDelta;
	         fFeedback += feedbackDelta;
	         fScratchPos += scratchDelta;

		 pLS->fPlayFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fPlayFadeAtten + pLS->fPlayFadeDelta);
		 pLS->fFeedFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fFeedFadeAtten + pLS->fFeedFadeDelta);
		 

		 lCurrPos =(unsigned int) fmod(loop->dCurrPos, loop->lLoopLength);
		 //rCurrPos = fmod (loop->dCurrPos - lOutputLatency), loop->lLoopLength);
		 rCurrPos = fmod (loop->dCurrPos - (fRate * (lOutputLatency + lInputLatency)), loop->lLoopLength);
		 if (rCurrPos < 0) {
		    rCurrPos += loop->lLoopLength;
		 }
		 rpCurrPos = fmod (loop->dCurrPos - (fRate * (lOutputLatency + lInputLatency)), srcloop->lLoopLength);
		 if (rpCurrPos < 0) {
		    rpCurrPos += srcloop->lLoopLength;
		 }
		 
		 pLoopSample = & pLS->pSampleBuf[(loop->lLoopStart + lCurrPos) & pLS->lBufferSizeMask];

		 if (pLS->lFramesUntilInput <= 0) {
			 rLoopSample = & pLS->pSampleBuf[(loop->lLoopStart + (unsigned int) rCurrPos) & pLS->lBufferSizeMask];
			 rpLoopSample = & pLS->pSampleBuf[(srcloop->lLoopStart + (unsigned int) rpCurrPos) & pLS->lBufferSizeMask];
			 pLS->fLoopFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopFadeAtten + pLS->fLoopFadeDelta);
			 pLS->fLoopSrcFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopSrcFadeAtten + pLS->fLoopSrcFadeDelta);
			 pLS->fFeedSrcFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fFeedSrcFadeAtten + pLS->fFeedSrcFadeDelta);
			 lInputReadPos = - pLS->lFramesUntilInput; // negate it
			 lInputReadPos = (lInputReadPos <= pLS->lInputBufWritePos)
				 ? (pLS->lInputBufWritePos - lInputReadPos)
				 : (pLS->lInputBufSize - (lInputReadPos - pLS->lInputBufWritePos)) ;

		 }
		 else { // jlc over
			 DBG(fprintf(stderr, "%u:%u  overdub frames until input: %ld\n", pLS->lLoopIndex, pLS->lChannelIndex, pLS->lFramesUntilInput));
			 rLoopSample = 0;
			 rpLoopSample = 0;
			 pLS->lFramesUntilInput--;
			 lInputReadPos = pLS->lInputBufWritePos;
		 }
		 
		 if (fSyncMode != 0) {
			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];

			 pLS->lSamplesSinceSync++;
			 
			 if (pfSyncInput[lSampleIndex] > 1.5f) {
				 // cerr << "od sync reset" << endl;
				 DBG(fprintf(stderr, "%u:%u  TS reset sync in overdub\n", pLS->lLoopIndex, pLS->lChannelIndex));
				 pLS->lSamplesSinceSync = 0;
			 }

			 if (fSyncMode == 2.0f) {
				 
				 if (pLS->recSyncEnded) {
					 // we just synced, need to noitfy slave... this could be problem
					 pfSyncOutput[lSampleIndex] = 2.0f;
					 DBG(fprintf(stderr, "%u:%u  sync out on relsync end on overdub/repl/sub\n", pLS->lLoopIndex, pLS->lChannelIndex));
					 pLS->recSyncEnded = false;
				 }
			 }
			 
		 }
		 else {
			 if (fQuantizeMode == QUANT_OFF 
			     || (fQuantizeMode == QUANT_CYCLE && (((int)rCurrPos + loop->lSyncPos) % loop->lCycleLength) == 0)
			     || (fQuantizeMode == QUANT_LOOP && (((int) rCurrPos +loop->lSyncPos) % loop->lLoopLength )== 0) 
			     || (fQuantizeMode == QUANT_8TH && (((int) rCurrPos + loop->lSyncPos) % eighthSamples) == 0))
			 {
				 pfSyncOutput[lSampleIndex] = 2.0f;
			 }
		 }
		 
		 if (pLS->waitingForSync && ((fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF) || pfSyncOutput[lSampleIndex] != 0.0f))
		 {
			 DBG(fprintf(stderr,"%u:%u  Finishing synced overdub/replace/subs\n", pLS->lLoopIndex, pLS->lChannelIndex));
			 loop = transitionToNext (pLS, loop, pLS->nextState);
			 pLS->nextState = -1;
			 pLS->waitingForSync = 0;
			 break;
		 }

		 
		 
		 //fInputSample = pfInput[lSampleIndex];
		 fInputSample = pfInputLatencyBuf[(lInputReadPos + lSampleIndex) & pLS->lInputBufMask];

		 //  xfade input into source loop (for cases immediately after record)
		 if (rpLoopSample) {
			 *(rpLoopSample) = ((*rpLoopSample) * pLS->fFeedSrcFadeAtten) +  pLS->fLoopSrcFadeAtten * fInputSample;
		 }

		 if (pLS->lFramesUntilFilled > 0) {
			 // fill from the record position * and for the play pos !!?
			 //DBG(fprintf(stderr, "filling rcurrpos=%u  pppos %d\n", (unsigned int) rCurrPos, lCurrPos));
			 fillLoops(pLS, loop, (unsigned int) rCurrPos, true);
			 pLS->lFramesUntilFilled--;
		 }

		 fillLoops(pLS, loop, lCurrPos, false);

		 
		 switch(pLS->state)
		 {
		 case STATE_OVERDUB:
			 // use our self as the source (we have been filled by the call above)
			 fOutputSample = fWet  *  *(pLoopSample)
				 + fDry * fInputSample;

			 if (rLoopSample) {
				 *(rLoopSample) =  
					 ((pLS->fLoopFadeAtten * fInputSample) + (fSafetyFeedback * pLS->fFeedFadeAtten * fFeedback *  *(rLoopSample)));
			 }
			 break;
		 case STATE_REPLACE:
			 // state REPLACE use only the new input
			 // use our self as the source (we have been filled by the call above)
			 fOutputSample = pLS->fPlayFadeAtten * fWet  *  *(pLoopSample)
				 + fDry * fInputSample;
			 
			 if (rLoopSample) {
				 *(rLoopSample) = fInputSample * pLS->fLoopFadeAtten +  (pLS->fFeedFadeAtten * fFeedback *  *(rLoopSample));
			 }
			 break;
		 case STATE_SUBSTITUTE:
		 default:
			 // use our self as the source (we have been filled by the call above)
			 // hear the loop
			 fOutputSample = fWet  *  *(pLoopSample)
				 + fDry * fInputSample;

			 // but not feed it back (xfade it really)
			 if (rLoopSample) {
				 *(rLoopSample) = fInputSample * pLS->fLoopFadeAtten + (pLS->fFeedFadeAtten * fFeedback *  *(rLoopSample));
			 }
			 break;
		 }
		 
		 pfOutput[lSampleIndex] = fOutputSample;
		 

		 // increment and wrap at the proper loop end
		 loop->dCurrPos = loop->dCurrPos + fRate;

		 //if (fSyncMode != 0.0 && pLS->fNextCurrRate != 0 && pfSyncInput[lSampleIndex] != 0.0) {
		 if (pLS->fNextCurrRate != 0 && pfSyncOutput[lSampleIndex] > 1.5f) {
		       // commit the new rate at boundary (quantized)
		       pLS->fCurrRate = pLS->fNextCurrRate;
		       pLS->fNextCurrRate = 0.0f;
		       DBG(fprintf(stderr, "%u:%u  Starting quantized rate change ovr at %d\n", pLS->lLoopIndex, pLS->lChannelIndex, lCurrPos));
		 }

		 
		 if (loop->dCurrPos < 0)
		 {
		    // our rate must be negative
		    // adjust around to the back
		    loop->dCurrPos += loop->lLoopLength;

		    if (pLS->fNextCurrRate != 0) {
		       // commit the new rate at boundary (quantized)
		       pLS->fCurrRate = pLS->fNextCurrRate;
		       pLS->fNextCurrRate = 0.0f;
		       DBG(fprintf(stderr, "%u:%u  Starting quantized rate change\n", pLS->lLoopIndex, pLS->lChannelIndex));
		    }

		 }
		 else if (loop->dCurrPos >= loop->lLoopLength) {
		    // wrap around length
		    loop->dCurrPos = fmod(loop->dCurrPos, loop->lLoopLength);
		    if (pLS->fNextCurrRate != 0) {
		       // commit the new rate at boundary (quantized)
		       pLS->fCurrRate = pLS->fNextCurrRate;
		       pLS->fNextCurrRate = 0.0f;
		       DBG(fprintf(stderr, "%u:%u  Starting quantized rate change\n", pLS->lLoopIndex, pLS->lChannelIndex));
		    }
		 }
		 
	      }


	      
	   }
	   else {
	      goto passthrough;

	   }
	   
	   
	} break;


	case STATE_MULTIPLY:
	{
	   if (loop && loop->lLoopLength && loop->srcloop)
	   {
	      srcloop = loop->srcloop;
	      firsttime = loop->firsttime;

	      if (pLS->nextState == STATE_MUTE) {
		 // no loop output
		 fWet = 0.0f;
		 wetDelta = 0.0f;
	      }
	      
	      loop = ensureLoopSpace (pLS, loop, SampleCount - lSampleIndex, NULL);

	      if (!loop || (loop->srcloop && !loop->srcloop->valid)) {
		      // out of space! give up for now!
		      // undo!
		      DBG(fprintf(stderr,"Multiply fail done! Out of memory or lost source!\n"));
		      if (loop) {
			      loop = endMultiply(pLS, loop, STATE_PLAY);
		      }
		      else {
			      pLS->rounding = false;
			      pLS->state = STATE_PLAY;
			      pLS->wasMuted = false;
			      undoLoop(pLS, false);
			      goto passthrough;
		      }
			      
		      break;
	      }
	      
	      for (;lSampleIndex < SampleCount;
		   lSampleIndex++)
	      {
	         fWet += wetDelta;
                 fDry += dryDelta;
	         fFeedback += feedbackDelta;
	         fScratchPos += scratchDelta;



		 
// 		 if (slCurrPos > 0 && (unsigned)(loop->pLoopStart + slCurrPos)
// 		     > (unsigned)(pLS->pSampleBuf + pLS->lBufferSize)) {
// 		    // out of space! give up for now!
// 		    // undo!
// 		    pLS->state = STATE_PLAY;
// 		    undoLoop(pLS);
// 		    DBG(fprintf(stderr,"Multiply Undone! Out of memory!\n"));
// 		    break;
// 		 }

		 lpCurrPos =(unsigned int) fmod(loop->dCurrPos + loop->lStartAdj, srcloop->lLoopLength);
		 slCurrPos =(long) loop->dCurrPos;

		 rCurrPos = fmod (loop->dCurrPos - lOutputLatency - lInputLatency, loop->lLoopLength);
		 if (rCurrPos < 0) {
		    rCurrPos += loop->lLoopLength;
		 }
		 rpCurrPos = fmod (loop->dCurrPos - lOutputLatency - lInputLatency, srcloop->lLoopLength);
		 if (rpCurrPos < 0) {
		    rpCurrPos += srcloop->lLoopLength;
		 }
		 
		 spLoopSample = & pLS->pSampleBuf[(srcloop->lLoopStart + lpCurrPos) & pLS->lBufferSizeMask];
		 pLoopSample = & pLS->pSampleBuf[(loop->lLoopStart + slCurrPos) & pLS->lBufferSizeMask];

		 if (pLS->lFramesUntilInput <= 0) {
			 rLoopSample = & pLS->pSampleBuf[(loop->lLoopStart + (unsigned int) rCurrPos) & pLS->lBufferSizeMask];
			 rpLoopSample = & pLS->pSampleBuf[(srcloop->lLoopStart + (unsigned int) rpCurrPos) & pLS->lBufferSizeMask];
			 pLS->fLoopFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopFadeAtten + pLS->fLoopFadeDelta);
			 pLS->fLoopSrcFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopSrcFadeAtten + pLS->fLoopSrcFadeDelta);
			 pLS->fFeedFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fFeedFadeAtten + pLS->fFeedFadeDelta);
			 pLS->fFeedSrcFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fFeedSrcFadeAtten + pLS->fFeedSrcFadeDelta);
			 lInputReadPos = - pLS->lFramesUntilInput; // negate it
			 lInputReadPos = (lInputReadPos <= pLS->lInputBufWritePos)
				 ? (pLS->lInputBufWritePos - lInputReadPos)
				 : (pLS->lInputBufSize - (lInputReadPos - pLS->lInputBufWritePos)) ;

		 }
		 else {
			 rLoopSample = 0;
			 rpLoopSample = 0;
			 pLS->lFramesUntilInput--;
			 lInputReadPos = pLS->lInputBufWritePos;
		 }
		 
		 //pLS->fLoopFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopFadeAtten + pLS->fLoopFadeDelta);
		 //pLS->fFeedFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fFeedFadeAtten + pLS->fFeedFadeDelta);


		 
		 if (fSyncMode != 0) {
			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];

			 if (fSyncMode == 2.0f) {
				 pLS->lSamplesSinceSync++;
				 
				 if (pfSyncInput[lSampleIndex] > 1.5f) {
					 // cerr << "mt sync reset" << endl;
					 DBG(fprintf(stderr, "%u:%u  TS reset sync in multiply\n", pLS->lLoopIndex, pLS->lChannelIndex));
					 pLS->lSamplesSinceSync = 0;
				 }
				 else if (pLS->recSyncEnded) {
					 // we just synced, need to noitfy slave... this could be problem
					 pfSyncOutput[lSampleIndex] = 2.0f;
					 DBG(fprintf(stderr, "%u:%u  sync out on relsync end on multiply\n", pLS->lLoopIndex, pLS->lChannelIndex));

					 //cerr << "mt notified: " <<  endl;
					 pLS->recSyncEnded = false;
				 }
			 }
			 
		 }
		 else {
			 if (fQuantizeMode == QUANT_OFF 
			     || (fQuantizeMode == QUANT_CYCLE && (((int) rCurrPos + loop->lSyncPos) % loop->lCycleLength) == 0))
			 {
				 pfSyncOutput[lSampleIndex] = 2.0f;
			 }
		 }
		 
		 
		 if (pLS->waitingForSync && (fSyncMode == 0.0f || pfSyncOutput[lSampleIndex] != 0.0f || slCurrPos  >= (long)(loop->lLoopLength)))
		 {
			 DBG(fprintf(stderr,"%u:%u  Finishing synced multiply\n", pLS->lLoopIndex, pLS->lChannelIndex));
			 loop = endMultiply (pLS, loop, pLS->nextState);

			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];
			 
			 pLS->waitingForSync = 0;
			 break;
		 }

		 
		 
		 fInputSample = pfInputLatencyBuf[(lInputReadPos + lSampleIndex) & pLS->lInputBufMask];
		 //fInputSample = pfInput[lSampleIndex];



		 //  xfade input into source loop (for cases immediately after record)
		 if (rpLoopSample) {
			 *(rpLoopSample) = ((*rpLoopSample) * pLS->fFeedSrcFadeAtten) +  pLS->fLoopSrcFadeAtten * fInputSample;
		 }

		 if (pLS->lFramesUntilFilled > 0) {
			 // fill source from the record position
			 fillLoops(pLS, loop, (unsigned int) rpCurrPos, true);
			 pLS->lFramesUntilFilled--;
		 }
		 
		 //fillLoops(pLS, loop, lpCurrPos, false);
		 fillLoops(pLS, loop, slCurrPos, false);
		 
		 
		 
		 // always use the source loop as the source
		 
		 fOutputSample = (fWet *  (*spLoopSample)
				  + fDry * fInputSample);


		 if (slCurrPos < 0) {
		    // this is part of the loop that we need to ignore
		    // fprintf(stderr, "Ignoring at %ul\n", lCurrPos);
		 }
		 else if ((loop->lCycles <=1 && fQuantizeMode != 0)) {
			 // do not include the new input
			 if (rLoopSample) {
				 *(rLoopSample)
					 = pLS->fFeedFadeAtten * fFeedback *  (*rpLoopSample);
			 }
			 //*(pLoopSample)
			 //	 = pLS->fFeedFadeAtten * fFeedback *  (*spLoopSample);

		 }
		 if ((slCurrPos > (long) loop->lMarkEndL &&  *pLS->pfRoundMode == 0)) {
			 // do not include the new input (at end) when not rounding
			 pLS->fLoopFadeDelta = -1.0f / xfadeSamples;

			 if (rLoopSample) {
				 *(rLoopSample) =  
					 ((pLS->fLoopFadeAtten * fInputSample) + (pLS->fFeedFadeAtten * fFeedback *  *(rpLoopSample)));
			 }
			 
			 //*(pLoopSample)
			 //	 = (pLS->fFeedFadeAtten * fFeedback *  (*spLoopSample)) +  (pLS->fLoopFadeAtten * fInputSample);
			 // fprintf(stderr, "Not including input at %ul\n", lCurrPos);
		 }
		 else {
			 pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
	      
			 if (rLoopSample) {
				 *(rLoopSample) =  
					 ((pLS->fLoopFadeAtten * fInputSample) + (pLS->fFeedFadeAtten * fSafetyFeedback * fFeedback *  *(rpLoopSample)));
			 }
			 //*(pLoopSample)
			 //	 = ( (pLS->fLoopFadeAtten * fInputSample) + (pLS->fFeedFadeAtten * fSafetyFeedback *  fFeedback * (*spLoopSample)));
		 }
		 
		 pfOutput[lSampleIndex] = fOutputSample;

		 
		 // increment 
		 loop->dCurrPos = loop->dCurrPos + fRate;
	      

		 // ASSUMPTION: our rate is +1 only		 
		 if ((unsigned long)loop->dCurrPos  >= (loop->lLoopLength)) {


			 if (loop->mult_out == (int) loop->lCycles
			     || (unsigned long)loop->dCurrPos >= loop->lMarkEndH) {
				 // we be done this only happens in round mode
				 // adjust curr position
				 loop->lMarkEndH = LONG_MAX;
				 backfill = loop->backfill = 0;
				 // do adjust it for our new length
				 loop->dCurrPos = 0.0f;
				 
				 loop->lLoopLength = loop->lCycles * loop->lCycleLength;
				 pLS->rounding = false;
				 // fprintf(stderr, "mult is over with %d cyc\n", loop->lCycles);
				 loop = transitionToNext(pLS, loop, pLS->nextState);
				 break;
			 }

			 
			 // increment cycle and looplength
			 loop->lCycles += 1;
			 loop->lLoopLength += loop->lCycleLength;
			 //loop->lLoopStop = loop->lLoopStart + loop->lLoopLength;
			 // this signifies the end of the original cycle
			 loop->firsttime = 0;
			 DBG(fprintf(stderr,"%u:%u  Multiply added cycle %lu  at %g\n", pLS->lLoopIndex, pLS->lChannelIndex, loop->lCycles, loop->dCurrPos));
			 
			 // now we set this to rise in case we were quantized
			 pLS->fLoopFadeDelta = 1.0f / xfadeSamples;
			 pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
			 
			 loop = ensureLoopSpace (pLS, loop, SampleCount - lSampleIndex, NULL);
			 if (!loop) {
				 // out of space! give up for now!
				 // undo!
				 pLS->state = STATE_PLAY;
				 pLS->wasMuted = false;
				 undoLoop(pLS, false);
				 DBG(fprintf(stderr,"dfMultiply Undone! Out of memory!\n"));
				 break;
			 }
			 
		 }
	      }
	   }
	   else {
	      goto passthrough;
	   }
	   
	} break;

	case STATE_INSERT:
	{
	   if (loop && loop->lLoopLength && loop->srcloop)
	   {
	      srcloop = loop->srcloop;
	      firsttime = loop->firsttime;

	      if (pLS->nextState == STATE_MUTE) {
		 // no loop output
		 fWet = 0.0f;
		 wetDelta = 0.0f;
	      }
	      

	      for (;lSampleIndex < SampleCount;
		   lSampleIndex++)
	      {
	         fWet += wetDelta;
                 fDry += dryDelta;
	         fFeedback += feedbackDelta;
	         fScratchPos += scratchDelta;

		 pLS->fPlayFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fPlayFadeAtten + pLS->fPlayFadeDelta);
		 //pLS->fLoopFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopFadeAtten + pLS->fLoopFadeDelta);
		 //pLS->fFeedFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fFeedFadeAtten + pLS->fFeedFadeDelta);
		 
		 lpCurrPos =(unsigned int) fmod(loop->dCurrPos, srcloop->lLoopLength);
		 lCurrPos =(unsigned int) loop->dCurrPos;

		 rCurrPos = fmod (loop->dCurrPos - lOutputLatency - lInputLatency, loop->lLoopLength);
		 if (rCurrPos < 0) {
		    rCurrPos += loop->lLoopLength;
		 }
		 rpCurrPos = fmod (loop->dCurrPos - lOutputLatency - lInputLatency, srcloop->lLoopLength);
		 if (rpCurrPos < 0) {
		    rpCurrPos += srcloop->lLoopLength;
		 }

		 spLoopSample = & pLS->pSampleBuf[(srcloop->lLoopStart + lpCurrPos) & pLS->lBufferSizeMask];
		 pLoopSample = & pLS->pSampleBuf[(loop->lLoopStart + lCurrPos) & pLS->lBufferSizeMask];

		 if (pLS->lFramesUntilInput <= 0) {
			 rLoopSample = & pLS->pSampleBuf[(loop->lLoopStart + (unsigned int) rCurrPos) & pLS->lBufferSizeMask];
			 rpLoopSample = & pLS->pSampleBuf[(srcloop->lLoopStart + (unsigned int) rpCurrPos) & pLS->lBufferSizeMask];
			 pLS->fLoopFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopFadeAtten + pLS->fLoopFadeDelta);
			 pLS->fLoopSrcFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopSrcFadeAtten + pLS->fLoopSrcFadeDelta);
			 pLS->fFeedFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fFeedFadeAtten + pLS->fFeedFadeDelta);
			 pLS->fFeedSrcFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fFeedSrcFadeAtten + pLS->fFeedSrcFadeDelta);
			 lInputReadPos = - pLS->lFramesUntilInput; // negate it
			 lInputReadPos = (lInputReadPos <= pLS->lInputBufWritePos)
				 ? (pLS->lInputBufWritePos - lInputReadPos)
				 : (pLS->lInputBufSize - (lInputReadPos - pLS->lInputBufWritePos)) ;

		 }
		 else {
			 rLoopSample = 0;
			 rpLoopSample = 0;
			 pLS->lFramesUntilInput--;
			 lInputReadPos = pLS->lInputBufWritePos;
		 }

		 
		 //fInputSample = pfInput[lSampleIndex];
		 fInputSample = pfInputLatencyBuf[(lInputReadPos + lSampleIndex) & pLS->lInputBufMask];

		 // xfade input into source loop (for cases immediately after record)
		 if (rpLoopSample) {
			 *(rpLoopSample) = ((*rpLoopSample) * pLS->fFeedSrcFadeAtten) +  pLS->fLoopSrcFadeAtten * fInputSample;
		 }

		 // fill from the record position
		 if (pLS->lFramesUntilFilled > 0) {
			 fillLoops(pLS, loop, (unsigned int) rCurrPos, true);
			 pLS->lFramesUntilFilled--;
		 }
		 
		 fillLoops(pLS, loop, lCurrPos, false);
		 

		 
		 if (firsttime && *pLS->pfQuantMode != 0 )
		 {
		    // just the source and input
			 fOutputSample = (pLS->fPlayFadeAtten * fWet *  (*spLoopSample))
				 + fDry * fInputSample;
		    
		    // do not include the new input
		    //*(loop->pLoopStart + lCurrPos)
		    //  = fFeedback *  *(srcloop->pLoopStart + lpCurrPos);
		    //*(pLoopSample) = (pLS->fFeedFadeAtten * fFeedback *  (*pLoopSample));
		 }
		 else if (lCurrPos > loop->lMarkEndL && *pLS->pfRoundMode == 0)
		 {
		    // insert zeros, we finishing an insert with nothingness
		    pLS->fLoopFadeDelta = -1.0f / xfadeSamples;

		    fOutputSample = fDry * fInputSample;

		    if (rLoopSample) {
			    *(rLoopSample) = fInputSample * pLS->fLoopFadeAtten;
		    }

		 }
		 else {
		    // just the input we are now inserting
		    pLS->fLoopFadeDelta = 1.0f / xfadeSamples;
		    pLS->fFeedFadeDelta = -1.0f / xfadeSamples;
		    pLS->fPlayFadeDelta = -1.0f / xfadeSamples;

		    fOutputSample = fDry * fInputSample  + (pLS->fPlayFadeAtten * fWet *  (*spLoopSample));

		    if (rLoopSample) {
			    *(rLoopSample) = (fInputSample * pLS->fLoopFadeAtten) + (pLS->fFeedFadeAtten * fFeedback *  (*rLoopSample));
		    }

		 }
		 
		 
		 pfOutput[lSampleIndex] = fOutputSample;

		 if (fSyncMode != 0) {
			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];

			 pLS->lSamplesSinceSync++;
			 
			 if (pfSyncInput[lSampleIndex] > 1.5f) {
				 //cerr << "ins sync reset" << endl;
				 pLS->lSamplesSinceSync = 0;
			 }

			 if (fSyncMode == 2.0f) {
				 if (pLS->recSyncEnded) {
					 // we just synced, need to noitfy slave... this could be problem
					 pfSyncOutput[lSampleIndex] = 2.0f;
					 DBG(fprintf(stderr, "%u:%u  sync out on relsync end on insert\n", pLS->lLoopIndex, pLS->lChannelIndex));

					 //cerr << "ins notified" << endl;
					 pLS->recSyncEnded = false;
				 }
			 }
		 }
		 else if (fQuantizeMode == QUANT_OFF 
			  || (fQuantizeMode == QUANT_CYCLE && (((int) rCurrPos + loop->lSyncPos) % loop->lCycleLength) == 0)
			  || (fQuantizeMode == QUANT_LOOP && (((int)rCurrPos + loop->lSyncPos) % loop->lLoopLength)== 0) 
			  || (fQuantizeMode == QUANT_8TH && (((int)rCurrPos + loop->lSyncPos) % eighthSamples) == 0)) {
			 pfSyncOutput[lSampleIndex] = 2.0f;
		 }
		 
		 
		 // increment 
		 loop->dCurrPos = loop->dCurrPos + fRate;
	      

		 
		 if ((unsigned long)loop->dCurrPos >= loop->lMarkEndH) {
		    // we be done.. this only happens in round mode
		    // adjust curr position to 0

		    
		    loop->lMarkEndL = (unsigned long) loop->dCurrPos;
		    loop->lMarkEndH = loop->lLoopLength - 1;
		    backfill = loop->backfill = 1;
		    pLS->rounding = false;
		    loop->lLoopLength = loop->lCycles * loop->lCycleLength;
		    
		    DBG(fprintf(stderr, "%u:%u  Looplength = %lu   cycles=%lu\n", pLS->lLoopIndex, pLS->lChannelIndex, loop->lLoopLength, loop->lCycles));
		    
		    loop = transitionToNext(pLS, loop, pLS->nextState);
		    DBG(fprintf(stderr,"%u:%u  Entering state %d from insert\n", pLS->lLoopIndex, pLS->lChannelIndex, pLS->state));
		    break;
		 }

		 // ASSUMPTION: our rate is +1 only		 
		 if (firsttime && ((unsigned long)rCurrPos % loop->lCycleLength) == 0)
		 {
		    firsttime = loop->firsttime = 0;
		    DBG(fprintf(stderr, "first time done\n"));
		    // now we set this to rise in case we were quantized
		    pLS->fLoopFadeDelta = 1.0f / xfadeSamples;
		    pLS->fFeedFadeDelta = -1.0f / xfadeSamples;
		 }
		 
		 if ((lCurrPos % loop->lCycleLength) == ((loop->lInsPos-1) % loop->lCycleLength)) {

			 if ((unsigned)(loop->lLoopLength + loop->lCycleLength)
			     > (unsigned)(pLS->lBufferSize))
				 
			 {
				 // out of space! give up for now!
				 pLS->rounding = false;
				 pLS->state = STATE_PLAY;
				 pLS->wasMuted = false;
				 //undoLoop(pLS);
				 DBG(fprintf(stderr,"Insert finish early! Out of memory!\n"));
				 break;
			 }
			 else {
				 // increment cycle and looplength
				 loop->lCycles += 1;
				 loop->lLoopLength += loop->lCycleLength;
				 //loop->lLoopStop = loop->lLoopStart + loop->lLoopLength;
				 // this signifies the end of the original cycle
				 DBG(fprintf(stderr,"insert added cycle. Total=%lu\n", loop->lCycles));
				 // now we set this to rise in case we were quantized
				 pLS->fLoopFadeDelta = 1.0f / xfadeSamples;
				 pLS->fFeedFadeDelta = -1.0f / xfadeSamples;
			 }
		 }
	      }
	   }
	   else {
	      goto passthrough;
	   }
	   
	} break;
	
	case STATE_UNDO_ALL:
	case STATE_REDO_ALL:
	case STATE_UNDO:
	case STATE_REDO:
	case STATE_PLAY:
	case STATE_ONESHOT:
	case STATE_SCRATCH:
	case STATE_MUTE:
	case STATE_PAUSED:
	{
	   //fprintf(stderr,"in play begin\n");	   
	   // play  the input out mixed with the recorded loop.
	   if (loop && loop->lLoopLength)
	   {
	      tmpWet = fWet;
	      
	      if(pLS->state == STATE_SCRATCH)
	      {
	      
		 // calculate new rate if rateSwitch is on
		 fPosRatio = (loop->dCurrPos / loop->lLoopLength);
	      
		 if (pLS->fLastScratchVal != fScratchPos
		     && pLS->lScratchSamples > 0) {
		    // we have a change in scratching pos. Find new rate

		    if (pLS->lScratchSamples < 14000) {
		       pLS->fCurrScratchRate = (fScratchPos - fPosRatio) * loop->lLoopLength
			  / pLS->lScratchSamples;

		    }
		    else if (pLS->bRateCtrlActive && pLS->pfRate) {
		       fRate = *pLS->pfRate;
		    }
		    else {
		       fRate = 0.0f;
		    }
		    
		    pLS->lScratchSamples = 0;
		    pLS->fLastScratchVal = fScratchPos;


		    
		    //fprintf(stderr, "fScratchPos: %f   fCurrScratchRate: %f  \n", fScratchPos,
		    //   pLS->fCurrScratchRate);
		 
		 }
		 else if (fabs(pLS->fCurrScratchRate) < 0.2f
			  || ( pLS->lScratchSamples > 14000)
			  || ( pLS->fCurrScratchRate > 0.0f && (fPosRatio >= pLS->fLastScratchVal ))
			  || ( pLS->fCurrScratchRate < 0.0f && (fPosRatio <= pLS->fLastScratchVal )))
		 {
		    // we have reached the destination, no more scratching
		    pLS->fCurrScratchRate = 0.0f;

		    if (pLS->bRateCtrlActive && pLS->pfRate) {
		       fRate = *pLS->pfRate;
		    }
		    else {
		       // pure scratching
		       fRate = 0.0f;
		    }
		    //fprintf(stderr, "fScratchPos: %f   fCurrScratchRate: %f  ******\n", fScratchPos,
		    //	   pLS->fCurrScratchRate);
		 
		 }
		 else {
		    fRate = pLS->fCurrScratchRate;
		 }

	      }


	      srcloop = loop->srcloop;

	      bool recenter = true;

	      
	      for (;lSampleIndex < SampleCount;
		   lSampleIndex++)
	      {

		 lCurrPos =(unsigned int) fmod(loop->dCurrPos, loop->lLoopLength);
		 //fprintf(stderr, "curr = %u\n", lCurrPos);
		 pLoopSample = & pLS->pSampleBuf[(loop->lLoopStart + lCurrPos) & pLS->lBufferSizeMask];
			  
			  if (pLS->state == STATE_UNDO){
				  prevloop = pLS->headLoopChunk->prev;
				  xCurrPos = (unsigned int) fmod(loop->dCurrPos, prevloop->lLoopLength);
				  xLoopSample = & pLS->pSampleBuf[(prevloop->lLoopStart + xCurrPos) & pLS->lBufferSizeMask];
			  }
			  if (pLS->state == STATE_REDO) {
				  nextloop = pLS->headLoopChunk->next;
				  xCurrPos = (unsigned int) fmod(loop->dCurrPos, nextloop->lLoopLength);
				  xLoopSample = & pLS->pSampleBuf[(nextloop->lLoopStart + xCurrPos) & pLS->lBufferSizeMask];
			  }
			  if (pLS->state == STATE_REDO_ALL) {
				  nextloop = pLS->headLoopChunk;
				  while (nextloop->next) {
					  nextloop = nextloop->next;
				  }
				  xCurrPos = (unsigned int) fmod(loop->dCurrPos, nextloop->lLoopLength);
				  xLoopSample = & pLS->pSampleBuf[(nextloop->lLoopStart + xCurrPos) & pLS->lBufferSizeMask];
			  }

		 rCurrPos = fmod (loop->dCurrPos - (fRate * (lOutputLatency + lInputLatency)), loop->lLoopLength);
		 if (rCurrPos < 0) {
			 rCurrPos += loop->lLoopLength;
		 }

		 rLoopSample = & pLS->pSampleBuf[(loop->lLoopStart + (unsigned int) rCurrPos) & pLS->lBufferSizeMask];
		 lInputReadPos = pLS->lInputBufWritePos;

		 if (rCurrPos == loop->lLoopLength-1) {
			 //DBG(cerr << "In play rcurrpos == : " << rCurrPos << "  val: " << *pLoopSample << endl;);
		 }

		 
		 
		 if (fSyncMode != 0.0f) {
			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];

			 pLS->lSamplesSinceSync++;
			 
			 if (pfSyncInput[lSampleIndex] > 1.5f) {
				 //DBG(cerr << pLS->lLoopIndex << ":" << pLS->lChannelIndex << " TS sync reset at: " << loop->dCurrPos << "  with since: " << pLS->lSamplesSinceSync << endl);
				 pLS->lSamplesSinceSync = 0;
			 }

			 if (fSyncMode == 2.0f) {
				 if (pLS->recSyncEnded) {
					 // we just synced, need to noitfy slave... this could be problem
					 pfSyncOutput[lSampleIndex] = 1.0f;
					 DBG(cerr << pLS->lLoopIndex << ":" << pLS->lChannelIndex << "  notified rel synced at " << loop->dCurrPos << endl);
					 pLS->recSyncEnded = false;
				 }
			 }
		 }
		 else if (fQuantizeMode == QUANT_OFF 
			  || (fQuantizeMode == QUANT_CYCLE && ((lCurrPos + loop->lSyncPos) % loop->lCycleLength) == 0)
			  || (fQuantizeMode == QUANT_LOOP && (((lCurrPos + loop->lSyncPos) % loop->lLoopLength) == 0)) 
			  || (fQuantizeMode == QUANT_8TH && ((lCurrPos + loop->lSyncPos) % eighthSamples) == 0)) {
			  //|| (fQuantizeMode == QUANT_CYCLE && ((lCurrPos) % loop->lCycleLength) == 0)
			  //|| (fQuantizeMode == QUANT_LOOP && (((lCurrPos) % loop->lLoopLength) == 0)) 
			  //|| (fQuantizeMode == QUANT_8TH && ((lCurrPos) % eighthSamples) == 0)) {
			 //DBG(fprintf(stderr, "x%08x  outputtuing sync at %d\n", (unsigned) pLS, lCurrPos));
			 pfSyncOutput[lSampleIndex] = 2.0f;
		 }


 		 if (pLS->fNextCurrRate != 0 && pfSyncOutput[lSampleIndex] > 1.5f && fTempo > 0.0f) {
 		       // commit the new rate at boundary (quantized)
 		       pLS->fCurrRate = pLS->fNextCurrRate;
 		       pLS->fNextCurrRate = 0.0f;
 		       DBG(fprintf(stderr, "%u%u  Starting quantized rate change at %d  %g\n", pLS->lLoopIndex, pLS->lChannelIndex, lCurrPos, pfSyncOutput[lSampleIndex]));
 		 }
		 
		 // test playback sync

		 if (syncSamples && fPlaybackSyncMode != 0.0f && fQuantizeMode != QUANT_OFF && !pLS->donePlaySync
		     && ( pfSyncInput[lSampleIndex] > 1.5f)
		     && (labs((long)(loop->lLoopLength - loop->lSyncPos) - lCurrPos) < syncSamples
		     	 || (lCurrPos + loop->lSyncPos) < syncSamples))
		 {
			 DBG(cerr << pLS->lLoopIndex << ":" << pLS->lChannelIndex  << "PLAYBACK SYNC " <<  "  hit at " << lCurrPos << endl);
			 //pLS->waitingForSync = 1;
			 pLS->donePlaySync = true;

			 if (pLS->fCurrRate > 0)
				 loop->dCurrPos = (double) (loop->lLoopLength - loop->lSyncPos) + fSyncOffsetSamples;
			 else
				 loop->dCurrPos = (loop->lLoopLength - loop->lSyncPos) - 1 - fSyncOffsetSamples;

			 pfSyncOutput[lSampleIndex] = 2.0f;
		 }
		 
		 
		 if (pLS->waitingForSync && 
		     ((fSyncMode == 0.0f && pfSyncOutput[lSampleIndex] != 0.0f) 
		      || pfSyncInput[lSampleIndex] != 0.0f
		      ||  (pLS->nextState == STATE_TRIGGER_PLAY && fSyncMode >= 1.0f && pLS->lSamplesSinceSync < eighthSamples))) // some slack
		 {
			 loop->dCurrPos = lCurrPos + modf(loop->dCurrPos, &dDummy);
				 
			 DBG(fprintf(stderr, "%u:%u  transition to next at: %lu: %u  %g  : %lu\n", pLS->lLoopIndex, pLS->lChannelIndex, lSampleIndex, lCurrPos, loop->dCurrPos, loop->lLoopLength));
			 loop = transitionToNext (pLS, loop, pLS->nextState);
			 if (loop)  srcloop = loop->srcloop;
			 else srcloop = NULL;
			 pLS->waitingForSync = 0;
			 recenter = false;
			 break;
		 }

		 fWet += wetDelta;
                 fDry += dryDelta;
     	         fFeedback += feedbackDelta;
	         fScratchPos += scratchDelta;
		 pLS->fLoopFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopFadeAtten + pLS->fLoopFadeDelta);
		 pLS->fLoopSrcFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopSrcFadeAtten + pLS->fLoopSrcFadeDelta);
		 pLS->fFeedSrcFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fFeedSrcFadeAtten + pLS->fFeedSrcFadeDelta);
		 pLS->fPlayFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fPlayFadeAtten + pLS->fPlayFadeDelta);
		 pLS->fFeedFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fFeedFadeAtten + pLS->fFeedFadeDelta);

		 tmpWet = fWet;
		 
		 //if (pLS->fPlayFadeAtten != 0.0f && pLS->fPlayFadeAtten != 1.0f) {
			 //cerr << "play fade: " << pLS->fPlayFadeAtten << endl;
		 //}
  
		      
		 tmpWet *= pLS->fPlayFadeAtten;

// 		 // modify fWet if we are in a ramp up/down
// 		 if (pLS->lRampSamples > 0) {
// 		    if (pLS->state == STATE_MUTE) {
// 		       //negative linear ramp
// 		       tmpWet = fWet * (pLS->lRampSamples * 1.0f) / xfadeSamples;
// 		    }
// 		    else {
// 		       // positive linear ramp
// 		       tmpWet = fWet * (xfadeSamples - pLS->lRampSamples)
// 			  * 1.0f / xfadeSamples;
// 		    }

// 		    pLS->lRampSamples -= 1;
// 		 }

		 
		 //fInputSample = pfInput[lSampleIndex];
		 fInputSample = pfInputLatencyBuf[(lInputReadPos + lSampleIndex) & pLS->lInputBufMask];

		 // fill from the record position ??
		 if (pLS->lFramesUntilFilled > 0) {
			 fillLoops(pLS, loop, (unsigned int) rCurrPos, true);
			 pLS->lFramesUntilFilled--;
		 }
		 
		 fillLoops(pLS, loop, lCurrPos, false);

	      
		 
		 fOutputSample =   tmpWet *  (*pLoopSample)
		    + fDry * fInputSample;
		 if (pLS->state == STATE_UNDO || pLS->state == STATE_REDO || pLS->state == STATE_REDO_ALL) {
			 //fprintf(stderr, "fading.. :%g\n", tmpWet);
			 fOutputSample =   tmpWet *  (*xLoopSample)
			 + fDry * fInputSample + (fWet-tmpWet) * (*pLoopSample);
		 }

		 // jlc play
		 // we might add a bit from the input still during xfadeout
		 *(rLoopSample) = ((*rLoopSample) * pLS->fFeedFadeAtten) +  pLS->fLoopFadeAtten * fInputSample;
		 // if (pLS->fLoopFadeAtten > 0.9 && pLS->fLoopFadeAtten < 1) fprintf(stderr, "fLoopFadeAtten: %g, SampleIndex: %d\n", pLS->fLoopFadeAtten, lCurrPos);

		 // optionally support feedback during playback (use rLoopSample??)
		 if (useFeedbackPlay) {
			 *(pLoopSample) *= fFeedback * pLS->fFeedFadeAtten;
		 }
		 
		 pfOutput[lSampleIndex] = fOutputSample;
			  
			  

		 if (pLS->state == STATE_PAUSED && pLS->fPlayFadeAtten == 0.0f) {
			 // do not increment time
		 }
		 else {
			 // increment and wrap at the proper loop end
			 loop->dCurrPos = loop->dCurrPos + fRate;
		 }

 		 if (pLS->fNextCurrRate != 0 && pfSyncOutput[lSampleIndex] > 1.5f && fTempo > 0.0f) {
 		       // commit the new rate at boundary (quantized)
 		       pLS->fCurrRate = pLS->fNextCurrRate;
 		       pLS->fNextCurrRate = 0.0f;
 		       DBG(fprintf(stderr, "%u:%u   Starting quantized rate change at %d\n", pLS->lLoopIndex, pLS->lChannelIndex, lCurrPos));
 		 }
		 
		 if (loop->dCurrPos >= loop->lLoopLength) {
		    if (pLS->state == STATE_ONESHOT) {
		       // done with one shot
			    DBG(fprintf(stderr, "%u:%u  finished ONESHOT  lcurrPos=%d\n", pLS->lLoopIndex, pLS->lChannelIndex, lCurrPos));
		       pLS->state = STATE_MUTE;
		       pLS->fPlayFadeDelta = -1.0f / xfadeSamples;

		       //pLS->lRampSamples = xfadeSamples;
		       //fWet = 0.0;
		    }

		    if (pLS->fNextCurrRate != 0 && pfSyncOutput[lSampleIndex] > 1.5f) {
			    // commit the new rate at boundary (quantized)
			    pLS->fCurrRate = pLS->fNextCurrRate;
			    pLS->fNextCurrRate = 0.0f;
			    DBG(fprintf(stderr, "%u:%u (2) Starting quantized rate change at %d\n", pLS->lLoopIndex, pLS->lChannelIndex, lCurrPos));
		    }

		    pLS->donePlaySync = false;
		 }
		 else if (loop->dCurrPos < 0)
		 {
		    // our rate must be negative
		    // adjust around to the back
		    loop->dCurrPos += loop->lLoopLength;
		    if (pLS->state == STATE_ONESHOT) {
		       // done with one shot
		       DBG(fprintf(stderr, "%u:%u  finished ONESHOT neg\n", pLS->lLoopIndex, pLS->lChannelIndex));
		       pLS->state = STATE_MUTE;
		       //fWet = 0.0;
		       pLS->fPlayFadeDelta = -1.0f / xfadeSamples;
		       //pLS->lRampSamples = xfadeSamples;
		    }

		    if (pLS->fNextCurrRate != 0 && pfSyncOutput[lSampleIndex] > 1.5f) {
		       // commit the new rate at boundary (quantized)
		       pLS->fCurrRate = pLS->fNextCurrRate;
		       pLS->fNextCurrRate = 0.0f;
		       DBG(fprintf(stderr, "%u:%u (3) Starting quantized rate change at %d\n", pLS->lLoopIndex, pLS->lChannelIndex, lCurrPos));
		    }

		    pLS->donePlaySync = false;

		 }


	      }
	      
		   if (pLS->state == STATE_UNDO && pLS->fPlayFadeAtten == 1.0f) {
			   // play some of the old loop first and switch later
			   undoLoop(pLS, false);
			   DBG(fprintf(stderr, "finished UNDO...\n"));
			   pLS->state = pLS->nextState;
		   }
		   if (pLS->state == STATE_REDO && pLS->fPlayFadeAtten == 1.0f) {
			   // play some of the old loop first and switch later
			   redoLoop(pLS);
			   DBG(fprintf(stderr, "finished REDO...\n"));
			   pLS->state = pLS->nextState;
		   }
		   if (pLS->state == STATE_UNDO_ALL && pLS->fPlayFadeAtten == 0.0f) {
			   // fade out the old loop and goto state_off
			   clearLoopChunks(pLS);
			   DBG(fprintf(stderr, "finished UNDO ALL...\n"));
				 cerr << "was muted: " << pLS->wasMuted << endl;
				 if (pLS->wasMuted)
			     pLS->state = STATE_OFF_MUTE;
				 else
			     pLS->state = STATE_OFF;
		   }
		   if (pLS->state == STATE_REDO_ALL && pLS->fPlayFadeAtten == 1.0f) {
			   // play some of the old loop first and switch later
			   lastloop = pLS->headLoopChunk;
			   redoLoop(pLS);
			   while (pLS->headLoopChunk != lastloop) {
				   lastloop = pLS->headLoopChunk;
				   redoLoop(pLS);
			   }
			   DBG(fprintf(stderr, "finished REDO ALL...\n"));
			   pLS->state = pLS->nextState;
		   }
		   
		   
	      // recenter around the mod
	      if (loop) {
		      lCurrPos = (unsigned int) fabs(fmod(loop->dCurrPos, loop->lLoopLength));
		      
		      if (recenter) {
			      loop->dCurrPos = lCurrPos + modf(loop->dCurrPos, &dDummy);
		      }
	      }
	   }
	   else {
	      goto passthrough;
	   }
	   
	} break;

	case STATE_DELAY:
	{
	   if (loop && loop->lLoopLength)
	   {
	      // the loop length is our delay time.
	      backfill = loop->backfill;
	      
	      for (;lSampleIndex < SampleCount;
		   lSampleIndex++)
	      {
	         fWet += wetDelta;
                 fDry += dryDelta;
     	         fFeedback += feedbackDelta;
	         fScratchPos += scratchDelta;
		      
		 // wrap properly
		 lCurrPos =(unsigned int) fmod(loop->dCurrPos, loop->lLoopLength);
		 pLoopSample = & pLS->pSampleBuf[(loop->lLoopStart + lCurrPos) & pLS->lBufferSizeMask];

		 fInputSample = pfInput[lSampleIndex];

		 if (backfill && lCurrPos >= loop->lMarkEndL && lCurrPos <= loop->lMarkEndH) {
		    // our delay buffer is invalid here, clear it
		    *(pLoopSample) = 0.0f;

		    if (fRate > 0) {
		       loop->lMarkEndL = lCurrPos;
		    }
		    else {
		       loop->lMarkEndH = lCurrPos;
		    }
		 }


		 fOutputSample =   fWet *  *(pLoopSample)
		    + fDry * fInputSample;


		 if (!pLS->bHoldMode) {
		    // now fill in from input if we are not holding the delay
		    *(pLoopSample) = 
		      (fInputSample +  fFeedback *  *(pLoopSample));
		 }
		 
		 pfOutput[lSampleIndex] = fOutputSample;

		 if (fSyncMode != 0 || fQuantizeMode == QUANT_OFF) {
			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];
		 }
		 else if (((lCurrPos + loop->lSyncPos) % loop->lCycleLength) == 0) {
			 pfSyncOutput[lSampleIndex] = 2.0f;
		 }

		 
		 // increment 
		 loop->dCurrPos = loop->dCurrPos + fRate;

		 if (backfill && loop->lMarkEndL == loop->lMarkEndH) {
		    // no need to clear the buf first now
		    backfill = loop->backfill = 0;
		 }

		 else if (loop->dCurrPos < 0)
		 {
		    // our rate must be negative
		    // adjust around to the back
		    loop->dCurrPos += loop->lLoopLength;
		 }

		 
	      }

	      // recenter around the mod
	      lCurrPos = (unsigned int) fabs(fmod(loop->dCurrPos, loop->lLoopLength));
	      
	      loop->dCurrPos = lCurrPos + modf(loop->dCurrPos, &dDummy); 

	      
	   }
	   else {
	      goto passthrough;
	   }
	} break;
	
	default:
	{
	   goto passthrough;

	}  break;
	
     }

     //goto loopend;
     continue;
     
    passthrough:

     // simply play the input out directly
     // no loop has been created yet
     for (;lSampleIndex < SampleCount;
	  lSampleIndex++)
     {
        fWet += wetDelta;
        fDry += dryDelta;
        fFeedback += feedbackDelta;
	fScratchPos += scratchDelta;
	     
	pfOutput[lSampleIndex] = fDry * pfInput[lSampleIndex];

	if (fSyncMode != 0 || fQuantizeMode == QUANT_OFF) {
		pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];
	}
	
	if (fSyncMode >= 1.0f) {
		pLS->lSamplesSinceSync++;
		if (pfSyncInput[lSampleIndex] > 1.5f) {
			//cerr << "passthru resetting sync: " << pLS << endl;
			pLS->lSamplesSinceSync = 0;
		}
	}
	
     }
     
  }
  
  // keep track of time between triggers to ignore settling issues
  // pLS->lRecTrigSamples += SampleCount;
  pLS->lScratchSamples += SampleCount;  
  pLS->lTapTrigSamples += SampleCount;

  // printf ("wet is %g    targ was %g\n", fWet, *(pLS->pfWet));
  pLS->fWetCurr = wetTarget;
  pLS->fWetTarget = wetTarget;

  pLS->fDryCurr = dryTarget;
  pLS->fDryTarget = dryTarget;
  
  pLS->fFeedbackCurr = feedbackTarget;
  pLS->fFeedbackTarget = feedbackTarget;


  pLS->fScratchPosCurr = scratchTarget;
  pLS->fScratchPosTarget = scratchTarget;
  
  // update output ports
  *pLS->pfStateOut = (LADSPA_Data) pLS->state;

  *pLS->pfNextStateOut = (LADSPA_Data) pLS->nextState;
  
  // either waiting for sync, waiting for a reverse, or rounding a mult
  *pLS->pfWaiting = (pLS->waitingForSync || pLS->state == STATE_TRIG_START || pLS->state == STATE_TRIG_STOP
		     || pLS->fNextCurrRate != 0.0f || pLS->rounding)  ? 1.0f: 0.0f;

  *pLS->pfRateOutput = (LADSPA_Data) pLS->fCurrRate *  (*pLS->pfRate);


  pLS->lInputBufWritePos = (pLS->lInputBufWritePos + SampleCount) & pLS->lInputBufMask;

  
  if (pLS->pfSecsFree) {
	  *pLS->pfSecsFree = pLS->lBufferSize / pLS->fSampleRate;
//      *pLS->pfSecsFree = (pLS->fTotalSecs) -
// 	(pLS->headLoopChunk ?
// 	 ((((unsigned)pLS->headLoopChunk->pLoopStop - (unsigned)pLS->pSampleBuf)
// 	  / sizeof(LADSPA_Data)) / pLS->fSampleRate)   :
// 	 0);
  }
  
  if (loop) {
     if (pLS->pfLoopPos)
	*pLS->pfLoopPos = (LADSPA_Data) (loop->dCurrPos / pLS->fSampleRate);

     if (pLS->pfLoopLength)
	*pLS->pfLoopLength = ((LADSPA_Data) loop->lLoopLength) / pLS->fSampleRate;

     if (pLS->pfCycleLength)
	*pLS->pfCycleLength = ((LADSPA_Data) loop->lCycleLength) / pLS->fSampleRate;


     
  }
  else {
     if (pLS->pfLoopPos)
	*pLS->pfLoopPos = 0.0f;
     if (pLS->pfLoopLength)
	*pLS->pfLoopLength = 0.0f;     
     if (pLS->pfCycleLength)
	*pLS->pfCycleLength = 0.0f;

     if (pLS->pfStateOut && pLS->state != STATE_OFF_MUTE  && pLS->state != STATE_MUTE && pLS->state != STATE_TRIG_START)
	*pLS->pfStateOut = (LADSPA_Data) STATE_OFF;

  }
  
  
}


/*****************************************************************************/

LADSPA_Descriptor * g_psDescriptor = NULL;

/*****************************************************************************/

/* sl_init() is called automatically when the plugin library is first
   loaded. */

LADSPA_Descriptor * create_sl_descriptor()
{
  char ** pcPortNames;
  LADSPA_PortDescriptor * piPortDescriptors;
  LADSPA_PortRangeHint * psPortRangeHints;
  LADSPA_Descriptor * psDescriptor;
    
  psDescriptor
    = (LADSPA_Descriptor *)malloc(sizeof(LADSPA_Descriptor));
  if (psDescriptor) {
    psDescriptor->UniqueID
      = 1601;
    psDescriptor->Label
      = strdup("SooperLooper");
    psDescriptor->Properties
      = LADSPA_PROPERTY_HARD_RT_CAPABLE;
    psDescriptor->Name 
      = strdup("SooperLooper");
    psDescriptor->Maker
      = strdup("Jesse Chappell");
    psDescriptor->Copyright
      = strdup("2002, Jesse Chappell");

    psDescriptor->PortCount 
      = PORT_COUNT;
    piPortDescriptors
      = (LADSPA_PortDescriptor *)calloc(PORT_COUNT, sizeof(LADSPA_PortDescriptor));
    psDescriptor->PortDescriptors 
      = (const LADSPA_PortDescriptor *)piPortDescriptors;
    piPortDescriptors[WetLevel]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[DryLevel]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;

    piPortDescriptors[Feedback]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[TriggerThreshold]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[Rate]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[ScratchPosition]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;

    piPortDescriptors[Multi]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;

    piPortDescriptors[TapDelayTrigger]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;

    piPortDescriptors[UseFeedbackPlay]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[Quantize]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[Round]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[Sync]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[PlaybackSync]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[UseRate]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[FadeSamples]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[TempoInput]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    
    piPortDescriptors[RedoTap]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    
    piPortDescriptors[AudioInputPort]
      = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
    piPortDescriptors[AudioOutputPort]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
    piPortDescriptors[SyncInputPort]
      = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
    piPortDescriptors[SyncOutputPort]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;

    
    piPortDescriptors[State]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[LoopPosition]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[LoopLength]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[CycleLength]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[LoopMemory]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[LoopFreeMemory]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[Waiting]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[TrueRate]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[NextState]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;


    
    pcPortNames
      = (char **)calloc(PORT_COUNT, sizeof(char *));
    psDescriptor->PortNames
      = (const char **)pcPortNames;
    pcPortNames[DryLevel] 
      = strdup("Dry Level (dB)");
    pcPortNames[WetLevel] 
      = strdup("Wet Level (dB)");

    pcPortNames[Feedback] 
      = strdup("Feedback");
    pcPortNames[TriggerThreshold] 
      = strdup("Trigger Threshold");
    pcPortNames[Rate] 
      = strdup("Rate");
    pcPortNames[ScratchPosition] 
      = strdup("Scratch Destination");

    pcPortNames[Multi] 
      = strdup("Multi Control");

    pcPortNames[TapDelayTrigger] 
      = strdup("Tap Delay Trigger");

    pcPortNames[UseFeedbackPlay] 
      = strdup("Use Feedback Play");
    pcPortNames[Quantize] 
      = strdup("Quantize Mode");
    pcPortNames[Round] 
      = strdup("Round Mode");
    pcPortNames[RedoTap] 
      = strdup("Redo Tap Mode");
    pcPortNames[Sync] 
      = strdup("Sync Mode");
    pcPortNames[PlaybackSync] 
      = strdup("Playback Sync Mode");
    pcPortNames[UseRate] 
      = strdup("Use Rate Ctrl");
    pcPortNames[FadeSamples] 
      = strdup("Fade samples");
    pcPortNames[TempoInput] 
      = strdup("Tempo");

    pcPortNames[InputLatency] 
      = strdup("InputLatency");
    pcPortNames[OutputLatency] 
      = strdup("OutputLatency");
    pcPortNames[TriggerLatency] 
      = strdup("TriggerLatency");
    pcPortNames[MuteQuantized] 
      = strdup("MuteQuantized");
    pcPortNames[OverdubQuantized] 
      = strdup("OverdubQuantized");
    
    pcPortNames[AudioInputPort] 
      = strdup("Input");
    pcPortNames[AudioOutputPort]
      = strdup("Output");

    pcPortNames[SyncInputPort] 
      = strdup("Sync Input");
    pcPortNames[SyncOutputPort]
      = strdup("Sync Output");
    
    pcPortNames[State] 
      = strdup("State Output");
    pcPortNames[LoopLength]
      = strdup("Loop Length Out (s)");
    pcPortNames[LoopPosition]
      = strdup("Loop Position Out (s)");
    pcPortNames[CycleLength]
      = strdup("Cycle Length Out (s)");

    pcPortNames[LoopMemory]
      = strdup("Total Sample Mem (s)");
    pcPortNames[LoopFreeMemory]
      = strdup("Free Sample Mem (s)");
    pcPortNames[Waiting]
      = strdup("Waiting");
    pcPortNames[TrueRate]
      = strdup("True Rate");
    pcPortNames[NextState] 
      = strdup("Next State Output");

    
    psPortRangeHints = ((LADSPA_PortRangeHint *)
			calloc(PORT_COUNT, sizeof(LADSPA_PortRangeHint)));
    psDescriptor->PortRangeHints
      = (const LADSPA_PortRangeHint *)psPortRangeHints;

    psPortRangeHints[DryLevel].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    psPortRangeHints[DryLevel].LowerBound 
      = -90.0f;
    psPortRangeHints[DryLevel].UpperBound
      = 0.0;

    psPortRangeHints[WetLevel].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    psPortRangeHints[WetLevel].LowerBound 
      = -90.0f;
    psPortRangeHints[WetLevel].UpperBound
      = 0.0;
    
    psPortRangeHints[Feedback].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    psPortRangeHints[Feedback].LowerBound 
      = 0.0;
    psPortRangeHints[Feedback].UpperBound
      = 1.0;

    psPortRangeHints[TriggerThreshold].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    psPortRangeHints[TriggerThreshold].LowerBound 
      = 0.0;
    psPortRangeHints[TriggerThreshold].UpperBound
      = 1.0;

    psPortRangeHints[Rate].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    psPortRangeHints[Rate].LowerBound 
      = -4.0;
    psPortRangeHints[Rate].UpperBound
      = 4.0;

    psPortRangeHints[TrueRate].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    psPortRangeHints[TrueRate].LowerBound 
      = -4.0;
    psPortRangeHints[TrueRate].UpperBound
      = 4.0;

    psPortRangeHints[ScratchPosition].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    psPortRangeHints[ScratchPosition].LowerBound 
      = 0.0;
    psPortRangeHints[ScratchPosition].UpperBound
      = 1.0;

    psPortRangeHints[Multi].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_INTEGER;
    psPortRangeHints[Multi].LowerBound 
      = 0.0;
    psPortRangeHints[Multi].UpperBound
      = 127.0;

    psPortRangeHints[TapDelayTrigger].HintDescriptor
      = LADSPA_HINT_TOGGLED;

    psPortRangeHints[UseFeedbackPlay].HintDescriptor
	    = LADSPA_HINT_TOGGLED;

    psPortRangeHints[Quantize].HintDescriptor
      = LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_INTEGER;
    psPortRangeHints[Quantize].LowerBound 
      = 0.0f;
    psPortRangeHints[Quantize].UpperBound
      = 3.0f;

    psPortRangeHints[FadeSamples].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_INTEGER;
    psPortRangeHints[FadeSamples].LowerBound 
      = 0.0f;
    psPortRangeHints[FadeSamples].UpperBound
      = 8192.0f;

    psPortRangeHints[TempoInput].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_INTEGER;
    psPortRangeHints[TempoInput].LowerBound 
      = 0.0f;
    psPortRangeHints[TempoInput].UpperBound
      = 1000.0f;

    psPortRangeHints[InputLatency].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_INTEGER;
    psPortRangeHints[InputLatency].LowerBound 
      = 0.0f;
    psPortRangeHints[InputLatency].UpperBound
      = 1000000.0f;
    psPortRangeHints[OutputLatency].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_INTEGER;
    psPortRangeHints[OutputLatency].LowerBound 
      = 0.0f;
    psPortRangeHints[OutputLatency].UpperBound
      = 1000000.0f;
    psPortRangeHints[TriggerLatency].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_INTEGER;
    psPortRangeHints[TriggerLatency].LowerBound 
      = 0.0f;
    psPortRangeHints[TriggerLatency].UpperBound
      = 1000000.0f;

    psPortRangeHints[MuteQuantized].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_TOGGLED;
    psPortRangeHints[MuteQuantized].LowerBound 
      = 0.0f;
    psPortRangeHints[MuteQuantized].UpperBound
      = 1.0f; 
    psPortRangeHints[OverdubQuantized].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_TOGGLED;
    psPortRangeHints[OverdubQuantized].LowerBound 
      = 0.0f;
    psPortRangeHints[OverdubQuantized].UpperBound
      = 1.0f;
   
    
    psPortRangeHints[Round].HintDescriptor
      = LADSPA_HINT_TOGGLED;
    psPortRangeHints[RedoTap].HintDescriptor
      = LADSPA_HINT_TOGGLED;
    psPortRangeHints[Sync].HintDescriptor
      = LADSPA_HINT_TOGGLED;
    psPortRangeHints[PlaybackSync].HintDescriptor
      = LADSPA_HINT_TOGGLED;
    psPortRangeHints[UseRate].HintDescriptor
      = LADSPA_HINT_TOGGLED;
    
    psPortRangeHints[AudioInputPort].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[AudioInputPort].LowerBound 
      = 0.0;
    psPortRangeHints[AudioOutputPort].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[AudioOutputPort].LowerBound 
      = 0.0;

    psPortRangeHints[State].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[LoopPosition].LowerBound 
      = 0.0;

    psPortRangeHints[LoopPosition].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[LoopPosition].LowerBound 
      = 0.0;
    
    psPortRangeHints[LoopLength].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[LoopLength].LowerBound 
      = 0.0;

    psPortRangeHints[CycleLength].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[CycleLength].LowerBound 
      = 0.0;

    psPortRangeHints[LoopMemory].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[LoopMemory].LowerBound 
      = 0.0;
    psPortRangeHints[LoopFreeMemory].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[LoopFreeMemory].LowerBound 
      = 0.0;

    psPortRangeHints[Waiting].HintDescriptor
      = LADSPA_HINT_TOGGLED;

    psPortRangeHints[NextState].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[NextState].LowerBound 
      = 0.0;

    
    psDescriptor->instantiate
      = instantiateSooperLooper;
    psDescriptor->connect_port 
      = connectPortToSooperLooper;
    psDescriptor->activate
      = activateSooperLooper;
    psDescriptor->run 
      = runSooperLooper;
    psDescriptor->run_adding
      = NULL;
    psDescriptor->set_run_adding_gain
      = NULL;
    psDescriptor->deactivate
      = NULL;
    psDescriptor->cleanup
      = cleanupSooperLooper;
  }
  
  return psDescriptor;
}

/*****************************************************************************/


void cleanup_sl_descriptor(LADSPA_Descriptor * psDescriptor)
{
  unsigned long lIndex;
  if (psDescriptor) {
    free((char *)psDescriptor->Label);
    free((char *)psDescriptor->Name);
    free((char *)psDescriptor->Maker);
    free((char *)psDescriptor->Copyright);
    free((LADSPA_PortDescriptor *)psDescriptor->PortDescriptors);
    for (lIndex = 0; lIndex < psDescriptor->PortCount; lIndex++)
      free((char *)(psDescriptor->PortNames[lIndex]));
    free((char **)psDescriptor->PortNames);
    free((LADSPA_PortRangeHint *)psDescriptor->PortRangeHints);
    free(psDescriptor);
	psDescriptor = 0;
  }
}


void sl_init()
{
	if (!g_psDescriptor) {
		g_psDescriptor = create_sl_descriptor();
	}
}

/* _fini() is called automatically when the library is unloaded. */
void 
sl_fini() {
	
	cleanup_sl_descriptor(g_psDescriptor);
}


/*****************************************************************************/

/* Return a descriptor of the requested plugin type. Only one plugin
   type is available in this library. */
const LADSPA_Descriptor * 
ladspa_descriptor(unsigned long Index) {
  if (Index == 0)
    return g_psDescriptor;
  else
    return NULL;
}

/*****************************************************************************/

/* EOF */
