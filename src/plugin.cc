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

using namespace SooperLooper;


/*****************************************************************************/
//#define LOOPDEBUG

#ifdef LOOPDEBUG
#define DBG(x) x
#else
#define DBG(x)
#endif

#define VERSION "1.0"

/* The maximum sample memory  (in seconds). */

#ifndef SAMPLE_MEMORY
#define SAMPLE_MEMORY 40.0
#endif

#define XFADE_SAMPLES 128

// settle time for tap trigger (trigger if two changes
// happen within at least X samples)
//#define TRIG_SETTLE  4410
#define TRIG_SETTLE  2205  

// another thing that shouldn't be hardcoded
#define MAX_LOOPS 512

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

#define STATE_TRIGGER_PLAY 16

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
	
	unsigned long frames_left = loop->lLoopLength - loop_offset;
	unsigned long startpos = (loop->lLoopStart + loop_offset) & pLS->lBufferSizeMask;
	unsigned long first_chunk;
	unsigned long second_chunk=0;

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
			DBG(fprintf(stderr, "invalidating %08x\n", (unsigned) tailLoop));
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
			DBG(fprintf(stderr, "we've hit the last tail this shoudlnt happen\n"));
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
			DBG(fprintf(stderr, "requesting more space than exists in buffer!\n"));
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
   

   DBG(fprintf(stderr, "New head is %08x   start: %lu\n", (unsigned)loop, loop->lLoopStart);)

   
   return loop;
}


// pop the head off and free it
static void popHeadLoop(SooperLooperI *pLS)
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
   else {
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

void undoLoop(SooperLooperI *pLS)
{
   LoopChunk *loop = pLS->headLoopChunk;
   LoopChunk *prevloop;
   
   prevloop = loop->prev;

   //if (prevloop && prevloop == loop->srcloop) {
      // if the previous was the source of the one we're undoing
      // pass the dCurrPos along, otherwise leave it be.
   // nevermind, ALWAYS pass it along, what the hell
   if (prevloop) {
	   prevloop->dCurrPos = fmod(loop->dCurrPos+loop->lStartAdj, prevloop->lLoopLength);
   }
   
   popHeadLoop(pLS);
   DBG(fprintf(stderr, "Undoing last loop %08x: new head is %08x\n", (unsigned)loop,
	       (unsigned)pLS->headLoopChunk);)
}


void redoLoop(SooperLooperI *pLS)
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
	   
	   pLS->headLoopChunk = nextloop;
	   
	   DBG(fprintf(stderr, "Redoing last loop %08x: new head is %08x\n", (unsigned)loop,
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
   // we'll warm up up to 30 secs worth of the loop mem as a tradeoff to the low-mem mac people
   memset (pLS->pSampleBuf, 0, min(pLS->lBufferSize, (unsigned long) (SampleRate * 30) ) * sizeof(LADSPA_Data));

   pLS->lLoopChunkCount = MAX_LOOPS;

   // not using calloc to force touching all memory ahead of time -- PSYCHE!
   pLS->pLoopChunks = (LoopChunk *) calloc(pLS->lLoopChunkCount,  sizeof(LoopChunk));
   if (pLS->pLoopChunks == NULL) {
	   goto cleanup;
   }
   memset (pLS->pLoopChunks, 0, pLS->lLoopChunkCount * sizeof(LoopChunk));

   pLS->lastLoopChunk = pLS->pLoopChunks + pLS->lLoopChunkCount - 1;
   
   /* just one for now */
   //pLS->lLoopStart = 0;
   //pLS->lLoopStop = 0;   
   //pLS->lCurrPos = 0;

   pLS->state = STATE_PLAY;
   pLS->wasMuted = false;
   pLS->recSyncEnded = false;
   
   DBG(fprintf(stderr,"instantiated with buffersize: %lu\n", pLS->lBufferSize));

   
   pLS->pfQuantMode = &pLS->fQuantizeMode;
   pLS->pfRoundMode = &pLS->fRoundMode;
   pLS->pfRedoTapMode = &pLS->fRedoTapMode;
   
   
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

/* Initialise and activate a plugin instance. */
void
activateSooperLooper(LADSPA_Handle Instance) {

  SooperLooperI * pLS;
  pLS = (SooperLooperI *)Instance;

	 
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
  pLS->fRedoTapMode = 1;
  pLS->bRateCtrlActive = (int) *pLS->pfRateCtrlActive;

  pLS->waitingForSync = 0;
  pLS->donePlaySync = false;
  pLS->rounding = false;
  
  pLS->fWetCurr = pLS->fWetTarget = *pLS->pfWet;
  pLS->fDryCurr = pLS->fDryTarget = *pLS->pfDry;
  pLS->fFeedbackCurr = pLS->fFeedbackTarget = *pLS->pfFeedback;
  pLS->fRateCurr = pLS->fRateTarget = *pLS->pfRate;
  pLS->fScratchPosCurr = pLS->fScratchPosTarget = *pLS->pfScratchPos;

  pLS->fLoopFadeDelta = 0.0f;
  pLS->fLoopFadeAtten = 0.0f;
  pLS->fPlayFadeDelta = 0.0f;
  pLS->fPlayFadeAtten = 0.0f;
  pLS->fFeedFadeDelta = 0.0f;
  pLS->fFeedFadeAtten = 1.0f;

  // todo make this a port, for now 2ms
  //pLS->fLoopXfadeSamples = 0.002 * pLS->fSampleRate;
  
  pLS->state = STATE_PLAY;
  pLS->nextState = -1;
  pLS->wasMuted = false;
  pLS->recSyncEnded = false;

  pLS->lSamplesSinceSync = 0;
  
  clearLoopChunks(pLS);


  if (pLS->pfSecsTotal) {
     *pLS->pfSecsTotal = (LADSPA_Data) pLS->fTotalSecs;
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

   //fprintf(stderr,"connectPortTo\n");  

   
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



static inline void fillLoops(SooperLooperI *pLS, LoopChunk *mloop, unsigned long lCurrPos)
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

      if (loop->frontfill && lCurrPos<=loop->lMarkH && lCurrPos>=loop->lMarkL)
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
		      DBG(fprintf(stderr,"front segment filled for %08x for %08x in at %lu\n",
				  (unsigned)loop, (unsigned) srcloop, loop->lMarkL);)
			      loop->frontfill = 0;
		      loop->lMarkL = loop->lMarkH = LONG_MAX;
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
		      DBG(fprintf(stderr,"back segment filled in for %08x from %08x at %lu\n",
				  (unsigned)loop, (unsigned)srcloop, loop->lMarkEndL);)
			      loop->backfill = 0;
		      loop->lMarkEndL = loop->lMarkEndH = LONG_MAX;
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

      pLS->state = STATE_MULTIPLY;

      if (*pLS->pfQuantMode == 0.0f) {
	      // we'll do this later if we are quantizing
	      pLS->fLoopFadeDelta = 1.0f / xfadeSamples;
	      pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
      }

      //pLS->fPlayFadeDelta = -1.0f / xfadeSamples;

      // start out with the single cycle as our length as a marker
      loop->lLoopLength = srcloop->lCycleLength;
      //loop->lLoopLength = srcloop->lLoopLength;

      // start out at same pos
      loop->dCurrPos = srcloop->dCurrPos;
      loop->lCycles = 1; // start with 1 by default
      //loop->lCycles = srcloop->lCycles; 

      loop->lCycleLength = srcloop->lCycleLength;
		       
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
		      loop->lStartAdj = ((int)floor(fabs((srcloop->dCurrPos-1) / srcloop->lCycleLength))
					 + 1) * srcloop->lCycleLength; 
		      loop->frontfill = 0; // no need.
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
	      DBG(fprintf(stderr,"Quantize ignoring first %d cycles.  Orig length %lu\n",
			  ((int)floor(srcloop->dCurrPos / srcloop->lCycleLength)+ 1),
			  srcloop->lLoopLength));
			  
      }
		    
      if (loop->dCurrPos > 0) {
	 loop->lMarkL = 0;
	 loop->lMarkH = (unsigned long) srcloop->dCurrPos - 1;
      }
      else {
	 // no need to frontfill
	 loop->frontfill = 0;
	 loop->lMarkL = loop->lMarkH = LONG_MAX;
      }
      
      loop->lMarkEndL = loop->lMarkEndH = LONG_MAX;

      
      DBG(fprintf(stderr,"Mark at L:%lu  h:%lu\n",loop->lMarkL, loop->lMarkH);
	  fprintf(stderr,"EndMark at L:%lu  h:%lu\n",loop->lMarkEndL, loop->lMarkEndH);
	  fprintf(stderr,"Entering MULTIPLY state  with cyclecount=%lu   curpos=%g   looplen=%lu\n", loop->lCycles, loop->dCurrPos, loop->lLoopLength));
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
      DBG(fprintf(stderr,"Zero length loop now: at %d!\n", (int)loop->dCurrPos));
      DBG(fprintf(stderr,"Entering %d from MULTIPLY\n", nextstate));
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
		       
      if (*pLS->pfRoundMode == 0) {
	 // calculate loop length
	 loop->lLoopLength = loop->lCycles * loop->lCycleLength;
	 loop->backfill = 1;

	 // adjust curr position
	 //loop->dCurrPos -= loop->lStartAdj;
			  
	 loop->lMarkEndL = (unsigned long)loop->dCurrPos;
	 loop->lMarkEndH = loop->lLoopLength - 1;

	 DBG(fprintf(stderr,"Entering %d from MULTIPLY. Length %lu.  %lu cycles\n",nextstate,
		 loop->lLoopLength, loop->lCycles)); 
	 
	 loop = transitionToNext(pLS, loop, nextstate);

   
	 pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
	 pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
	 
      }
      else {
	 // in round mode we need to wait it out
	 // and keep recording till the end
	 DBG(fprintf(stderr,"Finishing MULTIPLY rounded\n")); 
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
   
   // try to get a new one with at least 1 cycle more length
   loop = pushNewLoopChunk(pLS, loop->lLoopLength + loop->lCycleLength, loop);

   if (loop) {
      DBG(fprintf(stderr,"Entering INSERT state\n"));

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

      
      if (loop->dCurrPos > 0) {
	 loop->lMarkL = 0;
	 loop->lMarkH = (unsigned long) srcloop->dCurrPos - 1;
      }
      else {
	 loop->frontfill = 0; 
	 loop->lMarkL = loop->lMarkH = LONG_MAX;
      }
      
      loop->lMarkEndL = loop->lMarkEndH = LONG_MAX;
		       
      DBG(fprintf(stderr, "InsPos=%lu  RemLen=%lu\n", loop->lInsPos, loop->lRemLen));
      DBG(fprintf(stderr,"Total cycles now=%lu\n", loop->lCycles));
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

   loop->lMarkEndL = (unsigned long)loop->dCurrPos;
   loop->lMarkEndH = loop->lLoopLength - loop->lRemLen;

   DBG(fprintf(stderr,"Finishing INSERT... lMarkEndL=%lu  lMarkEndH=%lu  ll=%lu  rl=%lu\n",
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
   loop = pushNewLoopChunk(pLS, loop->lLoopLength, loop);
   if (loop) {
      pLS->state = STATE_OVERDUB;
      // always the same length as previous loop

      pLS->fLoopFadeDelta = 1.0f / xfadeSamples;
      
      if (!loop->prev) {
	      // then we are overwriting our own source
	      // lets just back out and revalidate the passed in loop
	      loop->next = NULL;

	      DBG(fprintf(stderr, "OVERdub using self\n"));
	      
	      loop = srcloop;
	      pLS->headLoopChunk = pLS->tailLoopChunk = loop;
	      loop->next = NULL;
	      loop->prev = NULL;
	      loop->valid = 1;

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

      
      loop->lStartAdj = 0;
      loop->lEndAdj = 0;
      pLS->nextState = -1;
      
      loop->dOrigFeedback = LIMIT_BETWEEN_0_AND_1(*pLS->pfFeedback);

      if (loop->dCurrPos > 0) 
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
	 loop->lMarkL = (unsigned long) loop->dCurrPos + 1;		       
	 loop->lMarkH = loop->lLoopLength - 1;
	 loop->lMarkEndL = 0;
	 loop->lMarkEndH = (unsigned long) loop->dCurrPos;
      } else {
	 pLS->fCurrRate = 1.0;
	 loop->lMarkL = 0;
	 loop->lMarkH = (unsigned long) loop->dCurrPos - 1;
	 loop->lMarkEndL = (unsigned long) loop->dCurrPos;
	 loop->lMarkEndH = loop->lLoopLength - 1;
      }
      
      DBG(fprintf(stderr,"Mark at L:%lu  h:%lu\n",loop->lMarkL, loop->lMarkH));
      DBG(fprintf(stderr,"EndMark at L:%lu  h:%lu\n",loop->lMarkEndL, loop->lMarkEndH));
      DBG(fprintf(stderr,"Entering OVERDUB/repl/subs state: srcloop is %08x\n", (unsigned)srcloop));
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
   
   switch(nextstate)
   {
      case STATE_PLAY:
	      pLS->fLoopFadeDelta = -1.0f / (xfadeSamples);
	      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
	      pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
	      pLS->wasMuted = false;
	      break;
      case STATE_MUTE:
	      pLS->fLoopFadeDelta = -1.0f / (xfadeSamples);
	      pLS->fPlayFadeDelta = -1.0f / xfadeSamples;
	      pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
	      pLS->wasMuted = true;
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
	      if (loop) {
		      pLS->state = STATE_PLAY;
		      nextstate = STATE_PLAY;
		      pLS->wasMuted = false;
		      if (pLS->fCurrRate > 0)
			      loop->dCurrPos = 0.0;
		      else
			      loop->dCurrPos = loop->lLoopLength - 1;
	      }
	      break;
      case STATE_ONESHOT:
	      // play the loop one_shot mode
	      pLS->fLoopFadeDelta = -1.0f / (xfadeSamples);
	      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
	      if (loop) {
		      DBG(fprintf(stderr,"Starting ONESHOT state\n"));
		      pLS->state = STATE_ONESHOT;
		      if (pLS->fCurrRate > 0)
			      loop->dCurrPos = 0.0;
		      else
			      loop->dCurrPos = loop->lLoopLength - 1;		       
	      }
	      break;
   }

   if (nextstate != -1) {
      DBG(fprintf(stderr,"Entering state %d from %d\n", nextstate, pLS->state));
      pLS->state = nextstate;

   }
   else {
      DBG(fprintf(stderr,"Next state is -1?? Why?\n"));
      pLS->state = STATE_PLAY;
      pLS->wasMuted = false;
   }
   
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
  unsigned int lpCurrPos = 0;  
  LADSPA_Data * pLoopSample, *spLoopSample;
  long slCurrPos;
  double dDummy;
  int firsttime, backfill;
  int useDelay = 0;
  
  float fPosRatio;
  int xfadeSamples = XFADE_SAMPLES;
  
  SooperLooperI * pLS;
  LoopChunk *loop, *srcloop;

  LADSPA_Data fSyncMode = 0.0f;
  LADSPA_Data fQuantizeMode = 0.0f;
  LADSPA_Data fPlaybackSyncMode = 0.0f;
  
  unsigned long lSampleIndex;

  
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
  

  xfadeSamples = (int) (*pLS->pfXfadeSamples);
  if (xfadeSamples < 1) xfadeSamples = 1;
  
  fTrigThresh = *pLS->pfTrigThresh;

  pLS->bRateCtrlActive = (int) *pLS->pfRateCtrlActive;

  fSyncMode = *pLS->pfSyncMode;

  fPlaybackSyncMode = *pLS->pfPlaybackSyncMode;

  fQuantizeMode = *pLS->pfQuantMode;

  
  eighthPerCycle = (unsigned int) *pLS->pfEighthPerCycle;
  
  fTempo = *pLS->pfTempo;
  if (fTempo > 0.0f) {
	  eighthSamples = (unsigned int) (pLS->fSampleRate * 30.0 / fTempo);

	  if (fQuantizeMode == QUANT_CYCLE || fQuantizeMode == QUANT_LOOP) {
		  syncSamples = (unsigned int) eighthSamples * eighthPerCycle / 2;
	  }
	  else if (fQuantizeMode == QUANT_8TH) {
		  syncSamples = eighthSamples / 2;
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
     pLS->lLastMultiCtrl = lMultiCtrl;
  }

  // force use delay
  if (lMultiCtrl == MULTI_REDO && *pLS->pfRedoTapMode != 0)
  {
     useDelay = 1;
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
	DBG(fprintf(stderr, "Tap triggered\n"));
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
  
     
  // transitions due to control triggering
  
  if (lMultiCtrl >= 0 && lMultiCtrl <= 127)
  {
     // fprintf(stderr, "Multictrl val is %ld\n", lMultiCtrl);

     //lMultiCtrl = lMultiCtrl;

     // change the value if necessary
     if (pLS->state == STATE_MUTE) {
	switch (lMultiCtrl) {
	   case MULTI_REDO:
	      lMultiCtrl = MULTI_REDO_TOG;
	      break;
	   case MULTI_REPLACE:
		   //lMultiCtrl = MULTI_QUANT_TOG;
	      break;
	   case MULTI_REVERSE:
		   //lMultiCtrl = MULTI_ROUND_TOG;
	      break;
	}
     }

     if (lMultiCtrl==MULTI_REDO && useDelay) {
	lMultiCtrl = MULTI_DELAY;
     }
     
     switch(lMultiCtrl)
     {
	case MULTI_RECORD:
	{
	   
	   switch(pLS->state) {
	      case STATE_RECORD:
		      
		      if (fSyncMode == 0.0f && (fTrigThresh==0.0f)) {
			      // skip trig stop
			      pLS->state = STATE_PLAY;
			      pLS->wasMuted = false;
			      pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
			      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
		      }
		      else {
			      pLS->state = STATE_TRIG_STOP;
			      pLS->nextState = STATE_PLAY;
			      DBG(fprintf(stderr,"Entering TRIG_STOP state\n"));
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
			      
		    pLS->state = STATE_PLAY;
		    pLS->wasMuted = false;
		    DBG(fprintf(stderr,"Entering PLAY state after Multiply NEW loop\n"));

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
		    
		    pLS->state = STATE_PLAY;
		    pLS->wasMuted = false;
		    DBG(fprintf(stderr,"Entering PLAY state after Multiply NEW loop\n"));

		 }
		 break;
		 
	      case STATE_DELAY:
		 // goes back to play mode
		 // pop the delay loop off....
		 if (loop) {
		    undoLoop(pLS);
		 }
		 
		 pLS->state = STATE_TRIG_START;
		 DBG(fprintf(stderr,"Entering TRIG_START state from DELAY\n"));
		 break;
		 
	      default:
		 pLS->state = STATE_TRIG_START;
		 DBG(fprintf(stderr,"Entering TRIG_START state\n"));
	   }
	} break;

	case MULTI_OVERDUB:
	{
	   switch(pLS->state) {
	      case STATE_OVERDUB:
		      // don't sync overdub ops
		      pLS->state = STATE_PLAY;
		      pLS->wasMuted = false;
		      DBG(fprintf(stderr,"Entering PLAY state\n"));
		      pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
		      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
		      pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
		      
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
		 
		 
	      case STATE_DELAY:
		 // goes back to overdub mode
		 // pop the delay loop off....
		 if (loop) {
		    undoLoop(pLS);
		    loop = pLS->headLoopChunk;
		 }
		 // continue through to default

	   case STATE_RECORD:
	   case STATE_TRIG_STOP:
		   // lets sync on ending record with overdub
		   if (fSyncMode == 0.0f) {
			   if (loop) {
				   loop = beginOverdub(pLS, loop);
				   if (loop)
					   srcloop = loop->srcloop;
				   else
					   srcloop = NULL;
			   }
		   } else {
			   DBG(fprintf(stderr, "starting syncwait for overdub:  %f\n", fSyncMode));
			   pLS->state = STATE_TRIG_STOP;
			   pLS->nextState = STATE_OVERDUB;
			   pLS->waitingForSync = 1;
		   }
		   
		   break;
	   default:
		   if (loop) {
			   loop = beginOverdub(pLS, loop);
			   if (loop)
				   srcloop = loop->srcloop;
			   else
				   srcloop = NULL;
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
					      
					      DBG(fprintf(stderr, "waiting for sync multi end\n"));
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
		    undoLoop(pLS);
		    loop = pLS->headLoopChunk;
		 }
		 // continue through to default

	      default:
		      if (fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF) {
			      if (loop) {
				      loop = beginMultiply(pLS, loop);
				      if (loop)
					      srcloop = loop->srcloop;
				      else
					      srcloop = NULL;
			      }

			      if (fQuantizeMode == QUANT_OFF) {
				      // then send out a sync here for any slaves
				      pfSyncOutput[0] = 1.0f;
			      }

		      } else {
			      if (pLS->state == STATE_RECORD) {
				      pLS->state = STATE_TRIG_STOP;
			      }
			      
			      DBG(fprintf(stderr, "starting syncwait for multiply\n"));
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
		       loop->dCurrPos = 0.0;
		    else
		       loop->dCurrPos = loop->lLoopLength - 1;		       
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
		    undoLoop(pLS);
		    loop = pLS->headLoopChunk;
		 }
		 // continue through to default

	      default:
		 // make new loop chunk
		      if (fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF) {
			      if (loop)
			      {
				      loop = beginInsert(pLS, loop);
				      if (loop) srcloop = loop->srcloop;
				      else srcloop = NULL;
			      }
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
		      if (fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF) {
			      pLS->state = STATE_PLAY;
			      pLS->wasMuted = false;
			      DBG(fprintf(stderr,"Entering PLAY state\n"));
			      pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
			      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
			      pLS->fFeedFadeDelta = 1.0f / xfadeSamples;

			      if (fQuantizeMode == QUANT_OFF) {
				      // then send out a sync here for any slaves
				      pfSyncOutput[0] = 1.0f;
			      }
			      
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
		      if (fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF) {
			      if (loop)
			      {
				      DBG(fprintf(stderr, "starting replace immediately\n"));
				      loop = beginReplace(pLS, loop);
				      if (loop) srcloop = loop->srcloop;
				      else srcloop = NULL;
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
		      if (fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF) {
			      pLS->state = STATE_PLAY;
			      pLS->wasMuted = false;
			      DBG(fprintf(stderr,"Entering PLAY state\n"));
			      pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
			      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
			      pLS->fFeedFadeDelta = 1.0f / xfadeSamples;

			      if (fQuantizeMode == QUANT_OFF) {
				      // then send out a sync here for any slaves
				      pfSyncOutput[0] = 1.0f;
			      }
			      
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
		      if (fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF) {
			      if (loop)
			      {
				      DBG(fprintf(stderr, "starting subst immediately\n"));
				      loop = beginSubstitute(pLS, loop);
				      if (loop) srcloop = loop->srcloop;
				      else srcloop = NULL;
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
	{
	   switch(pLS->state) {
	      case STATE_MUTE:
		      // reset for audio ramp
		      //pLS->lRampSamples = xfadeSamples;
	       case STATE_ONESHOT:
		 // this enters play mode but from the continuous position
		 pLS->state = STATE_PLAY;
		 pLS->wasMuted = false;
		 DBG(fprintf(stderr,"Entering PLAY state continuous\n"));
		 pLS->fPlayFadeDelta = 1.0f / xfadeSamples;

		 break;

	      case STATE_MULTIPLY:
		 if (loop) {
		    loop = endMultiply(pLS, loop, STATE_MUTE);
		 }
		 break;


	      case STATE_INSERT:
		 if (loop) {
		    loop = endInsert(pLS, loop, STATE_MUTE);
		 }
		 break;

		 
	      case STATE_DELAY:
		 // pop the delay loop off....
		 if (loop) {
		    undoLoop(pLS);
		    loop = pLS->headLoopChunk;
		 }
		 // continue through to default
		 
	      default:
		 pLS->state = STATE_MUTE;
		 DBG(fprintf(stderr,"Entering MUTE state\n"));
		 // reset for audio ramp
		 //pLS->lRampSamples = xfadeSamples;
		 pLS->fPlayFadeDelta = -1.0f / xfadeSamples;

		 pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
		 pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
		 pLS->wasMuted = true;

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
		    loop->dCurrPos = 0.0f;
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
		    pLS->state = STATE_ONESHOT;
		    if (pLS->fCurrRate > 0)
		       loop->dCurrPos = 0.0f;
		    else
		       loop->dCurrPos = loop->lLoopLength - 1;
		    
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
		      // POP the head off and start the previous
		      // one at the same position if possible
		      
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
		      else if (loop) {
			      undoLoop(pLS);
		      }
		      
		      // cancel whatever mode, back to play mode
		      pLS->state = pLS->wasMuted ? STATE_MUTE : STATE_PLAY;
		      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
		      pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
		      pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
		      DBG(fprintf(stderr,"Undoing and reentering PLAY state from UNDO\n"));
		      break;
		      
		      
	      case STATE_MUTE:
		 // undo ALL)
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
		      else {
			      clearLoopChunks(pLS);
			      DBG(fprintf(stderr,"UNDO all loops\n"));
		      }
		 break;
		 
	   }
	   
	}break;

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

		 // immediately redo last if possible
		 redoLoop(pLS);

		 pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
		 pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
		 pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
		 
		 pLS->state = pLS->wasMuted ? STATE_MUTE : STATE_PLAY;
		 DBG(fprintf(stderr,"Entering PLAY state from REDO\n"));
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
		      pLS->wasMuted = false;
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
		      if (fSyncMode == 0.0f) {
			      if (loop) {
				      DBG(fprintf(stderr,"Starting ONESHOT state\n"));
				      pLS->fPlayFadeDelta = 1.0f / xfadeSamples;
				      pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
				      pLS->fFeedFadeDelta = 1.0f / xfadeSamples;

				      pLS->state = STATE_ONESHOT;
				      if (pLS->fCurrRate > 0)
					      loop->dCurrPos = 0.0;
				      else
					      loop->dCurrPos = loop->lLoopLength - 1;		       
			      }

			      // then send out a sync here for any slaves
			      pfSyncOutput[0] = 1.0f;

		      }	else {
			      if (pLS->state == STATE_RECORD) {
				      pLS->state = STATE_TRIG_STOP;
			      }
			      DBG(fprintf(stderr, "starting syncwait for ONESHOT\n"));
			      pLS->nextState = STATE_ONESHOT;
			      pLS->waitingForSync = 1;
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
		      if (fSyncMode == 0.0f) {

			      transitionToNext (pLS, loop, STATE_TRIGGER_PLAY);

			      // then send out a sync here for any slaves
			      pfSyncOutput[0] = 1.0f;

		      }	else {
			      if (pLS->state == STATE_RECORD) {
				      pLS->state = STATE_TRIG_STOP;
			      }
			      DBG(fprintf(stderr, "starting syncwait for trigger\n"));
			      pLS->nextState = STATE_TRIGGER_PLAY;
			      pLS->waitingForSync = 1;
		      }
		      break;
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
	      
	      fInputSample = pfInput[lSampleIndex];
	      if ((fSyncMode == 0.0f && ((fInputSample > fTrigThresh) || (fTrigThresh==0.0f)))
		  || (fSyncMode == 2.0f) // relative sync offset mode 
		  || (fSyncMode > 0.0f && pfSyncInput[lSampleIndex] != 0.0f))
	      {
		 
		 loop = pushNewLoopChunk(pLS, 0, NULL);
		 if (loop) {
		    DBG(fprintf(stderr,"Entering RECORD state\n"));
		 
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
			    //cerr << "sync offset is: " << loop->lSyncOffset << endl;
		    }
		    
		    // cause input-to-loop fade in
		    pLS->fLoopFadeDelta = 1.0f / xfadeSamples;
		    //pLS->fPlayFadeDelta = -1.0f / xfadeSamples;

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
		   DBG(fprintf(stderr, "Entering PLAY state -- END of memory asdhsd! %08x\n",
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
	      
// 	      if (pLS->waitingForSync && (fSyncMode == 0.0 || pfSyncInput[lSampleIndex] != 0.0))
// 	      {
// 		      DBG(fprintf(stderr,"Finishing synced record\n"));
// 		      pLS->state = pLS->nextState;
// 		      pLS->nextState = -1;
// 		      pLS->waitingForSync = 0;
// 		      break;
// 	      }

	      if (fSyncMode == 2.0f) {
		      pLS->lSamplesSinceSync++;

		      if (pfSyncInput[lSampleIndex] != 0.0f) {
			      pLS->lSamplesSinceSync = 0;
			      //cerr << "rec reseting sync" << endl;
		      }
	      }
	      
	      // wrap at the proper loop end
	      lCurrPos = (unsigned int)loop->dCurrPos;
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
	   lCurrPos = ((unsigned int)loop->dCurrPos);
	   loop->lLoopLength = (unsigned long) lCurrPos;
	   loop->lCycleLength = loop->lLoopLength;

	   
	} break;

	case STATE_TRIG_STOP:
	{
	   //fprintf(stderr,"in trigstop\n");	   
	   // play the input out.  Keep recording until we go
	   // above the threshold, then go into next state.

	   loop = ensureLoopSpace (pLS, loop, SampleCount - lSampleIndex, NULL);
		
	   for (;lSampleIndex < SampleCount;
		lSampleIndex++)
	   {
	      fWet += wetDelta;
              fDry += dryDelta;
	      fFeedback += feedbackDelta;
	      fScratchPos += scratchDelta;
	      pLS->fLoopFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopFadeAtten + pLS->fLoopFadeDelta);
		   
	      lCurrPos = (unsigned int) loop->dCurrPos;
	      pLoopSample = & pLS->pSampleBuf[(loop->lLoopStart + lCurrPos) & pLS->lBufferSizeMask];
	      
	      fInputSample = pfInput[lSampleIndex];
	      
	      
// 	      if ((fSyncMode == 0.0f && ((fInputSample > fTrigThresh) || (fTrigThresh==0.0)))
// 		  || (fSyncMode > 0.0f && pfSyncInput[lSampleIndex] != 0.0))

	      
	      // exit immediately if syncmode is off, or we have a sync
	      if ((fSyncMode == 0.0f)
 		  || (fSyncMode == 1.0f && pfSyncInput[lSampleIndex] != 0.0f)
		  || (fSyncMode == 2.0f && pLS->lSamplesSinceSync == loop->lSyncOffset))
	      {
		      DBG(fprintf(stderr,"Entering %d state at %u\n", pLS->nextState, lCurrPos));
		 //pLS->state = pLS->nextState;
		 // reset for audio ramp
		 //pLS->lRampSamples = xfadeSamples;
		 //loop->dCurrPos = 0.0f;

		      if (fSyncMode == 2.0f) {
			      //cerr << "ending recstop sync2d: " << lCurrPos << endl;
			      pLS->recSyncEnded = true;
		      }
		      else {
			      //cerr << "ending recstop sync1: " << lCurrPos << endl;
		      }
		      
		      loop = transitionToNext (pLS, loop, pLS->nextState);
		      pLS->waitingForSync = 0;
		      break;
	      }

	      if (fSyncMode == 2.0f) {
		      pLS->lSamplesSinceSync++;

		      if (pfSyncInput[lSampleIndex] != 0.0f) {
			      pLS->lSamplesSinceSync = 0;
			      //cerr << "ts reseting sync" << endl;
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
	   if (loop &&  loop->lLoopLength)
	   {
	      
	      for (;lSampleIndex < SampleCount;
		   lSampleIndex++)
	      {
	         fWet += wetDelta;
                 fDry += dryDelta;
	         fFeedback += feedbackDelta;
	         fScratchPos += scratchDelta;

		 pLS->fPlayFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fPlayFadeAtten + pLS->fPlayFadeDelta);
		 pLS->fLoopFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopFadeAtten + pLS->fLoopFadeDelta);
		 pLS->fFeedFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fFeedFadeAtten + pLS->fFeedFadeDelta);
		 

		 lCurrPos =(unsigned int) fmod(loop->dCurrPos, loop->lLoopLength);
		 pLoopSample = & pLS->pSampleBuf[(loop->lLoopStart + lCurrPos) & pLS->lBufferSizeMask];

		 
		 if (fSyncMode != 0) {
			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];

			 if (fSyncMode == 2.0f) {
				 pLS->lSamplesSinceSync++;
				 
				 if (pfSyncInput[lSampleIndex] != 0.0f) {
					 // cerr << "od sync reset" << endl;
					 pLS->lSamplesSinceSync = 0;
				 }
				 else if (pLS->recSyncEnded) {
					 // we just synced, need to noitfy slave... this could be problem
					 pfSyncOutput[lSampleIndex] = 1.0f;
					 //cerr << "od notified" << endl;
					 pLS->recSyncEnded = false;
				 }
			 }
			 
		 }
		 else {
			 if (fQuantizeMode == QUANT_OFF || (fQuantizeMode == QUANT_CYCLE && (lCurrPos % loop->lCycleLength) == 0)
			     || (fQuantizeMode == QUANT_LOOP && (lCurrPos == 0)) || (fQuantizeMode == QUANT_8TH && (lCurrPos % eighthSamples) == 0))
			 {
				 pfSyncOutput[lSampleIndex] = 1.0f;
			 }
		 }
		 
		 if (pLS->waitingForSync && ((fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF) || pfSyncInput[lSampleIndex] != 0.0f))
		 {
			 DBG(fprintf(stderr,"Finishing synced overdub/replace/subs\n"));
			 loop = transitionToNext (pLS, loop, pLS->nextState);
			 pLS->nextState = -1;
			 pLS->waitingForSync = 0;
			 break;
		 }

		 
		 
		 fInputSample = pfInput[lSampleIndex];
		 
		 fillLoops(pLS, loop, lCurrPos);

		 switch(pLS->state)
		 {
		 case STATE_OVERDUB:
			 // use our self as the source (we have been filled by the call above)
			 fOutputSample = fWet  *  *(pLoopSample)
				 + fDry * fInputSample;
			 
			 *(pLoopSample) =  
				 ((pLS->fLoopFadeAtten * fInputSample) + (0.96f * pLS->fFeedFadeAtten * fFeedback *  *(pLoopSample)));
			 break;
		 case STATE_REPLACE:
			 // state REPLACE use only the new input
			 // use our self as the source (we have been filled by the call above)
			 fOutputSample = pLS->fPlayFadeAtten * fWet  *  *(pLoopSample)
				 + fDry * fInputSample;
			 
			 *(pLoopSample) = fInputSample * pLS->fLoopFadeAtten +  (pLS->fFeedFadeAtten * fFeedback *  *(pLoopSample));
			 break;
		 case STATE_SUBSTITUTE:
		 default:
			 // use our self as the source (we have been filled by the call above)
			 // hear the loop
			 fOutputSample = fWet  *  *(pLoopSample)
				 + fDry * fInputSample;

			 // but not feed it back (xfade it really)
			 *(pLoopSample) = fInputSample * pLS->fLoopFadeAtten + (pLS->fFeedFadeAtten * fFeedback *  *(pLoopSample));
			 break;
		 }
		 
		 pfOutput[lSampleIndex] = fOutputSample;
		 

		 // increment and wrap at the proper loop end
		 loop->dCurrPos = loop->dCurrPos + fRate;

		 //if (fSyncMode != 0.0 && pLS->fNextCurrRate != 0 && pfSyncInput[lSampleIndex] != 0.0) {
		 if (pLS->fNextCurrRate != 0 && pfSyncInput[lSampleIndex] != 0.0f) {
		       // commit the new rate at boundary (quantized)
		       pLS->fCurrRate = pLS->fNextCurrRate;
		       pLS->fNextCurrRate = 0.0f;
		       DBG(fprintf(stderr, "%08x Starting quantized rate change ovr at %d\n", (unsigned) pLS, lCurrPos));
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
		       DBG(fprintf(stderr, "Starting quantized rate change\n"));
		    }

		 }
		 else if (loop->dCurrPos >= loop->lLoopLength) {
		    // wrap around length
		    loop->dCurrPos = fmod(loop->dCurrPos, loop->lLoopLength);
		    if (pLS->fNextCurrRate != 0) {
		       // commit the new rate at boundary (quantized)
		       pLS->fCurrRate = pLS->fNextCurrRate;
		       pLS->fNextCurrRate = 0.0f;
		       DBG(fprintf(stderr, "Starting quantized rate change\n"));
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
			      undoLoop(pLS);
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
		 spLoopSample = & pLS->pSampleBuf[(srcloop->lLoopStart + lpCurrPos) & pLS->lBufferSizeMask];
		 pLoopSample = & pLS->pSampleBuf[(loop->lLoopStart + slCurrPos) & pLS->lBufferSizeMask];

		 pLS->fLoopFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopFadeAtten + pLS->fLoopFadeDelta);
		 pLS->fFeedFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fFeedFadeAtten + pLS->fFeedFadeDelta);


		 
		 if (fSyncMode != 0) {
			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];

			 if (fSyncMode == 2.0f) {
				 pLS->lSamplesSinceSync++;
				 
				 if (pfSyncInput[lSampleIndex] != 0.0f) {
					 // cerr << "mt sync reset" << endl;
					 pLS->lSamplesSinceSync = 0;
				 }
				 else if (pLS->recSyncEnded) {
					 // we just synced, need to noitfy slave... this could be problem
					 pfSyncOutput[lSampleIndex] = 1.0f;
					 //cerr << "mt notified: " <<  endl;
					 pLS->recSyncEnded = false;
				 }
			 }
			 
		 }
		 else {
			 if (fQuantizeMode == QUANT_OFF || (fQuantizeMode == QUANT_CYCLE && (slCurrPos % loop->lCycleLength) == 0))
			 {
				 pfSyncOutput[lSampleIndex] = 1.0f;
			 }
		 }
		 
		 
		 if (pLS->waitingForSync && (fSyncMode == 0.0f || pfSyncInput[lSampleIndex] != 0.0f || slCurrPos  >= (long)(loop->lLoopLength)))
		 {
			 DBG(fprintf(stderr,"Finishing synced multiply\n"));
			 loop = endMultiply (pLS, loop, pLS->nextState);

			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];
			 
			 pLS->waitingForSync = 0;
			 break;
		 }

		 
		 
		 fillLoops(pLS, loop, lpCurrPos);
		 
		 fInputSample = pfInput[lSampleIndex];


		 
		 
		 // always use the source loop as the source
		 
		 fOutputSample = (fWet *  (*spLoopSample)
				  + fDry * fInputSample);


		 if (slCurrPos < 0) {
		    // this is part of the loop that we need to ignore
		    // fprintf(stderr, "Ignoring at %ul\n", lCurrPos);
		 }
		 else if ((loop->lCycles <=1 && fQuantizeMode != 0)) {
			 // do not include the new input
			 *(pLoopSample)
				 = pLS->fFeedFadeAtten * fFeedback *  (*spLoopSample);

		 }
		 if ((slCurrPos > (long) loop->lMarkEndL &&  *pLS->pfRoundMode == 0)) {
			 // do not include the new input (at end) when not rounding
			 pLS->fLoopFadeDelta = -1.0f / xfadeSamples;
			 
			 *(pLoopSample)
				 = (pLS->fFeedFadeAtten * fFeedback *  (*spLoopSample)) +  (pLS->fLoopFadeAtten * fInputSample);
			 // fprintf(stderr, "Not including input at %ul\n", lCurrPos);
		 }
		 else {
			 pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
	      
			 *(pLoopSample)
				 = ( (pLS->fLoopFadeAtten * fInputSample) + (pLS->fFeedFadeAtten * 0.96f *  fFeedback * (*spLoopSample)));
		 }
		 
		 pfOutput[lSampleIndex] = fOutputSample;

		 
		 // increment 
		 loop->dCurrPos = loop->dCurrPos + fRate;
	      

		 // ASSUMPTION: our rate is +1 only		 
		 if ((unsigned long)loop->dCurrPos  > (loop->lLoopLength)) {


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
			 DBG(fprintf(stderr,"Multiply added cycle %lu  at %g\n", loop->lCycles, loop->dCurrPos));
			 
			 // now we set this to rise in case we were quantized
			 pLS->fLoopFadeDelta = 1.0f / xfadeSamples;
			 pLS->fFeedFadeDelta = 1.0f / xfadeSamples;
			 
			 loop = ensureLoopSpace (pLS, loop, SampleCount - lSampleIndex, NULL);
			 if (!loop) {
				 // out of space! give up for now!
				 // undo!
				 pLS->state = STATE_PLAY;
				 pLS->wasMuted = false;
				 undoLoop(pLS);
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
		 pLS->fLoopFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fLoopFadeAtten + pLS->fLoopFadeDelta);
		 pLS->fFeedFadeAtten = LIMIT_BETWEEN_0_AND_1 (pLS->fFeedFadeAtten + pLS->fFeedFadeDelta);
		 
		 lpCurrPos =(unsigned int) fmod(loop->dCurrPos, srcloop->lLoopLength);
		 lCurrPos =(unsigned int) loop->dCurrPos;
		 spLoopSample = & pLS->pSampleBuf[(srcloop->lLoopStart + lpCurrPos) & pLS->lBufferSizeMask];
		 pLoopSample = & pLS->pSampleBuf[(loop->lLoopStart + lCurrPos) & pLS->lBufferSizeMask];

		 fillLoops(pLS, loop, lCurrPos);
		 
		 fInputSample = pfInput[lSampleIndex];

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

		    *(pLoopSample) = fInputSample * pLS->fLoopFadeAtten;

		 }
		 else {
		    // just the input we are now inserting
		    pLS->fLoopFadeDelta = 1.0f / xfadeSamples;
		    pLS->fFeedFadeDelta = -1.0f / xfadeSamples;
		    pLS->fPlayFadeDelta = -1.0f / xfadeSamples;

		    fOutputSample = fDry * fInputSample  + (pLS->fPlayFadeAtten * fWet *  (*spLoopSample));

		    *(pLoopSample) = (fInputSample * pLS->fLoopFadeAtten) + (pLS->fFeedFadeAtten * fFeedback *  (*pLoopSample));

		 }
		 
		 
		 pfOutput[lSampleIndex] = fOutputSample;

		 if (fSyncMode != 0) {
			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];

			 if (fSyncMode == 2.0f) {
				 pLS->lSamplesSinceSync++;
				 
				 if (pfSyncInput[lSampleIndex] != 0.0f) {
					 //cerr << "ins sync reset" << endl;
					 pLS->lSamplesSinceSync = 0;
				 }
				 else if (pLS->recSyncEnded) {
					 // we just synced, need to noitfy slave... this could be problem
					 pfSyncOutput[lSampleIndex] = 1.0f;
					 //cerr << "ins notified" << endl;
					 pLS->recSyncEnded = false;
				 }
			 }
		 }
		 else if (fQuantizeMode == QUANT_OFF || (fQuantizeMode == QUANT_CYCLE && (lCurrPos % loop->lCycleLength) == 0)
			  || (fQuantizeMode == QUANT_LOOP && (lCurrPos == 0)) || (fQuantizeMode == QUANT_8TH && (lCurrPos % eighthSamples) == 0)) {
			 pfSyncOutput[lSampleIndex] = 1.0f;
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
		    
		    DBG(fprintf(stderr, "Looplength = %lu   cycles=%lu\n", loop->lLoopLength, loop->lCycles));
		    
		    loop = transitionToNext(pLS, loop, pLS->nextState);
		    DBG(fprintf(stderr,"Entering state %d from insert\n", pLS->state));
		    break;
		 }

		 // ASSUMPTION: our rate is +1 only		 
		 if (firsttime && (lCurrPos % loop->lCycleLength) == 0)
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

	
	
	case STATE_PLAY:
	case STATE_ONESHOT:
	case STATE_SCRATCH:
	case STATE_MUTE:
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


		 if (fSyncMode != 0.0f) {
			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];

			 if (fSyncMode == 2.0f) {
				 pLS->lSamplesSinceSync++;
				 
				 if (pfSyncInput[lSampleIndex] != 0.0f) {
					 //cerr << " sync reset: " << pLS << endl;
					 pLS->lSamplesSinceSync = 0;
				 }
				 else if (pLS->recSyncEnded) {
					 // we just synced, need to noitfy slave... this could be problem
					 pfSyncOutput[lSampleIndex] = 1.0f;
					 //cerr << "notified" << endl;
					 pLS->recSyncEnded = false;
				 }
			 }
		 }
		 else if (fQuantizeMode == QUANT_OFF || (fQuantizeMode == QUANT_CYCLE && (lCurrPos % loop->lCycleLength) == 0)
			  || (fQuantizeMode == QUANT_LOOP && (lCurrPos == 0)) || (fQuantizeMode == QUANT_8TH && (lCurrPos % eighthSamples) == 0)) {
			 //DBG(fprintf(stderr, "outputtuing sync at %d\n", lCurrPos));
			 pfSyncOutput[lSampleIndex] = 1.0f;
		 }


 		 if (pLS->fNextCurrRate != 0 && pfSyncInput[lSampleIndex] != 0.0f && fTempo > 0.0f) {
 		       // commit the new rate at boundary (quantized)
 		       pLS->fCurrRate = pLS->fNextCurrRate;
 		       pLS->fNextCurrRate = 0.0f;
 		       DBG(fprintf(stderr, "%08x Starting quantized rate change at %d\n", (unsigned) pLS, lCurrPos));
 		 }
		 
		 // test playback sync

		 if (syncSamples && fPlaybackSyncMode != 0.0f && fQuantizeMode != QUANT_OFF && !pLS->donePlaySync
		     && ( pfSyncInput[lSampleIndex] != 0.0f)
		     && ((lCurrPos + syncSamples) >= loop->lLoopLength
			 || (lCurrPos > 0 && lCurrPos < (unsigned int) lrintf(syncSamples) )))
		 {
			 //cerr << "PLAYBACK SYNC hit at " << lCurrPos << endl;
			 //pLS->waitingForSync = 1;
			 pLS->donePlaySync = true;
			 if (pLS->fCurrRate > 0)
				 loop->dCurrPos = 0.0;
			 else
				 loop->dCurrPos = loop->lLoopLength - 1;

			 pfSyncOutput[lSampleIndex] = 1.0f;
		 }
		 
		 
		 if (pLS->waitingForSync && ((fSyncMode == 0.0f && pfSyncOutput[lSampleIndex] != 0.0f) || pfSyncInput[lSampleIndex] != 0.0f))
		 {
			 DBG(fprintf(stderr, "transition to next at: %lu:  %g\n", lSampleIndex, loop->dCurrPos));
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

		 
		 // fill loops if necessary
		 fillLoops(pLS, loop, lCurrPos);


		 
		 fInputSample = pfInput[lSampleIndex];


		 
		 fOutputSample =   tmpWet *  (*pLoopSample)
		    + fDry * fInputSample;


		 // we might add a bit from the input still during xfadeout
		 *(pLoopSample) = ((*pLoopSample) * pLS->fFeedFadeAtten) +  pLS->fLoopFadeAtten * fInputSample;

		 // optionally support feedback during playback
		 if (useFeedbackPlay) {
			 *(pLoopSample) *= fFeedback * pLS->fFeedFadeAtten;
		 }
		 
		 
		 // increment and wrap at the proper loop end
		 loop->dCurrPos = loop->dCurrPos + fRate;

		 pfOutput[lSampleIndex] = fOutputSample;
		 

 		 if (pLS->fNextCurrRate != 0 && pfSyncInput[lSampleIndex] != 0.0f && fTempo > 0.0f) {
 		       // commit the new rate at boundary (quantized)
 		       pLS->fCurrRate = pLS->fNextCurrRate;
 		       pLS->fNextCurrRate = 0.0f;
 		       DBG(fprintf(stderr, "%08x Starting quantized rate change at %d\n", (unsigned) pLS, lCurrPos));
 		 }
		 
		 if (loop->dCurrPos >= loop->lLoopLength) {
		    if (pLS->state == STATE_ONESHOT) {
		       // done with one shot
		       DBG(fprintf(stderr, "finished ONESHOT\n"));
		       pLS->state = STATE_MUTE;
		       pLS->fPlayFadeDelta = -1.0f / xfadeSamples;

		       //pLS->lRampSamples = xfadeSamples;
		       //fWet = 0.0;
		    }

		    if (pLS->fNextCurrRate != 0 && pfSyncInput[lSampleIndex] != 0.0f) {
			    // commit the new rate at boundary (quantized)
			    pLS->fCurrRate = pLS->fNextCurrRate;
			    pLS->fNextCurrRate = 0.0f;
			    DBG(fprintf(stderr, "%08x 2 Starting quantized rate change at %d\n", (unsigned) pLS, lCurrPos));
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
		       DBG(fprintf(stderr, "finished ONESHOT neg\n"));
		       pLS->state = STATE_MUTE;
		       //fWet = 0.0;
		       pLS->fPlayFadeDelta = -1.0f / xfadeSamples;
		       //pLS->lRampSamples = xfadeSamples;
		    }

		    if (pLS->fNextCurrRate != 0 && pfSyncInput[lSampleIndex] != 0.0f) {
		       // commit the new rate at boundary (quantized)
		       pLS->fCurrRate = pLS->fNextCurrRate;
		       pLS->fNextCurrRate = 0.0f;
		       DBG(fprintf(stderr, "%08x 3 Starting quantized rate change at %d\n", (unsigned) pLS, lCurrPos));
		    }

		    pLS->donePlaySync = false;

		 }


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
		 else if ((lCurrPos % loop->lCycleLength) == 0) {
			 pfSyncOutput[lSampleIndex] = 1.0f;
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

	if (fSyncMode == 2.0f) {
		pLS->lSamplesSinceSync++;
		if (pfSyncInput[lSampleIndex] != 0.0f) {
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

     if (pLS->pfStateOut && pLS->state != STATE_MUTE && pLS->state != STATE_TRIG_START)
	*pLS->pfStateOut = (LADSPA_Data) STATE_OFF;

  }
  
  
}

/*****************************************************************************/

/* Throw away a simple delay line. */
void 
cleanupSooperLooper(LADSPA_Handle Instance) {

  SooperLooperI * pLS;

  pLS = (SooperLooperI *)Instance;

  free(pLS->pSampleBuf);
  free(pLS);
}

/*****************************************************************************/

LADSPA_Descriptor * g_psDescriptor = NULL;

/*****************************************************************************/

/* sl_init() is called automatically when the plugin library is first
   loaded. */
void 
sl_init() {

  char ** pcPortNames;
  LADSPA_PortDescriptor * piPortDescriptors;
  LADSPA_PortRangeHint * psPortRangeHints;

  g_psDescriptor
    = (LADSPA_Descriptor *)malloc(sizeof(LADSPA_Descriptor));
  if (g_psDescriptor) {
    g_psDescriptor->UniqueID
      = 1601;
    g_psDescriptor->Label
      = strdup("SooperLooper");
    g_psDescriptor->Properties
      = LADSPA_PROPERTY_HARD_RT_CAPABLE;
    g_psDescriptor->Name 
      = strdup("SooperLooper");
    g_psDescriptor->Maker
      = strdup("Jesse Chappell");
    g_psDescriptor->Copyright
      = strdup("2002, Jesse Chappell");

    g_psDescriptor->PortCount 
      = PORT_COUNT;
    piPortDescriptors
      = (LADSPA_PortDescriptor *)calloc(PORT_COUNT, sizeof(LADSPA_PortDescriptor));
    g_psDescriptor->PortDescriptors 
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
    g_psDescriptor->PortNames
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
    g_psDescriptor->PortRangeHints
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
    psPortRangeHints[LoopPosition].LowerBound 
      = 0.0;
    
    
    g_psDescriptor->instantiate
      = instantiateSooperLooper;
    g_psDescriptor->connect_port 
      = connectPortToSooperLooper;
    g_psDescriptor->activate
      = activateSooperLooper;
    g_psDescriptor->run 
      = runSooperLooper;
    g_psDescriptor->run_adding
      = NULL;
    g_psDescriptor->set_run_adding_gain
      = NULL;
    g_psDescriptor->deactivate
      = NULL;
    g_psDescriptor->cleanup
      = cleanupSooperLooper;
  }
}

/*****************************************************************************/

/* _fini() is called automatically when the library is unloaded. */
void 
sl_fini() {
  unsigned long lIndex;
  if (g_psDescriptor) {
    free((char *)g_psDescriptor->Label);
    free((char *)g_psDescriptor->Name);
    free((char *)g_psDescriptor->Maker);
    free((char *)g_psDescriptor->Copyright);
    free((LADSPA_PortDescriptor *)g_psDescriptor->PortDescriptors);
    for (lIndex = 0; lIndex < g_psDescriptor->PortCount; lIndex++)
      free((char *)(g_psDescriptor->PortNames[lIndex]));
    free((char **)g_psDescriptor->PortNames);
    free((LADSPA_PortRangeHint *)g_psDescriptor->PortRangeHints);
    free(g_psDescriptor);
  }
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
