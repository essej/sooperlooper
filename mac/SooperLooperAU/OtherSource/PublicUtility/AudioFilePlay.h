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
#ifndef __AudioFilePlay_H__
#define __AudioFilePlay_H__

#include <CoreServices/CoreServices.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*AudioFilePlayNotifier)(void 			*inRefCon,
									OSStatus		inStatus);
enum {
	kAudioFilePlayErr_FilePlayUnderrun = -10000,
	kAudioFilePlay_FileIsFinished = -10001,
	kAudioFilePlay_PlayerIsUninitialized = -10002
};

typedef	struct OpaqueFilePlayObj* AudioFilePlayID;

extern OSStatus NewAudioFilePlayID (const FSRef	*inFileRef, AudioFilePlayID	*outFilePlayID);

extern OSStatus DisposeAudioFilePlayID (AudioFilePlayID 	inFilePlayID);

// this assumes that the input format of the AudioUnit's bus is SET correctly!!!
// if you change this after you've set the destination, you should disconnect and reset the destination
extern OSStatus AFP_SetDestination (AudioFilePlayID 		inFilePlayID,
									AudioUnit 				inDestUnit, 
									UInt32					inDestBusNumber);

extern OSStatus AFP_SetLooping (AudioFilePlayID 			inFilePlayID,
									Boolean 				inShouldLoop);

extern OSStatus AFP_SetNotifier (AudioFilePlayID		 		inFilePlayID,
									AudioFilePlayNotifier	 	inNotifier, 
									void*						inUserData);

extern OSStatus AFP_Connect (AudioFilePlayID 		inFilePlayID);

extern OSStatus AFP_Disconnect (AudioFilePlayID 	inFilePlayID);

extern OSStatus AFP_IsConnected (AudioFilePlayID 	inFilePlayID,
									Boolean			*outIsConnected);

	// Any of the args can be null if you're not interested in result
	// ONLY valid if the playID is connected
extern OSStatus AFP_GetInfo (AudioFilePlayID 	inFilePlayID,
							AudioUnit			*outDestUnit,
							UInt32				*outBusNumber,
							AudioConverterRef 	*outConverter);

extern OSStatus AFP_Print (AudioFilePlayID 			inFilePlayID);

extern void PrintStreamDesc (AudioStreamBasicDescription* 	inDesc);

#ifdef __cplusplus
}
#endif

#endif
