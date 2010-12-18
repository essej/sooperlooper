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
#include "CAMIDIEndpoints.h"
#include <algorithm>

// ____________________________________________________________________________
// Obtain the name of an endpoint without regard for whether it has connections.
// The result should be released by the caller.
static CFStringRef EndpointName(MIDIEndpointRef endpoint, bool isExternal)
{
	CFStringRef result = CFSTR("");	// default
	
	MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &result);
	
	return result;
}

// ____________________________________________________________________________

CAMIDIEndpoints::Endpoint::Endpoint(MIDIEndpointRef endpoint, CFStringRef name, MIDIObjectRef connectedObj) :
	mUniqueID(kMIDIInvalidUniqueID), 
	mIOEndpoint(endpoint), 
	mName(name), 
	mEntity(NULL),
	mEmbeddedOrVirtual(false),
	mConnectedObj(connectedObj),
	mNext(NULL),
	mPairMate(NULL)
{
	MIDIObjectGetIntegerProperty(connectedObj ? connectedObj : endpoint, kMIDIPropertyUniqueID, &mUniqueID);

	// Is the endpoint that of an embedded entity? or virtual?
	MIDIEndpointGetEntity(endpoint, &mEntity);
	if (mEntity == NULL) {
		mEmbeddedOrVirtual = true;	// presumably virtual
	} else {
		SInt32 embedded = 0;
		MIDIObjectGetIntegerProperty(mEntity, kMIDIPropertyIsEmbeddedEntity, &embedded);
		if (embedded) {
			mEmbeddedOrVirtual = true;
		}
	}
}

CAMIDIEndpoints::Endpoint::~Endpoint()
{
	if (mName)
		CFRelease(mName);
}

bool	CAMIDIEndpoints::Endpoint::GetEndpointInfo(EMode mode, EndpointInfo &info)
{
	Endpoint *ept = this, *ept2;
	if (mode == kPairs) {
		if ((ept2 = ept->PairMate()) == NULL)
			return false;
		info.mSourceEndpoint = ept2->IOEndpoint();
		info.mDestinationEndpoint = ept->IOEndpoint();
	} else if (mode == kSources) {
		info.mSourceEndpoint = ept->IOEndpoint();
		info.mDestinationEndpoint = NULL;
	} else {
		info.mSourceEndpoint = NULL;
		info.mDestinationEndpoint = ept->IOEndpoint();
	}
	info.mUniqueID = ept->UniqueID();

	if (ept->DriverOwned() && ept->Next() != NULL) {
		// add one item for all connected items
		CFMutableStringRef names = CFStringCreateMutable(NULL, 0);
		bool first = true;
		while (true) {
			ept = ept->Next();
			if (ept == NULL)
				break;
			if (!first) {
				CFStringAppend(names, CFSTR(", "));
			} else first = false;
			CFStringAppend(names, ept->Name());
		}
		info.mDisplayName = names;
	} else {
		// a driver-owned endpoint with nothing connected externally,
		// or an external endpoint
		CFRetain(info.mDisplayName = ept->Name());
	}
	return true;
}

// ____________________________________________________________________________

CAMIDIEndpoints::CAMIDIEndpoints()
{
	UpdateFromCurrentState();
}

CAMIDIEndpoints::~CAMIDIEndpoints()
{
	Clear();
}

void	CAMIDIEndpoints::Clear()
{
	EndpointList::iterator epit;
	for (epit = mSources.begin(); epit != mSources.end(); ++epit) {
		delete *epit;
	}
	mSources.clear();
	for (epit = mDestinations.begin(); epit != mDestinations.end(); ++epit) {
		delete *epit;
	}
	mDestinations.clear();
}

void	CAMIDIEndpoints::UpdateFromCurrentState()
{
	Clear();

	UInt32 i, n;
	MIDIEndpointRef epRef;
	
	n = MIDIGetNumberOfSources();
	mSources.reserve(n);
	for (i = 0; i < n; ++i) {
		epRef = MIDIGetSource(i);
		if (epRef)
			AddEndpoints(epRef, mSources);
	}

	n = MIDIGetNumberOfDestinations();
	mDestinations.reserve(n);
	for (i = 0; i < n; ++i) {
		epRef = MIDIGetDestination(i);
		if (epRef)
			AddEndpoints(epRef, mDestinations);
	}
	
	// pairing
	for (EndpointList::iterator dit = mDestinations.begin(); dit != mDestinations.end(); ++dit) {
		Endpoint *ep = *dit;
		MIDIEntityRef destEntity = ep->Entity();
		if (destEntity != NULL) {
			for (EndpointList::iterator eit = mSources.begin(); eit != mSources.end(); ++eit) {
				Endpoint *ep2 = *eit;
				MIDIEntityRef srcEntity = ep2->Entity();
				if (srcEntity == destEntity && ep2->DriverOwned() == ep->DriverOwned()) {
					ep2->SetPairMate(ep);
					ep->SetPairMate(ep2);
				}
			}
		}
	}
}

void	CAMIDIEndpoints::AddEndpoints(MIDIEndpointRef endpoint, EndpointList &eplist)
{
	Endpoint *ep, *prev;
	OSStatus err;
	CFStringRef str;
	
	// Add the driver-owned endpoint
	ep = new Endpoint(endpoint, EndpointName(endpoint, false), NULL);
	eplist.push_back(ep);
	prev = ep;

	// Does the endpoint have connections?
	CFDataRef connections = NULL;
	int nConnected = 0;
	MIDIObjectGetDataProperty(endpoint, kMIDIPropertyConnectionUniqueID, &connections);
	if (connections != NULL) {
		// It has connections, follow them
		nConnected = CFDataGetLength(connections) / sizeof(MIDIUniqueID);
		if (nConnected) {
			const SInt32 *pid = reinterpret_cast<const SInt32 *>(CFDataGetBytePtr(connections));
			for (int i = 0; i < nConnected; ++i, ++pid) {
				MIDIUniqueID id = EndianS32_BtoN(*pid);
				MIDIObjectRef connObject;
				MIDIObjectType connObjectType;
				err = MIDIObjectFindByUniqueID(id, &connObject, &connObjectType);
				if (err == noErr) {
					if (connObjectType == kMIDIObjectType_ExternalSource 
					|| connObjectType == kMIDIObjectType_ExternalDestination) {
						// Connected to an external device's endpoint (10.3 and later).
						str = EndpointName(static_cast<MIDIEndpointRef>(connObject), true);
					} else {
						// Connected to an external device (10.2) (or something else, catch-all)
						str = NULL;
						MIDIObjectGetStringProperty(connObject, kMIDIPropertyName, &str);
					}
					if (str != NULL) {
						ep = new Endpoint(endpoint, str, connObject);
						eplist.push_back(ep);
						prev->SetNext(ep);
						prev = ep;
					}
				}
			}
		}
	}
}


class CompareEndpointsByName {
public:
	bool operator () (CAMIDIEndpoints::EndpointInfo *a, CAMIDIEndpoints::EndpointInfo *b)
	{
		CFStringRef namea = a->mDisplayName;
		return CFStringCompareWithOptions(namea, b->mDisplayName, CFRangeMake(0, CFStringGetLength(namea)), 0) < 0;
	}
};

CAMIDIEndpoints::EndpointInfoList *	CAMIDIEndpoints::GetEndpoints(EMode mode, UInt32 opts)
{
	EndpointList &srcList = (mode == kSources) ? mSources : mDestinations;
	EndpointInfoList *list = new EndpointInfoList;
	EndpointInfo info;
	
	for (EndpointList::iterator it = srcList.begin(); it != srcList.end(); ++it) {
		Endpoint *ept = *it;
		
		if (ept->DriverOwned()) {
			// driver-owned endpoint
			if (ept->Next() == NULL) {
				// nothing connected externally
				if ((opts & kOptIncludeUnconnectedExternalPorts) || ept->EmbeddedOrVirtual()) {
					if (ept->GetEndpointInfo(mode, info))
						list->push_back(new EndpointInfo(info));
				}
			} else if (opts & kOptCombineByPort) {
				// add one item for all connected items
				if (ept->GetEndpointInfo(mode, info))
					list->push_back(new EndpointInfo(info));
			}
			// else it has external connections, which we'll pick up separately
		} else {
			// external endpoint
			if (!(opts & kOptCombineByPort)) {
				if (ept->GetEndpointInfo(mode, info))
					list->push_back(new EndpointInfo(info));
			}
		}
	}
	
	if (opts & kOptSortByName) {
		std::sort(list->begin(), list->end(), CompareEndpointsByName() );
	}
	return list;
}

bool	CAMIDIEndpoints::FindEndpoint(EMode mode, EndpointInfo &info, UInt32 opts)
{
	EndpointList &srcList = (mode == kSources) ? mSources : mDestinations;
	
	for (EndpointList::iterator it = srcList.begin(); it != srcList.end(); ++it) {
		Endpoint *ept = *it;
		
		if (ept->UniqueID() == info.mUniqueID) {
			if (ept->GetEndpointInfo(mode, info))
				return true;
			break;
		}
	}
	info.mSourceEndpoint = NULL;
	info.mDestinationEndpoint = NULL;
	return false;
}


/*
	Resolving of persistent endpoint references
	
	Cases to handle:
	- Was referring to an external endpoint
		- missing
	- Was referring to a driver endpoint
		- missing
		- now has external endpoint(s) connected
*/
