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
#include "CALogger.h"
#include "CADebugMacros.h"

CALogger::CALogger () 
{
	mInitString = NULL;
	mStr = NULL;
}
		
CALogger::CALogger (char* str)
{
	mInitString = (char*)malloc (strlen(str) + 1);
	strcpy (mInitString, str);
	mStr = NULL;
}
		
CALogger::~CALogger ()
{
	if (mInitString)
		DebugMessageN1 ("-> %s", mInitString);
	if (mStr)
		DebugMessageN1 (" : %s", mStr);
	if (mInitString)
		DebugMessageN1 ("<- %s", mInitString);

	free (mInitString);
	free (mStr);
}
			
void CALogger::Add (char* str, ...)
{
	char tStr[1024];
	tStr[0] = 0;
	
	va_list args;
	va_start(args, str);
	vsprintf (tStr, str, args);
	va_end(args);

	int len = mStr ? strlen(mStr) : 0;
	mStr = (char*)realloc (mStr, len + strlen (tStr) + 5 + 1);
	if (len) {
		mStr[len] = '\n';
		mStr[len+1] = ' ';
		mStr[len+2] = ':';
		mStr[len+3] = ' ';
		mStr[len+4] = 0;
	} else
		mStr[0] = 0;

	strcat (mStr, tStr);
}

void CALogger::Clear ()
{
	free (mStr);
	mStr = NULL;

	free (mInitString);
	mInitString = NULL;
}
		
CALogger::CALogger (const CALogger& c) 
{
	mInitString = NULL;
	mStr = NULL;
}
	
CALogger& CALogger::operator= (const CALogger& c) 
{ 
	return *this; 
}
