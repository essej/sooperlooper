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
#if !defined(__CASampleTools_h__)
#define __CASampleTools_h__

//=============================================================================
//	Includes
//=============================================================================

#include <TargetConditionals.h>

#if TARGET_API_MAC_OSX
	#include <sys/types.h>
#else
	#include <MacTypes.h>
	#include "CFBase.h"
	typedef unsigned char		u_int8_t;
	
	typedef unsigned short		u_int16_t;
	
	typedef unsigned long		u_int32_t;
	
#if TYPE_LONGLONG
#if TARGET_OS_WIN32
	typedef SInt64				int64_t;
	typedef UInt64				u_int64_t;
#else
	typedef long long			int64_t;
	typedef unsigned long long	u_int64_t;
#endif
#endif
#endif

//=============================================================================
//	CASampleTools
//
//	This class contains routines for converting between
//	various linear PCM sample formats.
//=============================================================================

class	CASampleTools
{

//	Sample Value Constants
public:
	static const int8_t		sMaxSInt8Sample;
	static const int8_t		sMinSInt8Sample;
	static const float		sHalfRangeSInt8Sample;
	static const int16_t	sMaxSInt16Sample;
	static const int16_t	sMinSInt16Sample;
	static const float		sHalfRangeSInt16Sample ;//= 32768.0;
	static const float		sMaxSInt8Float32Sample ;//= static_cast<float>(sMaxSInt8Sample) / sHalfRangeSInt8Sample;
	static const float		sMinSInt8Float32Sample ;//= static_cast<float>(sMinSInt8Sample) / sHalfRangeSInt8Sample;
	static const float		sMaxSInt16Float32Sample ;//= static_cast<float>(sMaxSInt16Sample) / sHalfRangeSInt16Sample;
	static const float		sMinSInt16Float32Sample ;//= static_cast<float>(sMinSInt16Sample) / sHalfRangeSInt16Sample;

//	Endian Swapping Routines
public:
	static u_int16_t		Int16SwapEndian(u_int16_t inValue)
	{
		return ((inValue << 8) & 0xFF00) | ((inValue >> 8) & 0x00FF);
	}

	static u_int32_t		Int32SwapEndian(u_int32_t inValue)
	{
		return ((inValue << 24) & 0xFF000000UL) | ((inValue << 8) & 0x00FF0000UL) | ((inValue >> 8) & 0x0000FF00UL) | ((inValue >> 24) & 0x000000FFUL);
	}

#if TARGET_OS_WIN32
	/* the inline macros crash MSDEV's optimizer on Windows. */
	static u_int64_t			Int64SwapEndian(UInt64 inValue)
	{
		u_int64_t temp;
		((UnsignedWide*)&temp)->lo = Int32SwapEndian(((UnsignedWide*)&inValue)->hi);
		((UnsignedWide*)&temp)->hi = Int32SwapEndian(((UnsignedWide*)&inValue)->lo);
		return temp;
	}
#else
	static u_int64_t		Int64SwapEndian(u_int64_t inValue)
	{
		return ((inValue << 56) & 0xFF00000000000000ULL) | ((inValue << 40) & 0x00FF000000000000ULL) | ((inValue << 24) & 0x0000FF0000000000ULL) | ((inValue << 8) & 0x000000FF00000000ULL) | ((inValue >> 8) & 0x00000000FF000000ULL) | ((inValue >> 24) & 0x0000000000FF0000ULL) | ((inValue >> 40) & 0x000000000000FF00ULL) | ((inValue >> 56) & 0x00000000000000FFULL);
	}
#endif	
#if	TARGET_RT_BIG_ENDIAN

	static int16_t			SInt16BigToNativeEndian(int16_t inValue) { return inValue; }
	static int16_t			SInt16NativeToBigEndian(int16_t inValue) { return inValue; }

	static int16_t			SInt16LittleToNativeEndian(int16_t inValue) { return Int16SwapEndian(inValue); }
	static int16_t			SInt16NativeToLittleEndian(int16_t inValue) { return Int16SwapEndian(inValue); }

	static u_int16_t		UInt16BigToNativeEndian(u_int16_t inValue) { return inValue; }
	static u_int16_t		UInt16NativeToBigEndian(u_int16_t inValue) { return inValue; }

	static u_int16_t		UInt16LittleToNativeEndian(u_int16_t inValue) { return Int16SwapEndian(inValue); }
	static u_int16_t		UInt16NativeToLittleEndian(u_int16_t inValue) { return Int16SwapEndian(inValue); }

	static int32_t			SInt32BigToNativeEndian(int32_t inValue) { return inValue; }
	static int32_t			SInt32NativeToBigEndian(int32_t inValue) { return inValue; }

	static int32_t			SInt32LittleToNativeEndian(int32_t inValue) { return Int32SwapEndian(inValue); }
	static int32_t			SInt32NativeToLittleEndian(int32_t inValue) { return Int32SwapEndian(inValue); }

	static u_int32_t		UInt32BigToNativeEndian(u_int32_t inValue) { return inValue; }
	static u_int32_t		UInt32NativeToBigEndian(u_int32_t inValue) { return inValue; }

	static u_int32_t		UInt32LittleToNativeEndian(u_int32_t inValue) { return Int32SwapEndian(inValue); }
	static u_int32_t		UInt32NativeToLittleEndian(u_int32_t inValue) { return Int32SwapEndian(inValue); }

	static int64_t			SInt64BigToNativeEndian(int64_t inValue) { return inValue; }
	static int64_t			SInt64NativeToBigEndian(int64_t inValue) { return inValue; }

	static int64_t			SInt64LittleToNativeEndian(int64_t inValue) { return Int64SwapEndian(inValue); }
	static int64_t			SInt64NativeToLittleEndian(int64_t inValue) { return Int64SwapEndian(inValue); }

	static u_int64_t		UInt64BigToNativeEndian(u_int64_t inValue) { return inValue; }
	static u_int64_t		UInt64NativeToBigEndian(u_int64_t inValue) { return inValue; }

	static u_int64_t		UInt64LittleToNativeEndian(u_int64_t inValue) { return Int64SwapEndian(inValue); }
	static u_int64_t		UInt64NativeToLittleEndian(u_int64_t inValue) { return Int64SwapEndian(inValue); }

#else

	static int16_t			SInt16BigToNativeEndian(int16_t inValue) { return Int16SwapEndian(inValue); }
	static int16_t			SInt16NativeToBigEndian(int16_t inValue) { return Int16SwapEndian(inValue); }

	static int16_t			SInt16LittleToNativeEndian(int16_t inValue) { return inValue; }
	static int16_t			SInt16NativeToLittleEndian(int16_t inValue) { return inValue; }

	static u_int16_t		UInt16BigToNativeEndian(u_int16_t inValue) { return Int16SwapEndian(inValue); }
	static u_int16_t		UInt16NativeToBigEndian(u_int16_t inValue) { return Int16SwapEndian(inValue); }

	static u_int16_t		UInt16LittleToNativeEndian(u_int16_t inValue) { return inValue; }
	static u_int16_t		UInt16NativeToLittleEndian(u_int16_t inValue) { return inValue; }

	static int32_t			SInt32BigToNativeEndian(int32_t inValue) { return Int32SwapEndian(inValue); }
	static int32_t			SInt32NativeToBigEndian(int32_t inValue) { return Int32SwapEndian(inValue); }

	static int32_t			SInt32LittleToNativeEndian(int32_t inValue) { return inValue; }
	static int32_t			SInt32NativeToLittleEndian(int32_t inValue) { return inValue; }

	static u_int32_t		UInt32BigToNativeEndian(u_int32_t inValue) { return Int32SwapEndian(inValue); }
	static u_int32_t		UInt32NativeToBigEndian(u_int32_t inValue) { return Int32SwapEndian(inValue); }

	static u_int32_t		UInt32LittleToNativeEndian(u_int32_t inValue) { return inValue; }
	static u_int32_t		UInt32NativeToLittleEndian(u_int32_t inValue) { return inValue; }

	static int64_t			SInt64BigToNativeEndian(int64_t inValue) { return Int64SwapEndian(inValue); }
	static int64_t			SInt64NativeToBigEndian(int64_t inValue) { return Int64SwapEndian(inValue); }

	static int64_t			SInt64LittleToNativeEndian(int64_t inValue) { return inValue; }
	static int64_t			SInt64NativeToLittleEndian(int64_t inValue) { return inValue; }

	static u_int64_t		UInt64BigToNativeEndian(u_int64_t inValue) { return Int64SwapEndian(inValue); }
	static u_int64_t		UInt64NativeToBigEndian(u_int64_t inValue) { return Int64SwapEndian(inValue); }

	static u_int64_t		UInt64LittleToNativeEndian(u_int64_t inValue) { return inValue; }
	static u_int64_t		UInt64NativeToLittleEndian(u_int64_t inValue) { return inValue; }

#endif

//	Copy Routines
public:
	//	signed 8 bit integer to native endian 32 bit float
	static void				CopySInt8ToFloat32Samples(const int8_t* inSource, float* outDestination, u_int32_t inNumberSamples);

	//	signed little endian 16 bit integer to native endian 32 bit float
	static void				CopyLESInt16ToFloat32Samples(const int16_t* inSource, float* outDestination, u_int32_t inNumberSamples);

	//	signed big endian 16 bit integer to native endian 32 bit float
	static void				CopyBESInt16ToFloat32Samples(const int16_t* inSource, float* outDestination, u_int32_t inNumberSamples);
	
	//	signed little endian 32 bit integer to native endian 32 bit float
	static void				CopyLESInt32ToFloat32Samples(const int32_t* inSource, float* outDestination, u_int32_t inNumberSamples);

	//	signed big endian 32 bit integer to native endian 32 bit float
	static void				CopyBESInt32ToFloat32Samples(const int32_t* inSource, float* outDestination, u_int32_t inNumberSamples);
	
	//	native endian 32 bit float to signed 8 bit integer
	static void				CopyFloat32ToSInt8Samples(const float* inSource, int8_t* outDestination, u_int32_t inNumberSamples);

	//	native endian 32 bit float to signed little endian 16 bit integer
	static void				CopyFloat32ToLESInt16Samples(const float* inSource, int16_t* outDestination, u_int32_t inNumberSamples);

	//	native endian 32 bit float to signed big endian 16 bit integer
	static void				CopyFloat32ToBESInt16Samples(const float* inSource, int16_t* outDestination, u_int32_t inNumberSamples);

	//	native endian 32 bit float to signed little endian 32 bit integer
	static void				CopyFloat32ToLESInt32Samples(const float* inSource, int32_t* outDestination, u_int32_t inNumberSamples);

	//	native endian 32 bit float to signed big endian 32 bit integer
	static void				CopyFloat32ToBESInt32Samples(const float* inSource, int32_t* outDestination, u_int32_t inNumberSamples);

	//	mix two native endian 32 bit float buffers
	static void				MixFloat32Samples(const float* inSource, float* outDestination, u_int32_t inNumberSamples);

};

#endif
