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
#if !defined(__CALogger_h__)
#define __CALogger_h__

/*
	This class is a stack based class that can be used for logging. 
	
	If you use the CALogger macros defined below to instantiate log, then the loggings is automatically turned off for non-DEBUG builds
	
	You can start each use of the logger with a preamble, that will print as:
		-> Preamble
		 : -> each line of added log text
		<- Preamble
	
	If you reach a condition where you don't want logging, then you Clear() the logger.
	It shouldn't be used after that (its a way to signal that upon destruction you don't want a log entry
*/

class CALogger {
public:
		CALogger ();
		CALogger (char* str);
		~CALogger ();
			
		void Add (char* str, ...);
		
		void Clear ();
		
private:
		char* mInitString;
		char* mStr;

		CALogger (const CALogger& c);
		CALogger& operator= (const CALogger& c);
};


#if DEBUG || CoreAudioDebug

	#define CALoggerMake						CALogger __logger__
	#define CALoggerMakeMsg(msg)				CALogger __logger__(msg)

	#define CALoggerMsg(msg)					__logger__.Add(msg)
	#define CALoggerMsgN1(msg, N1)				__logger__.Add(msg, N1)
	#define CALoggerMsgN2(msg, N1, N2)			__logger__.Add(msg, N1, N2)
	#define CALoggerMsgN3(msg, N1, N2, N3)		__logger__.Add(msg, N1, N2, N3)
	#define CALoggerMsgN4(msg, N1, N2, N3, N4)	__logger__.Add(msg, N1, N2, N3, N4)
	
	#define CALoggerClear						__logger__.Clear()

#else

	#define CALoggerMake
	#define CALoggerMakeMsg(msg)

	#define CALoggerMsg(msg)
	#define CALoggerMsgN1(msg, N1)
	#define CALoggerMsgN2(msg, N1, N2)
	#define CALoggerMsgN3(msg, N1, N2, N3)
	#define CALoggerMsgN4(msg, N1, N2, N3, N4)
	
	#define CALoggerClear

#endif

#endif // #define __CALogger_h__
