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
//	System Includes
#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <AudioToolbox/AudioToolbox.h>
#else
	#include <AudioToolbox.h>
#endif
#include "MatchAudioChannelLayoutTagWithChannels.h"
#include <stdlib.h>


/*
MatchAudioChannelLayoutTagWithChannels determines whether an AudioChannelLayoutTag 
matches the channel descriptions in an AudioChannelLayout.
*/

#define OFFSETOF(class, field)((size_t)&((class*)0)->field)

Boolean MatchAudioChannelLayoutTagWithChannels(
	AudioChannelLayoutTag inTag, 
	AudioChannelLayout* inLayout)
{
	UInt32 inNumChannels = inLayout->mNumberChannelDescriptions;
	
	if (inNumChannels != AudioChannelLayoutTag_GetNumberOfChannels(inTag))
		return false;
	
	OSStatus err = noErr;
	UInt32 outSize;
	err = AudioFormatGetPropertyInfo(kAudioFormatProperty_ChannelLayoutForTag, sizeof(inTag), &inTag, &outSize);
	if (err) return false;
	
	UInt32 byteSize = OFFSETOF(AudioChannelLayout, mChannelDescriptions[inNumChannels]);
	AudioChannelLayout *testLayout = (AudioChannelLayout*)calloc(1, byteSize);

	err = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForTag, sizeof(inTag), &inTag, &outSize, testLayout);
	if (err) {
		free(testLayout);
		return false;
	}
	
	if (testLayout->mNumberChannelDescriptions != inNumChannels)
	{
		free(testLayout);
		return false;
	}
	
	for (UInt32 i=0; i<inNumChannels; ++i)
	{
		Boolean found = false;
		for (UInt32 j = 0; j<inNumChannels; ++j)
		{
			if (inLayout->mChannelDescriptions[i].mChannelLabel == testLayout->mChannelDescriptions[i].mChannelLabel)
			{
				found = true;
				break;
			}
		}
		if (!found) {
			free(testLayout);
			return false;
		}
	}
	free(testLayout);
	return true;
}

/*
FindTagForMatchAudioChannelLayoutInAnyOrder determines whether there is any AudioChannelLayoutTag that contains the same
channel labels as inLayout. If there is the first one that matches is returned in outTag and the function returns true.
Otherwise the function returns false.
*/

Boolean FindTagForMatchAudioChannelLayoutInAnyOrder(AudioChannelLayout* inLayout, AudioChannelLayoutTag &outTag)
{
	UInt32 inNumChannels = inLayout->mNumberChannelDescriptions;

	OSStatus err = noErr;
	UInt32 outSize;
	err = AudioFormatGetPropertyInfo(kAudioFormatProperty_TagsForNumberOfChannels, sizeof(UInt32), &inNumChannels, &outSize);
	if (err) return false;

	AudioChannelLayoutTag* tags = (AudioChannelLayoutTag*)calloc(1, outSize);
	
	err = AudioFormatGetProperty(kAudioFormatProperty_TagsForNumberOfChannels, sizeof(UInt32), &inNumChannels, &outSize, tags);
	if (err) {
		free(tags);
		return false;
	}
	
	UInt32 numTags = outSize / sizeof(AudioChannelLayoutTag);
	
	for (UInt32 i = 0; i<numTags; ++i)
	{
		Boolean success = MatchAudioChannelLayoutTagWithChannels(tags[i], inLayout);
		if (success) {
			outTag = tags[i];
			return true;
		}
	}
	
	return false;
}
