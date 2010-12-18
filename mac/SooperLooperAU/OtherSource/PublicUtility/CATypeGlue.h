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
#if !defined(__CATypeGlue_h__)
#define __CATypeGlue_h__

//==================================================================================================
//	Includes
//==================================================================================================

//	System Includes
#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreAudio/CoreAudioTypes.h>
#else
	#include <CoreAudioTypes.h>
#endif

//==================================================================================================
//	Types
//==================================================================================================

union CA_UInt64_UnsignedWide
{
	UInt64			mUInt64;
	UnsignedWide	mUnsignedWide;
};
typedef union CA_UInt64_UnsignedWide	CA_UInt64_UnsignedWide;

union CA_SInt64_Wide
{
	SInt64	mSInt64;
	wide	mWide;
};
typedef union CA_SInt64_Wide	CA_SInt64_Wide;

union CA_UInt32_Float32
{
	UInt32	mUInt32;
	Float32	mFloat32;
};
typedef union CA_UInt32_Float32	CA_UInt32_Float32;

union CA_UInt64_Float64
{
	UInt64	mUInt64;
	Float64	mFloat64;
};
typedef union CA_UInt64_Float64	CA_UInt64_Float64;

//==================================================================================================
//	Operations
//==================================================================================================

#define	CA_AssignUInt64ToUnsignedWide(outUnsignedWide, inUInt64)			\
{																			\
	CA_UInt64_UnsignedWide theUnion;										\
	theUnion.mUInt64 = (inUInt64);											\
	(outUnsignedWide) = theUnion.mUnsignedWide;								\
}

#define	CA_AssignUnsignedWideToUInt64(outUInt64, inUnsignedWide)			\
{																			\
	CA_UInt64_UnsignedWide theUnion;										\
	theUnion.mUnsignedWide = (inUnsignedWide);								\
	(outUnsignedWide) = theUnion.mUInt64;									\
}

#define	CA_AssignSInt64ToWide(outWide, inSInt64)							\
{																			\
	CA_SInt64_Wide theUnion;												\
	theUnion.mSInt64 = (inSInt64);											\
	(outWide) = theUnion.mWide;												\
}

#define	CA_AssignWideToSInt64(outSInt64, inWide)							\
{																			\
	CA_SInt64_Wide theUnion;												\
	theUnion.mWide = (inWide);												\
	(outSInt64) = theUnion.mSInt64;											\
}

#if defined(__cplusplus)

inline UnsignedWide	CA_ConvertUInt64ToUnsignedWide(UInt64 inUInt64)
{
	CA_UInt64_UnsignedWide theUnion;
	theUnion.mUInt64 = inUInt64;
	return theUnion.mUnsignedWide;
}

inline UInt64	CA_ConvertUnsignedWideToUInt64(UnsignedWide inUnsignedWide)
{
	CA_UInt64_UnsignedWide theUnion;
	theUnion.mUnsignedWide = inUnsignedWide;
	return theUnion.mUInt64;
}

inline wide	CA_ConvertSInt64ToWide(SInt64 inSInt64)
{
	CA_SInt64_Wide theUnion;
	theUnion.mSInt64 = inSInt64;
	return theUnion.mWide;
}

inline SInt64	CA_ConvertWideToSInt64(wide inWide)
{
	CA_SInt64_Wide theUnion;
	theUnion.mWide = inWide;
	return theUnion.mSInt64;
}

#endif	//	__cplusplus

#endif	//	__CATypeGlue_h__
