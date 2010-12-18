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
//==================================================================================================
//	Includes
//==================================================================================================

//	Self Include
#include "CAAudioValueRange.h"

//	Standard Library
#include <algorithm>

//==================================================================================================
//	CAAudioValueRange
//==================================================================================================

Float64 CAAudioValueRange::PickCommonSampleRate(const AudioValueRange& inRange)
{
	//  This routine will pick a "common" sample rate from the give range of rates or the maximum
	//  if no common rates can be found. It assumes that inRange contains a continuous range of
	//  sample rates.
	Float64 theAnswer = inRange.mMaximum;
	
	if(ContainsValue(inRange, 44100.0))
	{
		theAnswer = 44100.0;
	}
	else if(ContainsValue(inRange, 48000.0))
	{
		theAnswer = 48000.0;
	}
	else if(ContainsValue(inRange, 96000.0))
	{
		theAnswer = 96000.0;
	}
	else if(ContainsValue(inRange, 88200.0))
	{
		theAnswer = 88200.0;
	}
	else if(ContainsValue(inRange, 64000.0))
	{
		theAnswer = 64000.0;
	}
	else if(ContainsValue(inRange, 32000.0))
	{
		theAnswer = 32000.0;
	}
	else if(ContainsValue(inRange, 24000.0))
	{
		theAnswer = 24000.0;
	}
	else if(ContainsValue(inRange, 22050.0))
	{
		theAnswer = 22050.0;
	}
	else if(ContainsValue(inRange, 16000.0))
	{
		theAnswer = 16000.0;
	}
	else if(ContainsValue(inRange, 12000.0))
	{
		theAnswer = 12000.0;
	}
	else if(ContainsValue(inRange, 11025.0))
	{
		theAnswer = 11025.0;
	}
	else if(ContainsValue(inRange, 8000.0))
	{
		theAnswer = 8000.0;
	}
	
	return theAnswer;
}

bool	CAAudioValueRange::Intersection(const AudioValueRange& x, const AudioValueRange& y, AudioValueRange& outRange)
{
	bool isNonEmpty;
	if(!IsStrictlyLessThan(x, y) && !IsStrictlyGreaterThan(x, y))
	{
		outRange.mMinimum = std::max(x.mMinimum, y.mMinimum);
		outRange.mMaximum = std::min(x.mMaximum, y.mMaximum);
		isNonEmpty = true;
	}
	else
	{
		outRange.mMinimum = 0;
		outRange.mMaximum = 0;
		isNonEmpty = false;
	}
	return isNonEmpty;
}

bool	CAAudioValueRange::Union(const AudioValueRange& x, const AudioValueRange& y, AudioValueRange& outRange)
{
	bool isDisjoint;
	if(!IsStrictlyLessThan(x, y) && !IsStrictlyGreaterThan(x, y))
	{
		outRange.mMinimum = std::min(x.mMinimum, y.mMinimum);
		outRange.mMaximum = std::max(x.mMaximum, y.mMaximum);
		isDisjoint = false;
	}
	else
	{
		outRange.mMinimum = 0;
		outRange.mMaximum = 0;
		isDisjoint = true;
	}
	return isDisjoint;
}
