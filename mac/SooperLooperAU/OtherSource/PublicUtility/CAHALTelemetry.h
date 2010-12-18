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
#if !defined(__CAHALTelemetry_h__)
#define __CAHALTelemetry_h__

//=============================================================================
//	Includes
//=============================================================================

//	System Includes
#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreAudio/CoreAudioTypes.h>
#else
	#include <CoreAudioTypes.h>
#endif
#include <string.h>

//=============================================================================
//	CAHALIOCycleRawTelemetryEvent
//=============================================================================

struct	CAHALIOCycleRawTelemetryEvent
{
	UInt64	mEventTime;
	UInt32	mEventKind;
	UInt32	mIOCycleNumber;
	Float64	mRateScalar1;
	Float64	mRateScalar2;
	Float64	mSampleTime1;
	Float64	mSampleTime2;
	UInt64	mHostTime1;
	UInt64	mHostTime2;
	UInt32	mError;
	UInt32	mFlags;
	
	CAHALIOCycleRawTelemetryEvent() { memset(this, 0, sizeof(CAHALIOCycleRawTelemetryEvent)); }
	CAHALIOCycleRawTelemetryEvent(const CAHALIOCycleRawTelemetryEvent& inEvent) { memcpy(this, &inEvent, sizeof(CAHALIOCycleRawTelemetryEvent)); }
	CAHALIOCycleRawTelemetryEvent&	operator=(const CAHALIOCycleRawTelemetryEvent& inEvent) { memcpy(this, &inEvent, sizeof(CAHALIOCycleRawTelemetryEvent)); return *this; }

	struct	MatchEventKind
	{
		UInt32 mEventKind;
		MatchEventKind(UInt32 inEventKind) : mEventKind(inEventKind) {}
		bool operator()(const CAHALIOCycleRawTelemetryEvent& inEvent) const { return inEvent.mEventKind == mEventKind; }
	};
};

//=============================================================================
//	Constants
//=============================================================================

enum
{
	//	Message IDs
	kHALTelemetryMessageGetProperty						= 'getp',
	kHALTelemetryMessageSetProperty						= 'setp',
	kHALTelemetryMessageGetTelemetry					= 'tele',
	kHALTelemetryMessageClearTelemetry					= 'clrt',
	
	//	Property IDs
	kHALTelemetryPropertyIsCapturing					= 'capt',
	kHALTelemetryPropertyEndianness						= 'endi',
	
	//	Values for kHALTelemetryPropertyEndianness
	kHALTelemetryServerIsBigEndian						= 1UL,
	kHALTelemetryServerIsLittleEndian					= 0UL,
	
	//	Flags for CAHALIOCycleRawTelemetryEvent
	kHALIOCycleTelemetryFlagNoneIsValid					= 0,
	kHALIOCycleTelemetryFlagRateScalar1IsValid			= (1UL << 0),
	kHALIOCycleTelemetryFlagRateScalar2IsValid			= (1UL << 1),
	kHALIOCycleTelemetryFlagSampleTime1IsValid			= (1UL << 2),
	kHALIOCycleTelemetryFlagSampleTime2IsValid			= (1UL << 3),
	kHALIOCycleTelemetryFlagHostTime1IsValid			= (1UL << 4),
	kHALIOCycleTelemetryFlagHostTime2IsValid			= (1UL << 5),
	kHALIOCycleTelemetryFlagErrorIsValid				= (1UL << 6),
	kHALIOCycleTelemetryFlagTimeStamp1IsValid			= kHALIOCycleTelemetryFlagRateScalar1IsValid | kHALIOCycleTelemetryFlagSampleTime1IsValid | kHALIOCycleTelemetryFlagHostTime1IsValid,
	kHALIOCycleTelemetryFlagTimeStamp2IsValid			= kHALIOCycleTelemetryFlagRateScalar2IsValid | kHALIOCycleTelemetryFlagSampleTime2IsValid | kHALIOCycleTelemetryFlagHostTime2IsValid,
	kHALIOCycleTelemetryFlagBothTimeStampsAreValid		= kHALIOCycleTelemetryFlagTimeStamp1IsValid | kHALIOCycleTelemetryFlagTimeStamp2IsValid,

	//	Events specified in a CAHALIOCycleRawTelemetryEvent
	kHALIOCycleTelemetryEventInitializeBegin			= 'inib',	//	no data
	kHALIOCycleTelemetryEventInitializeEnd				= 'inie',	//	anchor time (kHALIOCycleTelemetryFlagHostTime1IsValid)
	kHALIOCycleTelemetryEventWorkLoopBegin				= 'wrkb',	//	now (kHALIOCycleTelemetryFlagHostTime1IsValid)
	kHALIOCycleTelemetryEventInputReadBegin				= 'redb',	//	input time (kHALIOCycleTelemetryFlagHostTime1IsValid)
	kHALIOCycleTelemetryEventInputReadEnd				= 'rede',	//	read error (kHALIOCycleTelemetryFlagErrorIsValid)
	kHALIOCycleTelemetryEventIOProcsBegin				= 'iopb',	//	no data
	kHALIOCycleTelemetryEventIOProcsEnd					= 'iope',	//	no data
	kHALIOCycleTelemetryEventIOProcCallBegin			= 'iiob',	//	address of IOProc (kHALIOCycleTelemetryFlagHostTime1IsValid)
	kHALIOCycleTelemetryEventIOProcCallEnd				= 'iioe',	//	no data
	kHALIOCycleTelemetryEventOutputWriteBegin			= 'wrib',	//	output time (kHALIOCycleTelemetryFlagHostTime1IsValid)
	kHALIOCycleTelemetryEventOutputWriteEnd				= 'wrie',	//	write error (kHALIOCycleTelemetryFlagErrorIsValid)
	kHALIOCycleTelemetryEventWorkLoopOverloadBegin		= 'ovrb',	//	current time, next wake time (kHALIOCycleTelemetryFlagBothTimeStampsAreValid)
	kHALIOCycleTelemetryEventWorkLoopOverloadEnd		= 'ovre',	//	anchor time (kHALIOCycleTelemetryFlagHostTime1IsValid)
	kHALIOCycleTelemetryEventWorkLoopEnd				= 'wrke',	//	current time, next wake time (kHALIOCycleTelemetryFlagBothTimeStampsAreValid)
	kHALIOCycleTelemetryEventTeardownBegin				= 'trdb',	//	no data
	kHALIOCycleTelemetryEventTeardownEnd				= 'trde',	//	no data
	kHALIOCycleTelemetryEventZeroTimeStampRecieved		= 'zror',	//	zero time (kHALIOCycleTelemetryFlagHostTime1IsValid)
	kHALIOCycleTelemetryEventZeroTimeStampApplied		= 'zroa',	//	zero time (kHALIOCycleTelemetryFlagHostTime1IsValid)
	kHALIOCycleTelemetryEventEarlyZeroTimeStamp			= 'zroe',	//	zero time (kHALIOCycleTelemetryFlagHostTime1IsValid)
	kHALIOCycleTelemetryEventOutOfBoundsZeroTimeStamp   = 'zrob',	//	zero time (kHALIOCycleTelemetryFlagHostTime1IsValid)
	kHALIOCycleTelemetryEventTimelineReset				= 'zres',   //  no data
	kHALIOCycleTelemetryEventResynch					= 'sync',   //  anchor time (kHALIOCycleTelemetryFlagHostTime1IsValid)
	kHALIOCycleTelemetryEventInputDataPresent			= 'inpt',   //  no data
	kHALIOCycleTelemetryEventOutputDataPresent			= 'outp',   //  no data
	kHALIOCycleTelemetryEventStartHardware				= 'gohw',   //  driver error (kHALIOCycleTelemetryFlagErrorIsValid)
	kHALIOCycleTelemetryEventStopHardware				= 'sthw',   //  driver error (kHALIOCycleTelemetryFlagErrorIsValid)
	kHALIOCycleTelemetryEventHardwareStarted			= 'hwgo',   //  no data
	kHALIOCycleTelemetryEventHardwareStopped			= 'hwst',   //  no data
	kHALIOCycleTelemetryEventHardwarePaused				= 'hwpa',   //  no data
	kHALIOCycleTelemetryEventHardwareResumed			= 'hwre',   //  no data
	kHALIOCycleTelemetryEventFormatChangeBegin			= 'fmtb',   //  no data
	kHALIOCycleTelemetryEventFormatChangeEnd			= 'fmte',   //  no data
	kHALIOCycleTelemetryEventMajorEngineChangeBegin		= 'mecb',	//  no data
	kHALIOCycleTelemetryEventMajorEngineChangeEnd		= 'mece'	//  no data
	
};

//	Strings
#define	kHALIOCycleTelemetryServerPortNameFormat		"com.apple.audio.CoreAudio.pid-%d.%lu.%qd.Telemetry.IOCycle"	//	args: pid, the CFHash of the device's UID, and the sum of all unicode characters in device UID
#define	kHALIOCycleTelemetryRunLoopMode					"IOCycleTelemetryMode"
#define	kHALIOCycleTelemetryReturnErrorKey				"return error"
#define	kHALIOCycleTelemetryPropertyIDKey				"property id"
#define	kHALIOCycleTelemetryPropertyValueKey			"property value"

#endif
