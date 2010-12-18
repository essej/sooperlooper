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
#ifndef __CAFileHandling_h__
#define __CAFileHandling_h__

#include <CoreServices/CoreServices.h>
#include "CAComponent.h"

// Creates a Tree from Library/Audio/
// Each contained tree's context.info is a CFURLRef - The client should NOT release this URL
// The Returned Trees are owned by the objects
// if you save a file in one of the trees, it is added to the tree for you

// The Root of the tree handed back contains the parent dir from which all of the tree is built

// A CAFileHandling subclass is designed to deal with files of a particular type
// Thus, the IsItem call will return true if Tree node matches the described files

// Thanks to Marc Poirier for suggestions incorporated into this implementation

class CAFileHandling 
{
public:
		// *all* files have the same name key
	static const CFStringRef	kItemNameKey;
	
											// may return NULL if no tree in this domain
	CFTreeRef							GetLocalTree () const { return mLocalTree; }
	CFTreeRef							GetUserTree () const { return mUserTree; }
	CFTreeRef							GetNetworkTree () const { return mNetworkTree; }
	
	OSStatus							SaveInDirectory (CFTreeRef inTree, CFStringRef fileName, CFPropertyListRef inData);
											
											//can pass in a pointer to a CFString if this is NOT null on return - must release it
	OSStatus							ReadFromTreeLeaf (CFTreeRef inTreeLeaf, CFPropertyListRef &outData, 
																			CFStringRef *outErrString = NULL) const;
	
	bool								IsDirectory (CFTreeRef inTree) const;

	bool								IsItem (CFTreeRef inTree) const;
    
    bool								IsUserContext (CFTreeRef inTree);
    bool								IsNetworkContext (CFTreeRef inTree);
    bool								IsLocalContext (CFTreeRef inTree);
    
											// these calls can create the sub directories
											// that should exist in the specified locale
											// this will create root trees for these directories
											// returns noErr if these directories exist
	OSStatus							CreateUserDirectories ();
	OSStatus							CreateNetworkDirectories ();
	OSStatus							CreateLocalDirectories ();
										
	OSStatus							CreateDirectory (CFTreeRef inDirTree, CFStringRef inDirName, CFTreeRef *outTree);
    
	OSStatus							GetNameCopy (CFTreeRef inTree, CFStringRef &outName) const;
    OSStatus							GetPathCopy (CFTreeRef inTree, CFURLRef &outURL) const;
    
	void								ShowEntireTree (CFTreeRef inTree);
	
protected:
										CAFileHandling (CFStringRef inSubDir, bool inShouldSearchNetwork);
										virtual ~CAFileHandling ();

	static OSStatus						FindSpecifiedDir (const FSRef &inParentDir, CFStringRef inSubDir, FSRef &outDir, bool inCreateDir = false);
	bool								HasValidDir () const { return mHasUserDir || mHasLocalDir || mHasNetworkDir; }
	const FSRef*						GetUserDir () const { return mHasUserDir ? &mUserDir : NULL; }
	const FSRef*						GetLocalDir () const { return mHasLocalDir ? &mLocalDir : NULL; }
	const FSRef*						GetNetworkDir () const { return mHasNetworkDir ? &mNetworkDir : NULL; }
    
	void								CreateTrees (bool inShouldSearchNetwork);
		
		// if NULL it will invalidate user dir
		// if NOT NULL AND user dir is valid, resets base level directory
	void  								SetUserDir (FSRef *inRef);
	void  								SetLocalDir (FSRef *inRef);
	void  								SetNetworkDir (FSRef *inRef);

	virtual	const CFStringRef			GetExtension () const = 0;
	virtual bool						ValidPropertyList (CFPropertyListRef inData);
	virtual OSStatus					CreateSubDirectories (FSRef &inParentRef, SInt16 inDomain);
	virtual OSStatus					PremassageDataToSave (CFPropertyListRef inData, CFStringRef inName, CFPropertyListRef &outData)
										{
											outData = inData;
											return noErr;
										}
	virtual void						PostmassageSavedData (CFPropertyListRef inData) {}
	virtual void						PostProcessReadData (CFPropertyListRef readData, CFPropertyListRef &outData) const
										{
											outData = readData;
										}

private:
	static OSStatus			FindSpecifiedDir (SInt16 inDomain, CFStringRef inAudioSubDirName, FSRef &outDir, bool inCreateDir = false);

	static CFTreeRef 		AddFileItemToTree (const FSRef &inRef, CFTreeRef inParentTree);
	static CFTreeRef		CreateTree (const FSRef &inRef);
	static CFTreeRef		CreateTree (CFURLRef inURL);
	CFTreeRef 				CreateNewTree (const FSRef* inRef, CFTreeRef inTree);
	void					Scan (const FSRef &inParentDir, CFTreeRef inParentTree);

	FSRef			mLocalDir, mNetworkDir, mUserDir;
	bool			mHasLocalDir, mHasNetworkDir, mHasUserDir;
	CFTreeRef		mLocalTree, mUserTree, mNetworkTree;
	CFStringRef		mSubDirName;
};

#endif
