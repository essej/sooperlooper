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
#ifndef __CASMPTETimeBase_h__
#define __CASMPTETimeBase_h__

#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreAudio/CoreAudioTypes.h>
#else
	#include <CoreAudioTypes.h>
#endif

typedef UInt32 SMPTE_HMSF;  // nibbles: hhmmssff

class CASMPTETimeBase {
public:
	// Initialization
	CASMPTETimeBase(UInt32 format = kSMPTETimeType2997) { SetFormat(format); }

	bool	SetFormat(UInt32 format);
				// return true if format is valid
	
	// Accessors
	
	UInt32  GetFormat() const { return mFormat; }
	double  GetFramesPerSecond() const { return mFramesPerSecond; }				// e.g. 24, 25, 29.97, 30, 59.94, 60
	int		GetFormatFramesPerSecond() const { return mFormatFramesPerSecond; } // e.g. 24, 25, 30, 60
	int		GetMTCType() const { return mMTCType; }

	// Conversions

	void	SecondsToSMPTETime( Float64					inSeconds,
								UInt16					inSubframeDivisor,
								SMPTETime &				outSMPTETime) const;
	
	bool	SMPTETimeToSeconds( const SMPTETime &		inSMPTETime,
								Float64 &				outSeconds) const;
				// return true for success; false if the smpte time is invalid
				// This member method ignores inSMPTETime's mType.
	
	UInt32		HMSFToAbsoluteFrame(SMPTE_HMSF			inHMSF) const;
				// convert a 32-bit hhmmssff representation to number of frames since 0
	
	SMPTE_HMSF  AdvanceFrame(		SMPTE_HMSF			inHMSF, int nToAdvance) const;
				// increment a 32-bit hhmmssff representation to the next frame number, returning it
	
private:
	UInt32				mFormat;
	bool				mIsDropFrame;
	int					mMTCType;				// 0=24, 1=24, 2=30 DF, 3=30 ND, -1=other
	double				mFramesPerSecond;
	int					mFormatFramesPerSecond;
};

#endif // __CASMPTETimeBase_h__
