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
//=============================================================================
//	Includes
//=============================================================================

//	Self Include
#include "CAHALIOCycleTelemetryClient.h"

//	PublicUtility Includes
#include "CAAudioHardwareDevice.h"
#include "CACFArray.h"
#include "CACFData.h"
#include "CACFDictionary.h"
#include "CACFString.h"
#include "CACFMessagePort.h"
#include "CADebugMacros.h"
#include "CAException.h"
#include "CAHostTimeBase.h"
#include "CALatencyLog.h"

//	System Includes
#include <CoreAudio/HostTime.h>

//	Standard Library Includes
#include <math.h>
#include <algorithm>

//=============================================================================
//	CAHALIOCycleTelemetryEvent
//=============================================================================

static inline SInt64	SubtractUInt64(UInt64 inX, UInt64 inY)
{
	SInt64 theAnswer;
	if(inX >= inY)
	{
		theAnswer = inX - inY;
	}
	else
	{
		theAnswer = inY - inX;
		theAnswer *= -1;
	}
	return theAnswer;
}

static inline Float64	ConvertHostTimeToDisplayTime(SInt64 inHostTime)
{
	//	convert to milliseconds
	Float64 theAnswer = 0;
	if(inHostTime >= 0)
	{
		theAnswer = AudioConvertHostTimeToNanos(inHostTime);
	}
	else
	{
		theAnswer = AudioConvertHostTimeToNanos(-1 * inHostTime);
		theAnswer *= -1;
	}
	theAnswer /= 1000000.0;
	return theAnswer;
}

CAHALIOCycleTelemetry::CAHALIOCycleTelemetry()
:
	mLastCycleEnd(),
	mRawEvents()
{
}

CAHALIOCycleTelemetry::~CAHALIOCycleTelemetry()
{
}

UInt32	CAHALIOCycleTelemetry::GetIOCycleNumber() const
{
	UInt32 theAnswer = 0;
	
	if(mRawEvents.size() > 0)
	{
		theAnswer = mRawEvents.begin()->mIOCycleNumber;
	}
	
	return theAnswer;
}

UInt64	CAHALIOCycleTelemetry::GetIntendedStartTime() const
{
	return mLastCycleEnd.mHostTime2;
}

UInt64	CAHALIOCycleTelemetry::GetStartTime() const
{
	UInt64 theAnswer = 0;

	CAHALIOCycleRawTelemetryEvent theEvent;
	if(GetBeginRawEvent(theEvent))
	{
		if(theEvent.mEventKind == kHALIOCycleTelemetryEventWorkLoopBegin)
		{
			//	the time stamp in the data of the work loop begin event is more accurate
			theAnswer = theEvent.mHostTime1;
		}
		else
		{
			theAnswer = theEvent.mEventTime;
		}
	}
	
	return theAnswer;
}

UInt64	CAHALIOCycleTelemetry::GetEndTime() const
{
	UInt64 theAnswer = 0;

	CAHALIOCycleRawTelemetryEvent theEvent;
	if(GetEndRawEvent(theEvent))
	{
		if(theEvent.mEventKind == kHALIOCycleTelemetryEventWorkLoopEnd)
		{
			//	the time stamp in the data of the work loop end event is more accurate
			theAnswer = theEvent.mHostTime1;
		}
		else
		{
			theAnswer = theEvent.mEventTime;
		}
	}
	
	return theAnswer;
}

Float64	CAHALIOCycleTelemetry::GetDuration() const
{
	//	find the start time
	UInt64 theStartTime = 0;
	CAHALIOCycleRawTelemetryEvent theEvent;
	if(GetBeginRawEvent(theEvent))
	{
		if(theEvent.mEventKind == kHALIOCycleTelemetryEventWorkLoopBegin)
		{
			//	the time stamp in the data of the work loop begin event is more accurate
			theStartTime = theEvent.mHostTime1;
		}
		else
		{
			theStartTime = theEvent.mEventTime;
		}
	}
	
	//	find the end time
	UInt64 theEndTime = 0;
	if(GetEndRawEvent(theEvent))
	{
		theEndTime = theEvent.mEventTime;
	}
	
	return ConvertHostTimeToDisplayTime(theEndTime - theStartTime);
}

Float64	CAHALIOCycleTelemetry::GetRateScalar() const
{
	Float64 theAnswer = 0;
	
	//	any of the events that provide an IOProc time stamp should be good for this info
	//	except for the now time, since that is taken prior to the application of the
	//	zero time stamp, so go for the end event
	CAHALIOCycleRawTelemetryEvent theEvent;
	if(GetRawEventByKind(kHALIOCycleTelemetryEventWorkLoopEnd, theEvent))
	{
		theAnswer = theEvent.mRateScalar1;
	}
	
	return theAnswer;
}

void	CAHALIOCycleTelemetry::GetIOProcTimes(AudioTimeStamp& outNow, AudioTimeStamp& outInputTime, AudioTimeStamp& outOutputTime) const
{
	CAHALIOCycleRawTelemetryEvent theEvent;

	//	now is with the work loop begin event
	memset(&outNow, 0, sizeof(AudioTimeStamp));
	if(GetRawEventByKind(kHALIOCycleTelemetryEventWorkLoopBegin, theEvent))
	{
		outNow.mRateScalar = theEvent.mRateScalar1;
		outNow.mSampleTime = theEvent.mSampleTime1;
		outNow.mHostTime = theEvent.mHostTime1;
		outNow.mFlags = kAudioTimeStampSampleTimeValid | kAudioTimeStampHostTimeValid | kAudioTimeStampRateScalarValid;
	}

	//	input time is with the read begin event
	memset(&outInputTime, 0, sizeof(AudioTimeStamp));
	if(GetRawEventByKind(kHALIOCycleTelemetryEventInputReadBegin, theEvent))
	{
		outInputTime.mRateScalar = theEvent.mRateScalar1;
		outInputTime.mSampleTime = theEvent.mSampleTime1;
		outInputTime.mHostTime = theEvent.mHostTime1;
		outInputTime.mFlags = kAudioTimeStampSampleTimeValid | kAudioTimeStampHostTimeValid | kAudioTimeStampRateScalarValid;
	}

	//	output time is with the write begin event
	memset(&outOutputTime, 0, sizeof(AudioTimeStamp));
	if(GetRawEventByKind(kHALIOCycleTelemetryEventOutputWriteBegin, theEvent))
	{
		outOutputTime.mRateScalar = theEvent.mRateScalar1;
		outOutputTime.mSampleTime = theEvent.mSampleTime1;
		outOutputTime.mHostTime = theEvent.mHostTime1;
		outOutputTime.mFlags = kAudioTimeStampSampleTimeValid | kAudioTimeStampHostTimeValid | kAudioTimeStampRateScalarValid;
	}
}

bool	CAHALIOCycleTelemetry::HasError() const
{
	bool theAnswer = false;
	RawEventList::const_iterator theIterator = mRawEvents.begin();
	while(!theAnswer && (theIterator != mRawEvents.end()))
	{
		theAnswer = CAHALIOCycleTelemetryClient::IsRawEventError(*theIterator);
		std::advance(theIterator, 1);
	}
	return theAnswer;
}

bool	CAHALIOCycleTelemetry::HasOverload() const
{
	CAHALIOCycleRawTelemetryEvent theEvent;
	bool theAnswer = GetRawEventByKind(kHALIOCycleTelemetryEventWorkLoopOverloadBegin, theEvent);
	if(!theAnswer)
	{
		theAnswer = GetRawEventByKind(kHALIOCycleTelemetryEventWorkLoopOverloadEnd, theEvent);
	}
	return theAnswer;
}

bool	CAHALIOCycleTelemetry::HasInputError() const
{
	//	the input error is stored with the read end event
	bool theAnswer = false;
	CAHALIOCycleRawTelemetryEvent theEvent;
	if(GetRawEventByKind(kHALIOCycleTelemetryEventInputReadEnd, theEvent))
	{
		theAnswer = theEvent.mError != 0;
	}
	return theAnswer;
}

bool	CAHALIOCycleTelemetry::HasOutputError() const
{
	//	the output error is stored with the write end event
	bool theAnswer = false;
	CAHALIOCycleRawTelemetryEvent theEvent;
	if(GetRawEventByKind(kHALIOCycleTelemetryEventOutputWriteEnd, theEvent))
	{
		theAnswer = theEvent.mError != 0;
	}
	return theAnswer;
}

bool	CAHALIOCycleTelemetry::HasZeroTime() const
{
	bool theAnswer = false;
	CAHALIOCycleRawTelemetryEvent theEvent;
	theAnswer = GetRawEventByKind(kHALIOCycleTelemetryEventZeroTimeStampRecieved, theEvent);
	return theAnswer;
}

bool	CAHALIOCycleTelemetry::HasSignal() const
{
	bool theAnswer = false;
	CAHALIOCycleRawTelemetryEvent theEvent;
	theAnswer = GetRawEventByKind(kHALIOCycleTelemetryEventInputDataPresent, theEvent);
	if(!theAnswer)
	{
		theAnswer = GetRawEventByKind(kHALIOCycleTelemetryEventOutputDataPresent, theEvent);
	}
	return theAnswer;
}

bool	CAHALIOCycleTelemetry::GetZeroTimeStamp(AudioTimeStamp& outZeroTime) const
{
	bool theAnswer = false;
	CAHALIOCycleRawTelemetryEvent theEvent;
	if(GetRawEventByKind(kHALIOCycleTelemetryEventZeroTimeStampRecieved, theEvent))
	{
		outZeroTime.mRateScalar = theEvent.mRateScalar1;
		outZeroTime.mSampleTime = theEvent.mSampleTime1;
		outZeroTime.mHostTime = theEvent.mHostTime1;
		outZeroTime.mFlags = kAudioTimeStampSampleTimeValid | kAudioTimeStampHostTimeValid | kAudioTimeStampRateScalarValid;
		theAnswer = true;
	}
	return theAnswer;
}

bool	CAHALIOCycleTelemetry::GetNextWakeUpTime(AudioTimeStamp& outWakeTime) const
{
	//	the next wake up time is stored with the work loop end event
	bool theAnswer = false;
	CAHALIOCycleRawTelemetryEvent theEvent;
	if(GetRawEventByKind(kHALIOCycleTelemetryEventWorkLoopEnd, theEvent))
	{
		outWakeTime.mRateScalar = theEvent.mRateScalar1;
		outWakeTime.mSampleTime = theEvent.mSampleTime1;
		outWakeTime.mHostTime = theEvent.mHostTime1;
		outWakeTime.mFlags = kAudioTimeStampSampleTimeValid | kAudioTimeStampHostTimeValid | kAudioTimeStampRateScalarValid;
		theAnswer = true;
	}
	return theAnswer;
}

Float64	CAHALIOCycleTelemetry::GetTotalLoad() const
{
	return 0;
}

Float64	CAHALIOCycleTelemetry::GetSchedulingLoad() const
{
	return 0;
}

Float64	CAHALIOCycleTelemetry::GetReadLoad() const
{
	return 0;
}

Float64	CAHALIOCycleTelemetry::GetIOProcLoad() const
{
	return 0;
}

Float64	CAHALIOCycleTelemetry::GetWriteLoad() const
{
	return 0;
}

bool	CAHALIOCycleTelemetry::AssimilateRawEvent(const CAHALIOCycleRawTelemetryEvent& inRawEvent)
{
	bool theAnswer = false;
	
	if(mRawEvents.size() > 0)
	{
		//	make sure the new one has the same IO cycle number
		if(inRawEvent.mIOCycleNumber == mRawEvents.begin()->mIOCycleNumber)
		{
			mRawEvents.push_back(inRawEvent);
			theAnswer = true;
		}
	}
	else
	{
		//	first raw event presented for assimilation
		mRawEvents.push_back(inRawEvent);
		theAnswer = true;
	}
	
	return theAnswer;
}

void	CAHALIOCycleTelemetry::SetLastCycleEnd(const CAHALIOCycleRawTelemetryEvent& inRawEvent)
{
	mLastCycleEnd = inRawEvent;
}
	
bool	CAHALIOCycleTelemetry::GetRawEventByKind(UInt32 inEventKind, CAHALIOCycleRawTelemetryEvent& outRawEvent) const
{
	bool theAnswer = false;
	
	RawEventList::const_iterator theIterator = std::find_if(mRawEvents.begin(), mRawEvents.end(), CAHALIOCycleRawTelemetryEvent::MatchEventKind(inEventKind));
	if(theIterator != mRawEvents.end())
	{
		outRawEvent = *theIterator;
		theAnswer = true;
	}
	
	return theAnswer;
}

bool	CAHALIOCycleTelemetry::GetBeginRawEvent(CAHALIOCycleRawTelemetryEvent& outRawEvent) const
{
	bool theAnswer = false;
	CAHALIOCycleRawTelemetryEvent theEvent;
	if(GetRawEventByKind(kHALIOCycleTelemetryEventWorkLoopBegin, theEvent))
	{
		outRawEvent = theEvent;
		theAnswer = true;
	}
	else if(GetRawEventByKind(kHALIOCycleTelemetryEventInitializeBegin, theEvent))
	{
		outRawEvent = theEvent;
		theAnswer = true;
	}
	else if(mRawEvents.size() > 0)
	{
		outRawEvent = mRawEvents.front();
		theAnswer = true;
	}
	return theAnswer;
}

bool	CAHALIOCycleTelemetry::GetEndRawEvent(CAHALIOCycleRawTelemetryEvent& outRawEvent) const
{
	bool theAnswer = false;
	CAHALIOCycleRawTelemetryEvent theEvent;
	if(GetRawEventByKind(kHALIOCycleTelemetryEventWorkLoopEnd, theEvent))
	{
		outRawEvent = theEvent;
		theAnswer = true;
	}
	else if(mRawEvents.size() > 0)
	{
		outRawEvent = mRawEvents.back();
		theAnswer = true;
	}
	return theAnswer;
}

UInt32	CAHALIOCycleTelemetry::GetNumberRawEvents() const
{
	return mRawEvents.size();
}

bool	CAHALIOCycleTelemetry::GetRawEventByIndex(UInt32 inEventIndex, CAHALIOCycleRawTelemetryEvent& outRawEvent) const
{
	bool theAnswer = false;
	RawEventList::const_iterator theIterator = mRawEvents.begin();
	std::advance(theIterator, inEventIndex);
	if(theIterator != mRawEvents.end())
	{
		outRawEvent = *theIterator;
		theAnswer = true;
	}
	return theAnswer;
}

//=============================================================================
//	CAHALIOCycleTelemetryClient
//=============================================================================

static void	CAHALIOCycleTelemetryClient_MessagePortIsInvalid(CFMessagePortRef inMessagePort, void* inInfo);

CAHALIOCycleTelemetryClient::CAHALIOCycleTelemetryClient(const char* inLatencyTraceFileName, const char* inLatencyTraceFileNameExtension)
:
	mMessagePort(NULL),
	mServerIsFlipped(false),
	mRawTelemetry(),
	mIOCycleList(),
	mErrorCycleList(),
	mZerosCycleList(),
	mLatencyTraceFileName(inLatencyTraceFileName),
	mLatencyTraceFileNameExtension(inLatencyTraceFileNameExtension),
	mLatencyTracingEnabled(false),
	mTraceTaken(false),
	mLatencyLog(NULL),
	mIOThreadSchedulingLatencyTrigger(0),
	mIOCycleDurationTrigger(0),
	mOverloadTrigger(false)
{
}

CAHALIOCycleTelemetryClient::~CAHALIOCycleTelemetryClient()
{
	if(mLatencyLog != NULL)
	{
		delete mLatencyLog;
		mLatencyLog = NULL;
	}
}

bool	CAHALIOCycleTelemetryClient::Initialize(pid_t inProcess, AudioDeviceID inDevice)
{
	bool theAnswer = false;
	
	if(mMessagePort == NULL)
	{
		CAAudioHardwareDevice theDevice(inDevice);
		CACFString theUID(theDevice.CopyUID());
		CACFString thePortName(CreatePortName(inProcess, theUID.GetCFString()));
		mMessagePort = new CACFRemoteMessagePort(thePortName.GetCFString(), (CFMessagePortInvalidationCallBack)CAHALIOCycleTelemetryClient_MessagePortIsInvalid);
		if(mMessagePort->IsValid())
		{
			try
			{
				//	get the endianness of the HAL
				bool theServerIsFlipped = false;
				OSStatus theError = GetServerIsFlippedFromServer(theServerIsFlipped);
				ThrowIfError(theError, CAException(theError), "CAHALIOCycleTelemetryClient::Initialize: couldn't get the endianness of the server");
				mServerIsFlipped = theServerIsFlipped;
				
				//	tell the HAL to enable telemetry
				theError = SetIsEnabledOnServer(true);
				ThrowIfError(theError, CAException(theError), "CAHALIOCycleTelemetryClient::Initialize: couldn't enable telemetry on the server");
				
				//	clear out the existing data
				Clear();
			}
			catch(...)
			{
				delete mMessagePort;
				mMessagePort = NULL;
			}
		}
		else
		{
			delete mMessagePort;
			mMessagePort = NULL;
		}
	}
	
	mTraceTaken = false;
	
	return theAnswer;
}

void	CAHALIOCycleTelemetryClient::Teardown()
{
	Clear();
				
	if(mMessagePort != NULL)
	{
		UInt32 theError = SetIsEnabledOnServer(false);
		ThrowIfError(theError, CAException(theError), "CAHALIOCycleTelemetryClient::Teardown: couldn't disable telemetry on the server");
	
		delete mMessagePort;
		mMessagePort = NULL;
	}
	
	mTraceTaken = false;
}

bool	CAHALIOCycleTelemetryClient::IsEnabled() const
{
	bool theAnswer = mMessagePort != NULL;
	
	if(theAnswer)
	{
		theAnswer = mMessagePort->IsValid();
	}
		
	return theAnswer;
}

static inline Float64	SwapFloat64(Float64 inNumber)
{
	union FakeFloat64
	{
		Float64 mFloat64;
		UInt64	mUInt64;
	} theAnswer;
	theAnswer.mFloat64 = inNumber;
		
	theAnswer.mUInt64 = CFSwapInt64(theAnswer.mUInt64);
	return theAnswer.mFloat64;
}

CFStringRef	CAHALIOCycleTelemetryClient::CreatePortName(pid_t inProcessID, CFStringRef inUID)
{
	//	get the CFHash of the UID
	UInt32 theHashCode = CFHash(inUID);
	
	//	sum all the characters in the UID
	UInt64 theSum = 0;
	UInt32 theNumberCharacters = CFStringGetLength(inUID);
	for(UInt32 theCharacterIndex = 0; theCharacterIndex < theNumberCharacters; ++theCharacterIndex)
	{
		UniChar theCharacter = CFStringGetCharacterAtIndex(inUID, theCharacterIndex);
		theSum += theCharacter;
	}
	
	//	build a string out of the hash code and character sum
	CFStringRef thePortName = CFStringCreateWithFormat(NULL, NULL, CFSTR(kHALIOCycleTelemetryServerPortNameFormat), inProcessID, theHashCode, theSum);
	
	return thePortName;
}

bool	CAHALIOCycleTelemetryClient::Update()
{
	bool theAnswer = false;
	try
	{
		ThrowIfNULL(mMessagePort, CAException(kAudioHardwareIllegalOperationError), "CAHALIOCycleTelemetryClient::Update: no port");
		
		UInt32 theNumberEvents = 0;
		do
		{
			//	build the request
			CACFDictionary theMessageData(true);
			CACFData theMessageCFData(CFPropertyListCreateXMLData(NULL, theMessageData.GetCFDictionary()));
			
			//	send the request to the HAL
			CFDataRef __theReturnCFData = NULL;
			SInt32 theError = mMessagePort->SendRequest(kHALTelemetryMessageGetTelemetry, theMessageCFData.GetCFData(), 10.0, 10.0, kCFRunLoopDefaultMode, __theReturnCFData);
			ThrowIfError(theError, CAException(theError), "CAHALIOCycleTelemetryClient::Update: sending the request failed");
			ThrowIfNULL(__theReturnCFData, CAException(kAudioHardwareUnspecifiedError), "CAHALIOCycleTelemetryClient::Update: no return result from the server");
			
			//	the returned data is a buffer full of CAHALIOCycleRawTelemetryEvent structs
			CACFData theReturnCFData(__theReturnCFData);
			UInt32 theSize = theReturnCFData.GetSize();
			theNumberEvents = theSize / sizeof(CAHALIOCycleRawTelemetryEvent);
			
			//	catalog the new events
			for(UInt32 theEventIndex = 0; theEventIndex < theNumberEvents; ++theEventIndex)
			{
				//	get the event out of the array
				CAHALIOCycleRawTelemetryEvent theEvent;
				theReturnCFData.CopyData(theEventIndex * sizeof(CAHALIOCycleRawTelemetryEvent), &theEvent, sizeof(CAHALIOCycleRawTelemetryEvent));
				
				//	flip it if necessary
				if(mServerIsFlipped)
				{
					theEvent.mEventTime = CFSwapInt64(theEvent.mEventTime);
					theEvent.mEventKind = CFSwapInt32(theEvent.mEventKind);
					theEvent.mIOCycleNumber = CFSwapInt32(theEvent.mIOCycleNumber);
					theEvent.mRateScalar1 = SwapFloat64(theEvent.mRateScalar1);
					theEvent.mRateScalar2 = SwapFloat64(theEvent.mRateScalar2);
					theEvent.mSampleTime1 = SwapFloat64(theEvent.mSampleTime1);
					theEvent.mSampleTime2 = SwapFloat64(theEvent.mSampleTime2);
					theEvent.mHostTime1 = CFSwapInt64(theEvent.mHostTime1);
					theEvent.mHostTime2 = CFSwapInt64(theEvent.mHostTime2);
					theEvent.mError = CFSwapInt64(theEvent.mError);
					theEvent.mFlags = CFSwapInt64(theEvent.mFlags);
				}
				
				//	stick it into the list
				mRawTelemetry.push_back(theEvent);
				
				//	figure out which cycle telemetry this raw event belongs to
				
				//	starting with the last one on the list
				bool wasPlaced = false;
				if(mIOCycleList.size() > 0)
				{
					wasPlaced = mIOCycleList.back().AssimilateRawEvent(theEvent);
				}
				
				//	didn't belong there
				if(!wasPlaced)
				{
					//	so make a new one
					CAHALIOCycleTelemetry theIOCycle;
					
					//	assimilate this event
					theIOCycle.AssimilateRawEvent(theEvent);
					
					//	set the end event
					CAHALIOCycleRawTelemetryEvent theLastEvent;
					if(mIOCycleList.size() > 0)
					{
						wasPlaced = mIOCycleList.back().GetEndRawEvent(theLastEvent);
					}
					theIOCycle.SetLastCycleEnd(theLastEvent);
					
					//	push it onto the list
					mIOCycleList.push_back(theIOCycle);
				}
				
				//	do the latency tracing
				if(mLatencyTracingEnabled && (mLatencyLog != NULL) && !mTraceTaken)
				{
					//	get some values we're about to use
					UInt64 theStartTime = mIOCycleList.back().GetStartTime();
					UInt64 theEndTime = mIOCycleList.back().GetEndTime();
					UInt64 theIntendedStartTime = mIOCycleList.back().GetIntendedStartTime();
					bool hasOverload = mIOCycleList.back().HasOverload();
					
					//	check for a scheduling latency in the IO thread
					if((mIOThreadSchedulingLatencyTrigger != 0) && (theIntendedStartTime > 0))
					{
						//	calculate the lateness (in milliseconds)
						Float64 theLateness = ConvertHostTimeToDisplayTime(SubtractUInt64(theStartTime, theIntendedStartTime));
						if(theLateness > mIOThreadSchedulingLatencyTrigger)
						{
							//	log it
							mLatencyLog->Capture(theIntendedStartTime - CAHostTimeBase::ConvertFromNanos(10 * 1000 * 1000), theStartTime + CAHostTimeBase::ConvertFromNanos(1 * 1000 * 1000), true);
							
							//	print how late we are
							DebugMessageN1("Woke up late by %f milliseconds, logging trace...", theLateness);
							
							mTraceTaken = true;
						}
					}
					
					//	check to see if the cycle took too long
					if((mIOCycleDurationTrigger != 0) && (theStartTime != 0) && (theEndTime != 0))
					{
						//	get the duration of the cycle (in milliseconds)
						Float64 theDuration = mIOCycleList.back().GetDuration();
						if(theDuration > mIOCycleDurationTrigger)
						{
							//	log it
							mLatencyLog->Capture(theStartTime - CAHostTimeBase::ConvertFromNanos(1 * 1000 * 1000), theEndTime + CAHostTimeBase::ConvertFromNanos(1 * 1000 * 1000), true);
							
							//	print how late we are
							DebugMessageN1("IO cycle took %f milliseconds, logging trace...", theDuration);
							
							mTraceTaken = true;
						}
					}
					
					//	check to see if an overload took place
					if(mOverloadTrigger && hasOverload && (theStartTime != 0) && (theEndTime != 0))
					{
						//	log it
						mLatencyLog->Capture(theStartTime - CAHostTimeBase::ConvertFromNanos(1 * 1000 * 1000), theEndTime + CAHostTimeBase::ConvertFromNanos(1 * 1000 * 1000), true);
						
						//	print how late we are
						DebugMessage("IO cycle had an overload, logging trace...");
							
						mTraceTaken = true;
					}
				}
			}
			theAnswer = theAnswer || (theNumberEvents > 0);
		}
		while(theNumberEvents > 0);
	}
	catch(const CAException& inException)
	{
	}
	catch(...)
	{
	}
	
	return theAnswer;
}

void	CAHALIOCycleTelemetryClient::Clear(bool inOnServer)
{
	mRawTelemetry.clear();
	mIOCycleList.clear();
	mErrorCycleList.clear();
	mZerosCycleList.clear();
	if(inOnServer)
	{
		ClearDataOnServer();
	}
	mTraceTaken = false;
}

UInt32	CAHALIOCycleTelemetryClient::GetNumberIOCycles() const
{
	return mIOCycleList.size();
}

UInt32	CAHALIOCycleTelemetryClient::GetPreviousErrorIOCycleIndex(UInt32 inCurrentIndex) const
{
	UInt32 theAnswer = 0xFFFFFFFF;
	IOCycleList::const_iterator theIOCycleIterator = mIOCycleList.begin();
	if(inCurrentIndex < mIOCycleList.size())
	{
		std::advance(theIOCycleIterator, inCurrentIndex);
	}
	if(theIOCycleIterator->HasError())
	{
		--inCurrentIndex;
		--theIOCycleIterator;
	}
	while((theAnswer == 0xFFFFFFFF) && (theIOCycleIterator != mIOCycleList.begin()))
	{
		if(theIOCycleIterator->HasError())
		{
			theAnswer = inCurrentIndex;
		}
		else
		{
			--inCurrentIndex;
			--theIOCycleIterator;
		}
	}
	
	return theAnswer;
}

UInt32	CAHALIOCycleTelemetryClient::GetNextErrorIOCycleIndex(UInt32 inCurrentIndex) const
{
	UInt32 theAnswer = 0xFFFFFFFF;
	IOCycleList::const_iterator theIOCycleIterator = mIOCycleList.begin();
	if(inCurrentIndex < mIOCycleList.size())
	{
		std::advance(theIOCycleIterator, inCurrentIndex);
	}
	if(theIOCycleIterator->HasError())
	{
		++inCurrentIndex;
		std::advance(theIOCycleIterator, 1);
	}
	while((theAnswer == 0xFFFFFFFF) && (theIOCycleIterator != mIOCycleList.end()))
	{
		if(theIOCycleIterator->HasError())
		{
			theAnswer = inCurrentIndex;
		}
		else
		{
			++inCurrentIndex;
			std::advance(theIOCycleIterator, 1);
		}
	}
	
	return theAnswer;
}

UInt32	CAHALIOCycleTelemetryClient::GetNumberEventsInIOCycle(UInt32 inIOCycleIndex) const
{
	UInt32 theAnswer = 0;
	
	IOCycleList::const_iterator theIOCycleIterator = mIOCycleList.begin();
	std::advance(theIOCycleIterator, inIOCycleIndex);
	if(theIOCycleIterator != mIOCycleList.end())
	{
		theAnswer = theIOCycleIterator->GetNumberRawEvents();
	}
	
	return theAnswer;
}

bool	CAHALIOCycleTelemetryClient::IOCycleHasError(UInt32 inIOCycleIndex) const
{
	bool theAnswer = false;
	IOCycleList::const_iterator theIOCycleIterator = mIOCycleList.begin();
	std::advance(theIOCycleIterator, inIOCycleIndex);
	if(theIOCycleIterator != mIOCycleList.end())
	{
		theAnswer = theIOCycleIterator->HasError();
	}
	return theAnswer;
}

bool	CAHALIOCycleTelemetryClient::IOCycleHasSignal(UInt32 inIOCycleIndex) const
{
	bool theAnswer = false;
	IOCycleList::const_iterator theIOCycleIterator = mIOCycleList.begin();
	std::advance(theIOCycleIterator, inIOCycleIndex);
	if(theIOCycleIterator != mIOCycleList.end())
	{
		theAnswer = theIOCycleIterator->HasSignal();
	}
	return theAnswer;
}

bool	CAHALIOCycleTelemetryClient::EventInIOCycleHasError(UInt32 inIOCycleIndex, UInt32 inEventIndex) const
{
	bool theAnswer = false;
	IOCycleList::const_iterator theIOCycleIterator = mIOCycleList.begin();
	std::advance(theIOCycleIterator, inIOCycleIndex);
	if(theIOCycleIterator != mIOCycleList.end())
	{
		CAHALIOCycleRawTelemetryEvent theEvent;
		if(theIOCycleIterator->GetRawEventByIndex(inEventIndex, theEvent))
		{
			theAnswer = IsRawEventError(theEvent);
		}
	}
	return theAnswer;
}

bool	CAHALIOCycleTelemetryClient::EventInIOCycleHasSignal(UInt32 inIOCycleIndex, UInt32 inEventIndex) const
{
	bool theAnswer = false;
	IOCycleList::const_iterator theIOCycleIterator = mIOCycleList.begin();
	std::advance(theIOCycleIterator, inIOCycleIndex);
	if(theIOCycleIterator != mIOCycleList.end())
	{
		CAHALIOCycleRawTelemetryEvent theEvent;
		if(theIOCycleIterator->GetRawEventByIndex(inEventIndex, theEvent))
		{
			theAnswer = IsRawEventSignal(theEvent);
		}
	}
	return theAnswer;
}

void	CAHALIOCycleTelemetryClient::CreateSummaryHeaderForIOCycle(char* outSummary, bool inForSpreadSheet) const
{
	if(!inForSpreadSheet)
	{
		sprintf(outSummary, "  %12s (%7s %7s) %8s  {%9s, %12s} {%9s, %12s} {%9s, %12s} {%9s, %12s} {%9s, %12s}", "Start", "Offset", "Late", "Rate", "Now:samp", "Now:host", "In:samp", "In:host", "Out:samp", "Out:host", "Wake:samp", "Wake:host", "Zero:samp", "Zero:host");
	}
	else
	{
		sprintf(outSummary, "%s\t%7s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\r", "Start", "Offset", "Late", "Rate", "Now:samp", "Now:host", "In:samp", "In:host", "Out:samp", "Out:host", "Wake:samp", "Wake:host", "Zero:samp", "Zero:host");
	}
}

void	CAHALIOCycleTelemetryClient::CreateSummaryForIOCycle(UInt32 inIOCycleIndex, char* outSummary, bool inForSpreadSheet) const
{
	UInt64 theAnchorTime = mRawTelemetry.front().mEventTime;
	IOCycleList::const_iterator theIOCycleIterator = mIOCycleList.begin();
	std::advance(theIOCycleIterator, inIOCycleIndex);
	if(theIOCycleIterator != mIOCycleList.end())
	{
		IOCycleList::const_iterator theIOCyclePreviousIterator = theIOCycleIterator;
		if(inIOCycleIndex > 0)
		{
			--theIOCyclePreviousIterator;
		}
		AudioTimeStamp theNow, theInput, theOutput, theNext;
		theIOCycleIterator->GetIOProcTimes(theNow, theInput, theOutput);
		theIOCycleIterator->GetNextWakeUpTime(theNext);
		Float64 theStartTime = ConvertHostTimeToDisplayTime(SubtractUInt64(theIOCycleIterator->GetStartTime(), theAnchorTime));
		Float64 theOffsetFromPreviousStartTime = ConvertHostTimeToDisplayTime(SubtractUInt64(theIOCycleIterator->GetStartTime(), theIOCyclePreviousIterator->GetStartTime()));
		Float64 theLateness = (theIOCycleIterator->GetIntendedStartTime() > 0) ? ConvertHostTimeToDisplayTime(SubtractUInt64(theIOCycleIterator->GetStartTime(), theIOCycleIterator->GetIntendedStartTime())) : 0;
		Float64	theRateScalar = theIOCycleIterator->GetRateScalar();
		Float64 theNowHostTime = ((theNow.mFlags & kAudioTimeStampHostTimeValid) != 0) ? ConvertHostTimeToDisplayTime(SubtractUInt64(theNow.mHostTime, theAnchorTime)) : 0;
		Float64 theInputHostTime = ((theInput.mFlags & kAudioTimeStampHostTimeValid) != 0) ? ConvertHostTimeToDisplayTime(SubtractUInt64(theInput.mHostTime, theAnchorTime)) : 0;
		Float64 theOutputHostTime = ((theOutput.mFlags & kAudioTimeStampHostTimeValid) != 0) ? ConvertHostTimeToDisplayTime(SubtractUInt64(theOutput.mHostTime, theAnchorTime)) : 0;
		Float64 theNextHostTime = ((theNext.mFlags & kAudioTimeStampHostTimeValid) != 0) ? ConvertHostTimeToDisplayTime(SubtractUInt64(theNext.mHostTime, theAnchorTime)) : 0;
		if(!theIOCycleIterator->HasZeroTime())
		{
			if(!inForSpreadSheet)
			{
				sprintf(outSummary, "%12.3f (%+7.3f %+7.3f) %-9.6f {%9.0f, %12.3f} {%9.0f, %12.3f} {%9.0f, %12.3f} {%9.0f, %12.3f}", theStartTime, theOffsetFromPreviousStartTime, theLateness, theRateScalar, theNow.mSampleTime, theNowHostTime, theInput.mSampleTime, theInputHostTime, theOutput.mSampleTime, theOutputHostTime, theNext.mSampleTime, theNextHostTime);
			}
			else
			{
				sprintf(outSummary, "%.3f\t%.3f\t%.3f\t%.6f\t%.0f\t%.3f\t%.0f\t%.3f\t%.0f\t%.3f\t%.0f\t%.3f\r", theStartTime, theOffsetFromPreviousStartTime, theLateness, theRateScalar, theNow.mSampleTime, theNowHostTime, theInput.mSampleTime, theInputHostTime, theOutput.mSampleTime, theOutputHostTime, theNext.mSampleTime, theNextHostTime);
			}
		}
		else
		{
			AudioTimeStamp theZero;
			theIOCycleIterator->GetZeroTimeStamp(theZero);
			Float64 theZeroHostTime = ConvertHostTimeToDisplayTime(SubtractUInt64(theZero.mHostTime, theAnchorTime));
			if(!inForSpreadSheet)
			{
				sprintf(outSummary, "%12.3f (%+7.3f %+7.3f) %-9.6f {%9.0f, %12.3f} {%9.0f, %12.3f} {%9.0f, %12.3f} {%9.0f, %12.3f} {%9.0f, %12.3f}", theStartTime, theOffsetFromPreviousStartTime, theLateness, theRateScalar, theNow.mSampleTime, theNowHostTime, theInput.mSampleTime, theInputHostTime, theOutput.mSampleTime, theOutputHostTime, theNext.mSampleTime, theNextHostTime, theZero.mSampleTime, theZeroHostTime);
			}
			else
			{
				sprintf(outSummary, "%.3f\t%.3f\t%.3f\t%.6f\t%.0f\t%.3f\t%.0f\t%.3f\t%.0f\t%.3f\t%.0f\t%.3f\t%.0f\t%.3f\r", theStartTime, theOffsetFromPreviousStartTime, theLateness, theRateScalar, theNow.mSampleTime, theNowHostTime, theInput.mSampleTime, theInputHostTime, theOutput.mSampleTime, theOutputHostTime, theNext.mSampleTime, theNextHostTime, theZero.mSampleTime, theZeroHostTime);
			}
		}
	}
}

void	CAHALIOCycleTelemetryClient::CreateSummaryForEventInIOCycle(UInt32 inIOCycleIndex, UInt32 inEventIndex, char* outSummary) const
{
	UInt64 theAnchorTime = mRawTelemetry.front().mEventTime;
	IOCycleList::const_iterator theIOCycleIterator = mIOCycleList.begin();
	std::advance(theIOCycleIterator, inIOCycleIndex);
	if(theIOCycleIterator != mIOCycleList.end())
	{
		CAHALIOCycleRawTelemetryEvent theEvent;
		if(theIOCycleIterator->GetRawEventByIndex(inEventIndex, theEvent))
		{
			CAHALIOCycleRawTelemetryEvent thePreviousEvent = theEvent;
			if(inEventIndex > 0)
			{
				theIOCycleIterator->GetRawEventByIndex(inEventIndex - 1, thePreviousEvent);
			}
			CreateSummaryForRawEvent(theEvent, thePreviousEvent, theAnchorTime, outSummary);
		}
	}
}

UInt32	CAHALIOCycleTelemetryClient::GetNumberRawEvents() const
{
	return mRawTelemetry.size();
}

UInt32	CAHALIOCycleTelemetryClient::GetPreviousErrorRawEventIndex(UInt32 inCurrentIndex) const
{
	UInt32 theAnswer = 0xFFFFFFFF;
	EventList::const_iterator theIOCycleIterator = mRawTelemetry.begin();
	if(inCurrentIndex < mRawTelemetry.size())
	{
		std::advance(theIOCycleIterator, inCurrentIndex);
	}
	if(IsRawEventError(*theIOCycleIterator))
	{
		--inCurrentIndex;
		--theIOCycleIterator;
	}
	while((theAnswer == 0xFFFFFFFF) && (theIOCycleIterator != mRawTelemetry.begin()))
	{
		if(IsRawEventError(*theIOCycleIterator))
		{
			theAnswer = inCurrentIndex;
		}
		else
		{
			--inCurrentIndex;
			--theIOCycleIterator;
		}
	}
	
	return theAnswer;
}

UInt32	CAHALIOCycleTelemetryClient::GetNextErrorRawEventIndex(UInt32 inCurrentIndex) const
{
	UInt32 theAnswer = 0xFFFFFFFF;
	EventList::const_iterator theIOCycleIterator = mRawTelemetry.begin();
	if(inCurrentIndex < mRawTelemetry.size())
	{
		std::advance(theIOCycleIterator, inCurrentIndex);
	}
	if(IsRawEventError(*theIOCycleIterator))
	{
		++inCurrentIndex;
		std::advance(theIOCycleIterator, 1);
	}
	while((theAnswer == 0xFFFFFFFF) && (theIOCycleIterator != mRawTelemetry.end()))
	{
		if(IsRawEventError(*theIOCycleIterator))
		{
			theAnswer = inCurrentIndex;
		}
		else
		{
			++inCurrentIndex;
			std::advance(theIOCycleIterator, 1);
		}
	}
	
	return theAnswer;
}

bool	CAHALIOCycleTelemetryClient::IsRawEventError(const CAHALIOCycleRawTelemetryEvent& inEvent)
{
	bool theAnswer = false;
	switch(inEvent.mEventKind)
	{
		case kHALIOCycleTelemetryEventInitializeBegin:
			break;
		case kHALIOCycleTelemetryEventInitializeEnd:
			break;
		case kHALIOCycleTelemetryEventWorkLoopBegin:
			break;
		case kHALIOCycleTelemetryEventInputReadBegin:
			break;
		case kHALIOCycleTelemetryEventInputReadEnd:
			theAnswer = inEvent.mError != 0;
			break;
		case kHALIOCycleTelemetryEventIOProcsBegin:
			break;
		case kHALIOCycleTelemetryEventIOProcsEnd:
			break;
		case kHALIOCycleTelemetryEventOutputWriteBegin:
			break;
		case kHALIOCycleTelemetryEventOutputWriteEnd:
			theAnswer = inEvent.mError != 0;
			break;
		case kHALIOCycleTelemetryEventWorkLoopOverloadBegin:
			theAnswer = true;
			break;
		case kHALIOCycleTelemetryEventWorkLoopOverloadEnd:
			theAnswer = true;
			break;
		case kHALIOCycleTelemetryEventWorkLoopEnd:
			break;
		case kHALIOCycleTelemetryEventTeardownBegin:
			break;
		case kHALIOCycleTelemetryEventTeardownEnd:
			break;
		case kHALIOCycleTelemetryEventZeroTimeStampRecieved:
			break;
		case kHALIOCycleTelemetryEventZeroTimeStampApplied:
			break;
		case kHALIOCycleTelemetryEventEarlyZeroTimeStamp:
			theAnswer = true;
			break;
		case kHALIOCycleTelemetryEventOutOfBoundsZeroTimeStamp:
			theAnswer = true;
			break;
		case kHALIOCycleTelemetryEventTimelineReset:
			theAnswer = true;
			break;
		case kHALIOCycleTelemetryEventResynch:
			break;
		case kHALIOCycleTelemetryEventInputDataPresent:
			break;
		case kHALIOCycleTelemetryEventOutputDataPresent:
			break;
		case kHALIOCycleTelemetryEventStartHardware:
			theAnswer = inEvent.mError != 0;
			break;
		case kHALIOCycleTelemetryEventStopHardware:
			theAnswer = inEvent.mError != 0;
			break;
		case kHALIOCycleTelemetryEventHardwareStarted:
			break;
		case kHALIOCycleTelemetryEventHardwareStopped:
			break;
		case kHALIOCycleTelemetryEventHardwarePaused:
			break;
		case kHALIOCycleTelemetryEventHardwareResumed:
			break;
		case kHALIOCycleTelemetryEventFormatChangeBegin:
			break;
		case kHALIOCycleTelemetryEventFormatChangeEnd:
			break;
		case kHALIOCycleTelemetryEventMajorEngineChangeBegin:
			break;
		case kHALIOCycleTelemetryEventMajorEngineChangeEnd:
			break;
	};
	return theAnswer;
}

bool	CAHALIOCycleTelemetryClient::IsRawEventSignal(const CAHALIOCycleRawTelemetryEvent& inEvent)
{
	return (inEvent.mEventKind == kHALIOCycleTelemetryEventInputDataPresent) || (inEvent.mEventKind == kHALIOCycleTelemetryEventOutputDataPresent);
}

bool	CAHALIOCycleTelemetryClient::IsRawEventError(UInt32 inEventIndex) const
{
	bool theAnswer = false;
	EventList::const_iterator theIterator = mRawTelemetry.begin();
	std::advance(theIterator, inEventIndex);
	if(theIterator != mRawTelemetry.end())
	{
		theAnswer = IsRawEventError(*theIterator);
	}
	return theAnswer;
}

bool	CAHALIOCycleTelemetryClient::IsRawEventSignal(UInt32 inEventIndex) const
{
	bool theAnswer = false;
	EventList::const_iterator theIterator = mRawTelemetry.begin();
	std::advance(theIterator, inEventIndex);
	if(theIterator != mRawTelemetry.end())
	{
		theAnswer = IsRawEventSignal(*theIterator);
	}
	return theAnswer;
}

void	CAHALIOCycleTelemetryClient::CreateSummaryHeaderForRawEvent(char* outSummary) const
{
	sprintf(outSummary, "  Event     %8s %8s (%8s) Data", "Cycle", "Time", "Offset");
}

void	CAHALIOCycleTelemetryClient::CreateSummaryForRawEvent(UInt32 inEventIndex, char* outSummary) const
{
	UInt64 theAnchorTime = mRawTelemetry.front().mEventTime;
	EventList::const_iterator theIterator = mRawTelemetry.begin();
	std::advance(theIterator, inEventIndex);
	if(theIterator != mRawTelemetry.end())
	{
		CAHALIOCycleRawTelemetryEvent thePreviousEvent;
		if(theIterator != mRawTelemetry.begin())
		{
			thePreviousEvent = *(theIterator - 1);
		}
		else
		{
			thePreviousEvent = *theIterator;
		}
		CreateSummaryForRawEvent(*theIterator, thePreviousEvent, theAnchorTime, outSummary);
	}
}

void	CAHALIOCycleTelemetryClient::CreateSummaryForRawEvent(const CAHALIOCycleRawTelemetryEvent& inEvent, const CAHALIOCycleRawTelemetryEvent& inPreviousEvent, UInt64 inAnchorTime, char* outSummary)
{
	Float64 theEventTime = ConvertHostTimeToDisplayTime(SubtractUInt64(inEvent.mEventTime, inAnchorTime));
	Float64 theEventTimeDifference = ConvertHostTimeToDisplayTime(SubtractUInt64(inEvent.mEventTime, inPreviousEvent.mEventTime));
	Float64 theEventHostTime1 = ConvertHostTimeToDisplayTime(SubtractUInt64(inEvent.mHostTime1, inAnchorTime));
	Float64 theEventHostTime2 = ConvertHostTimeToDisplayTime(SubtractUInt64(inEvent.mHostTime2, inAnchorTime));
	
	switch(inEvent.mEventKind)
	{
		case kHALIOCycleTelemetryEventInitializeBegin:
			sprintf(outSummary, "->Init:   %8u %8.3f (%+8.3f)" , (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventInitializeEnd:
			sprintf(outSummary, "<-Init:   %8u %8.3f (%+8.3f) Anchor:  (%9.0f, %12.3f, %-9.6f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, inEvent.mSampleTime1, theEventHostTime1, inEvent.mRateScalar1);
			break;
		case kHALIOCycleTelemetryEventWorkLoopBegin:
			sprintf(outSummary, "->Cycle:  %8u %8.3f (%+8.3f) Now:     (%9.0f, %12.3f, %-9.6f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, inEvent.mSampleTime1, theEventHostTime1, inEvent.mRateScalar1);
			break;
		case kHALIOCycleTelemetryEventInputReadBegin:
			sprintf(outSummary, "->Read:   %8u %8.3f (%+8.3f) Input:   (%9.0f, %12.3f, %-9.6f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, inEvent.mSampleTime1, theEventHostTime1, inEvent.mRateScalar1);
			break;
		case kHALIOCycleTelemetryEventInputReadEnd:
			sprintf(outSummary, "<-Read:   %8u %8.3f (%+8.3f) Error: 0x%X", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, (unsigned int)inEvent.mError);
			break;
		case kHALIOCycleTelemetryEventIOProcsBegin:
			sprintf(outSummary, "->Calls:  %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventIOProcsEnd:
			sprintf(outSummary, "<-Calls:  %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventIOProcCallBegin:
			sprintf(outSummary, "->IOProc: %8u %8.3f (%+8.3f) IOProc: 0x%qX", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, inEvent.mHostTime1);
			break;
		case kHALIOCycleTelemetryEventIOProcCallEnd:
			sprintf(outSummary, "<-IOProc: %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventOutputWriteBegin:
			sprintf(outSummary, "->Write:  %8u %8.3f (%+8.3f) Output:  (%9.0f, %12.3f, %-9.6f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, inEvent.mSampleTime1, theEventHostTime1, inEvent.mRateScalar1);
			break;
		case kHALIOCycleTelemetryEventOutputWriteEnd:
			sprintf(outSummary, "<-Write:  %8u %8.3f (%+8.3f) Error: 0x%X", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, (unsigned int)inEvent.mError);
			break;
		case kHALIOCycleTelemetryEventWorkLoopOverloadBegin:
			sprintf(outSummary, "->Over:   %8u %8.3f (%+8.3f) Current: (%9.0f, %12.3f, %-9.6f) Wake: (%9.0f, %12.3f, %-9.6f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, inEvent.mSampleTime1, theEventHostTime1, inEvent.mRateScalar1, inEvent.mSampleTime2, theEventHostTime2, inEvent.mRateScalar2);
			break;
		case kHALIOCycleTelemetryEventWorkLoopOverloadEnd:
			sprintf(outSummary, "<-Over:   %8u %8.3f (%+8.3f) Anchor:  (%9.0f, %12.3f, %-9.6f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, inEvent.mSampleTime1, theEventHostTime1, inEvent.mRateScalar1);
			break;
		case kHALIOCycleTelemetryEventWorkLoopEnd:
			sprintf(outSummary, "<-Cycle:  %8u %8.3f (%+8.3f) Current: (%9.0f, %12.3f, %-9.6f) Wake: (%9.0f, %12.3f, %-9.6f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, inEvent.mSampleTime1, theEventHostTime1, inEvent.mRateScalar1, inEvent.mSampleTime2, theEventHostTime2, inEvent.mRateScalar2);
			break;
		case kHALIOCycleTelemetryEventTeardownBegin:
			sprintf(outSummary, "->Down:   %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventTeardownEnd:
			sprintf(outSummary, "<-Down:   %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventZeroTimeStampRecieved:
			sprintf(outSummary, "0 Receiv: %8u %8.3f (%+8.3f) Zero:    (%9.0f, %12.3f, %-9.6f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, inEvent.mSampleTime1, theEventHostTime1, inEvent.mRateScalar1);
			break;
		case kHALIOCycleTelemetryEventZeroTimeStampApplied:
			sprintf(outSummary, "0 Apply:  %8u %8.3f (%+8.3f) Zero:    (%9.0f, %12.3f, %-9.6f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, inEvent.mSampleTime1, theEventHostTime1, inEvent.mRateScalar1);
			break;
		case kHALIOCycleTelemetryEventEarlyZeroTimeStamp:
			sprintf(outSummary, "0 Early:  %8u %8.3f (%+8.3f) Zero:    (%9.0f, %12.3f, %-9.6f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, inEvent.mSampleTime1, theEventHostTime1, inEvent.mRateScalar1);
			break;
		case kHALIOCycleTelemetryEventOutOfBoundsZeroTimeStamp:
			sprintf(outSummary, "0 OoB:    %8u %8.3f (%+8.3f) Zero:    (%9.0f, %12.3f, %-9.6f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, inEvent.mSampleTime1, theEventHostTime1, inEvent.mRateScalar1);
			break;
		case kHALIOCycleTelemetryEventTimelineReset:
			sprintf(outSummary, "reset:    %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventResynch:
			sprintf(outSummary, "resynch:  %8u %8.3f (%+8.3f) Anchor:  (%9.0f, %12.3f, %-9.6f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, inEvent.mSampleTime1, theEventHostTime1, inEvent.mRateScalar1);
			break;
		case kHALIOCycleTelemetryEventInputDataPresent:
			sprintf(outSummary, "sig in:   %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventOutputDataPresent:
			sprintf(outSummary, "sig out:  %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventStartHardware:
			sprintf(outSummary, "start hw: %8u %8.3f (%+8.3f) Error: 0x%X", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, (unsigned int)inEvent.mError);
			break;
		case kHALIOCycleTelemetryEventStopHardware:
			sprintf(outSummary, "stop hw:  %8u %8.3f (%+8.3f) Error: 0x%X", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference, (unsigned int)inEvent.mError);
			break;
		case kHALIOCycleTelemetryEventHardwareStarted:
			sprintf(outSummary, "hw start: %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventHardwareStopped:
			sprintf(outSummary, "hw stop:  %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventHardwarePaused:
			sprintf(outSummary, "hw pause: %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventHardwareResumed:
			sprintf(outSummary, "hw resum: %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventFormatChangeBegin:
			sprintf(outSummary, "->hw fmt: %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventFormatChangeEnd:
			sprintf(outSummary, "<-hw fmt: %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventMajorEngineChangeBegin:
			sprintf(outSummary, "->mec:    %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
		case kHALIOCycleTelemetryEventMajorEngineChangeEnd:
			sprintf(outSummary, "<-mec:    %8u %8.3f (%+8.3f)", (unsigned int)inEvent.mIOCycleNumber, theEventTime, theEventTimeDifference);
			break;
	};
}

OSStatus	CAHALIOCycleTelemetryClient::GetIsEnabledFromServer(bool& outIsEnabled)
{
	UInt32 theError = 0;
	CFDataRef theReturnCFData = NULL;
	
	outIsEnabled = false;
	
	try
	{
		if(mMessagePort != NULL)
		{
			//	build the request
			CACFDictionary theMessageData(true);
			theMessageData.AddUInt32(CFSTR(kHALIOCycleTelemetryPropertyIDKey), kHALTelemetryPropertyIsCapturing);
			CACFData theMessageCFData(CFPropertyListCreateXMLData(NULL, theMessageData.GetCFDictionary()));
			
			//	send the request to the HAL
			theError = mMessagePort->SendRequest(kHALTelemetryMessageGetProperty, theMessageCFData.GetCFData(), 10.0, 10.0, kCFRunLoopDefaultMode, theReturnCFData);
			ThrowIfError(theError, CAException(theError), "CAHALIOCycleTelemetryClient::GetIsEnabledFromServer: sending the request failed");
			ThrowIfNULL(theReturnCFData, CAException(kAudioHardwareUnspecifiedError), "CAHALIOCycleTelemetryClient::GetIsEnabledFromServer: no return result from the server");
			
			//	evaluate the returned data
			CACFDictionary theReturnData(static_cast<CFDictionaryRef>(CFPropertyListCreateFromXMLData(NULL, theReturnCFData, kCFPropertyListImmutable, NULL)), true);
			
			//	check the error
			theError = 0;
			ThrowIf(!theReturnData.GetUInt32(CFSTR(kHALIOCycleTelemetryReturnErrorKey), theError), CAException(kAudioHardwareUnspecifiedError), "CAHALIOCycleTelemetryClient::GetIsEnabledFromServer: no error in the return data");
			ThrowIfError(theError, CAException(theError), "CAHALIOCycleTelemetryClient::GetIsEnabledFromServer: the return data contained an error");
			
			//	get the value of the property
			UInt32 thePropertyValue = 0;
			ThrowIf(!theReturnData.GetUInt32(CFSTR(kHALIOCycleTelemetryPropertyValueKey), thePropertyValue), CAException(kAudioHardwareUnspecifiedError), "CAHALIOCycleTelemetryClient::GetIsEnabledFromServer: no return value in the return data");
			outIsEnabled = thePropertyValue != 0;
		}
	}
	catch(const CAException& inException)
	{
		theError = inException.GetError();
	}
	catch(...)
	{
		theError = kAudioHardwareUnspecifiedError;
	}
	
	if(theReturnCFData != NULL)
	{
		CFRelease(theReturnCFData);
	}
	
	return theError;
}

OSStatus	CAHALIOCycleTelemetryClient::SetIsEnabledOnServer(bool inIsEnabled)
{
	UInt32 theError = 0;
	CFDataRef theReturnCFData = NULL;
	
	try
	{
		if(mMessagePort != NULL)
		{
			//	build the request
			CACFDictionary theMessageData(true);
			theMessageData.AddUInt32(CFSTR(kHALIOCycleTelemetryPropertyIDKey), kHALTelemetryPropertyIsCapturing);
			theMessageData.AddUInt32(CFSTR(kHALIOCycleTelemetryPropertyValueKey), (inIsEnabled ? 1L : 0L));
			CACFData theMessageCFData(CFPropertyListCreateXMLData(NULL, theMessageData.GetCFDictionary()));
			
			//	send the request to the HAL
			theError = mMessagePort->SendRequest(kHALTelemetryMessageSetProperty, theMessageCFData.GetCFData(), 10.0, 10.0, kCFRunLoopDefaultMode, theReturnCFData);
			ThrowIfError(theError, CAException(theError), "CAHALIOCycleTelemetryClient::SetIsEnabledOnServer: sending the request failed");
			ThrowIfNULL(theReturnCFData, CAException(kAudioHardwareUnspecifiedError), "CAHALIOCycleTelemetryClient::SetIsEnabledOnServer: no return result from the server");
			
			//	evaluate the returned data
			CACFDictionary theReturnData(static_cast<CFDictionaryRef>(CFPropertyListCreateFromXMLData(NULL, theReturnCFData, kCFPropertyListImmutable, NULL)), true);
			
			//	check the error
			theError = 0;
			ThrowIf(!theReturnData.GetUInt32(CFSTR(kHALIOCycleTelemetryReturnErrorKey), theError), CAException(kAudioHardwareUnspecifiedError), "CAHALIOCycleTelemetryClient::SetIsEnabledOnServer: no error in the return data");
			ThrowIfError(theError, CAException(theError), "CAHALIOCycleTelemetryClient::SetIsEnabledOnServer: the return data contained an error");
		}
	}
	catch(const CAException& inException)
	{
		theError = inException.GetError();
	}
	catch(...)
	{
		theError = kAudioHardwareUnspecifiedError;
	}
	
	if(theReturnCFData != NULL)
	{
		CFRelease(theReturnCFData);
	}
	
	return theError;
}

OSStatus	CAHALIOCycleTelemetryClient::GetServerIsFlippedFromServer(bool& outServerIsFlipped)
{
	UInt32 theError = 0;
	CFDataRef theReturnCFData = NULL;
	
	outServerIsFlipped = false;
	
	try
	{
		if(mMessagePort != NULL)
		{
			//	build the request
			CACFDictionary theMessageData(true);
			theMessageData.AddUInt32(CFSTR(kHALIOCycleTelemetryPropertyIDKey), kHALTelemetryPropertyEndianness);
			CACFData theMessageCFData(CFPropertyListCreateXMLData(NULL, theMessageData.GetCFDictionary()));
			
			//	send the request to the HAL
			theError = mMessagePort->SendRequest(kHALTelemetryMessageGetProperty, theMessageCFData.GetCFData(), 10.0, 10.0, kCFRunLoopDefaultMode, theReturnCFData);
			ThrowIfError(theError, CAException(theError), "CAHALIOCycleTelemetryClient::GetServerIsFlippedFromServer: sending the request failed");
			ThrowIfNULL(theReturnCFData, CAException(kAudioHardwareUnspecifiedError), "CAHALIOCycleTelemetryClient::GetServerIsFlippedFromServer: no return result from the server");
			
			//	evaluate the returned data
			CACFDictionary theReturnData(static_cast<CFDictionaryRef>(CFPropertyListCreateFromXMLData(NULL, theReturnCFData, kCFPropertyListImmutable, NULL)), true);
			
			//	check the error
			theError = 0;
			ThrowIf(!theReturnData.GetUInt32(CFSTR(kHALIOCycleTelemetryReturnErrorKey), theError), CAException(kAudioHardwareUnspecifiedError), "CAHALIOCycleTelemetryClient::GetServerIsFlippedFromServer: no error in the return data");
			ThrowIfError(theError, CAException(theError), "CAHALIOCycleTelemetryClient::GetServerIsFlippedFromServer: the return data contained an error");
			
			//	get the value of the property
			UInt32 thePropertyValue = 0;
			ThrowIf(!theReturnData.GetUInt32(CFSTR(kHALIOCycleTelemetryPropertyValueKey), thePropertyValue), CAException(kAudioHardwareUnspecifiedError), "CAHALIOCycleTelemetryClient::GetServerIsFlippedFromServer: no return value in the return data");
			#if TARGET_RT_BIG_ENDIAN
				outServerIsFlipped = thePropertyValue == kHALTelemetryServerIsLittleEndian;
			#else
				outServerIsFlipped = thePropertyValue == kHALTelemetryServerIsBigEndian;
			#endif
		}
	}
	catch(const CAException& inException)
	{
		theError = inException.GetError();
	}
	catch(...)
	{
		theError = kAudioHardwareUnspecifiedError;
	}
	
	if(theReturnCFData != NULL)
	{
		CFRelease(theReturnCFData);
	}
	
	return theError;
}

OSStatus	CAHALIOCycleTelemetryClient::ClearDataOnServer()
{
	UInt32 theError = 0;
	CFDataRef theReturnCFData = NULL;
	
	try
	{
		if(mMessagePort != NULL)
		{
			//	build the request
			CACFDictionary theMessageData(true);
			CACFData theMessageCFData(CFPropertyListCreateXMLData(NULL, theMessageData.GetCFDictionary()));
			
			//	send the request to the HAL
			theError = mMessagePort->SendRequest(kHALTelemetryMessageClearTelemetry, theMessageCFData.GetCFData(), 10.0, 10.0, kCFRunLoopDefaultMode, theReturnCFData);
			ThrowIfError(theError, CAException(theError), "CAHALIOCycleTelemetryClient::ClearDataOnServer: sending the request failed");
		}
	}
	catch(const CAException& inException)
	{
		theError = inException.GetError();
	}
	catch(...)
	{
		theError = kAudioHardwareUnspecifiedError;
	}
	
	if(theReturnCFData != NULL)
	{
		CFRelease(theReturnCFData);
	}
	
	return theError;
}

static void	CAHALIOCycleTelemetryClient_MessagePortIsInvalid(CFMessagePortRef /*inMessagePort*/, void* /*inInfo*/)
{
}

bool	CAHALIOCycleTelemetryClient::CanDoLatencyTracing() const
{
	return CALatencyLog::CanUse();
}

bool	CAHALIOCycleTelemetryClient::IsLatencyTracingEnabled() const
{
	return mLatencyTracingEnabled;
}

void	CAHALIOCycleTelemetryClient::SetIsLatencyTracingEnabled(bool inIsEnabled)
{
	if(inIsEnabled != mLatencyTracingEnabled)
	{
		mLatencyTracingEnabled = inIsEnabled;
		
		//	allocate or deallocate the CALatencyLog as appropriate
		if(mLatencyTracingEnabled && (mLatencyLog == NULL))
		{
			mLatencyLog = new CALatencyLog(mLatencyTraceFileName, mLatencyTraceFileNameExtension);
		}
		else if(!mLatencyTracingEnabled && (mLatencyLog != NULL))
		{
			delete mLatencyLog;
			mLatencyLog = NULL;
		}
	}
}

Float64	CAHALIOCycleTelemetryClient::GetIOThreadSchedulingLatencyTrigger() const
{
	//	this is in milliseconds
	return mIOThreadSchedulingLatencyTrigger;
}

void	CAHALIOCycleTelemetryClient::SetIOThreadSchedulingLatencyTrigger(Float64 inTrigger)
{
	//	this is in milliseconds
	mIOThreadSchedulingLatencyTrigger = inTrigger;
}

Float64	CAHALIOCycleTelemetryClient::GetIOCycleDurationTrigger() const
{
	//	this is in milliseconds
	return mIOCycleDurationTrigger;
}

void	CAHALIOCycleTelemetryClient::SetIOCycleDurationTrigger(Float64 inTrigger)
{
	//	this is in milliseconds
	mIOCycleDurationTrigger = inTrigger;
}

bool	CAHALIOCycleTelemetryClient::GetOverloadTrigger() const
{
	return mOverloadTrigger;
}

void	CAHALIOCycleTelemetryClient::SetOverloadTrigger(bool inTrigger)
{
	mOverloadTrigger = inTrigger;
}
