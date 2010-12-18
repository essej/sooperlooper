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
#import "CAMIDIEndpointMenu2.h"
#include <vector>

class MIDIEndpointInfoMgr {
public:
	struct EndpointInfo {
		MIDIUniqueID	mUniqueID;
		NSString *		mName;
		MIDIEndpointRef	mEndpoint;
	};
	
	typedef std::vector<EndpointInfo> EndpointInfoList;

	MIDIEndpointInfoMgr() { UpdateFromCurrentState(); }
	
	void	Clear() {
		EndpointInfoList::iterator eit;
		for (eit = mSources.begin(); eit != mSources.end(); ++eit)
			[(*eit).mName release];
		for (eit = mDestinations.begin(); eit != mDestinations.end(); ++eit)
			[(*eit).mName release];
		mSources.clear();
		mDestinations.clear();
	}

	void	UpdateFromCurrentState() {
		Clear();
		
		int i, n;
		MIDIEndpointRef e;
		
		n = MIDIGetNumberOfDestinations();
		for (i = 0; i < n; ++i) {
			e = MIDIGetDestination(i);
			if (e == NULL) continue;
			
			EndpointInfo ei;
			ei.mEndpoint = e;
			if (MIDIObjectGetIntegerProperty(e, kMIDIPropertyUniqueID, &ei.mUniqueID)) continue;
			if (MIDIObjectGetStringProperty(e, kMIDIPropertyDisplayName, (CFStringRef *)&ei.mName)) continue;
			mDestinations.push_back(ei);
		}
		
		n = MIDIGetNumberOfSources();
		for (i = 0; i < n; ++i) {
			e = MIDIGetSource(i);
			if (e == NULL) continue;
			
			EndpointInfo ei;
			ei.mEndpoint = e;
			if (MIDIObjectGetIntegerProperty(e, kMIDIPropertyUniqueID, &ei.mUniqueID)) continue;
			if (MIDIObjectGetStringProperty(e, kMIDIPropertyDisplayName, (CFStringRef *)&ei.mName)) continue;
			mSources.push_back(ei);
		}
	}
	
	EndpointInfoList &	Sources() { return mSources; }
	EndpointInfoList &	Destinations() { return mDestinations; }
	
private:
	EndpointInfoList	mSources;
	EndpointInfoList	mDestinations;
};

static MIDIEndpointInfoMgr *gMIDIEndpoints = NULL;
static NSMutableSet *		gInstances = NULL;
static MIDIClientRef		gClient = NULL;

// CoreMIDI callback for when endpoints change -- rebuilds all menu instances
static void NotifyProc(const MIDINotification *message, void *refCon)
{
	if (message->messageID == kMIDIMsgSetupChanged) {
		gMIDIEndpoints->UpdateFromCurrentState();
		
		NSEnumerator *e = [gInstances objectEnumerator];
		CAMIDIEndpointMenu *menu;
		while ((menu = [e nextObject]) != nil)
			[menu rebuildMenu];
	}
}

@implementation CAMIDIEndpointMenu

- (void)_init
{
	if (gInstances == NULL)
		gInstances = [[NSMutableSet alloc] init];
	[gInstances addObject: self];
	mInited = YES;
	mType = -1;
	mOptions = 0;
	mSelectedUniqueID = 0;
}

- (id)initWithFrame: (NSRect)frame
{
    self = [super initWithFrame: frame];
    if (self)
		[self _init];
    return self;
}

- (void)dealloc
{
	[gInstances removeObject: self];
	if ([gInstances count] == 0) {
		delete gMIDIEndpoints;	gMIDIEndpoints = NULL;
		[gInstances release];	gInstances = nil;
		if (gClient) {
			MIDIClientDispose(gClient);
			gClient = NULL;
		}
	}
	[super dealloc];
}

- (void)buildMenu: (int)type opts: (int)opts
{
	if (!mInited)
		[self _init];
	
	if (gClient == NULL)
		MIDIClientCreate(CFSTR(""), NotifyProc, NULL, &gClient);
	if (gMIDIEndpoints == NULL)
		gMIDIEndpoints = new MIDIEndpointInfoMgr;
	
	mType = type;
	mOptions = opts;
	[self rebuildMenu];
}

- (void)syncSelectedName
{
}

static NSString *UniqueTitle(NSString *name, NSMutableDictionary *previousTitles)
{
	NSString *newItemTitle = name;
	int suffix = 0;
	while (true) {
		if ([previousTitles objectForKey: newItemTitle] == nil)
			break;
		if (suffix == 0) suffix = 2; else ++suffix;
		newItemTitle = [NSString stringWithFormat: @"%@ #%d", name, suffix];
	}
	[previousTitles setObject: newItemTitle forKey: newItemTitle];
	return newItemTitle;
}

- (void)rebuildMenu
{
	int itemsToKeep = (mOptions & kMIDIEndpointMenuOpt_CanSelectNone) ? 1 : 0;
	
	while ([self numberOfItems] > itemsToKeep)
		[self removeItemAtIndex: itemsToKeep];

	MIDIEndpointInfoMgr::EndpointInfoList &eil = (mType == kMIDIEndpointMenuSources) ? gMIDIEndpoints->Sources() : gMIDIEndpoints->Destinations();
	
	NSMutableDictionary *previousTitles = [[NSMutableDictionary alloc] init];
	int n = eil.size();
	bool foundSelection = false;
	for (int i = 0; i < n; ++i) {
		MIDIEndpointInfoMgr::EndpointInfo *ei = &eil[i];
		NSString *name = ei->mName;
		NSString *newItemTitle = UniqueTitle(name, previousTitles);
		// see if that collides with any previous item -- base class requires unique titles
		
		[self addItemWithTitle: newItemTitle]; // cast from CFString
		if (ei->mUniqueID == mSelectedUniqueID) {
			[self selectItemAtIndex: itemsToKeep + i];
			[self syncSelectedName];
			foundSelection = true;
		}
	}
	if (!foundSelection)
		[self selectItemAtIndex: 0];
	[previousTitles release];
}

- (MIDIEndpointRef)selectedEndpoint
{
	int itemsToIgnore = (mOptions & kMIDIEndpointMenuOpt_CanSelectNone) ? 1 : 0;
	int i = [self indexOfSelectedItem];
	if (i >= itemsToIgnore) {
		MIDIEndpointInfoMgr::EndpointInfoList &eil = (mType == kMIDIEndpointMenuSources) ? gMIDIEndpoints->Sources() : gMIDIEndpoints->Destinations();
	
		MIDIEndpointInfoMgr::EndpointInfo *ei = &eil[i - itemsToIgnore];
		mSelectedUniqueID = ei->mUniqueID;
		[self syncSelectedName];
		return ei->mEndpoint;
	}
	return NULL;
}

- (MIDIUniqueID)selectedUniqueID
{
	[self syncSelectedName];
	int itemsToIgnore = (mOptions & kMIDIEndpointMenuOpt_CanSelectNone) ? 1 : 0;
	int i = [self indexOfSelectedItem];
	MIDIEndpointInfoMgr::EndpointInfoList &eil = (mType == kMIDIEndpointMenuSources) ? gMIDIEndpoints->Sources() : gMIDIEndpoints->Destinations();
	MIDIUniqueID uid = (i >= itemsToIgnore) ? eil[i - itemsToIgnore].mUniqueID : kMIDIInvalidUniqueID;
	mSelectedUniqueID = uid;
	return uid;
}

- (BOOL)selectUniqueID: (MIDIUniqueID)uniqueID
{
	mSelectedUniqueID = uniqueID;
	int itemsToIgnore = (mOptions & kMIDIEndpointMenuOpt_CanSelectNone) ? 1 : 0;
	if (uniqueID == kMIDIInvalidUniqueID && itemsToIgnore == 1) {
		[self selectItemAtIndex: 0];
		[self syncSelectedName];
		return YES;
	}
	MIDIEndpointInfoMgr::EndpointInfoList &eil = (mType == kMIDIEndpointMenuSources) ? gMIDIEndpoints->Sources() : gMIDIEndpoints->Destinations();

	int n = eil.size();
	for (int i = 0; i < n; ++i) {
		MIDIEndpointInfoMgr::EndpointInfo *ei = &eil[i];
		if (ei->mUniqueID == uniqueID) {
			[self selectItemAtIndex: itemsToIgnore + i];
			[self syncSelectedName];
			return YES;
		}
	}
	return NO;
}

@end
