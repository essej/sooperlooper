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
#ifndef __DebuggingPP_h__
#define __DebuggingPP_h__

// versions of macros in Debugging.h that throw exceptions instead of gotos

#if !__COREAUDIO_USE_FLAT_INCLUDES__
	#include <CoreServices/CoreServices.h>
#endif

#if DEBUGLEVEL == DEBUG_LEVEL_PRODUCTION
	
	#define Require(assertion, exception)	do { if( !(assertion) ) throw exception; } while (false)
	#define RequireNoErr(error)	do { if( (error) != noErr ) throw OSStatus(error); } while (false)
	#define RequireNoErrString(error, message)	do { if( (error) != noErr ) throw OSStatus(error); } while (false)

#else

	#define Require(assertion, exception) \
	do {                                                                \
		if (!(assertion))                                               \
		{                                                               \
			DEBUGASSERTMSG(COMPONENT_SIGNATURE, DEBUG_NO_OPTIONS,       \
						QuoteExceptionString(assertion),                \
						QuoteExceptionString(exception),                \
						NULL, __FILE__, __LINE__, 0);                   \
			throw exception;                                            \
		}                                                               \
	} while (false)
	
	#define RequireNoErr(error)                                         \
	do {                                                                \
		OSStatus localError = error;                                    \
		if (localError != noErr)                                        \
		{                                                               \
			DEBUGASSERTMSG(COMPONENT_SIGNATURE, DEBUG_NO_OPTIONS,       \
						QuoteExceptionString(error) "== noErr",         \
						QuoteExceptionString(exception),                \
						NULL, __FILE__, __LINE__, (void *)localError ); \
			throw localError;                                           \
		}                                                               \
	} while (false)

	#define	RequireNoErrString(error, message)							  \
	do {                                                                \
		OSStatus localError = error;                                    \
		if (localError != noErr)                                        \
		{                                                               \
			DEBUGASSERTMSG(COMPONENT_SIGNATURE, DEBUG_NO_OPTIONS,       \
						QuoteExceptionString(error) "== noErr",         \
						message,                                        \
						NULL, __FILE__, __LINE__, (void *)localError ); \
			throw localError;                                           \
		}                                                               \
	} while (false)
#endif

#endif // __DebuggingPP_h__
