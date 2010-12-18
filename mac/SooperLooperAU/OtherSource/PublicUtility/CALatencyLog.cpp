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

#include "CALatencyLog.h"
#include "CADebugMacros.h"
#include "CAException.h"
#include "CAHostTimeBase.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

//	routines from latency.c
#if Panther_LatencyLog
extern "C" void initialize();
extern "C" void sample_sc(uint64_t start, uint64_t stop, char *binPointer);
#else
extern "C" void		InitializeLatencyTracing(const char* inLogFile);
extern "C" void		TeardownLatencyTracing();
extern "C" FILE*	GetLatencyLogFile();
extern "C" void		SetLatencyLogFile(FILE* inLogFile);
extern "C" void		sample_sc(uint64_t start, uint64_t stop);
#endif

//=============================================================================
//	CALatencyLog
//=============================================================================

CALatencyLog::CALatencyLog(const char* inLogFileName, const char* inLogFileExtension)
{
	mIsRoot = CanUse();
	Assert(mIsRoot, "CALatencyLog::CALatencyLog: have to be root to use");
	
#if Panther_LatencyLog
	memset(mBuffer, 0, kLatencyBufferSize);
	strcpy(mLogFileName, inLogFileName);
	strcpy(mLogFileExtension, inLogFileExtension);
	
	if(mIsRoot)
	{
		initialize();
	}
#else
	BuildFileName(inLogFileName, inLogFileExtension, mLogFileName);
	
	if(mIsRoot)
	{
		InitializeLatencyTracing(mLogFileName);
	}
#endif
}

CALatencyLog::~CALatencyLog()
{
#if !Panther_LatencyLog
	if(mIsRoot)
	{
		TeardownLatencyTracing();
	}
#endif
}

void	CALatencyLog::Capture(UInt64 inStartTime, UInt64 inEndTime, bool inWriteToFile, const char* header)
{
	if(mIsRoot)
	{
#if Panther_LatencyLog
		sample_sc(inStartTime, inEndTime, mBuffer);
		
		if(inWriteToFile)
		{
			char	theLogFileName[PATH_MAX];
			BuildFileName(mLogFileName, mLogFileExtension, theLogFileName);
			
			DebugMessageN3("Capturing latency log from %fmics to %fmics into \"%s\"\n", CAHostTimeBase::ConvertToNanos(inStartTime) / 1000.0, CAHostTimeBase::ConvertToNanos(inEndTime) / 1000.0, theLogFileName);
			
			UInt32 theLogSize = strlen(mBuffer);
			
			//	open the file
			FILE* theFile = fopen(theLogFileName, "a+");
			if(theFile != NULL)
			{
				if (header)
					fwrite (header, 1, strlen (header), theFile);
					
				//	write the data at the end
				fwrite(mBuffer, 1, theLogSize, theFile);
								
				//	close the file
				fclose(theFile);
			}
		}
#else
		FILE* theLogFile = NULL;
		FILE* theSavedLogFile = NULL;
		
		if(inWriteToFile)
		{
			theLogFile = GetLatencyLogFile();
		}
		else
		{
			theLogFile = stdout;
			theSavedLogFile = GetLatencyLogFile();
			SetLatencyLogFile(stdout);
		}
		
		if(header != NULL)
		{
			fprintf(theLogFile, "%s", header);
		}
		
		sample_sc(inStartTime, inEndTime);
		
		if(theSavedLogFile != NULL)
		{
			SetLatencyLogFile(theSavedLogFile);
		}
#endif
	}
	else
	{
		DebugMessage("CALatencyLog::Capture: must be root to use.");
	}
}

#if Panther_LatencyLog
void	CALatencyLog::GetLog(char* outLog, UInt32& ioLogSize) const
{
	if(mIsRoot)
	{
		UInt32 theLogSize = strlen(mBuffer) + 1;
		
		if(theLogSize < ioLogSize)
		{
			ioLogSize = theLogSize;
		}
		
		strncpy(outLog, mBuffer, ioLogSize);
	}
	else
	{
		DebugMessage("CALatencyLog::Capture: must be root to use.");
	}
}
	
UInt32	CALatencyLog::GetLogSize() const
{
	UInt32 theAnswer = 0;
	
	if(mIsRoot)
	{
		theAnswer = strlen(mBuffer) + 1;
	}
	else
	{
		DebugMessage("CALatencyLog::Capture: must be root to use.");
	}
	
	return theAnswer;
}
#endif

void	CALatencyLog::BuildFileName(const char* inRoot, const char* inExtension, char* outFileName)
{
	sprintf(outFileName, "%s%s", inRoot, inExtension);
	
	struct stat	statbuf;
	if(stat(outFileName, &statbuf) == 0)
	{
		UInt32 theFileNumber = 1;
		while(stat(outFileName, &statbuf) == 0)
		{
			sprintf(outFileName, "%s-%d%s", inRoot, (int)theFileNumber, inExtension);
			++theFileNumber;
		}
	}
	
}
