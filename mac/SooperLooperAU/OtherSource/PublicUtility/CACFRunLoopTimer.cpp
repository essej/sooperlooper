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

#include "CACFRunLoopTimer.h"
#include "CADebugMacros.h"
#include "CAException.h"

//=============================================================================
//	CACFRunLoopTimer
//=============================================================================

CACFRunLoopTimer::CACFRunLoopTimer(CFAbsoluteTime inFireTime, CFTimeInterval inFireInterval)
:
	mRunLoopTimer(NULL)
{
	//	initialize the context
	mRunLoopTimerContext.version = 0;
	mRunLoopTimerContext.info = this;
	mRunLoopTimerContext.retain = NULL;
	mRunLoopTimerContext.release = NULL;
	mRunLoopTimerContext.copyDescription = NULL;
	
	mRunLoopTimer = CFRunLoopTimerCreate(NULL, inFireTime, inFireInterval, 0, 0, (CFRunLoopTimerCallBack)TimerCallBack, &mRunLoopTimerContext);
	ThrowIf(mRunLoopTimer == NULL, CAException('what'), "CACFRunLoopTimer::CACFRunLoopTimer: couldn't create the timer object");
}

CACFRunLoopTimer::~CACFRunLoopTimer()
{
	CFRunLoopTimerInvalidate(mRunLoopTimer);
	CFRelease(mRunLoopTimer);
}

void	CACFRunLoopTimer::FireAt(CFAbsoluteTime inFireTime)
{
	CFRunLoopTimerSetNextFireDate(mRunLoopTimer, inFireTime);
}

void	CACFRunLoopTimer::Cancel()
{
	static const CFAbsoluteTime kLongTimeInTheFuture = 1000000000000.0;
	FireIn(kLongTimeInTheFuture);
}

void	CACFRunLoopTimer::Fire()
{
}

void	CACFRunLoopTimer::TimerCallBack(CFRunLoopTimerRef /*inTimer*/, CACFRunLoopTimer* inTimerObject)
{
	#if	Time_Notification_Thread
		Float64 theStartTime = (Float64)CAHostTimeBase::GetCurrentTimeInNanos();
		printf("-->CACFRunLoopTimer::TimerCallBack at: %.3f\n", theStartTime / 1000000.0);
	#endif

	if(inTimerObject != NULL)
	{
		inTimerObject->Fire();
	}

	#if	Time_Notification_Thread
		Float64 theEndTime = (Float64)CAHostTimeBase::GetCurrentTimeInNanos();
		printf("<--CACFRunLoopTimer::TimerCallBack at: %.3f duration: %.3f\n", theEndTime / 1000000.0, (theStartTime - theEndTime) / 1000000.0);
		fflush(stdout);
	#endif
}
