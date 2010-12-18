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
#include <ApplicationServices/ApplicationServices.h>
#include "CAFileHandling.h"
#include "CACFDictionary.h"

/*
  kLocalDomain                  = -32765, // Domain from '/'
  kNetworkDomain                = -32764, // Domain from '/Network/
  kUserDomain                   = -32763, // Domain from '~/'
*/

const CFStringRef	CAFileHandling::kItemNameKey = CFSTR("name");

CAFileHandling::CAFileHandling (CFStringRef inSubDir, bool inShouldSearchNetwork)
	: mHasLocalDir (false), mHasNetworkDir(false), mHasUserDir(false),
	  mLocalTree (NULL), mUserTree (NULL), mNetworkTree (NULL),
	  mSubDirName (inSubDir)
{
	mHasLocalDir = FindSpecifiedDir (kLocalDomain, inSubDir, mLocalDir) == noErr;
	mHasUserDir = FindSpecifiedDir (kUserDomain, inSubDir, mUserDir) == noErr;
	if (inShouldSearchNetwork)
		mHasNetworkDir = FindSpecifiedDir (kNetworkDomain, inSubDir, mNetworkDir) == noErr;
}

CAFileHandling::~CAFileHandling() 
{
	if (mLocalTree)
		CFRelease (mLocalTree);
	if (mUserTree)
		CFRelease (mUserTree);
	if (mNetworkTree)
		CFRelease (mNetworkTree);
}

OSStatus	CAFileHandling::FindSpecifiedDir (SInt16 inDomain, CFStringRef inAudioSubDirName, FSRef &outDir, bool inCreateDir)
{
	OSStatus result;
	FSRef parentDir;
	if (result = FSFindFolder (inDomain, kAudioSupportFolderType, (Boolean)inCreateDir, &parentDir))
		return result;

	if (result = FindSpecifiedDir (parentDir, inAudioSubDirName, outDir, inCreateDir)) 
	{
		return result;
	}
	return noErr;
}

OSStatus	CAFileHandling::FindSpecifiedDir (const FSRef &inParentDir, CFStringRef inSubDirName, FSRef &outDir, bool inCreateDir)
{
	UInt32 strLen = CFStringGetLength(inSubDirName);
	UniChar * chars = new UniChar[strLen];
	CFRange range;
	range.location = 0;
	range.length = strLen;
	
	CFStringGetCharacters (inSubDirName, range, chars);
		
	OSStatus result = FSMakeFSRefUnicode (&inParentDir, strLen, chars, kTextEncodingDefaultFormat, &outDir);
	if (result)
	{
		if (result == fnfErr && inCreateDir) {
			result = FSCreateDirectoryUnicode (&inParentDir, strLen, chars, kFSCatInfoNone, NULL, &outDir, NULL, NULL);		
		}
	}
	delete [] chars;
	
	return result;
}

void		CAFileHandling::SetUserDir (FSRef *inRef)
{
	if (inRef) {
		mUserDir = *inRef;
		mHasUserDir = true;
	} else {
		mHasUserDir = false;
	}
}

void		CAFileHandling::SetLocalDir (FSRef *inRef)
{
	if (inRef) {
		mLocalDir = *inRef;
		mHasLocalDir = true;
	} else {
		mHasLocalDir = false;
	}
}

void		CAFileHandling::SetNetworkDir (FSRef *inRef)
{
	if (inRef) {
		mNetworkDir = *inRef;
		mHasNetworkDir = true;
	} else {
		mHasNetworkDir = false;
	}
}

OSStatus		CAFileHandling::CreateSubDirectories (FSRef &inRef, SInt16 inDomain)
{
	switch (inDomain) {
		case kUserDomain: SetUserDir (&inRef); break;
		case kLocalDomain: SetLocalDir (&inRef); break;
		case kNetworkDomain: SetNetworkDir (&inRef); break;
	}
    
	return noErr;
}

void TreeShow (const void *value, void *context)
{
	if (value) {
		CAFileHandling* This = (CAFileHandling*)context;
		This->ShowEntireTree ((CFTreeRef)value);
	}
}

void		CAFileHandling::ShowEntireTree (CFTreeRef inTree)
{
	CFShow (inTree);
	if (inTree)
		CFTreeApplyFunctionToChildren (inTree, TreeShow, this);
}

CFTreeRef 	CAFileHandling::AddFileItemToTree (const FSRef &inRef, CFTreeRef inParentTree)
{
	CFTreeRef newTree = CreateTree (inRef);
	CFTreeAppendChild (inParentTree, newTree);
#if 0
	CFShow (newTree);
#endif
	CFRelease(newTree);

	return newTree;
}



void		CAFileHandling::CreateTrees (bool inShouldSearchNetwork)
{
	mLocalTree = CreateNewTree (GetLocalDir(), mLocalTree);
	if (inShouldSearchNetwork)
		mNetworkTree = CreateNewTree (GetNetworkDir(), mNetworkTree);
	mUserTree = CreateNewTree (GetUserDir(), mUserTree);
}
		
CFTreeRef 		CAFileHandling::CreateNewTree (const FSRef* inRef, CFTreeRef inTree)
{
	if (inRef) {
		if (inTree)
			CFRelease (inTree);
		CFTreeRef tree = CreateTree (*inRef); 
		Scan (*inRef, tree);
		return tree;
	}
	return NULL;
}

CFTreeRef	CAFileHandling::CreateTree (const FSRef &inRef)
{
	CFURLRef url = CFURLCreateFromFSRef (kCFAllocatorDefault, &inRef);
	return CreateTree (url);
}

CFTreeRef	CAFileHandling::CreateTree (CFURLRef inURL)
{
	CFTreeContext treeContext;

	treeContext.version = 0;
	treeContext.info = (void*)inURL;
	treeContext.retain = CFRetain;
	treeContext.release = CFRelease;
	treeContext.copyDescription = CFCopyDescription;

	CFTreeRef tree = CFTreeCreate (kCFAllocatorDefault, &treeContext);
	CFRelease(inURL);
	
	return tree;
}

void		CAFileHandling::Scan (const FSRef &inParentDir, CFTreeRef inParentTree)
{		
	OSStatus result;
	FSIterator iter;
	LSItemInfoRecord info;
	bool fileIsBundle;
	
	if (FSOpenIterator (&inParentDir, kFSIterateFlat, &iter))
		return;

	do {
		ItemCount numItems = 0;

		const FSCatalogInfoBitmap whichInfo = kFSCatInfoNodeFlags;

		FSCatalogInfo catalogInfo;
		FSRef theFSRef;
		result = FSGetCatalogInfoBulk (iter, 1, &numItems, NULL, whichInfo, &catalogInfo, &theFSRef, NULL, NULL);

		if (!result && (numItems > 0)) 
		{
			if (catalogInfo.nodeFlags & kFSNodeIsDirectoryMask)
			{
				// WE FOUND A SUB DIRECTORY
				// only add it to the tree if it's NOT a package directory
				result = LSCopyItemInfoForRef( &theFSRef, kLSRequestBasicFlagsOnly, &info );
				fileIsBundle = false;
				if (	(result == noErr) &&
						((kLSItemInfoIsPackage & info.flags) != 0) )
					fileIsBundle = true;
				
				if (!fileIsBundle) {
					CFTreeRef newSubTree = AddFileItemToTree (theFSRef, inParentTree);
	#if 0
					printf ("\n* * * ADDING DIR * * *\n");
					CFShow (newSubTree);
	#endif
					Scan (theFSRef, newSubTree);
				}
			}
			else
			{
				// WE FOUND A FILE
				CFURLRef fileURL = CFURLCreateFromFSRef (kCFAllocatorDefault, &theFSRef);
				CFStringRef fNameExt = CFURLCopyPathExtension (fileURL);
				bool matches = false;
				if (fNameExt) {
					matches = CFStringCompare(fNameExt, GetExtension(), kCFCompareCaseInsensitive) == kCFCompareEqualTo;
					CFRelease (fNameExt);
				}
				CFRelease (fileURL);
				if (matches) {
#if 0
					printf ("* * * ADDING FILE * * *\n");
#endif
					AddFileItemToTree (theFSRef, inParentTree);
				}
			}
		}
	} while (!result);

	// clean up
	FSCloseIterator (iter);
}

bool			CAFileHandling::IsDirectory (CFTreeRef inTree) const
{
	CFTreeContext context;
	CFTreeGetContext (inTree, &context);

	return CFURLHasDirectoryPath ((CFURLRef)context.info);
}

bool			CAFileHandling::IsItem (CFTreeRef inTree) const
{
	CFTreeContext context;
	CFTreeGetContext (inTree, &context);

	CFStringRef fNameExt = CFURLCopyPathExtension ((CFURLRef)context.info);
	if (fNameExt) {
		bool matches = CFStringCompare(fNameExt, GetExtension(), kCFCompareCaseInsensitive) == kCFCompareEqualTo;
		CFRelease (fNameExt);
		return matches;
	}
	return false;
}

bool	CAFileHandling::IsUserContext (CFTreeRef inTree)
{
	CFTreeContext context;
	CFTreeGetContext (inTree, &context);
	CFURLRef URL = (CFURLRef)context.info;
    
    CFURLRef userURL = CFURLCreateFromFSRef (NULL, &mUserDir);
    if (userURL && URL)
        if (CFStringHasPrefix (CFURLGetString(URL), CFURLGetString(userURL)))
            return true;
    
    return false;
}

bool	CAFileHandling::IsNetworkContext (CFTreeRef inTree)
{
	CFTreeContext context;
	CFTreeGetContext (inTree, &context);
	CFURLRef URL = (CFURLRef)context.info;
    
    CFURLRef networkURL = CFURLCreateFromFSRef (NULL, &mNetworkDir);
    if (networkURL && URL)
        if (CFStringHasPrefix (CFURLGetString(URL), CFURLGetString(networkURL)))
            return true;
    
    return false;
}

bool	CAFileHandling::IsLocalContext (CFTreeRef inTree)
{
	CFTreeContext context;
	CFTreeGetContext (inTree, &context);
	CFURLRef URL = (CFURLRef)context.info;
    
    CFURLRef localURL = CFURLCreateFromFSRef (NULL, &mLocalDir);
    if (localURL && URL)
        if (CFStringHasPrefix (CFURLGetString(URL), CFURLGetString(localURL)))
            return true;
    
    return false;
}

OSStatus		CAFileHandling::CreateUserDirectories ()
{
	if (GetUserDir() && mUserTree)
		return noErr;
	
	OSStatus result;
	if (result = FindSpecifiedDir (kUserDomain, mSubDirName, mUserDir, true))
		return result;
	
	if (result = CreateSubDirectories (mUserDir, kUserDomain))
		return result;

	mUserTree = CreateNewTree (GetUserDir(), mUserTree);

	return noErr;	
}

OSStatus		CAFileHandling::CreateLocalDirectories ()
{
	if (GetLocalDir() && mLocalTree)
		return noErr;
	
	OSStatus result;
	if (result = FindSpecifiedDir (kLocalDomain, mSubDirName, mLocalDir, true))
		return result;
	
	if (result = CreateSubDirectories (mLocalDir, kLocalDomain))
		return result;

	mLocalTree = CreateNewTree (GetLocalDir(), mLocalTree);

	return noErr;	
}

OSStatus		CAFileHandling::CreateNetworkDirectories ()
{
	if (GetNetworkDir() && mNetworkTree)
		return noErr;
	
	OSStatus result;
	if (result = FindSpecifiedDir (kNetworkDomain, mSubDirName, mNetworkDir, true))
		return result;
	
	if (result = CreateSubDirectories (mNetworkDir, kNetworkDomain))
		return result;

	mNetworkTree = CreateNewTree (GetNetworkDir(), mNetworkTree);

	return noErr;	
}

OSStatus		CAFileHandling::CreateDirectory (CFTreeRef inDirTree, CFStringRef inDirName, CFTreeRef *outTree)
{
	if (IsDirectory (inDirTree) == false)
		return -1;

	CFTreeContext context;
	CFTreeGetContext (inDirTree, &context);

	FSRef parentDir;	
	if (CFURLGetFSRef((CFURLRef)context.info, &parentDir))
		return -1;
	
	FSRef subDir;
	OSStatus result;
	if (result = FindSpecifiedDir (parentDir, inDirName, subDir, true))
		return result;
	
	CFTreeRef newTree;
	newTree = CreateTree (subDir);
	CFTreeAppendChild (inDirTree, newTree);
	
	if (outTree)
		*outTree = newTree;
		
	return noErr;
}


bool		CAFileHandling::ValidPropertyList (CFPropertyListRef inData)
{
    // is our data good? (i.e., is it an instance of CFPropertyListRef?)
    if (!(	CFPropertyListIsValid (inData, kCFPropertyListOpenStepFormat)		||
            CFPropertyListIsValid (inData, kCFPropertyListXMLFormat_v1_0)		||
            CFPropertyListIsValid (inData, kCFPropertyListBinaryFormat_v1_0)	))
	{
		return false;
    }
	return true;
}

OSStatus		CAFileHandling::SaveInDirectory (CFTreeRef inParentTree, CFStringRef inFileName, CFPropertyListRef inData)
{
	CFTreeContext context;
	CFTreeGetContext (inParentTree, &context);
    
	if (CFURLHasDirectoryPath ((CFURLRef)context.info) == false)
		return paramErr;
	
	if (ValidPropertyList (inData) == false)
		return -1;
	
	CFPropertyListRef plist;
    OSStatus result;
	if (result = PremassageDataToSave (inData, inFileName, plist))
		return result;
    
	CFMutableStringRef fileName = NULL;
	CFURLRef fileURL = NULL;
	
    // Convert the property list into XML data.
	CFDataRef xmlData = CFPropertyListCreateXMLData (kCFAllocatorDefault, plist);
	if (xmlData == NULL) {
		result = paramErr;
		goto home;
	}
	
	fileName = CFStringCreateMutableCopy (kCFAllocatorDefault, 1024, inFileName);
	UniChar dot;
	dot = '.';
	CFStringAppendCharacters (fileName, &dot, 1);
	CFStringAppend (fileName, GetExtension());
	
	fileURL = CFURLCreateCopyAppendingPathComponent (kCFAllocatorDefault, (CFURLRef)context.info, fileName, false);

	// Write the XML data to the file.
	Boolean status;
	status = CFURLWriteDataAndPropertiesToResource (fileURL, xmlData, NULL, &result);
    
	CFRelease(xmlData);
	
	if (status == false || result) {
		CFRelease(fileURL);
		if (!result) result = -1;
		goto home;
	}
	
	CFTreeRef newTree;
	newTree = CreateTree (fileURL); //this releases the URL
	CFTreeAppendChild (inParentTree, newTree);

home:
	if (fileName)
		CFRelease (fileName);
	PostmassageSavedData (plist);
	return result;
}
	
OSStatus		CAFileHandling::ReadFromTreeLeaf (CFTreeRef inTreeLeaf, CFPropertyListRef &outData, CFStringRef *outErrString) const
{
	CFTreeContext context;
	CFTreeGetContext (inTreeLeaf, &context);

	if (CFURLHasDirectoryPath ((CFURLRef)context.info))
		return paramErr;
	
	CFDataRef         resourceData = NULL;
	SInt32            result;
    
   // Read the XML file.
   Boolean status = CFURLCreateDataAndPropertiesFromResource (kCFAllocatorDefault, (CFURLRef)context.info,
                                                                &resourceData,	// place to put file data
                                                                NULL, NULL, &result);
        if (status == false || result) {
            if (resourceData) 
				CFRelease (resourceData);
            return result;
        }
    
	CFStringRef errString = NULL;
	CFPropertyListRef theData = CFPropertyListCreateFromXMLData (kCFAllocatorDefault, resourceData,  
													kCFPropertyListImmutable, &errString);
        if (theData == NULL || errString) {
            if (resourceData) 
				CFRelease (resourceData);
			if (errString) {
				if (outErrString)
					*outErrString = errString;
				else
					CFRelease (errString);
			}
            return -1;
       }
		    
	CFRelease (resourceData);
    
	PostProcessReadData (theData, outData);
	
	return noErr;
}

OSStatus		CAFileHandling::GetNameCopy (CFTreeRef inTree, CFStringRef &outName) const
{
    // first check for directory
    CFTreeContext context;
    CFTreeGetContext (inTree, &context);
    
    CFURLRef url = (CFURLRef)context.info;
    if (!url) return paramErr;
    if (CFURLHasDirectoryPath (url)) {
        outName = CFURLCopyLastPathComponent (url);
        return noErr;
    }
    
    // else check for item name
    CFPropertyListRef data;
	OSStatus result;
    if ((result = ReadFromTreeLeaf (inTree, data)) == noErr) {
		if (CFGetTypeID(data) == CFDictionaryGetTypeID()) {
			CFStringRef name = (CFStringRef)CFDictionaryGetValue ((CFDictionaryRef)data, kItemNameKey);
			if (name) {
				CFRetain (name);
				outName = name;
			}
			CFRelease (data);
			return name ? noErr : OSStatus(paramErr);
		}
		return paramErr;
	}
	return result;
}

OSStatus		CAFileHandling::GetPathCopy (CFTreeRef inTree, CFURLRef &outURL) const
{
    // first check for directory
    CFTreeContext context;
    CFTreeGetContext (inTree, &context);
    
    CFURLRef url = (CFURLRef)context.info;
    CFRetain(url);
    outURL = url;
    return noErr;
}
