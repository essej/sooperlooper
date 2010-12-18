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
#ifndef __AUTracer__
#define __AUTracer__

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

#include "CALatencyLog.h"

/*
	To Use this:
		(1) You must execute the program as root
		(2)
		// you have to keep the tracer around of course :)
	static AUTracer tracer;
	
	bool profileOverload = ...;
		//ie. this is a boolean to set at your discretion as to whether you wish to profile
		// overloads or spikes in CPU Usage
		// to profile overloads, the AU you pass in must be an AUHAL or DefaultOutput AU
		// to use Spikes, first see what CPU load spike you wish to diagnose
		// for instance, normal load is say 2%, but every now and then, you see an 11% CPU load
		// so you want to find out if some system activity is causing this spike
		// so, you pass in 0.11 (which represents 11% of the I/O Duty Cycle)
		
	if (profileOverload) {
		printf ("\nProfiling Overload Latencies to log files: /tmp/MyOverloads.txt\n");
		tracer.Establish (myOutputAU, "/tmp/MyOverloadTraces");
	} else {
		printf ("\nProfiling Spike Load Latencies to log files: /tmp/MySpikeTraces.txt: threshold = %d %%\n", 11);
		tracer.Establish (myAU, "/tmp/MySpikeTraces", 0.11);
	}
		(3)
		This file requires the following files (all in PublicUtility)
				CALatencyLog (.cpp/.h)
				kdebug.h
				latency (.c/.h)
				
		(4)
		AUTracer::Establish ()
			AudioUnit inUnit 	=> the AU that you profile its total render time activities
			char* inFName		=> the FULL PATH of the trace files to generate
			Float64 inSpikeLimit 		=> if > 0 Spikes will be traced. if == 0 (default) overloads are traced
			UInt32 secsBetweenTraces 	=> Tracing can cause a cascading series of distrubances to the real time behaviour of the system. So, some period should elapse once a trace is taken before consequent traces are taken. A default delay value of 20 seconds is provided. This figure also represents the earliest time that a trace will be taken after the Establish call is made.
			
	The Trace Logs:
	Entries are entered into the trace log to determine the period within with the program has determined that the "bad" thing has happened.

	* Overload Traces:
The start of the I/O Cycle is generally indicated with an entry like the following:
    455.2    1.4[1089.4]    DECR_SET                                                          18a6c60  0  kernel_task
    456.9    1.7            MACH_SCHED    kernel_task @ pri   0  -->    MyExName @ pri  97K   18a6c60  0  **SCHEDULED**
	
If the I/O Cycle is attached to a device that has Audio Input, you should also see the following entries after the above:
    477.0     10.5          MSC_iokit_user_client_trap   3503       0          33bb      0    22ba000   0  MyExName
    487.6     10.6(10.6) 	MSC_iokit_user_client_trap   0          0                         22ba000   0  MyExName

In the above 22ba000 is the Thread ID of the I/O Thread. After the Input is read, the HAL calls your I/O Proc (the AU's Render actions will be invoked).

	When the program has received the I/O Cycle overload, it will enter a trace code "deadca04" - this appears as follows:

    1967.6    23.2			deadca04                     0          0          0         0     22ba000   0  MyExName
	
The time difference between the 456.9 and this trace entry (1967.6)	represents the time measured for that period, in this case it is 1510.7 microseconds.

	* Spike Logs:
When taking spike logs, an entry is added to the log ("deadca00") at the start of each render cycle. When a spike is detected, the "deadca04" trace entry is made. Thus, the activity between these two tags represents system activity that occured between these start and end times - this is the period of time and activity that you are interested in diagnosing.

For instance:
    498.4      4.0			deadca00                     0          0          0         0    22ba000   0  MyExName
		.. various entries...
		
    767.6     23.2			deadca04                     0          0          0         0    22ba000   0  MyExName

Some general information is added to the top of the trace log providing information about the conditions of this spike:

Captured Latency Log for Spiked I/O Cyclye: time taken=18.08% (262.43 mics), spikeLimit=11% (159.64 mics)
	numFrames=64, sampleRate=44100, duty cycle=1451.25 mics, last Wrote=36 secs
	
The time taken described above (264.43 mics) should be roughly equivalent to the difference between the two trace codes (787.6-498.4=269.2 mics) of the begin/end marker trace entries.

last Wrote=36 secs is the time elapsed since the last trace log (or if the first, since instantiation) was taken.
*/

class AUTracer {
public:
				AUTracer () : mProfiler (0), mSpikeLimit(0), mAU(0) {}
				~AUTracer ();
				
	OSStatus	Establish (AudioUnit inUnit, char* inFName, Float64 inSpikeLimit = 0, UInt32 secsBetweenTraces = 20);
	
private:
	CALatencyLog			*mProfiler;
	UInt64					mLastTimeWrote;
	Float64					mSpikeLimit;
	UInt64					mProfileRenderStartTime;
	AudioUnit				mAU;
	Float64					mSampleRate;
	
	static OSStatus 		CPUSpikeProfiler (void 					*inRefCon, 
										AudioUnitRenderActionFlags *ioActionFlags, 
										const AudioTimeStamp 		*inTimeStamp, 
										UInt32 						inBusNumber, 
										UInt32 						inNumberFrames, 
										AudioBufferList 			*ioData);

	static OSStatus 		OverloadTagger (void 					*inRefCon, 
										AudioUnitRenderActionFlags *ioActionFlags, 
										const AudioTimeStamp 		*inTimeStamp, 
										UInt32 						inBusNumber, 
										UInt32 						inNumberFrames, 
										AudioBufferList 			*ioData);

	void					DoSpikeAnalysis (UInt32 inNumberFrames);

	static OSStatus 		OverlaodListenerProc(	AudioDeviceID			inDevice,
													UInt32					inChannel,
													Boolean					isInput,
													AudioDevicePropertyID	inPropertyID,
													void*					inClientData);
	void					DoOverload ();

	static void 			SampleRateListener (void 					*inRefCon, 
												AudioUnit 				ci, 
												AudioUnitPropertyID 	inID, 
												AudioUnitScope 			inScope, 
												AudioUnitElement 		inElement);

	Float64					GetSampleRate() { return mSampleRate; }
};


#endif // __AUTracer__
