/*	Copyright © 2007 Apple Inc. All Rights Reserved.
	
	Disclaimer: IMPORTANT:  This Apple software is supplied to you by 
			Apple Inc. ("Apple") in consideration of your agreement to the
			following terms, and your use, installation, modification or
			redistribution of this Apple software constitutes acceptance of these
			terms.  If you do not agree with these terms, please do not use,
			install, modify or redistribute this Apple software.
			
			In consideration of your agreement to abide by the following terms, and
			subject to these terms, Apple grants you a personal, non-exclusive
			license, under Apple's copyrights in this original Apple software (the
			"Apple Software"), to use, reproduce, modify and redistribute the Apple
			Software, with or without modifications, in source and/or binary forms;
			provided that if you redistribute the Apple Software in its entirety and
			without modifications, you must retain this notice and the following
			text and disclaimers in all such redistributions of the Apple Software. 
			Neither the name, trademarks, service marks or logos of Apple Inc. 
			may be used to endorse or promote products derived from the Apple
			Software without specific prior written permission from Apple.  Except
			as expressly stated in this notice, no other rights or licenses, express
			or implied, are granted by Apple herein, including but not limited to
			any patent rights that may be infringed by your derivative works or by
			other works in which the Apple Software may be incorporated.
			
			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
			MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
			THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
			FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
			OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
			
			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
			OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
			SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
			INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
			MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
			AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
			STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
			POSSIBILITY OF SUCH DAMAGE.
*/
#include "AUTracer.h"
#include "CAHostTimeBase.h"

static UInt64 s1msec = 0;
static UInt64 sTimeOut = 1000000000LL;

#include <sys/syscall.h>
#include <unistd.h>

static UInt32 startTrace = 0xDEADCA00;
static UInt32 endTrace = 0xDEADCA04;

AUTracer::~AUTracer ()
{
	if (mProfiler)
		delete mProfiler;
	
	if (mAU) {
		if (mSpikeLimit == 0) {
			AudioDeviceID theDevice;
			UInt32 theSize = sizeof (theDevice);
			AudioUnitGetProperty (mAU, kAudioOutputUnitProperty_CurrentDevice, 
													0, 0, &theDevice, &theSize);
			AudioDeviceRemovePropertyListener(theDevice, 0, false,
												kAudioDeviceProcessorOverload, 
												AUTracer::OverlaodListenerProc);
		} else {
			AudioUnitRemoveRenderNotify (mAU, AUTracer::CPUSpikeProfiler, this);
		}
		
		AudioUnitRemovePropertyListener (mAU,
										kAudioUnitProperty_SampleRate,
										AUTracer::SampleRateListener);
	}
}

OSStatus	AUTracer::Establish (AudioUnit inUnit, char* inFName, Float64 inSpikeLimit, UInt32 secsBetweenTraces)
{
	if (CALatencyLog::CanUse() == false)
		return -1;
	
	OSStatus result;

	if (s1msec == 0)
		s1msec = CAHostTimeBase::ConvertFromNanos (1000 * 1000);

			// listen for overloads
	if (inSpikeLimit == 0) 
	{
		AudioDeviceID theDevice;
		UInt32 theSize = sizeof (theDevice);
		require_noerr (result = AudioUnitGetProperty (inUnit, kAudioOutputUnitProperty_CurrentDevice, 
												0, 0, &theDevice, &theSize), home);
		if (mProfiler)
			delete mProfiler;
			
		mProfiler = new CALatencyLog (inFName, ".txt");
	
		require_noerr (result = AudioDeviceAddPropertyListener(theDevice, 0, false,
											kAudioDeviceProcessorOverload, 
											AUTracer::OverlaodListenerProc, this), home);
		
		require_noerr (result = AudioUnitAddRenderNotify (inUnit, AUTracer::OverloadTagger, this), home);
	}
	else // listen for spikes...
	{
		if (mProfiler)
			delete mProfiler;
		
		mProfiler = new CALatencyLog (inFName, ".txt");
		mSpikeLimit = inSpikeLimit;
  		
		require_noerr (result = AudioUnitAddRenderNotify (inUnit, AUTracer::CPUSpikeProfiler, this), home);
	}
			
	if (!result) {
		mAU = inUnit;
			
			// lets do this one to just load this code...
		mProfiler->Capture((CAHostTimeBase::GetTheCurrentTime() - s1msec), 
							CAHostTimeBase::GetTheCurrentTime(), 
							false, "DUMMY TRACE");


		require_noerr (result = AudioUnitAddPropertyListener (mAU,
										kAudioUnitProperty_SampleRate,
										AUTracer::SampleRateListener, this), home);
			
		sTimeOut *= secsBetweenTraces;
		
			// we start doing this straight away...
		mLastTimeWrote = CAHostTimeBase::ConvertFromNanos (CAHostTimeBase::ConvertToNanos (CAHostTimeBase::GetTheCurrentTime() - (sTimeOut * 2)));

		UInt32 dataSize = sizeof (Float64);
		require_noerr (result = AudioUnitGetProperty (inUnit, kAudioUnitProperty_SampleRate,
								kAudioUnitScope_Output, 0, &mSampleRate, &dataSize), home);
	}
		
home:
	return result;
}

void 		AUTracer::SampleRateListener (void			*inRefCon, 
									AudioUnit			ci, 
									AudioUnitPropertyID inID, 
									AudioUnitScope		inScope, 
									AudioUnitElement	/*inElement*/)
{
	if (inScope == kAudioUnitScope_Output && inID == kAudioUnitProperty_SampleRate) {
		AUTracer* This = static_cast<AUTracer*>(inRefCon);
		UInt32 dataSize = sizeof (Float64);
		AudioUnitGetProperty (ci, kAudioUnitProperty_SampleRate,
								kAudioUnitScope_Output, 0, &This->mSampleRate, &dataSize);
	}
}

OSStatus 	AUTracer::OverloadTagger (void 							*inRefCon, 
										AudioUnitRenderActionFlags  *ioActionFlags, 
										const AudioTimeStamp 		* /*inTimeStamp*/, 
										UInt32 						inBusNumber, 
										UInt32 						/*inNumberFrames*/, 
										AudioBufferList 			* /*ioData*/)
{
	if (inBusNumber == 0) {
		if ((*ioActionFlags & kAudioUnitRenderAction_PreRender)) {
			AUTracer* This = static_cast<AUTracer*>(inRefCon);
			syscall(180, startTrace, 0, 0, 0, 0);
			This->mProfileRenderStartTime = CAHostTimeBase::GetTheCurrentTime();
		}
	}
	return noErr;
}


OSStatus 	AUTracer::OverlaodListenerProc(	AudioDeviceID	/*inDevice*/,
									UInt32					/*inChannel*/,
									Boolean					/*isInput*/,
									AudioDevicePropertyID	/*inPropertyID*/,
									void*					inClientData)
{
	AUTracer* This = static_cast<AUTracer*>(inClientData);
	This->DoOverload();
	return noErr;
}

void			AUTracer::DoOverload ()
{
	UInt64 now = CAHostTimeBase::GetTheCurrentTime();

	UInt64 isWindow = CAHostTimeBase::ConvertToNanos (mLastTimeWrote) + sTimeOut;
	if (isWindow > CAHostTimeBase::CAHostTimeBase::ConvertToNanos(now))
		return;
	
	syscall(180, endTrace, 0, 0, 0, 0);

//	UInt64 elapseTime = UInt64 ((1024.0 / GetSampleRate()) * 1000000000.0);
//	UInt64 htElapseTime = CAHostTimeBase::ConvertFromNanos (elapseTime);

	mLastTimeWrote = CAHostTimeBase::GetTheCurrentTime();
	
	mProfiler->Capture((mProfileRenderStartTime - s1msec), mLastTimeWrote, true, "Captured Latency Log for I/O Cycle Overload\n");
}



  
OSStatus 		AUTracer::CPUSpikeProfiler (void 			*inRefCon, 
								AudioUnitRenderActionFlags 	*ioActionFlags, 
								const AudioTimeStamp 		* /*inTimeStamp*/, 
								UInt32 						inBusNumber, 
								UInt32 						inNumberFrames, 
								AudioBufferList 			* /*ioData*/)
{		
	if (inBusNumber == 0) {
		if ((*ioActionFlags & kAudioUnitRenderAction_PreRender)) {
			AUTracer *This = (AUTracer*)inRefCon;
			syscall(180, startTrace, 0, 0, 0, 0);
			This->mProfileRenderStartTime = CAHostTimeBase::GetTheCurrentTime();
		} else if (*ioActionFlags & kAudioUnitRenderAction_PostRender) {
			AUTracer *This = (AUTracer*)inRefCon;
			This->DoSpikeAnalysis (inNumberFrames);
		}
	}
	return noErr;
}

void		AUTracer::DoSpikeAnalysis (UInt32 inNumberFrames)
{
	UInt64 now = CAHostTimeBase::GetTheCurrentTime();
	Float64 iocycleTime = inNumberFrames / GetSampleRate() * 1000000000.0;
	Float64 nanosUsage = CAHostTimeBase::ConvertToNanos(now - mProfileRenderStartTime);
	Float64 usage = nanosUsage / iocycleTime;

	if (usage > mSpikeLimit) {
		UInt64 isWindow = CAHostTimeBase::ConvertToNanos (mLastTimeWrote) + sTimeOut;
		if (isWindow > CAHostTimeBase::CAHostTimeBase::ConvertToNanos(now))
			return;

		syscall(180, endTrace, 0, 0, 0, 0);
	
		static char string[1024];
		double howLongAgoWrote = CAHostTimeBase::ConvertToNanos (now - mLastTimeWrote) / 1000000000.0;
		
		sprintf (string, "Captured Latency Log for Spiked I/O Cyclye: time taken=%.2lf%% (%.2lf mics), spikeLimit=%d%% (%.2lf mics)\n\tnumFrames=%ld, sampleRate=%d, duty cycle=%.2lf mics, last Wrote=%.0lf secs\n\n", 
				(usage*100.0), (nanosUsage / 1000.0), 
				int(mSpikeLimit * 100 + 0.5), (iocycleTime * mSpikeLimit / 1000.0), 
				inNumberFrames, (int)GetSampleRate(), (iocycleTime / 1000.0), howLongAgoWrote);
		
		mLastTimeWrote = CAHostTimeBase::GetTheCurrentTime();
		mProfiler->Capture ((mProfileRenderStartTime - s1msec), mLastTimeWrote, true, string);
		
		printf ("%s", string);
	}
}

