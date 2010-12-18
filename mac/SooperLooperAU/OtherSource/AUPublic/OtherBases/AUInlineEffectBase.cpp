/*	Copyright: 	© Copyright 2005 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under AppleÕs
			copyrights in this original Apple software (the "Apple Software"), to use,
			reproduce, modify and redistribute the Apple Software, with or without
			modifications, in source and/or binary forms; provided that if you redistribute
			the Apple Software in its entirety and without modifications, you must retain
			this notice and the following text and disclaimers in all such redistributions of
			the Apple Software.  Neither the name, trademarks, service marks or logos of
			Apple Computer, Inc. may be used to endorse or promote products derived from the
			Apple Software without specific prior written permission from Apple.  Except as
			expressly stated in this notice, no other rights or licenses, express or implied,
			are granted by Apple herein, including but not limited to any patent rights that
			may be infringed by your derivative works or by other works in which the Apple
			Software may be incorporated.

			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
			WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
			WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
			PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
			COMBINATION WITH YOUR PRODUCTS.

			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
			CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
			GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
			ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
			OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
			(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
			ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*
 * AUInlineEffectBase.cp
 *
 */

#include "AUInlineEffectBase.h"


class AUInlineOutputElement : public AUOutputElement {
public:
	AUInlineOutputElement(AUBase * base) : AUOutputElement(base) {}
	virtual bool	NeedsBufferSpace() const { return false; }
};

AUElement *			AUInlineEffectBase::CreateElement(AudioUnitScope scope, AudioUnitElement element)
{
	AUElement *		elem;
	
	if (scope == kAudioUnitScope_Output) {
		elem = new AUInlineOutputElement(this);
	}
	else {
		elem = AUEffectBase::CreateElement(scope, element);
	}
	
	return elem;
}

ComponentResult	AUInlineEffectBase::Process(AudioUnitRenderActionFlags & flags, AudioBufferList & buffer, UInt32 frames)
{
	return ProcessBufferLists(flags, buffer, buffer, frames);
}

ComponentResult	AUInlineEffectBase::RenderBus(AudioUnitRenderActionFlags & flags, 
												const AudioTimeStamp & timestamp, UInt32 bus, UInt32 frames)
{
	if ((bus != 0) || (! HasInput(0))) {
		return kAudioUnitErr_NoConnection;
	}

	ComponentResult	result;
	AUInputElement *theInput = GetInput(0);
	result = theInput->PullInput(flags, timestamp, 0, frames);
	if (result == noErr) {
		AudioBufferList &	inputbuffers = theInput->GetBufferList();
		if (! IsBypassEffect()) {
			result = ProcessBufferLists(flags, inputbuffers, inputbuffers, frames);
		}
		GetOutput(0)->SetBufferList(inputbuffers);
	}
	
	return result;
}


void	AUInlineKernelBase::Process(const Float32 * srcp, Float32 * dstp, UInt32 frames, UInt32 channels, bool & silence)
{
	if ((srcp != dstp) || (channels != 1)) {
		COMPONENT_THROW(kAudioUnitErr_FormatNotSupported);
	}
	
	Process(dstp, frames, silence);
}

