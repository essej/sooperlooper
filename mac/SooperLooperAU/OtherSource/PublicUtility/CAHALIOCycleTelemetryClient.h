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
#if !defined(__CAHALIOCycleTelemetryClient_h__)
#define __CAHALIOCycleTelemetryClient_h__

//=============================================================================
//	Includes
//=============================================================================

//	PublicUtility Includes
#include "CAHALTelemetry.h"

//	System Includes
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

//	Standard Library Includes
#include <deque>
#include <vector>

//=============================================================================
//	Types
//=============================================================================

class	CACFRemoteMessagePort;
class	CALatencyLog;

//=============================================================================
//	CAHALIOCycleTelemetry
//
//	This class encapsulates and unifies the raw telemetry data for a single IO cycle.
//	Host times are convertered to microseconds.
//=============================================================================

class CAHALIOCycleTelemetry
{

//	Construction/Destruction
public:
				CAHALIOCycleTelemetry();
				~CAHALIOCycleTelemetry();

//	Operations
public:
	UInt32		GetIOCycleNumber() const;
	UInt64		GetIntendedStartTime() const;
	UInt64		GetStartTime() const;
	UInt64		GetEndTime() const;
	Float64		GetDuration() const;
	Float64		GetRateScalar() const;
	void		GetIOProcTimes(AudioTimeStamp& outNow, AudioTimeStamp& outInputTime, AudioTimeStamp& outOutputTime) const;
	bool		HasError() const;
	bool		HasOverload() const;
	bool		HasInputError() const;
	bool		HasOutputError() const;
	bool		HasZeroTime() const;
	bool		HasSignal() const;
	bool		GetZeroTimeStamp(AudioTimeStamp& outZeroTime) const;
	bool		GetNextWakeUpTime(AudioTimeStamp& outWakeTime) const;
	
	Float64		GetTotalLoad() const;
	Float64		GetSchedulingLoad() const;
	Float64		GetReadLoad() const;
	Float64		GetIOProcLoad() const;
	Float64		GetWriteLoad() const;
	
	bool		AssimilateRawEvent(const CAHALIOCycleRawTelemetryEvent& inRawEvent);
	void		SetLastCycleEnd(const CAHALIOCycleRawTelemetryEvent& inRawEvent);
	
	bool		GetRawEventByKind(UInt32 inEventKind, CAHALIOCycleRawTelemetryEvent& outRawEvent) const;
	bool		GetBeginRawEvent(CAHALIOCycleRawTelemetryEvent& outRawEvent) const;
	bool		GetEndRawEvent(CAHALIOCycleRawTelemetryEvent& outRawEvent) const;
	
	UInt32		GetNumberRawEvents() const;
	bool		GetRawEventByIndex(UInt32 inEventIndex, CAHALIOCycleRawTelemetryEvent& outRawEvent) const;

//	Implementation
private:
	typedef std::vector<CAHALIOCycleRawTelemetryEvent>	RawEventList;
	
	CAHALIOCycleRawTelemetryEvent	mLastCycleEnd;
	RawEventList					mRawEvents;

};

//=============================================================================
//	CAHALIOCycleTelemetryClient
//=============================================================================

class CAHALIOCycleTelemetryClient
{

//	Construction/Destruction
public:
							CAHALIOCycleTelemetryClient(const char* inLatencyTraceFileName, const char* inLatencyTraceFileNameExtension);
	virtual					~CAHALIOCycleTelemetryClient();

	bool					Initialize(pid_t inProcess, AudioDeviceID inDevice);
	void					Teardown();

//	Attributes
public:
	bool					IsEnabled() const;

//	Operations
public:
	static CFStringRef		CreatePortName(pid_t inProcessID, CFStringRef inUID);
	
	bool					Update();
	void					Clear(bool inOnServer = true);
	
	UInt32					GetNumberIOCycles() const;
	UInt32					GetPreviousErrorIOCycleIndex(UInt32 inCurrentIndex) const;
	UInt32					GetNextErrorIOCycleIndex(UInt32 inCurrentIndex) const;
	UInt32					GetNumberEventsInIOCycle(UInt32 inIOCycleIndex) const;
	bool					IOCycleHasError(UInt32 inIOCycleIndex) const;
	bool					IOCycleHasSignal(UInt32 inIOCycleIndex) const;
	bool					EventInIOCycleHasError(UInt32 inIOCycleIndex, UInt32 inEventIndex) const;
	bool					EventInIOCycleHasSignal(UInt32 inIOCycleIndex, UInt32 inEventIndex) const;
	void					CreateSummaryHeaderForIOCycle(char* outSummary, bool inForSpreadSheet) const;
	void					CreateSummaryForIOCycle(UInt32 inIOCycleIndex, char* outSummary, bool inForSpreadSheet) const;
	void					CreateSummaryForEventInIOCycle(UInt32 inIOCycleIndex, UInt32 inEventIndex, char* outSummary) const;
	
	UInt32					GetNumberRawEvents() const;
	UInt32					GetPreviousErrorRawEventIndex(UInt32 inCurrentIndex) const;
	UInt32					GetNextErrorRawEventIndex(UInt32 inCurrentIndex) const;
	bool					IsRawEventError(UInt32 inEventIndex) const;
	bool					IsRawEventSignal(UInt32 inEventIndex) const;
	void					CreateSummaryHeaderForRawEvent(char* outSummary) const;
	void					CreateSummaryForRawEvent(UInt32 inEventIndex, char* outSummary) const;
	static void				CreateSummaryForRawEvent(const CAHALIOCycleRawTelemetryEvent& inEvent, const CAHALIOCycleRawTelemetryEvent& inPreviousEvent, UInt64 inAnchorTime, char* outSummary);
	static bool				IsRawEventError(const CAHALIOCycleRawTelemetryEvent& inEvent);
	static bool				IsRawEventSignal(const CAHALIOCycleRawTelemetryEvent& inEvent);

//	Server Operations
public:
	OSStatus				GetIsEnabledFromServer(bool& outIsEnabled);
	OSStatus				SetIsEnabledOnServer(bool inIsEnabled);
	OSStatus				GetServerIsFlippedFromServer(bool& outServerIsFlipped);
	OSStatus				ClearDataOnServer();

//	Latency Tracing
public:
	bool					CanDoLatencyTracing() const;
	bool					IsLatencyTracingEnabled() const;
	void					SetIsLatencyTracingEnabled(bool inIsEnabled);
	Float64					GetIOThreadSchedulingLatencyTrigger() const;
	void					SetIOThreadSchedulingLatencyTrigger(Float64 inTrigger);
	Float64					GetIOCycleDurationTrigger() const;
	void					SetIOCycleDurationTrigger(Float64 inTrigger);
	bool					GetOverloadTrigger() const;
	void					SetOverloadTrigger(bool inTrigger);

//	Implementation
private:
	typedef std::deque<CAHALIOCycleRawTelemetryEvent>	EventList;
	typedef std::deque<UInt32>							Index;
	typedef std::deque<CAHALIOCycleTelemetry>			IOCycleList;
	
	CACFRemoteMessagePort*	mMessagePort;
	bool					mServerIsFlipped;
	EventList				mRawTelemetry;
	IOCycleList				mIOCycleList;
	Index					mErrorCycleList;
	Index					mZerosCycleList;
	
	const char*				mLatencyTraceFileName;
	const char*				mLatencyTraceFileNameExtension;
	bool					mLatencyTracingEnabled;
	bool					mTraceTaken;
	CALatencyLog*			mLatencyLog;
	Float64					mIOThreadSchedulingLatencyTrigger;
	Float64					mIOCycleDurationTrigger;
	bool					mOverloadTrigger;

};

#endif
