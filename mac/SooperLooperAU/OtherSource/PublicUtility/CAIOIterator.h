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
#if !defined(__CAIOIterator_h__)
#define __CAIOIterator_h__

//=============================================================================
//	Includes
//=============================================================================

//	System Includes
#include <IOKit/IOKitLib.h>

#if !defined(IO_OBJECT_NULL)
	#define	IO_OBJECT_NULL	((io_object_t) 0)
#endif

//=============================================================================
//	CAIOIterator
//=============================================================================

class CAIOIterator
{

//	Construction/Destruction
public:
					CAIOIterator() : mIOIterator(IO_OBJECT_NULL), mWillRelease(true), mLastKernelError(0) {}
					CAIOIterator(io_iterator_t inIOIterator, bool inWillRelease = true) : mIOIterator(inIOIterator), mWillRelease(inWillRelease), mLastKernelError(0) {}
					CAIOIterator(io_object_t inParent, const io_name_t inPlane) : mIOIterator(IO_OBJECT_NULL), mWillRelease(true), mLastKernelError(0) { mLastKernelError = IORegistryEntryGetChildIterator(inParent, inPlane, &mIOIterator); }
					CAIOIterator(io_object_t inChild, const io_name_t inPlane, bool /*inGetParent*/) : mIOIterator(IO_OBJECT_NULL), mWillRelease(true), mLastKernelError(0) { mLastKernelError = IORegistryEntryGetParentIterator(inChild, inPlane, &mIOIterator); }
					~CAIOIterator() { Release(); }
					CAIOIterator(const CAIOIterator& inIterator) : mIOIterator(inIterator.mIOIterator), mWillRelease(inIterator.mWillRelease), mLastKernelError(0) { Retain(); }
	CAIOIterator&   operator=(const CAIOIterator& inIterator) { Release(); mIOIterator = inIterator.mIOIterator; mWillRelease = inIterator.mWillRelease; Retain(); mLastKernelError = 0; return *this; }
	CAIOIterator&   operator=(io_iterator_t inIOIterator) { Release(); mIOIterator = inIOIterator; mWillRelease = true; return *this; }

private:
	void			Retain() { if(mWillRelease && (mIOIterator != IO_OBJECT_NULL)) { IOObjectRetain(mIOIterator); } }
	void			Release() { if(mWillRelease && (mIOIterator != IO_OBJECT_NULL)) { IOObjectRelease(mIOIterator); mIOIterator = IO_OBJECT_NULL; } }
	
	io_iterator_t   mIOIterator;
	bool			mWillRelease;
	kern_return_t   mLastKernelError;

//	Operations
public:
	void			AllowRelease() { mWillRelease = true; }
	void			DontAllowRelease() { mWillRelease = false; }
	bool			IsValid() const { return mIOIterator != IO_OBJECT_NULL; }
	bool			IsEqual(io_iterator_t inIOIterator) { return IOObjectIsEqualTo(inIOIterator, mIOIterator); }
	io_object_t		Next() { return IOIteratorNext(mIOIterator); }

//	Value Access
public:
	io_iterator_t   GetIOIterator() const { return mIOIterator; }
	io_iterator_t   CopyIOIterator() const { if(mIOIterator != IO_OBJECT_NULL) { IOObjectRetain(mIOIterator); } return mIOIterator; }
	kern_return_t   GetLastKernelError() const { return mLastKernelError; }

};

#endif
