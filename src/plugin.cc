/* SooperLooper.c  :  
   Copyright (C) 2002 Jesse Chappell <jesse@essej.net>

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


   This LADSPA plugin provides an Echoplex like realtime sampling
   looper.  Plus some extra features.

   There is a fixed maximum sample memory.  The featureset is derived
   from the Gibson-Oberheim Echoplex Digital Pro.


Example ecasound call assuming the following MIDI controllers on channel 0:
  CC0 - footswitch for setting tap tempo
  CC2 - Wet level
  CC3 - Feedback (and Rate for scratch mode)
  CC5 - Scratch position
  MIDI Program changes for MultiControl triggering.

ecasound -r -X -z:nointbuf -z:noxruns -z:nodb -z:psr -f:s16_le,1,44100 -i:/dev/dsp -f:s16_le,1,44100 -o:/dev/dsp -b:128 -el:SooperLooper,0.1,1,1,1,1,0,-1,0,0,0,0,0  -km:3,1,0,2,0 -km:5,0,2,3,0 -km:6,-1,1,5,0 -km:7,0,127,32,0,1 -km:4,1,0,3,0 -km:8,0,1,0,0
   
*/
   
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <values.h>
#include <string.h>
#include <math.h>

/*****************************************************************************/

#include "ladspa.h"

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
#define SAMPLE_MEMORY 200.0
#endif

#define XFADE_SAMPLES 512

// settle time for tap trigger (trigger if two changes
// happen within at least X samples)
//#define TRIG_SETTLE  4410
#define TRIG_SETTLE  2205  

/*****************************************************************************/

/* The port numbers for the plugin: */

#define PORT_COUNT  26

#define SDL_THRESH       0
#define SDL_DRY          1
#define SDL_WET          2
#define SDL_FEEDBACK     3
#define SDL_RATE         4
#define SDL_SCRATCH_POS  5
#define SDL_MULTICTRL    6
#define SDL_TAP_TRIG     7
#define SDL_MULTITENS    8
#define SDL_QUANTMODE    9
#define SDL_ROUNDMODE    10
#define SDL_REDOTAPMODE  11
#define SDL_SYNCMODE     12
#define SDL_USERATE      13
#define SDL_XFADESAMPLES 14


// control outs
#define SDL_STATE_OUT     15
#define SDL_LOOPLEN_OUT   16
#define SDL_LOOPPOS_OUT   17
#define SDL_CYCLELEN_OUT  18
#define SDL_SECSFREE_OUT  19
#define SDL_SECSTOTAL_OUT 20
#define SDL_WAITING       21

/* audio */
#define SDL_INPUT        22
#define SDL_OUTPUT       23
#define SDL_SYNC_INPUT        24
#define SDL_SYNC_OUTPUT       25



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

enum {
	QUANT_OFF=0,
	QUANT_CYCLE,
	QUANT_8TH,
	QUANT_LOOP
};


/*****************************************************************************/

#define LIMIT_BETWEEN_0_AND_1(x)          \
(((x) < 0) ? 0 : (((x) > 1) ? 1 : (x)))

#define LIMIT_BETWEEN_NEG1_AND_1(x)          \
(((x) < -1) ? -1 : (((x) > 1) ? 1 : (x)))

#define LIMIT_BETWEEN_0_AND_MAX_DELAY(x)  \
(((x) < 0) ? 0 : (((x) > MAX_DELAY) ? MAX_DELAY : (x)))

// Convert a value in dB's to a coefficent
#define DB_CO(g) ((g) > -90.0f ? powf(10.0f, (g) * 0.05f) : 0.0f)
#define CO_DB(v) (20.0f * log10f(v))



/*****************************************************************************/

// defines all a loop needs to know to cycle properly in memory
// one of these will prefix the actual loop data in our buffer memory
typedef struct _LoopChunk {

    /* pointers in buffer memory. */
    LADSPA_Data * pLoopStart;
    LADSPA_Data * pLoopStop;    
    //unsigned long lLoopStart;
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

    int firsttime;
    int frontfill;
    int backfill;
    
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
    char * pSampleBuf;
    
    /* Buffer size, not necessarily a power of two. */
    unsigned long lBufferSize;

    LADSPA_Data fTotalSecs;	
	
    /* the current state of the sampler */
    int state;

    int nextState;

    int waitingForSync;
	
    long lLastMultiCtrl;

    // initial location of params
    LADSPA_Data fQuantizeMode;
    LADSPA_Data fRoundMode;    
    LADSPA_Data fRedoTapMode;
    LADSPA_Data fSyncMode;

    
    // used only when in DELAY mode
    int bHoldMode;

    
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

    /* This specifies which multiple of ten this plugin responds to
     * for the multi-control port.  For instance, if 0 is given we respond
     * to 0-9 on the multi control port, if 1 is given, 10-19.  This allows you
     * to separately control multiple looper instances with the same footpedal,
     * for instance.  Range is 0-12.
     */
    LADSPA_Data * pfMultiTens;
    
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
    LADSPA_Data *pfRateCtrlActive;

    LADSPA_Data *pfXfadeSamples;
	
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
	
} SooperLooper;



// creates a new loop chunk and puts it on the head of the list
// returns the new chunk
static LoopChunk * pushNewLoopChunk(SooperLooper* pLS, unsigned long initLength)
{
   //LoopChunk * loop = malloc(sizeof(LoopChunk));
   LoopChunk * loop;   

   if (pLS->headLoopChunk) {
      // use the next spot in memory
      loop  = (LoopChunk *) pLS->headLoopChunk->pLoopStop;

      if ((char *)((char*)loop + sizeof(LoopChunk) + (initLength * sizeof(LADSPA_Data)))
	  >= (pLS->pSampleBuf + pLS->lBufferSize)) {
	 // out of memory, return NULL
	 DBG(fprintf(stderr, "Error pushing new loop, out of loop memory\n");)
	 return NULL;
      }
      
      loop->prev = pLS->headLoopChunk;
      loop->next = NULL;
      
      loop->prev->next = loop;
      
      // the loop data actually starts directly following this struct
      loop->pLoopStart = (LADSPA_Data *) (loop + sizeof(LoopChunk));

      // the stop will be filled in later
      
      // we are the new head
      pLS->headLoopChunk = loop;
      
   }
   else {
      // first loop on the list!
      loop = (LoopChunk *) pLS->pSampleBuf;
      loop->next = loop->prev = NULL;
      pLS->headLoopChunk = pLS->tailLoopChunk = loop;
      loop->pLoopStart = (LADSPA_Data *) (loop + sizeof(LoopChunk));
   }
   

   DBG(fprintf(stderr, "New head is %08x\n", (unsigned)loop);)

   
   return loop;
}

// pop the head off and free it
static void popHeadLoop(SooperLooper *pLS)
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
      //free(dead);
   }
   else {
      pLS->headLoopChunk = NULL;
      // pLS->tailLoopChunk is still valid to support redo
      // from nothing
   }
}

// clear all LoopChunks (undoAll , can still redo them back)
static void clearLoopChunks(SooperLooper *pLS)
{
   /*
   LoopChunk *prev, *tmp;
   
   prev = pLS->headLoopChunk;
   
   while (prev)
   {
      tmp = prev->prev;
      free(prev);
      prev = tmp;
   }
   */
   
   pLS->headLoopChunk = NULL;
}

void undoLoop(SooperLooper *pLS)
{
   LoopChunk *loop = pLS->headLoopChunk;
   LoopChunk *prevloop;
   
   prevloop = loop->prev;
   if (prevloop && prevloop == loop->srcloop) {
      // if the previous was the source of the one we're undoing
      // pass the dCurrPos along, otherwise leave it be.
      prevloop->dCurrPos = fmod(loop->dCurrPos+loop->lStartAdj, prevloop->lLoopLength);
   }
   
   popHeadLoop(pLS);
   DBG(fprintf(stderr, "Undoing last loop %08x: new head is %08x\n", (unsigned)loop,
	       (unsigned)pLS->headLoopChunk);)
}


void redoLoop(SooperLooper *pLS)
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
      
      if (loop && loop == nextloop->srcloop) {
	 // if the next is using us as a source
	 // pass the dCurrPos along, otherwise leave it be.
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

   SooperLooper * pLS;
   char * sampmem;
   
   // important note: using calloc to zero all data
   pLS = (SooperLooper *) calloc(1, sizeof(SooperLooper));
   
   if (pLS == NULL) 
      return NULL;
   
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
   pLS->lBufferSize = (unsigned long)((LADSPA_Data)SampleRate * pLS->fTotalSecs * sizeof(LADSPA_Data));
   
   pLS->pSampleBuf = (char *) calloc(pLS->lBufferSize, 1);
   if (pLS->pSampleBuf == NULL) {
      free(pLS);
      return NULL;
   }

   /* just one for now */
   //pLS->lLoopStart = 0;
   //pLS->lLoopStop = 0;   
   //pLS->lCurrPos = 0;

   pLS->state = STATE_PLAY;

   DBG(fprintf(stderr,"instantiated\n"));

   
   pLS->pfQuantMode = &pLS->fQuantizeMode;
   pLS->pfRoundMode = &pLS->fRoundMode;
   pLS->pfRedoTapMode = &pLS->fRedoTapMode;
   
   
   return pLS;
}

/*****************************************************************************/

/* Initialise and activate a plugin instance. */
void
activateSooperLooper(LADSPA_Handle Instance) {

  SooperLooper * pLS;
  pLS = (SooperLooper *)Instance;

	 
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
  
  pLS->fWetCurr = pLS->fWetTarget = *pLS->pfWet;
  pLS->fDryCurr = pLS->fDryTarget = *pLS->pfDry;
  pLS->fFeedbackCurr = pLS->fFeedbackTarget = *pLS->pfFeedback;
  pLS->fRateCurr = pLS->fRateTarget = *pLS->pfRate;
  pLS->fScratchPosCurr = pLS->fScratchPosTarget = *pLS->pfScratchPos;

  pLS->state = STATE_PLAY;

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
   
   SooperLooper * pLS;

   //fprintf(stderr,"connectPortTo\n");  

   
   pLS = (SooperLooper *)Instance;
   switch (Port) {
      case SDL_DRY:
	 pLS->pfDry = DataLocation;
	 break;
      case SDL_WET:
	 pLS->pfWet = DataLocation;
	 break;

      case SDL_FEEDBACK:
	 pLS->pfFeedback = DataLocation;
	 break;
      case SDL_THRESH:
	 pLS->pfTrigThresh = DataLocation;
	 break;
      case SDL_RATE:
	 pLS->pfRate = DataLocation;
	 break;
      case SDL_SCRATCH_POS:
	 pLS->pfScratchPos = DataLocation;
	 break;
      case SDL_MULTICTRL:
	 pLS->pfMultiCtrl = DataLocation;
	 break;
      case SDL_TAP_TRIG:
	 pLS->pfTapCtrl = DataLocation;
	 break;
      case SDL_MULTITENS:
	 pLS->pfMultiTens = DataLocation;
	 break;
      case SDL_QUANTMODE:
	 pLS->pfQuantMode = DataLocation;
	 break;
      case SDL_SYNCMODE:
	 pLS->pfSyncMode = DataLocation;
	 break;
      case SDL_USERATE:
	 pLS->pfRateCtrlActive = DataLocation;
	 break;
      case SDL_XFADESAMPLES:
	 pLS->pfXfadeSamples = DataLocation;
	 break;
      case SDL_ROUNDMODE:
	 pLS->pfRoundMode = DataLocation;
	 break;
      case SDL_REDOTAPMODE:
	 pLS->pfRedoTapMode = DataLocation;
	 break;

	 
      case SDL_INPUT:
	 pLS->pfInput = DataLocation;
	 break;
      case SDL_OUTPUT:
	 pLS->pfOutput = DataLocation;
	 break;
      case SDL_SYNC_INPUT:
	 pLS->pfSyncInput = DataLocation;
	 break;
      case SDL_SYNC_OUTPUT:
	 pLS->pfSyncOutput = DataLocation;
	 break;

      case SDL_STATE_OUT:
	 pLS->pfStateOut = DataLocation;
	 break;
      case SDL_LOOPLEN_OUT:
	 pLS->pfLoopLength = DataLocation;
	 break;
      case SDL_LOOPPOS_OUT:
	 pLS->pfLoopPos = DataLocation;
	 break;
      case SDL_CYCLELEN_OUT:
	 pLS->pfCycleLength = DataLocation;
	 break;
      case SDL_SECSFREE_OUT:
	 pLS->pfSecsFree = DataLocation;
	 break;
      case SDL_SECSTOTAL_OUT:
	 pLS->pfSecsTotal= DataLocation;
	 break;
      case SDL_WAITING:
	 pLS->pfWaiting= DataLocation;
	 break;


   }
}



static void fillLoops(SooperLooper *pLS, LoopChunk *mloop, unsigned long lCurrPos)
{
   LoopChunk *loop=NULL, *nloop, *srcloop;

   // descend to the oldest unfilled loop
   for (nloop=mloop; nloop; nloop = nloop->srcloop)
   {
      if (nloop->frontfill || nloop->backfill) {
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
	 // we need to finish off a previous
	 *(loop->pLoopStart + lCurrPos) = 
	    *(srcloop->pLoopStart + (lCurrPos % srcloop->lLoopLength));
      
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
	    loop->lMarkL = loop->lMarkH = MAXLONG;
	 }
      }
      else if (loop->backfill && lCurrPos<=loop->lMarkEndH && lCurrPos>=loop->lMarkEndL)		
      {

	 // we need to finish off a previous
	 *(loop->pLoopStart + lCurrPos) = 
	    *(srcloop->pLoopStart +
	      ((lCurrPos  + loop->lStartAdj - loop->lEndAdj) % srcloop->lLoopLength));
	 
	  
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
	    loop->lMarkEndL = loop->lMarkEndH = MAXLONG;
	 }

      }

      if (mloop == loop) break;
   }

}

static LoopChunk* transitionToNext(SooperLooper *pLS, LoopChunk *loop, int nextstate);


static LoopChunk* beginMultiply(SooperLooper *pLS, LoopChunk *loop)
{
   LoopChunk * srcloop;
   
   // make new loop chunk
   loop = pushNewLoopChunk(pLS, loop->lCycleLength);
   if (loop) {
      pLS->state = STATE_MULTIPLY;
      loop->srcloop = srcloop = loop->prev;
		       
      // start out with the single cycle as our length as a marker
      loop->lLoopLength = srcloop->lCycleLength;
      loop->pLoopStop = loop->pLoopStart + loop->lLoopLength;

      // start out at same pos
      loop->dCurrPos = srcloop->dCurrPos;
      loop->lCycles = 1; // start with 1 by default
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
      if (*pLS->pfQuantMode != 0 && srcloop->lCycles > 1) {
	 // we effectively remove the first cycles from our new one
	 loop->lStartAdj = ((int)floor(srcloop->dCurrPos / srcloop->lCycleLength)
			    + 1) * srcloop->lCycleLength; 

	 // adjust dCurrPos by start adj.
	 // we handle this properly in the processing section
	 loop->dCurrPos = loop->dCurrPos - loop->lStartAdj;
			  
	 // start with 1 because we could end up with none!
	 // which will be subtracted at the end
	 loop->lCycles = 1;
	 //loop->lLoopLength = 0;
	 loop->frontfill = 0; // no need.
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
	 loop->lMarkL = loop->lMarkH = MAXLONG;
      }
      
      loop->lMarkEndL = loop->lMarkEndH = MAXLONG;

      
      DBG(fprintf(stderr,"Mark at L:%lu  h:%lu\n",loop->lMarkL, loop->lMarkH);
	  fprintf(stderr,"EndMark at L:%lu  h:%lu\n",loop->lMarkEndL, loop->lMarkEndH);
	  fprintf(stderr,"Entering MULTIPLY state  with cyclecount=%lu   curpos=%g   looplen=%lu\n", loop->lCycles, loop->dCurrPos, loop->lLoopLength));
   }

   return loop;

}


// encapsulates what happens when a multiply is ended
static LoopChunk * endMultiply(SooperLooper *pLS, LoopChunk *loop, int nextstate)
{
   LoopChunk *srcloop;
   
   srcloop = loop->srcloop;
		    
   if (*pLS->pfQuantMode != 0 && srcloop->lCycles > 1 && loop->lCycles<1)
   {
      DBG(fprintf(stderr,"Zero length loop now: at %d!\n", (int)loop->dCurrPos));
      DBG(fprintf(stderr,"Entering %d from MULTIPLY\n", nextstate));
      pLS->state = nextstate;

      loop->backfill = 0;
      loop->pLoopStop = loop->pLoopStart;
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
	 loop->pLoopStop = loop->pLoopStart + loop->lLoopLength;
	 loop->backfill = 1;

	 // adjust curr position
	 //loop->dCurrPos -= loop->lStartAdj;
			  
	 loop->lMarkEndL = (unsigned long)loop->dCurrPos;
	 loop->lMarkEndH = loop->lLoopLength - 1;

	 DBG(fprintf(stderr,"Entering %d from MULTIPLY. Length %lu.  %lu cycles\n",nextstate,
		 loop->lLoopLength, loop->lCycles)); 
	 
	 loop = transitionToNext(pLS, loop, nextstate);

      }
      else {
	 // in round mode we need to wait it out
	 // and keep recording till the end
	 DBG(fprintf(stderr,"Finishing MULTIPLY rounded\n")); 
	 loop->lMarkEndL = (unsigned long)loop->dCurrPos;
//	 loop->lMarkEndH = loop->lLoopLength + loop->lStartAdj - 1;
	 loop->lMarkEndH = loop->lLoopLength - 1;
	 pLS->nextState = nextstate;
      }
   }
   
   return loop;
}


static LoopChunk * beginInsert(SooperLooper *pLS, LoopChunk *loop)
{
   LoopChunk *srcloop;
   // try to get a new one with at least 1 cycle more length
   loop = pushNewLoopChunk(pLS, loop->lLoopLength + loop->lCycleLength);

   if (loop) {
      DBG(fprintf(stderr,"Entering INSERT state\n"));
      pLS->state = STATE_INSERT;
		       
      loop->srcloop = srcloop = loop->prev;
		       
      // start out with the single cycle extra as our length
      loop->lLoopLength = srcloop->lLoopLength + srcloop->lCycleLength;
      loop->pLoopStop = loop->pLoopStart + loop->lLoopLength;
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
		       
      if (*pLS->pfQuantMode != 0) {
	 // the next cycle boundary
	 loop->lInsPos =
	    ((int)floor(srcloop->dCurrPos / srcloop->lCycleLength)
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
	 loop->lMarkL = loop->lMarkH = MAXLONG;
      }
      
      loop->lMarkEndL = loop->lMarkEndH = MAXLONG;
		       
      DBG(fprintf(stderr, "InsPos=%lu  RemLen=%lu\n", loop->lInsPos, loop->lRemLen));
      DBG(fprintf(stderr,"Total cycles now=%lu\n", loop->lCycles));
   }

   return loop;

}

// encapsulates what happens when a insert is ended
static LoopChunk * endInsert(SooperLooper *pLS, LoopChunk *loop, int nextstate)
{
   LoopChunk *srcloop;
   
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
   
   return loop;
   
}


static LoopChunk * beginOverdub(SooperLooper *pLS, LoopChunk *loop)
{
   LoopChunk * srcloop;
   // make new loop chunk
   loop = pushNewLoopChunk(pLS, loop->lLoopLength);
   if (loop) {
      pLS->state = STATE_OVERDUB;
      // always the same length as previous loop
      loop->srcloop = srcloop = loop->prev;
      loop->lCycleLength = srcloop->lCycleLength;
      loop->lLoopLength = srcloop->lLoopLength;
      loop->pLoopStop = loop->pLoopStart + loop->lLoopLength;
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
      DBG(fprintf(stderr,"Entering OVERDUB state: srcloop is %08x\n", (unsigned)srcloop));
   }

   return loop;
}

static LoopChunk * beginReplace(SooperLooper *pLS, LoopChunk *loop)
{
   LoopChunk * srcloop;

   // NOTE: THIS SHOULD BE IDENTICAL TO OVERDUB
   // make new loop chunk
   loop = pushNewLoopChunk(pLS, loop->lLoopLength);
   if (loop)
   {
      pLS->state = STATE_REPLACE;
		       
      // always the same length as previous loop
      loop->srcloop = srcloop = loop->prev;
      loop->lCycleLength = srcloop->lCycleLength;
      loop->dOrigFeedback = LIMIT_BETWEEN_0_AND_1(*pLS->pfFeedback);
		       
      loop->lLoopLength = srcloop->lLoopLength;
      loop->pLoopStop = loop->pLoopStart + loop->lLoopLength;
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
      // we let the  loop itself do this when it gets around to it
		       
		       
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
		       
      DBG(fprintf(stderr,"Mark at L:%lu  h:%lu\n",loop->lMarkL, loop->lMarkH);
	  fprintf(stderr,"EndMark at L:%lu  h:%lu\n",loop->lMarkEndL, loop->lMarkEndH);
	  fprintf(stderr,"Entering REPLACE state: srcloop is %08x\n", (unsigned)srcloop));
   }

   return loop;
}


static LoopChunk * transitionToNext(SooperLooper *pLS, LoopChunk *loop, int nextstate)
{
   LoopChunk * newloop = loop;
   
   switch(nextstate)
   {
      case STATE_PLAY:
      case STATE_MUTE:
	 // nothing special
	 break;

      case STATE_OVERDUB:
	 newloop = beginOverdub(pLS, loop);
	 break;

      case STATE_REPLACE:
	 newloop = beginReplace(pLS, loop);
	 break;

      case STATE_INSERT:
	 newloop = beginInsert(pLS, loop);
	 break;

      case STATE_MULTIPLY:
	 newloop = beginMultiply(pLS, loop);
	 break;

      case STATE_TRIGGER_PLAY:
	      if (loop) {
		      pLS->state = STATE_PLAY;
		      nextstate = STATE_PLAY;
		      if (pLS->fCurrRate > 0)
			      loop->dCurrPos = 0.0;
		      else
			      loop->dCurrPos = loop->lLoopLength - 1;
	      }
	      break;
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
   }

   if (nextstate != -1) {
      DBG(fprintf(stderr,"Entering state %d from %d\n", nextstate, pLS->state));
      pLS->state = nextstate;

   }
   else {
      DBG(fprintf(stderr,"Next state is -1?? Why?\n"));
      pLS->state = STATE_PLAY;
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
  LADSPA_Data rateDelta=0.0f, scratchDelta=0.0f, rateTarget=1.0f, scratchTarget=0.0f;
  LADSPA_Data fTrigThresh = 0.0f;
  
  int lMultiCtrl=-1, lMultiTens=0;  
  LADSPA_Data fTapTrig = 0.0f;
  
  LADSPA_Data fFeedback = 1.0f;
  LADSPA_Data feedbackDelta=0.0f, feedbackTarget=1.0f;
  unsigned int lCurrPos = 0;
  unsigned int lpCurrPos = 0;  
  long slCurrPos;
  double dDummy;
  int firsttime, backfill;
  int useDelay = 0;
  
  float fPosRatio;
  int xfadeSamples = XFADE_SAMPLES;
  
  SooperLooper * pLS;
  LoopChunk *loop, *srcloop;

  LADSPA_Data fSyncMode = 0.0f;
  LADSPA_Data fQuantizeMode = 0.0f;

  unsigned long lSampleIndex;

  
  pLS = (SooperLooper *)Instance;

  if (!pLS || !pLS->pfInput || !pLS->pfOutput) {
     // something is badly wrong!!!
     return;
  }
  
  pfInput = pLS->pfInput;
  pfOutput = pLS->pfOutput;
  pfBuffer = (LADSPA_Data *)pLS->pSampleBuf;
  pfSyncOutput = pLS->pfSyncOutput;
  pfSyncInput = pLS->pfSyncInput;
  
  // we set up default bindings in case the host hasn't
  if (!pLS->pfQuantMode)
     pLS->pfQuantMode = &pLS->fQuantizeMode;
  if (!pLS->pfRoundMode)
     pLS->pfRoundMode = &pLS->fRoundMode;
  if (!pLS->pfRedoTapMode)
     pLS->pfRedoTapMode = &pLS->fRedoTapMode;

  xfadeSamples = (int) (*pLS->pfXfadeSamples);

  if (pLS->pfTrigThresh) {
     fTrigThresh = *pLS->pfTrigThresh;
  }

  if (pLS->pfRateCtrlActive) {
     pLS->bRateCtrlActive = (int) *pLS->pfRateCtrlActive;
  }
  if (pLS->pfSyncMode) {
	  fSyncMode = *pLS->pfSyncMode;
  }
  if (pLS->pfQuantMode) {
	  fQuantizeMode = *pLS->pfQuantMode;
  }
  
  if (pLS->pfMultiCtrl && pLS->pfMultiTens) {
     lMultiCtrl = (int) *(pLS->pfMultiCtrl);
     lMultiTens = (int) *(pLS->pfMultiTens);
  }

  if (pLS->pfTapCtrl) {
     fTapTrig = *(pLS->pfTapCtrl);
  }
  
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
	  scratchDelta = (scratchTarget - fScratchPos) / (SampleCount - 1);
  }
  
  // the rate switch is ON if it is below 1 but not 0
  // rate is 1 if rate switch is off
  //if (fRateSwitch > 1.0 || fRateSwitch==0.0) {
  //  fRate = 1.0;
  //}
  //else {
     //fprintf(stderr, "rateswitch is 1.0: %f!\n", fRate);
  //}

  if (pLS->pfWet) {
	  wetTarget = LIMIT_BETWEEN_0_AND_1(*(pLS->pfWet));
	  fWet =  LIMIT_BETWEEN_0_AND_1(pLS->fWetCurr);
	  wetDelta = (wetTarget - fWet) / (SampleCount - 1);

// 	  wetTarget += wetDelta;
// 	  fWet = fWet * 0.1f + wetTarget * 0.9f;

  }
  
  if (pLS->pfDry) {
	  dryTarget = LIMIT_BETWEEN_0_AND_1(*(pLS->pfDry));
	  fDry =  LIMIT_BETWEEN_0_AND_1(pLS->fDryCurr);
	  dryDelta = (dryTarget - fDry) / (SampleCount - 1);
  }

  if (pLS->pfFeedback) {
	  feedbackTarget = LIMIT_BETWEEN_0_AND_1(*(pLS->pfFeedback));
	  fFeedback =  LIMIT_BETWEEN_0_AND_1(pLS->fFeedbackCurr);
	  feedbackDelta = (feedbackTarget - fFeedback) / (SampleCount - 1);
	  
	  // probably against the rules, but I'm doing it anyway
	  *pLS->pfFeedback = feedbackTarget;
  }


  loop = pLS->headLoopChunk;

  // clear sync out
  memset(pfSyncOutput, 0, SampleCount * sizeof(LADSPA_Data));
  
     
  // transitions due to control triggering
  
  if (lMultiCtrl >= 0 && lMultiCtrl <= 127)
	  //&& (lMultiCtrl/10) == lMultiTens)
  {
     // fprintf(stderr, "Multictrl val is %ld\n", lMultiCtrl);

     lMultiCtrl = lMultiCtrl;

     // change the value if necessary
     if (pLS->state == STATE_MUTE) {
	switch (lMultiCtrl) {
	   case MULTI_REDO:
	      lMultiCtrl = MULTI_REDO_TOG;
	      break;
	   case MULTI_REPLACE:
	      lMultiCtrl = MULTI_QUANT_TOG;
	      break;
	   case MULTI_REVERSE:
	      lMultiCtrl = MULTI_ROUND_TOG;
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
		      
		      if (fSyncMode == 0.0f && (fTrigThresh==0.0)) {
			      // skip trig stop
			      pLS->state = STATE_PLAY;
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
		    loop->pLoopStop = loop->pLoopStart + loop->lLoopLength;
		    loop->lCycleLength = loop->lLoopLength;
		    loop->lCycles = 1;
		    
		    pLS->state = STATE_PLAY;
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
		    loop->pLoopStop = loop->pLoopStart + loop->lLoopLength;
		    loop->lCycleLength = loop->lLoopLength;
		    loop->lCycles = 1;
		    
		    pLS->state = STATE_PLAY;
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
		      DBG(fprintf(stderr,"Entering PLAY state\n"));
		      
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
		 
	      default:
		      // lets not sync overdub ops
		      
//		      if (fSyncMode == 0.0f) {
			      if (loop) {
				      loop = beginOverdub(pLS, loop);
				      if (loop)
					      srcloop = loop->srcloop;
				      else
					      srcloop = NULL;
			      }
// 		      } else {
// 			      DBG(fprintf(stderr, "starting syncwait for overdub:  %f\n", fSyncMode));
// 			      pLS->nextState = STATE_OVERDUB;
// 			      pLS->waitingForSync = 1;
// 		      }
	   }
	} break;

	case MULTI_MULTIPLY:
	{
	   switch(pLS->state) {
	      case STATE_MULTIPLY:
		 // set mark
		      if (fSyncMode == 0.0f) {
			      if (loop) {
				      loop = endMultiply(pLS, loop, STATE_PLAY);
			      }

			      // put a sync marker at the beginning here
			      // mostly for slave purposes, it might screw up others
			      pfSyncOutput[0] = 1.0f;
			      
		      } else {
			      DBG(fprintf(stderr, "waiting for sync multi end\n"));
			      pLS->nextState = STATE_PLAY;
			      pLS->waitingForSync = 1;
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
			      DBG(fprintf(stderr,"Entering PLAY state\n"));
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
			      DBG(fprintf(stderr, "starting syncwait for replace\n"));
			      pLS->nextState = STATE_REPLACE;
			      pLS->waitingForSync = 1;
		      }
	   }

	} break;

	case MULTI_MUTE:
	{
	   switch(pLS->state) {
	      case STATE_MUTE:
		 // reset for audio ramp
		 pLS->lRampSamples = xfadeSamples;
	      case STATE_ONESHOT:
		 // this enters play mode but from the continuous position
		 pLS->state = STATE_PLAY;
		 DBG(fprintf(stderr,"Entering PLAY state continuous\n"));

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
		 pLS->lRampSamples = xfadeSamples;

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
		    loop->pLoopStop = loop->pLoopStart + loop->lLoopLength;
		    loop->lCycleLength = loop->lLoopLength;

		    
		    if ((char *)loop->pLoopStop > (pLS->pSampleBuf + pLS->lBufferSize)) {
		       // ignore this tap, it's too long
		       // just treat as first
		       loop->lLoopLength = 0;
		       loop->lCycleLength = 0;
		       loop->pLoopStop = loop->pLoopStart;
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
	      case STATE_RECORD:		 
	      case STATE_INSERT:
	      case STATE_OVERDUB:
	      case STATE_MULTIPLY:
		 // nothing happens
		 break;
		 
	      default:


		 loop = pushNewLoopChunk(pLS, 0);
		 if (loop)
		 {
		    pLS->state = STATE_DELAY;
		    loop->srcloop = NULL;
		    
		    // initial delay length 0 until next tap
		    loop->lLoopLength = 0;
		    loop->pLoopStop = loop->pLoopStart;
		    
		    
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
		 DBG(fprintf(stderr,"Entering PLAY state\n"));
		 break;

	      case STATE_MUTE:
	      case STATE_ONESHOT:		 
		 // this restarts the loop from beginnine
		 pLS->state = STATE_PLAY;
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
	      case STATE_DELAY:
		 // POP the head off and start the previous
		 // one at the same position if possible
		 if (loop) {
		    undoLoop(pLS);
		 }
		 
		 // cancel whatever mode, back to play mode
		 pLS->state = STATE_PLAY;
		 DBG(fprintf(stderr,"Undoing and reentering PLAY state from UNDO\n"));
		 break;


	      case STATE_MUTE:
		 // undo ALL)
		 clearLoopChunks(pLS);
		 DBG(fprintf(stderr,"UNDO all loops\n"));
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

		 // immediately redo last if possible
		 redoLoop(pLS);

		 pLS->state = STATE_PLAY;
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
				      pLS->state = STATE_ONESHOT;
				      if (pLS->fCurrRate > 0)
					      loop->dCurrPos = 0.0;
				      else
					      loop->dCurrPos = loop->lLoopLength - 1;		       
			      }
		      }	else {
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
		      }	else {
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
	      if ((fSyncMode == 0.0f && ((fInputSample > fTrigThresh) || (fTrigThresh==0.0)))
		  || (fSyncMode > 0.0f && pfSyncInput[lSampleIndex] != 0.0))
	      {
		 
		 loop = pushNewLoopChunk(pLS, 0);
		 if (loop) {
		    DBG(fprintf(stderr,"Entering RECORD state\n"));
		    pLS->state = STATE_RECORD;
		    // force rate to be 1.0
		    fRate = pLS->fCurrRate = 1.0f;

		    loop->pLoopStop = loop->pLoopStart;
		    loop->lLoopLength = 0;
		    loop->lStartAdj = 0;
		    loop->lEndAdj = 0;
		    loop->dCurrPos = 0.0;
		    loop->firsttime = 0;
		    loop->lMarkL = loop->lMarkEndL = MAXLONG;
		    loop->frontfill = loop->backfill = 0;
		    loop->lCycles = 1; // at first just one		 
		    loop->srcloop = NULL;
		    pLS->nextState = -1;
		    loop->dOrigFeedback = fFeedback;
		 }
		 else {
		    DBG(fprintf(stderr, "out of memory! back to PLAY mode\n"));
		    pLS->state = STATE_PLAY;
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
	   
	   for (;lSampleIndex < SampleCount;
		lSampleIndex++)
	   {
	      fWet += wetDelta;
              fDry += dryDelta;
	      fFeedback += feedbackDelta;
	      fScratchPos += scratchDelta;
		   
// 	      if (pLS->waitingForSync && (fSyncMode == 0.0 || pfSyncInput[lSampleIndex] != 0.0))
// 	      {
// 		      DBG(fprintf(stderr,"Finishing synced record\n"));
// 		      pLS->state = pLS->nextState;
// 		      pLS->nextState = -1;
// 		      pLS->waitingForSync = 0;
// 		      break;
// 	      }
		   
	      // wrap at the proper loop end
	      lCurrPos = (unsigned int)loop->dCurrPos;
	      if ((char *)(lCurrPos + loop->pLoopStart) >= (pLS->pSampleBuf + pLS->lBufferSize)) {
		 // stop the recording RIGHT NOW
		 // we don't support loop crossing the end of memory
		 // it's easier.
		 DBG(fprintf(stderr, "Entering PLAY state -- END of memory! %08x\n",
			     (unsigned) (pLS->pSampleBuf + pLS->lBufferSize) ));
		 pLS->state = STATE_PLAY;
		 break;
	      }

	      if (lCurrPos == 0) {
		      pfSyncOutput[lSampleIndex] = 1.0f;
	      }
		      
	      
	      fInputSample = pfInput[lSampleIndex];
	      
	      *(loop->pLoopStart + lCurrPos) = fInputSample;
	      
	      // increment according to current rate
	      loop->dCurrPos = loop->dCurrPos + fRate;
	      
	      
	      pfOutput[lSampleIndex] = fDry * fInputSample;
	   }

	   // update loop values (in case we get stopped by an event)
	   lCurrPos = ((unsigned int)loop->dCurrPos);
	   loop->pLoopStop = loop->pLoopStart + lCurrPos;
	   loop->lLoopLength = (unsigned long) (loop->pLoopStop - loop->pLoopStart);
	   loop->lCycleLength = loop->lLoopLength;

	   
	} break;

	case STATE_TRIG_STOP:
	{
	   //fprintf(stderr,"in trigstop\n");	   
	   // play the input out.  Keep recording until we go
	   // above the threshold, then go into next state.
	   
	   for (;lSampleIndex < SampleCount;
		lSampleIndex++)
	   {
	      fWet += wetDelta;
              fDry += dryDelta;
	      fFeedback += feedbackDelta;
	      fScratchPos += scratchDelta;
		   
	      lCurrPos = (unsigned int) loop->dCurrPos;
	      
	      fInputSample = pfInput[lSampleIndex];
	      
	      
	      if ((fSyncMode == 0.0f && ((fInputSample > fTrigThresh) || (fTrigThresh==0.0)))
		  || (fSyncMode > 0.0f && pfSyncInput[lSampleIndex] != 0.0))
	      {
		 DBG(fprintf(stderr,"Entering %d state\n", pLS->nextState));
		 //pLS->state = pLS->nextState;
		 // reset for audio ramp
		 pLS->lRampSamples = xfadeSamples;
		 //loop->dCurrPos = 0.0f;

		 loop = transitionToNext (pLS, loop, pLS->nextState);
		 pLS->waitingForSync = 0;
		 break;
	      }

	      
	      *(loop->pLoopStart + lCurrPos) = fInputSample;
	      
	      // increment according to current rate
	      loop->dCurrPos = loop->dCurrPos + fRate;


	      if ((char *)(loop->pLoopStart + (unsigned int)loop->dCurrPos)
		  > (pLS->pSampleBuf + pLS->lBufferSize)) {
		 // out of space! give up for now!
		 // undo!
		 pLS->state = STATE_PLAY;
		 //undoLoop(pLS);
		 DBG(fprintf(stderr,"Record Stopped early! Out of memory!\n"));
		 loop->dCurrPos = 0.0f;
		 break;
	      }

	      
	      pfOutput[lSampleIndex] = fDry * fInputSample;

	      
	   }

	   // update loop values (in case we get stopped by an event)
	   if (loop) {
		   loop->pLoopStop = loop->pLoopStart + lCurrPos;
		   loop->lLoopLength = (unsigned long) (loop->pLoopStop - loop->pLoopStart);
		   loop->lCycleLength = loop->lLoopLength;
		   loop->lCycles = 1;
	   }
	   
	} break;


	
	case STATE_OVERDUB:
	case STATE_REPLACE:
	{
	   if (loop &&  loop->lLoopLength && loop->srcloop)
	   {
	      srcloop = loop->srcloop;
	      
	      for (;lSampleIndex < SampleCount;
		   lSampleIndex++)
	      {
	         fWet += wetDelta;
                 fDry += dryDelta;
	         fFeedback += feedbackDelta;
	         fScratchPos += scratchDelta;


		 lCurrPos =(unsigned int) fmod(loop->dCurrPos, loop->lLoopLength);

		 
		 if (fSyncMode != 0) {
			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];
		 }
		 else {
			 if (fQuantizeMode == QUANT_OFF || (fQuantizeMode == QUANT_CYCLE && (lCurrPos % loop->lCycleLength) == 0)
			     || (fQuantizeMode == QUANT_LOOP && (lCurrPos == 0)))
			 {
				 pfSyncOutput[lSampleIndex] = 1.0f;
			 }
		 }
		 
		 if (pLS->waitingForSync && ((fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF) || pfSyncInput[lSampleIndex] != 0.0f))
		 {
			 DBG(fprintf(stderr,"Finishing synced overdub/replace\n"));
			 pLS->state = pLS->nextState;
			 pLS->nextState = -1;
			 pLS->waitingForSync = 0;
			 break;
		 }

		 
		 
		 fInputSample = pfInput[lSampleIndex];
		 
		 fillLoops(pLS, loop, lCurrPos);

		 if (pLS->state == STATE_OVERDUB)
		 {
		    // use our self as the source (we have been filled by the call above)
		    fOutputSample = fWet  *  *(loop->pLoopStart + lCurrPos)
		       + fDry * fInputSample;
		    
		    *(loop->pLoopStart + lCurrPos) =  
		       (fInputSample + 0.96f * fFeedback *  *(loop->pLoopStart + lCurrPos));
		 }
		 else {
		    // state REPLACE use only the new input
		    // use our self as the source (we have been filled by the call above)
		    fOutputSample = fDry * fInputSample;
		    
		    *(loop->pLoopStart + lCurrPos) = fInputSample;

		 }
		 
		 pfOutput[lSampleIndex] = fOutputSample;
		 

		 // increment and wrap at the proper loop end
		 loop->dCurrPos = loop->dCurrPos + fRate;

		 if (fSyncMode != 0.0 && pLS->fNextCurrRate != 0 && pfSyncInput[lSampleIndex] != 0.0) {
		       // commit the new rate at boundary (quantized)
		       pLS->fCurrRate = pLS->fNextCurrRate;
		       pLS->fNextCurrRate = 0.0f;
		       DBG(fprintf(stderr, "Starting quantized rate change\n"));
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
	      

	      for (;lSampleIndex < SampleCount;
		   lSampleIndex++)
	      {
	         fWet += wetDelta;
                 fDry += dryDelta;
	         fFeedback += feedbackDelta;
	         fScratchPos += scratchDelta;


		 if (pLS->waitingForSync && (fSyncMode == 0.0f || pfSyncInput[lSampleIndex] != 0.0f))
		 {
			 DBG(fprintf(stderr,"Finishing synced multiply\n"));
			 loop = endMultiply (pLS, loop, pLS->nextState);

			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];
			 
			 pLS->waitingForSync = 0;
			 break;
		 }

		 
		 lpCurrPos =(unsigned int) fmod(loop->dCurrPos + loop->lStartAdj, srcloop->lLoopLength);
		 slCurrPos =(long) loop->dCurrPos;

		 fillLoops(pLS, loop, lpCurrPos);
		 
		 fInputSample = pfInput[lSampleIndex];

		 if (fSyncMode != 0) {
			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];
		 }
		 else {
			 if (fQuantizeMode == QUANT_OFF || (fQuantizeMode == QUANT_CYCLE && (lCurrPos % loop->lCycleLength) == 0))
			 {
				 pfSyncOutput[lSampleIndex] = 1.0f;
			 }
		 }

		 
		 
		 // always use the source loop as the source
		 
		 fOutputSample = (fWet *  *(srcloop->pLoopStart + lpCurrPos)
				  + fDry * fInputSample);


		 if (slCurrPos < 0) {
		    // this is part of the loop that we need to ignore
		    // fprintf(stderr, "Ignoring at %ul\n", lCurrPos);
		 }
		 else if ((loop->lCycles <=1 && *pLS->pfQuantMode != 0)
		     || (slCurrPos > (long) loop->lMarkEndL &&  *pLS->pfRoundMode == 0)) {
		    // do not include the new input
		    *(loop->pLoopStart + slCurrPos)
		       = fFeedback *  *(srcloop->pLoopStart + lpCurrPos);
		    // fprintf(stderr, "Not including input at %ul\n", lCurrPos);
		 }
		 else {
		    *(loop->pLoopStart + slCurrPos)
		       = (fInputSample + 0.96f *  fFeedback *  *(srcloop->pLoopStart + lpCurrPos));
		 }
		 
		 pfOutput[lSampleIndex] = fOutputSample;

		 
		 // increment 
		 loop->dCurrPos = loop->dCurrPos + fRate;
	      

		 if (slCurrPos > 0 && (unsigned)(loop->pLoopStart + slCurrPos)
		     > (unsigned)(pLS->pSampleBuf + pLS->lBufferSize)) {
		    // out of space! give up for now!
		    // undo!
		    pLS->state = STATE_PLAY;
		    undoLoop(pLS);
		    DBG(fprintf(stderr,"Multiply Undone! Out of memory!\n"));
		    break;
		 }

		 // ASSUMPTION: our rate is +1 only		 
		 if (loop->dCurrPos  >= (loop->lLoopLength)) {
		    if (loop->dCurrPos >= loop->lMarkEndH) {
		       // we be done this only happens in round mode
		       // adjust curr position
		       loop->lMarkEndH = MAXLONG;
		       backfill = loop->backfill = 0;
		       // do adjust it for our new length
		       loop->dCurrPos = 0.0f;

		       loop->lLoopLength = loop->lCycles * loop->lCycleLength;
		       loop->pLoopStop = loop->pLoopStart + loop->lLoopLength;
		       
		       
		       loop = transitionToNext(pLS, loop, pLS->nextState);
		       break;
		    }
		    // increment cycle and looplength
		    loop->lCycles += 1;
		    loop->lLoopLength += loop->lCycleLength;
		    loop->pLoopStop = loop->pLoopStart + loop->lLoopLength;
		    //loop->lLoopStop = loop->lLoopStart + loop->lLoopLength;
		    // this signifies the end of the original cycle
		    loop->firsttime = 0;
		    DBG(fprintf(stderr,"Multiply added cycle %lu  at %g\n", loop->lCycles, loop->dCurrPos));

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

		 lpCurrPos =(unsigned int) fmod(loop->dCurrPos, srcloop->lLoopLength);
		 lCurrPos =(unsigned int) loop->dCurrPos;

		 fillLoops(pLS, loop, lCurrPos);
		 
		 fInputSample = pfInput[lSampleIndex];

		 if (firsttime && *pLS->pfQuantMode != 0 )
		 {
		    // just the source and input
		    fOutputSample = (fWet *  *(srcloop->pLoopStart + lpCurrPos)
				     + fDry * fInputSample);
		    
		    // do not include the new input
		    //*(loop->pLoopStart + lCurrPos)
		    //  = fFeedback *  *(srcloop->pLoopStart + lpCurrPos);

		 }
		 else if (lCurrPos > loop->lMarkEndL && *pLS->pfRoundMode == 0)
		 {
		    // insert zeros, we finishing an insert with nothingness
		    fOutputSample = fDry * fInputSample;

		    *(loop->pLoopStart + lCurrPos) = 0.0f;

		 }
		 else {
		    // just the input we are now inserting
		    fOutputSample = fDry * fInputSample;

		    *(loop->pLoopStart + lCurrPos) = (fInputSample);

		 }
		 
		 
		 pfOutput[lSampleIndex] = fOutputSample;

		 if (fSyncMode != 0) {
			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];
		 }
		 else if (fQuantizeMode == QUANT_OFF || (fQuantizeMode == QUANT_CYCLE && (lCurrPos % loop->lCycleLength) == 0)
			  || (fQuantizeMode == QUANT_LOOP && (lCurrPos == 0))) {
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

		    loop->lLoopLength = loop->lCycles * loop->lCycleLength;
		    loop->pLoopStop = loop->pLoopStart + loop->lLoopLength;
		    
		    DBG(fprintf(stderr, "Looplength = %d   cycles=%d\n", loop->lLoopLength, loop->lCycles));
		    
		    loop = transitionToNext(pLS, loop, pLS->nextState);
		    DBG(fprintf(stderr,"Entering state %d from insert\n", pLS->state));
		    break;
		 }

		 // ASSUMPTION: our rate is +1 only		 
		 if (firsttime && lCurrPos % loop->lCycleLength == 0)
		 {
		    firsttime = loop->firsttime = 0;
		    DBG(fprintf(stderr, "first time done\n"));
		 }
		 
		 if ((lCurrPos % loop->lCycleLength) == ((loop->lInsPos-1) % loop->lCycleLength)) {

		    if ((unsigned)(loop->pLoopStart + loop->lLoopLength + loop->lCycleLength)
			> (unsigned)(pLS->pSampleBuf + pLS->lBufferSize))
		    {
		       // out of space! give up for now!
		       pLS->state = STATE_PLAY;
		       //undoLoop(pLS);
		       DBG(fprintf(stderr,"Insert finish early! Out of memory!\n"));
		       break;
		    }
		    else {
		       // increment cycle and looplength
		       loop->lCycles += 1;
		       loop->lLoopLength += loop->lCycleLength;
		       loop->pLoopStop = loop->pLoopStart + loop->lLoopLength;
		       //loop->lLoopStop = loop->lLoopStart + loop->lLoopLength;
		       // this signifies the end of the original cycle
		       DBG(fprintf(stderr,"insert added cycle. Total=%lu\n", loop->lCycles));
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
	      
	      if (pLS->state == STATE_MUTE) {
		 if (pLS->lRampSamples <= 0)
		    tmpWet = 0.0f;
		 // otherwise the ramp takes care of it
	      }
	      else if(pLS->state == STATE_SCRATCH)
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
		       fRate = 0.0;
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


		 if (fSyncMode != 0.0f) {
			 pfSyncOutput[lSampleIndex] = pfSyncInput[lSampleIndex];
		 }
		 else if (fQuantizeMode == QUANT_OFF || (fQuantizeMode == QUANT_CYCLE && (lCurrPos % loop->lCycleLength) == 0)
			  || (fQuantizeMode == QUANT_LOOP && (lCurrPos == 0))) {
			 pfSyncOutput[lSampleIndex] = 1.0f;
		 }

		 
		 if (pLS->waitingForSync && ((fSyncMode == 0.0f && fQuantizeMode == QUANT_OFF) || pfSyncInput[lSampleIndex] != 0.0f))
		 {
			 fprintf(stderr, "transition to next at: %lu\n", lSampleIndex);
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

		 
		      


		 // modify fWet if we are in a ramp up/down
		 if (pLS->lRampSamples > 0) {
		    if (pLS->state == STATE_MUTE) {
		       //negative linear ramp
		       tmpWet = fWet * (pLS->lRampSamples * 1.0f) / xfadeSamples;
		    }
		    else {
		       // positive linear ramp
		       tmpWet = fWet * (xfadeSamples - pLS->lRampSamples)
			  * 1.0f / xfadeSamples;
		    }

		    pLS->lRampSamples -= 1;
		 }

		 
		 // fill loops if necessary
		 fillLoops(pLS, loop, lCurrPos);

		 		    
		 fInputSample = pfInput[lSampleIndex];
		 fOutputSample =   tmpWet *  *(loop->pLoopStart + lCurrPos)
		    + fDry * fInputSample;



		 
		 
		 // increment and wrap at the proper loop end
		 loop->dCurrPos = loop->dCurrPos + fRate;

		 pfOutput[lSampleIndex] = fOutputSample;
		 

		 if (fSyncMode != 0.0 && pLS->fNextCurrRate != 0 && pfSyncInput[lSampleIndex] != 0.0) {
		       // commit the new rate at boundary (quantized)
		       pLS->fCurrRate = pLS->fNextCurrRate;
		       pLS->fNextCurrRate = 0.0f;
		       DBG(fprintf(stderr, "Starting quantized rate change\n"));
		 }
		 
		 if (loop->dCurrPos >= loop->lLoopLength) {
		    if (pLS->state == STATE_ONESHOT) {
		       // done with one shot
		       DBG(fprintf(stderr, "finished ONESHOT\n"));
		       pLS->state = STATE_MUTE;
		       pLS->lRampSamples = xfadeSamples;
		       //fWet = 0.0;
		    }

		    if (pLS->fNextCurrRate != 0) {
		       // commit the new rate at boundary (quantized)
		       pLS->fCurrRate = pLS->fNextCurrRate;
		       pLS->fNextCurrRate = 0.0f;
		       DBG(fprintf(stderr, "Starting quantized rate change\n"));
		    }
		    
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
		       pLS->lRampSamples = xfadeSamples;
		    }

		    if (pLS->fNextCurrRate != 0) {
		       // commit the new rate at boundary (quantized)
		       pLS->fCurrRate = pLS->fNextCurrRate;
		       pLS->fNextCurrRate = 0.0f;
		       DBG(fprintf(stderr, "Starting quantized rate change\n"));
		    }

		 }


	      }
	      
	      
	      // recenter around the mod
	      lCurrPos = (unsigned int) fabs(fmod(loop->dCurrPos, loop->lLoopLength));

	      if (recenter) {
		      loop->dCurrPos = lCurrPos + modf(loop->dCurrPos, &dDummy);
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

		 fInputSample = pfInput[lSampleIndex];

		 if (backfill && lCurrPos >= loop->lMarkEndL && lCurrPos <= loop->lMarkEndH) {
		    // our delay buffer is invalid here, clear it
		    *(loop->pLoopStart + lCurrPos) = 0.0f;

		    if (fRate > 0) {
		       loop->lMarkEndL = lCurrPos;
		    }
		    else {
		       loop->lMarkEndH = lCurrPos;
		    }
		 }


		 fOutputSample =   fWet *  *(loop->pLoopStart + lCurrPos)
		    + fDry * fInputSample;


		 if (!pLS->bHoldMode) {
		    // now fill in from input if we are not holding the delay
		    *(loop->pLoopStart + lCurrPos) = 
		      (fInputSample +  fFeedback *  *(loop->pLoopStart + lCurrPos));
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
  if (pLS->pfStateOut) {
     *pLS->pfStateOut = (LADSPA_Data) pLS->state;
  }

  *pLS->pfWaiting = pLS->waitingForSync ? 1.0f: 0.0f;
  
  if (pLS->pfSecsFree) {
     *pLS->pfSecsFree = (pLS->fTotalSecs) -
	(pLS->headLoopChunk ?
	 ((((unsigned)pLS->headLoopChunk->pLoopStop - (unsigned)pLS->pSampleBuf)
	  / sizeof(LADSPA_Data)) / pLS->fSampleRate)   :
	 0);
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

  SooperLooper * pLS;

  pLS = (SooperLooper *)Instance;

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
    piPortDescriptors[SDL_WET]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_DRY]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;

    piPortDescriptors[SDL_FEEDBACK]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_THRESH]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_RATE]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_SCRATCH_POS]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;

    piPortDescriptors[SDL_MULTICTRL]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;

    piPortDescriptors[SDL_TAP_TRIG]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;

    piPortDescriptors[SDL_MULTITENS]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_QUANTMODE]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_ROUNDMODE]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_SYNCMODE]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_USERATE]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_XFADESAMPLES]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    
    piPortDescriptors[SDL_REDOTAPMODE]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    
    piPortDescriptors[SDL_INPUT]
      = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
    piPortDescriptors[SDL_OUTPUT]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
    piPortDescriptors[SDL_SYNC_INPUT]
      = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
    piPortDescriptors[SDL_SYNC_OUTPUT]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;

    
    piPortDescriptors[SDL_STATE_OUT]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_LOOPPOS_OUT]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_LOOPLEN_OUT]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_CYCLELEN_OUT]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_SECSTOTAL_OUT]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_SECSFREE_OUT]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_WAITING]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;


    
    pcPortNames
      = (char **)calloc(PORT_COUNT, sizeof(char *));
    g_psDescriptor->PortNames
      = (const char **)pcPortNames;
    pcPortNames[SDL_DRY] 
      = strdup("Dry Level (dB)");
    pcPortNames[SDL_WET] 
      = strdup("Wet Level (dB)");

    pcPortNames[SDL_FEEDBACK] 
      = strdup("Feedback");
    pcPortNames[SDL_THRESH] 
      = strdup("Trigger Threshold");
    pcPortNames[SDL_RATE] 
      = strdup("Rate");
    pcPortNames[SDL_SCRATCH_POS] 
      = strdup("Scratch Destination");

    pcPortNames[SDL_MULTICTRL] 
      = strdup("Multi Control");

    pcPortNames[SDL_TAP_TRIG] 
      = strdup("Tap Delay Trigger");

    pcPortNames[SDL_MULTITENS] 
      = strdup("MultiCtrl 10s");
    pcPortNames[SDL_QUANTMODE] 
      = strdup("Quantize Mode");
    pcPortNames[SDL_ROUNDMODE] 
      = strdup("Round Mode");
    pcPortNames[SDL_REDOTAPMODE] 
      = strdup("Redo Tap Mode");
    pcPortNames[SDL_SYNCMODE] 
      = strdup("Sync Mode");
    pcPortNames[SDL_USERATE] 
      = strdup("Use Rate Ctrl");
    pcPortNames[SDL_XFADESAMPLES] 
      = strdup("Fade samples");

    
    pcPortNames[SDL_INPUT] 
      = strdup("Input");
    pcPortNames[SDL_OUTPUT]
      = strdup("Output");

    pcPortNames[SDL_SYNC_INPUT] 
      = strdup("Sync Input");
    pcPortNames[SDL_SYNC_OUTPUT]
      = strdup("Sync Output");
    
    pcPortNames[SDL_STATE_OUT] 
      = strdup("State Output");
    pcPortNames[SDL_LOOPLEN_OUT]
      = strdup("Loop Length Out (s)");
    pcPortNames[SDL_LOOPPOS_OUT]
      = strdup("Loop Position Out (s)");
    pcPortNames[SDL_CYCLELEN_OUT]
      = strdup("Cycle Length Out (s)");

    pcPortNames[SDL_SECSTOTAL_OUT]
      = strdup("Total Sample Mem (s)");
    pcPortNames[SDL_SECSFREE_OUT]
      = strdup("Free Sample Mem (s)");
    pcPortNames[SDL_WAITING]
      = strdup("Waiting");

    
    psPortRangeHints = ((LADSPA_PortRangeHint *)
			calloc(PORT_COUNT, sizeof(LADSPA_PortRangeHint)));
    g_psDescriptor->PortRangeHints
      = (const LADSPA_PortRangeHint *)psPortRangeHints;

    psPortRangeHints[SDL_DRY].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    psPortRangeHints[SDL_DRY].LowerBound 
      = -90.0f;
    psPortRangeHints[SDL_DRY].UpperBound
      = 0.0;

    psPortRangeHints[SDL_WET].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    psPortRangeHints[SDL_WET].LowerBound 
      = -90.0f;
    psPortRangeHints[SDL_WET].UpperBound
      = 0.0;
    
    psPortRangeHints[SDL_FEEDBACK].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    psPortRangeHints[SDL_FEEDBACK].LowerBound 
      = 0.0;
    psPortRangeHints[SDL_FEEDBACK].UpperBound
      = 1.0;

    psPortRangeHints[SDL_THRESH].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    psPortRangeHints[SDL_THRESH].LowerBound 
      = 0.0;
    psPortRangeHints[SDL_THRESH].UpperBound
      = 1.0;

    psPortRangeHints[SDL_RATE].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    psPortRangeHints[SDL_RATE].LowerBound 
      = -4.0;
    psPortRangeHints[SDL_RATE].UpperBound
      = 4.0;
    
    psPortRangeHints[SDL_SCRATCH_POS].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    psPortRangeHints[SDL_SCRATCH_POS].LowerBound 
      = 0.0;
    psPortRangeHints[SDL_SCRATCH_POS].UpperBound
      = 1.0;

    psPortRangeHints[SDL_MULTICTRL].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_INTEGER;
    psPortRangeHints[SDL_MULTICTRL].LowerBound 
      = 0.0;
    psPortRangeHints[SDL_MULTICTRL].UpperBound
      = 127.0;

    psPortRangeHints[SDL_TAP_TRIG].HintDescriptor
      = LADSPA_HINT_TOGGLED;

    psPortRangeHints[SDL_MULTITENS].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_INTEGER;
    psPortRangeHints[SDL_MULTITENS].LowerBound 
      = 0.0;
    psPortRangeHints[SDL_MULTITENS].UpperBound
      = 12.0;

    psPortRangeHints[SDL_QUANTMODE].HintDescriptor
      = LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_INTEGER;
    psPortRangeHints[SDL_QUANTMODE].LowerBound 
      = 0.0f;
    psPortRangeHints[SDL_QUANTMODE].UpperBound
      = 3.0f;

    psPortRangeHints[SDL_XFADESAMPLES].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_INTEGER;
    psPortRangeHints[SDL_XFADESAMPLES].LowerBound 
      = 0.0f;
    psPortRangeHints[SDL_XFADESAMPLES].UpperBound
      = 8192.0f;
    
    
    psPortRangeHints[SDL_ROUNDMODE].HintDescriptor
      = LADSPA_HINT_TOGGLED;
    psPortRangeHints[SDL_REDOTAPMODE].HintDescriptor
      = LADSPA_HINT_TOGGLED;
    psPortRangeHints[SDL_SYNCMODE].HintDescriptor
      = LADSPA_HINT_TOGGLED;
    psPortRangeHints[SDL_USERATE].HintDescriptor
      = LADSPA_HINT_TOGGLED;
    
    psPortRangeHints[SDL_INPUT].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[SDL_INPUT].LowerBound 
      = 0.0;
    psPortRangeHints[SDL_OUTPUT].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[SDL_OUTPUT].LowerBound 
      = 0.0;

    psPortRangeHints[SDL_STATE_OUT].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[SDL_LOOPPOS_OUT].LowerBound 
      = 0.0;

    psPortRangeHints[SDL_LOOPPOS_OUT].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[SDL_LOOPPOS_OUT].LowerBound 
      = 0.0;
    
    psPortRangeHints[SDL_LOOPLEN_OUT].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[SDL_LOOPLEN_OUT].LowerBound 
      = 0.0;

    psPortRangeHints[SDL_CYCLELEN_OUT].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[SDL_CYCLELEN_OUT].LowerBound 
      = 0.0;

    psPortRangeHints[SDL_SECSTOTAL_OUT].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[SDL_SECSTOTAL_OUT].LowerBound 
      = 0.0;
    psPortRangeHints[SDL_SECSFREE_OUT].HintDescriptor
      = LADSPA_HINT_BOUNDED_BELOW;
    psPortRangeHints[SDL_SECSFREE_OUT].LowerBound 
      = 0.0;

    psPortRangeHints[SDL_WAITING].HintDescriptor
      = LADSPA_HINT_TOGGLED;
    
    
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
