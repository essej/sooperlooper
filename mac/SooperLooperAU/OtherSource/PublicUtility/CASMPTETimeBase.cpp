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
#include "CASMPTETimeBase.h"
#include "CAException.h"

/*
enum
{
    kSMPTETimeType24        = 0,
    kSMPTETimeType25        = 1,
    kSMPTETimeType30Drop    = 2,
    kSMPTETimeType30        = 3,
    kSMPTETimeType2997      = 4,
    kSMPTETimeType2997Drop  = 5,
    kSMPTETimeType60        = 6,
    kSMPTETimeType5994      = 7
};
*/

static const SInt8 gMTCTypes[] = { 0, 1, 2, 3, 3, 2, -1, -1 };	// map our constants to MIDI Time Code representations
static const double gFramesPerSecond[] = { 24., 25., 29.97, 30., 29.97, 29.97 * .999 /* ????? */, 60., 59.94 };
static const UInt8 gFormatFramesPerSecond[] = { 24, 25, 30, 30, 30, 30, 60, 60 };

bool	CASMPTETimeBase::SetFormat(UInt32	inSMPTEFormat)
{
	if (inSMPTEFormat > kSMPTETimeType5994)
		return false;

	mMTCType = gMTCTypes[inSMPTEFormat];
	mIsDropFrame = (mMTCType == 2);
	mFramesPerSecond = gFramesPerSecond[inSMPTEFormat];
	mFormatFramesPerSecond = gFormatFramesPerSecond[inSMPTEFormat];
	mFormat = inSMPTEFormat;
	return true;
}

/*
	http://www.phatnav.com/wiki/wiki.phtml?title=SMPTE_time_code

	To correct this, drop frame SMPTE timecode drops frame numbers 0 and 1 of the first second of every minute, and includes them when the number of minutes is divisible by ten. This almost perfectly compensates for the difference in rate, leaving a residual timing error of roughly 86.4 microseconds per day, an error of only 0.001 ppm. Note: only timecode frame numbers are dropped. Video frames continue in sequence.
	
	NOTE: drop frame calculations all assume 30 fps = 1800 frames/minute = 18000 frames/10 minutes
*/

void	CASMPTETimeBase::SecondsToSMPTETime(Float64					inSeconds,
											UInt16					inSubframeDivisor,
											SMPTETime &				outSMPTETime) const
{
	// time in subframes
	int bitsPerSecond = inSubframeDivisor * mFormatFramesPerSecond;
 	int absBits = int(inSeconds * bitsPerSecond + 0.5); // round
	int bitsInDay = bitsPerSecond * 60 * 60 * 24;
	while (absBits < 0)
		absBits += bitsInDay;
	while (absBits >= bitsInDay)
		absBits -= bitsInDay;
	
	// if it's drop frame, add back a number of frames to fix up representation
	if (mIsDropFrame) { // drop frame
		int bitsPer10Minutes = inSubframeDivisor * 17982;	// 18000 - 18 dropped
		// how many 10-minute periods? count 18 dropped frames for each
		int minutes10 = (absBits / bitsPer10Minutes);
		int extraFrames = 18 * minutes10;
		int frames10 = (absBits / inSubframeDivisor) % 17982;  // position in 10-minute cycle
		if (frames10 >= 1800) {
			// at or past the first minute
			// minute 0 has 1800 frames, subsequent minutes have 1798 frames
			// so add 2 frames per minute
			extraFrames += 2 + 2 * ((frames10 - 1800) / 1798);
		}
		absBits = int(inSeconds * bitsPerSecond + 0.5) + extraFrames * inSubframeDivisor;
	}
	int div;	
	div = (bitsPerSecond * 60 * 60);
	outSMPTETime.mHours = absBits / div;
	absBits %= div;
	
	div = (bitsPerSecond * 60);
	outSMPTETime.mMinutes = absBits / div;
	absBits %= div;
	
	outSMPTETime.mSeconds = absBits / bitsPerSecond;
	absBits %= bitsPerSecond;
	
	outSMPTETime.mFrames = absBits / inSubframeDivisor;
	outSMPTETime.mSubframes = absBits %= inSubframeDivisor;
	outSMPTETime.mSubframeDivisor = inSubframeDivisor;
	
	outSMPTETime.mType = mFormat;
}



bool	CASMPTETimeBase::SMPTETimeToSeconds(const SMPTETime &		inSMPTETime,
											Float64 &				outSeconds) const
{
	if (inSMPTETime.mHours > 23 || inSMPTETime.mMinutes > 59 || inSMPTETime.mSeconds > 59 || inSMPTETime.mFrames >= mFormatFramesPerSecond || inSMPTETime.mSubframes >= inSMPTETime.mSubframeDivisor)
		return false;

	// first, compute it assuming no dropped frames
	int frames = (inSMPTETime.mFrames + mFormatFramesPerSecond * // frames
						(inSMPTETime.mSeconds + 60 * // seconds
						(inSMPTETime.mMinutes + 60 *  // minutes
						 inSMPTETime.mHours))); // hours
	// now, figure out the number of dropped frames
	if (mIsDropFrame) {
		frames -= inSMPTETime.mHours * 6 * 18; // hours, 18 frames for each 10 minutes
		int minutes = inSMPTETime.mMinutes;
		frames -= (minutes / 10) * 18; // 18 frames per 10-minute period
		minutes %= 10;
		if (minutes) {
			if (inSMPTETime.mFrames < 2)
				return false;   // illegal -- dropped frame
			frames -= minutes * 2;
		}
	}
	int bits = inSMPTETime.mSubframes + inSMPTETime.mSubframeDivisor * frames;
	outSeconds = double(bits) / double(mFormatFramesPerSecond * inSMPTETime.mSubframeDivisor);
	return true;
}

// increment a 32-bit MTC SMPTE frame representation
// HHMMSSFF (nibbles)
SMPTE_HMSF  CASMPTETimeBase::AdvanceFrame(SMPTE_HMSF hmsf, int nToAdvance) const
{
	while (--nToAdvance >= 0) {
		hmsf += 1;
		if ((hmsf & 0xFF) == UInt32(mFormatFramesPerSecond)) {
			// next second
			hmsf &= ~0xFF;
			hmsf += 0x100;
			if ((hmsf & 0xFF00) == (60 << 8)) {
				// 60 seconds
				hmsf &= ~0xFF00;
				hmsf += 0x10000;
				if ((hmsf & 0xFF0000) == (60 << 16)) {
					// 60 minutes
					hmsf &= ~0xFF0000;
					hmsf += 0x1000000;
					if ((hmsf & 0x1F000000) == (24 << 24))
						// 24 hours
						hmsf &= ~0x1F000000;
				}
			}
		}
		if (mIsDropFrame &&
				(hmsf & 0xFFFE) == 0 && 	// either of first two frames of first second
				((hmsf & 0x00FF0000) >> 16) % 10 != 0)	// not the first of a 10-minute period
			hmsf += 2; // drop the two frames
	}
	return hmsf;
}

// convert SMPTE representation as four bytes in a UInt32 to number of frames since 0
UInt32	CASMPTETimeBase::HMSFToAbsoluteFrame(SMPTE_HMSF hmsf) const
{
	// first, compute it assuming no dropped frames
	UInt32 frames = (hmsf & 0xFF) + mFormatFramesPerSecond * ( // frames
						((hmsf >> 8) & 0xFF) + 60 * ( // seconds
						((hmsf >> 16) & 0xFF) + 60 * ( // minutes
						 (hmsf >> 24) & 0x1F))); // hours
	// now, figure out the number of dropped frames
	if (mIsDropFrame) {
		frames -= ((hmsf & 0x1F000000) >> 24) * 6 * 18; // hours
		UInt16 minutes = (hmsf & 0x00FF0000) >> 16;
		frames -= (minutes / 10) * 18; // 10-minute periods
		minutes %= 10;
		if (minutes)
			frames -= minutes * 2;
	}
	return frames;
}
