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
#ifndef __CAAudioConverter_h__
#define __CAAudioConverter_h__

#include <AudioToolbox/AudioConverter.h>
#include "CAXException.h"
#include "CAStreamBasicDescription.h"

class CAAudioConverter {
public:
	CAAudioConverter(const AudioStreamBasicDescription &inFormat, const AudioStreamBasicDescription &outFormat)
	{
		XThrowIfError(AudioConverterNew(&inFormat, &outFormat, &mConverter), "AudioConverterNew");
		mInputFormat = inFormat;
		mOutputFormat = outFormat;
	}
	
	virtual ~CAAudioConverter()
	{
		if (mConverter) {
			verify_noerr(AudioConverterDispose(mConverter));
			mConverter = NULL;
		}
	}
	
	virtual OSStatus	Reset () { return AudioConverterReset(mConverter); }

	OSStatus	SetProperty(AudioConverterPropertyID	inPropertyID,
							UInt32						inPropertyDataSize,
							const void *				inPropertyData)
	{
		return AudioConverterSetProperty(mConverter, inPropertyID, inPropertyDataSize, inPropertyData);
	}
	
	OSStatus	GetProperty(AudioConverterPropertyID	inPropertyID,
							UInt32 &					ioPropertyDataSize,
							void *						outPropertyData)
	{
		return AudioConverterGetProperty(mConverter, inPropertyID, &ioPropertyDataSize, outPropertyData);
	}

	OSStatus	GetPropertyInfo(AudioConverterPropertyID	inPropertyID,
								UInt32 &					outPropertyDataSize,
								Boolean &					outWritable)
	{
		return AudioConverterGetPropertyInfo(mConverter, inPropertyID, &outPropertyDataSize, &outWritable);
	}

	OSStatus	FillComplexBuffer(	UInt32 &							ioOutputDataPacketSize,
									AudioBufferList &					outOutputData,
									AudioStreamPacketDescription*		outPacketDescription)
	{
		OSStatus err;
		err = AudioConverterFillComplexBuffer(mConverter, InputProc, this, &ioOutputDataPacketSize, &outOutputData, outPacketDescription);
		return err;
	}
	
	const CAStreamBasicDescription &	GetInputFormat()	{ return mInputFormat; }

	const CAStreamBasicDescription &	GetOutputFormat()	{ return mOutputFormat; }	
	
protected:
	virtual OSStatus	ProvideInput(	
								UInt32 &						ioNumberDataPackets,
								AudioBufferList &				ioData,
								AudioStreamPacketDescription**	outDataPacketDescription) = 0;
	
private:
	static OSStatus	InputProc(	AudioConverterRef				inAudioConverter,
								UInt32*							ioNumberDataPackets,
								AudioBufferList*				ioData,
								AudioStreamPacketDescription**	outDataPacketDescription,
								void*							inUserData)
	{
		OSStatus err;
		try {
			CAAudioConverter *This = static_cast<CAAudioConverter *>(inUserData);
			err = This->ProvideInput(*ioNumberDataPackets, *ioData, outDataPacketDescription);
		}
		catch (OSStatus err) {
			return err;
		}
		catch (...) {
			return -1;
		}
		return err;
	}

#if DEBUG
public:
	void Show()
	{
		CAShow(mConverter);
	}
#endif

protected:
	AudioConverterRef			mConverter;
	CAStreamBasicDescription	mInputFormat, mOutputFormat;
};


#endif // __CAAudioConverter_h__
