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

#include "CASampleTools.h"

#define ASM __asm__ volatile		// bad things happen with plain "asm"

// ____________________________________________________________________________
//
//	Types for use in algorithms
class CAFloat32 {
public:
	typedef float value_type;
	
	static value_type load(const value_type *p) { return *p; }
	static void store(value_type *p, float val)	{ *p = val; }
};

class CASInt8 {
public:
	typedef int8_t value_type;
	
	static value_type load(const value_type *p) { return *p; }
	static void store(value_type *p, int val)	{ *p = val; }
};

class CAUInt8 {
public:
	typedef u_int8_t value_type;
	
	static value_type load(const value_type *p) { return *p ^ 0x80; }
	static void store(value_type *p, int val)	{ *p = val ^ 0x80; }
};

class CASInt16Native {
public:
	typedef int16_t value_type;
	
	static value_type load(const value_type *p) { return *p; }
	static void store(value_type *p, int val)	{ *p = val; }
};

class CASInt16Swap {
public:
	typedef int16_t value_type;
	
	static value_type load(const value_type *p)
	{
#if	TARGET_CPU_PPC || TARGET_CPU_PPC64
		register int32_t result;
		ASM("lhbrx %0, %1, %2" 
			/* outputs:  */ : "=r" (result) 
			/* inputs:   */ : "b%" (0), "r" (p) 
			/* clobbers: */ : "memory");
		return result;
#else
		return CASampleTools::Int16SwapEndian(*p);
#endif
	}
	static void store(value_type *p, int val)
	{
#if	TARGET_CPU_PPC || TARGET_CPU_PPC64
		ASM( "sthbrx %0, 0, %1" : : "r" (val), "r" (p) );
#else
		*p = CASampleTools::Int16SwapEndian(val);
#endif
	}
};

class CASInt32Native {
public:
	typedef int32_t value_type;
	
	static value_type load(const value_type *p) { return *p; }
	static void store(value_type *p, int val)	{ *p = val; }
};

class CASInt32Swap {
public:
	typedef u_int32_t value_type;
	
	static value_type load(const value_type *p)
	{
#if	TARGET_CPU_PPC || TARGET_CPU_PPC64
		register u_int32_t lwbrxResult;
		__asm__ volatile("lwbrx %0, %1, %2" : "=r" (lwbrxResult) : "b%" (0), "r" (p) : "memory");
		return lwbrxResult;
#else
		return CASampleTools::Int32SwapEndian(*p);
#endif
	}
	static void store(value_type *p, int val)
	{
		*p = val;
#if	TARGET_CPU_PPC || TARGET_CPU_PPC64
		ASM( "stwbrx %0, 0, %1" : : "r" (val), "r" (p) );
#else
		*p = CASampleTools::Int32SwapEndian(val);
#endif
	}
};

#if TARGET_CPU_PPC || TARGET_CPU_PPC64
	typedef CASInt16Native	CASInt16Big;
	typedef CASInt32Native	CASInt32Big;
	typedef CASInt16Swap	CASInt16Little;
	typedef CASInt32Swap	CASInt32Little;
#else
	typedef CASInt16Native	CASInt16Little;
	typedef CASInt32Native	CASInt32Little;
	typedef CASInt16Swap	CASInt16Big;
	typedef CASInt32Swap	CASInt32Big;
#endif

// ____________________________________________________________________________
//
// CAFloatToInt
inline int32_t CAFloatToInt(double inf)
{
#if	TARGET_CPU_PPC || TARGET_CPU_PPC64
	int32_t i;
	union {
		double	d;
		u_int32_t	i[2];
	} u;
	double temp;
	
	ASM( "fctiw %0,%1" : "=f" (temp) : "f" (inf));
		// rounds, doesn't truncate towards zero like fctiwz
	u.d = temp;
	i = u.i[1];
	return i;
#else
	return (int32_t)inf;
#endif
}

inline u_int32_t CAFloatToUInt(double inf)
{
	return u_int32_t(CAFloatToInt(inf));
}

// ____________________________________________________________________________
//
// CAFastInt16ToFloat32
//
// for use in tight loops, keeps compiler from generating redundant instructions
class CAFastInt16ToFloat32 {
public:
#if	TARGET_CPU_PPC || TARGET_CPU_PPC64
	CAFastInt16ToFloat32(float /*scale*/, int shift)
	{
		u_int32_t signExp = (0x97 - shift) << 23;
		*(u_int32_t *)mBuf = *(u_int32_t *)&mOffset = signExp | 0x8000;
	}
	float operator() (int16_t val)
	{
		mBuf[1] = u_int16_t(val) ^ 0x8000;
		return *(float *)mBuf - mOffset;
	}

	u_int16_t	mBuf[2];
	float		mOffset;
#else
	CAFastInt16ToFloat32(float scale, int shift) : mScale(scale)
	{
	}
	
	float operator() (int16_t val) { return (float)val * mScale; }
	
	float		mScale;
#endif
};

// ____________________________________________________________________________
//
// CAFastInt32ToFloat64
//
// for use in tight loops, keeps compiler from generating redundant instructions
class CAFastInt32ToFloat64 {
public:
#if	TARGET_CPU_PPC || TARGET_CPU_PPC64
	CAFastInt32ToFloat64(double /*scale*/, int shift)
	{
		// this is the saving: we don't keep stuffing this value into memory
		// for every sample
		mBuf[0] = (0x434 - shift) << 20;
		mBuf[1] = 0x80000000;
		mOffset = *(double *)mBuf;
	}
	double operator() (int32_t val)
	{
		// this is the rest of what the compiler does to convert int -> float
		mBuf[1] = val ^ 0x80000000;
		return (*(double *)mBuf - mOffset);
	}

	u_int32_t		mBuf[2];
	double		mOffset;
#else
	CAFastInt32ToFloat64(double scale, int shift) : mScale(scale)
	{
	}
	
	double operator() (int32_t val) { return (double)val * mScale; }
	
	const double		mScale;
#endif
};

//=============================================================================
//	CASampleTools
//=============================================================================

const int8_t	CASampleTools::sMaxSInt8Sample = 127;
const int8_t	CASampleTools::sMinSInt8Sample = -128;
const float		CASampleTools::sHalfRangeSInt8Sample = 128.0;
const int16_t	CASampleTools::sMaxSInt16Sample = 32767;
const int16_t	CASampleTools::sMinSInt16Sample = -32768;
const float		CASampleTools::sHalfRangeSInt16Sample = 32768.0;

const float	CASampleTools::sMaxSInt8Float32Sample = static_cast<float>(sMaxSInt8Sample) / sHalfRangeSInt8Sample;
const float	CASampleTools::sMinSInt8Float32Sample = static_cast<float>(sMinSInt8Sample) / sHalfRangeSInt8Sample;
const float	CASampleTools::sMaxSInt16Float32Sample = static_cast<float>(sMaxSInt16Sample) / sHalfRangeSInt16Sample;
const float	CASampleTools::sMinSInt16Float32Sample = static_cast<float>(sMinSInt16Sample) / sHalfRangeSInt16Sample;

void	CASampleTools::CopySInt8ToFloat32Samples(const int8_t* inSource, float* outDestination, u_int32_t inNumberSamples)
{
	const CASInt8::value_type *src = (const CASInt8::value_type *)inSource - 1;
	CAFloat32::value_type *dest = (CAFloat32::value_type *)outDestination - 1;
	CAFastInt16ToFloat32 i2f(1.0 / float(1UL << (8 - 1)), 8);
	int count;
	
	count = inNumberSamples >> 2;
	while (count--) {
		int i1 = CASInt8::load(++src);
		int i2 = CASInt8::load(++src);
		int i3 = CASInt8::load(++src);
		int i4 = CASInt8::load(++src);
		CAFloat32::value_type f1 = i2f(i1);
		CAFloat32::value_type f2 = i2f(i2);
		CAFloat32::value_type f3 = i2f(i3);
		CAFloat32::value_type f4 = i2f(i4);
		CAFloat32::store(++dest, f1);
		CAFloat32::store(++dest, f2);
		CAFloat32::store(++dest, f3);
		CAFloat32::store(++dest, f4);
	}
	
	count = inNumberSamples & 3;
	while (count--) {
		int i1 = CASInt8::load(++src);
		CAFloat32::value_type f1 = i2f(i1);
		CAFloat32::store(++dest, f1);
	}
}

void	CASampleTools::CopyLESInt16ToFloat32Samples(const int16_t* inSource, float* outDestination, u_int32_t inNumberSamples)
{
	const CASInt16Little::value_type *src = (const CASInt16Little::value_type *)inSource - 1;
	CAFloat32::value_type *dest = (CAFloat32::value_type *)outDestination - 1;
	CAFastInt16ToFloat32 i2f(1.0 / float(1UL << (16 - 1)), 16);
	int count;
	
	count = inNumberSamples >> 2;
	while (count--) {
		int i1 = CASInt16Little::load(++src);
		int i2 = CASInt16Little::load(++src);
		int i3 = CASInt16Little::load(++src);
		int i4 = CASInt16Little::load(++src);
		CAFloat32::value_type f1 = i2f(i1);
		CAFloat32::value_type f2 = i2f(i2);
		CAFloat32::value_type f3 = i2f(i3);
		CAFloat32::value_type f4 = i2f(i4);
		CAFloat32::store(++dest, f1);
		CAFloat32::store(++dest, f2);
		CAFloat32::store(++dest, f3);
		CAFloat32::store(++dest, f4);
	}
	
	count = inNumberSamples & 3;
	while (count--) {
		int i1 = CASInt16Little::load(++src);
		CAFloat32::value_type f1 = i2f(i1);
		CAFloat32::store(++dest, f1);
	}
}

void	CASampleTools::CopyBESInt16ToFloat32Samples(const int16_t* inSource, float* outDestination, u_int32_t inNumberSamples)
{
	const CASInt16Big::value_type *src = (const CASInt16Big::value_type *)inSource - 1;
	CAFloat32::value_type *dest = (CAFloat32::value_type *)outDestination - 1;
	CAFastInt16ToFloat32 i2f(1.0 / float(1UL << (16 - 1)), 16);
	int count;
	
	count = inNumberSamples >> 2;
	while (count--) {
		int i1 = CASInt16Big::load(++src);
		int i2 = CASInt16Big::load(++src);
		int i3 = CASInt16Big::load(++src);
		int i4 = CASInt16Big::load(++src);
		CAFloat32::value_type f1 = i2f(i1);
		CAFloat32::value_type f2 = i2f(i2);
		CAFloat32::value_type f3 = i2f(i3);
		CAFloat32::value_type f4 = i2f(i4);
		CAFloat32::store(++dest, f1);
		CAFloat32::store(++dest, f2);
		CAFloat32::store(++dest, f3);
		CAFloat32::store(++dest, f4);
	}
	
	count = inNumberSamples & 3;
	while (count--) {
		int i1 = CASInt16Big::load(++src);
		CAFloat32::value_type f1 = i2f(i1);
		CAFloat32::store(++dest, f1);
	}
}

void	CASampleTools::CopyLESInt32ToFloat32Samples(const int32_t* inSource, float* outDestination, u_int32_t inNumberSamples)
{
	const CASInt32Little::value_type *src = (const CASInt32Little::value_type *)inSource - 1;
	CAFloat32::value_type *dest = (CAFloat32::value_type *)outDestination - 1;
	CAFastInt32ToFloat64 i2f(1.0 / float(1UL << (32 - 1)), 32);
	int count;
	
	count = inNumberSamples >> 2;
	while (count--) {
		int i1 = CASInt32Little::load(++src);
		int i2 = CASInt32Little::load(++src);
		int i3 = CASInt32Little::load(++src);
		int i4 = CASInt32Little::load(++src);
		CAFloat32::value_type f1 = i2f(i1);
		CAFloat32::value_type f2 = i2f(i2);
		CAFloat32::value_type f3 = i2f(i3);
		CAFloat32::value_type f4 = i2f(i4);
		CAFloat32::store(++dest, f1);
		CAFloat32::store(++dest, f2);
		CAFloat32::store(++dest, f3);
		CAFloat32::store(++dest, f4);
	}
	
	count = inNumberSamples & 3;
	while (count--) {
		int i1 = CASInt32Little::load(++src);
		CAFloat32::value_type f1 = i2f(i1);
		CAFloat32::store(++dest, f1);
	}
}

void	CASampleTools::CopyBESInt32ToFloat32Samples(const int32_t* inSource, float* outDestination, u_int32_t inNumberSamples)
{
	const CASInt32Big::value_type *src = (const CASInt32Big::value_type *)inSource - 1;
	CAFloat32::value_type *dest = (CAFloat32::value_type *)outDestination - 1;
	CAFastInt32ToFloat64 i2f(1.0 / float(1UL << (32 - 1)), 32);
	int count;
	
	count = inNumberSamples >> 2;
	while (count--) {
		int i1 = CASInt32Big::load(++src);
		int i2 = CASInt32Big::load(++src);
		int i3 = CASInt32Big::load(++src);
		int i4 = CASInt32Big::load(++src);
		CAFloat32::value_type f1 = i2f(i1);
		CAFloat32::value_type f2 = i2f(i2);
		CAFloat32::value_type f3 = i2f(i3);
		CAFloat32::value_type f4 = i2f(i4);
		CAFloat32::store(++dest, f1);
		CAFloat32::store(++dest, f2);
		CAFloat32::store(++dest, f3);
		CAFloat32::store(++dest, f4);
	}
	
	count = inNumberSamples & 3;
	while (count--) {
		int i1 = CASInt32Big::load(++src);
		CAFloat32::value_type f1 = i2f(i1);
		CAFloat32::store(++dest, f1);
	}
}

void	CASampleTools::CopyFloat32ToSInt8Samples(const float* inSource, int8_t* outDestination, u_int32_t inNumberSamples)
{
	const CAFloat32::value_type *src = (const CAFloat32::value_type *)inSource - 1;
	CASInt8::value_type *dest = (CASInt8::value_type *)outDestination - 1;
	float maxInt32 = 2147483648.0;	// 1 << 31
	int shift = (32 - 8), count;
	
	count = inNumberSamples >> 2;
	while (count--) {
		CAFloat32::value_type f1 = CAFloat32::load(++src) * maxInt32;
		CAFloat32::value_type f2 = CAFloat32::load(++src) * maxInt32;
		CAFloat32::value_type f3 = CAFloat32::load(++src) * maxInt32;
		CAFloat32::value_type f4 = CAFloat32::load(++src) * maxInt32;
		int i1 = CAFloatToInt(f1) >> shift;
		int i2 = CAFloatToInt(f2) >> shift;
		int i3 = CAFloatToInt(f3) >> shift;
		int i4 = CAFloatToInt(f4) >> shift;
		CASInt8::store(++dest, i1);
		CASInt8::store(++dest, i2);
		CASInt8::store(++dest, i3);
		CASInt8::store(++dest, i4);
	}
	
	count = inNumberSamples & 3;
	while (count--) {
		CAFloat32::value_type f1 = CAFloat32::load(++src) * maxInt32;
		int i1 = CAFloatToInt(f1) >> shift;
		CASInt8::store(++dest, i1);
	}
}

void	CASampleTools::CopyFloat32ToLESInt16Samples(const float* inSource, int16_t* outDestination, u_int32_t inNumberSamples)
{
	const CAFloat32::value_type *src = (const CAFloat32::value_type *)inSource - 1;
	CASInt16Little::value_type *dest = (CASInt16Little::value_type *)outDestination - 1;
	float maxInt32 = 2147483648.0;	// 1 << 31
	int shift = (32 - 16), count;
	
	count = inNumberSamples >> 2;
	while (count--) {
		CAFloat32::value_type f1 = CAFloat32::load(++src) * maxInt32;
		CAFloat32::value_type f2 = CAFloat32::load(++src) * maxInt32;
		CAFloat32::value_type f3 = CAFloat32::load(++src) * maxInt32;
		CAFloat32::value_type f4 = CAFloat32::load(++src) * maxInt32;
		int i1 = CAFloatToInt(f1) >> shift;
		int i2 = CAFloatToInt(f2) >> shift;
		int i3 = CAFloatToInt(f3) >> shift;
		int i4 = CAFloatToInt(f4) >> shift;
		CASInt16Little::store(++dest, i1);
		CASInt16Little::store(++dest, i2);
		CASInt16Little::store(++dest, i3);
		CASInt16Little::store(++dest, i4);
	}
	
	count = inNumberSamples & 3;
	while (count--) {
		CAFloat32::value_type f1 = CAFloat32::load(++src) * maxInt32;
		int i1 = CAFloatToInt(f1) >> shift;
		CASInt16Little::store(++dest, i1);
	}
}

void	CASampleTools::CopyFloat32ToBESInt16Samples(const float* inSource, int16_t* outDestination, u_int32_t inNumberSamples)
{
	const CAFloat32::value_type *src = (const CAFloat32::value_type *)inSource - 1;
	CASInt16Big::value_type *dest = (CASInt16Big::value_type *)outDestination - 1;
	float maxInt32 = 2147483648.0;	// 1 << 31
	int shift = (32 - 16), count;
	
	count = inNumberSamples >> 2;
	while (count--) {
		CAFloat32::value_type f1 = CAFloat32::load(++src) * maxInt32;
		CAFloat32::value_type f2 = CAFloat32::load(++src) * maxInt32;
		CAFloat32::value_type f3 = CAFloat32::load(++src) * maxInt32;
		CAFloat32::value_type f4 = CAFloat32::load(++src) * maxInt32;
		int i1 = CAFloatToInt(f1) >> shift;
		int i2 = CAFloatToInt(f2) >> shift;
		int i3 = CAFloatToInt(f3) >> shift;
		int i4 = CAFloatToInt(f4) >> shift;
		CASInt16Big::store(++dest, i1);
		CASInt16Big::store(++dest, i2);
		CASInt16Big::store(++dest, i3);
		CASInt16Big::store(++dest, i4);
	}
	
	count = inNumberSamples & 3;
	while (count--) {
		CAFloat32::value_type f1 = CAFloat32::load(++src) * maxInt32;
		int i1 = CAFloatToInt(f1) >> shift;
		CASInt16Big::store(++dest, i1);
	}
}

void	CASampleTools::CopyFloat32ToLESInt32Samples(const float* inSource, int32_t* outDestination, u_int32_t inNumberSamples)
{
	const CAFloat32::value_type *src = (const CAFloat32::value_type *)inSource - 1;
	CASInt32Little::value_type *dest = (CASInt32Little::value_type *)outDestination - 1;
	float maxInt32 = 2147483648.0;	// 1 << 31
	int shift = (32 - 32), count;
	
	count = inNumberSamples >> 2;
	while (count--) {
		CAFloat32::value_type f1 = CAFloat32::load(++src) * maxInt32;
		CAFloat32::value_type f2 = CAFloat32::load(++src) * maxInt32;
		CAFloat32::value_type f3 = CAFloat32::load(++src) * maxInt32;
		CAFloat32::value_type f4 = CAFloat32::load(++src) * maxInt32;
		int i1 = CAFloatToInt(f1) >> shift;
		int i2 = CAFloatToInt(f2) >> shift;
		int i3 = CAFloatToInt(f3) >> shift;
		int i4 = CAFloatToInt(f4) >> shift;
		CASInt32Little::store(++dest, i1);
		CASInt32Little::store(++dest, i2);
		CASInt32Little::store(++dest, i3);
		CASInt32Little::store(++dest, i4);
	}
	
	count = inNumberSamples & 3;
	while (count--) {
		CAFloat32::value_type f1 = CAFloat32::load(++src) * maxInt32;
		int i1 = CAFloatToInt(f1) >> shift;
		CASInt32Little::store(++dest, i1);
	}
}

void	CASampleTools::CopyFloat32ToBESInt32Samples(const float* inSource, int32_t* outDestination, u_int32_t inNumberSamples)
{
	const CAFloat32::value_type *src = (const CAFloat32::value_type *)inSource - 1;
	CASInt32Big::value_type *dest = (CASInt32Big::value_type *)outDestination - 1;
	float maxInt32 = 2147483648.0;	// 1 << 31
	int shift = (32 - 32), count;
	
	count = inNumberSamples >> 2;
	while (count--) {
		CAFloat32::value_type f1 = CAFloat32::load(++src) * maxInt32;
		CAFloat32::value_type f2 = CAFloat32::load(++src) * maxInt32;
		CAFloat32::value_type f3 = CAFloat32::load(++src) * maxInt32;
		CAFloat32::value_type f4 = CAFloat32::load(++src) * maxInt32;
		int i1 = CAFloatToInt(f1) >> shift;
		int i2 = CAFloatToInt(f2) >> shift;
		int i3 = CAFloatToInt(f3) >> shift;
		int i4 = CAFloatToInt(f4) >> shift;
		CASInt32Big::store(++dest, i1);
		CASInt32Big::store(++dest, i2);
		CASInt32Big::store(++dest, i3);
		CASInt32Big::store(++dest, i4);
	}
	
	count = inNumberSamples & 3;
	while (count--) {
		CAFloat32::value_type f1 = CAFloat32::load(++src) * maxInt32;
		int i1 = CAFloatToInt(f1) >> shift;
		CASInt32Big::store(++dest, i1);
	}
}

void	CASampleTools::MixFloat32Samples(const float* inSource, float* outDestination, u_int32_t inNumberSamples)
{
	const CAFloat32::value_type *src = (const CAFloat32::value_type *)inSource - 1;
	CAFloat32::value_type *dest = (CAFloat32::value_type *)outDestination - 1;
	int count;
	
	count = inNumberSamples >> 2;
	while (count--) {
		CAFloat32::value_type f1a = CAFloat32::load(++src);
		CAFloat32::value_type f2a = CAFloat32::load(++src);
		CAFloat32::value_type f3a = CAFloat32::load(++src);
		CAFloat32::value_type f4a = CAFloat32::load(++src);
		CAFloat32::value_type f1b = CAFloat32::load(dest + 0);
		CAFloat32::value_type f2b = CAFloat32::load(dest + 1);
		CAFloat32::value_type f3b = CAFloat32::load(dest + 2);
		CAFloat32::value_type f4b = CAFloat32::load(dest + 3);
		CAFloat32::store(++dest, f1a + f1b);
		CAFloat32::store(++dest, f2a + f2b);
		CAFloat32::store(++dest, f3a + f3b);
		CAFloat32::store(++dest, f4a + f4b);
	}
	
	count = inNumberSamples & 3;
	while (count--) {
		CAFloat32::value_type f1a = CAFloat32::load(++src);
		CAFloat32::value_type f1b = CAFloat32::load(dest);
		CAFloat32::store(++dest, f1a + f1b);
	}
}
