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
#include "CAAUPresetFile.h"

const CFStringRef  	CAAUPresetFile::kAUPresetFileExtension = CFSTR("aupreset");
const CFStringRef 	CAAUPresetFile::kAUPresetFileDirName = CFSTR("Presets");
const CFStringRef   CAAUPresetFile::kAUPresetNameKeyString = CFSTR(kAUPresetNameKey);
const CFStringRef   CAAUPresetFile::kAUPresetDataKeyString = CFSTR(kAUPresetDataKey);

static const CFStringRef	kAUUnknownName = CFSTR("Unnamed Audio Unit");
static const CFStringRef	kAUUnknownMfr = CFSTR("Unknown Manufacturer");

CAAUPresetFile::CAAUPresetFile (CAComponent inComp, bool inShouldSearchNetwork)
	: CAFileHandling(CAAUPresetFile::kAUPresetFileDirName, inShouldSearchNetwork),
	  mComp (inComp)
{
	CFStringRef name = mComp.GetAUName();
	CFStringRef manu = mComp.GetAUManu();
	
	if (!name && !manu) {
		SetUserDir (NULL);
		SetLocalDir (NULL);
		SetNetworkDir (NULL);
		return;
	}
	
	if (!name)
		name = kAUUnknownName;
	
	if (!manu)
		manu = kAUUnknownMfr;
	
	FSRef ref;
	
	const FSRef *startRef = GetUserDir ();
	if (startRef) {
		if (GetRootDir (*startRef, manu, name, ref) == noErr)
			SetUserDir (&ref);
		else
			SetUserDir (NULL);
	}
	
	startRef = GetLocalDir ();
	if (startRef) {
		if (GetRootDir (*startRef, manu, name, ref) == noErr)
			SetLocalDir (&ref);
		else
			SetLocalDir (NULL);
	}
	
	if (inShouldSearchNetwork)
	{
		startRef = GetNetworkDir ();
		if (startRef) {
			if (GetRootDir (*startRef, manu, name, ref) == noErr)
				SetNetworkDir (&ref);
			else
				SetNetworkDir (NULL);
		}
	}
	
	if (HasValidDir())
		CreateTrees (inShouldSearchNetwork);
}

CAAUPresetFile::~CAAUPresetFile()
{
}

OSStatus		CAAUPresetFile::CreateSubDirectories (FSRef &inParentRef, SInt16 inDomain)
{
	CFStringRef name = mComp.GetAUName();
	CFStringRef manu = mComp.GetAUManu();
	if (!name && !manu)
		return -1;
	
	if (!name)
		name = kAUUnknownName;
	
	if (!manu)
		manu = kAUUnknownMfr;
	
	OSStatus result;
	FSRef ref;
	if (result = GetRootDir (inParentRef, manu, name, ref, true)) {
        switch (inDomain) {
            case kUserDomain: SetUserDir (NULL); break;
            case kLocalDomain: SetLocalDir (NULL); break;
            case kNetworkDomain: SetNetworkDir (NULL); break;
        }
        return result;
    }
	
    return CAFileHandling::CreateSubDirectories (ref, inDomain);
}

OSStatus		CAAUPresetFile::GetRootDir (const FSRef &inRef, CFStringRef manuName, CFStringRef compName, FSRef &outRef, bool inCreateDir)
{
	FSRef ref;	
	OSStatus result;
	if (result = FindSpecifiedDir (inRef, manuName, ref, inCreateDir))
		return result;

	if (result = FindSpecifiedDir (ref, compName, outRef, inCreateDir))
		return result;
	
	return noErr;
}

bool		CAAUPresetFile::ValidPropertyList (CFPropertyListRef inData)
{
	if (CAFileHandling::ValidPropertyList (inData) == false)
		return false;
		
	CFDictionaryRef dict = static_cast<CFDictionaryRef>(inData);
	CFNumberRef cfnum = static_cast<CFNumberRef>(CFDictionaryGetValue(dict, CFSTR(kAUPresetTypeKey)));
	if (cfnum == NULL) {
		return false;
	}
	OSType type;
	CFNumberGetValue (cfnum, kCFNumberSInt32Type, &type);
	
	cfnum = static_cast<CFNumberRef>(CFDictionaryGetValue(dict, CFSTR(kAUPresetSubtypeKey)));
	if (cfnum == NULL) {
		return false;
	}
	OSType subtype;
	CFNumberGetValue (cfnum, kCFNumberSInt32Type, &subtype);

	cfnum = static_cast<CFNumberRef>(CFDictionaryGetValue(dict, CFSTR(kAUPresetManufacturerKey)));
	if (cfnum == NULL) {
		return false;
	}
	OSType manu;
	CFNumberGetValue (cfnum, kCFNumberSInt32Type, &manu);
				
	if (!(manu == mComp.Desc().Manu() && subtype == mComp.Desc().SubType() && type == mComp.Desc().Type()))
		return false;

	return true;
}

bool		CAAUPresetFile::IsPartPreset (CFTreeRef inTree) const
{
	CFPropertyListRef preset;
	OSStatus result;
	if ((result = ReadFromTreeLeaf (inTree, preset)) == noErr) 
	{
		CFStringRef partKey = CFSTR (kAUPresetPartKey);
		if (CFGetTypeID(preset) == CFDictionaryGetTypeID()) {
			bool res = CFDictionaryContainsKey ((CFDictionaryRef)preset, partKey);
			CFRelease (preset);
			return res;
		}
		CFRelease (preset);
		return false;
	}
	return false;
}
