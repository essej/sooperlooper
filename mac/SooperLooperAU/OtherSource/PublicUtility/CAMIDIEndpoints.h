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
#ifndef __CAMIDIEndpoints_h__
#define __CAMIDIEndpoints_h__

#include <CoreMIDI/CoreMIDI.h>
#include <vector>

// ____________________________________________________________________________
// This class manages persistent references to MIDI endpoints,
// and provides user-visible names for them.
class CAMIDIEndpoints {
public:
	// ____________________________________________________________________________
	class EndpointInfo {
	public:
		// when asking for pairs, both source and destination are valid,
		// otherwise only the appropriate MIDIEndpointRef is valid.
		MIDIEndpointRef		mSourceEndpoint;		// driver-owned, can be used for I/O
		MIDIEndpointRef		mDestinationEndpoint;	// driver-owned, can be used for I/O
		CFStringRef			mDisplayName;
		MIDIUniqueID		mUniqueID;			// may refer to a driver or external endpoint.
												// use this as your permanent reference to it.
		
		EndpointInfo(MIDIUniqueID uid=kMIDIInvalidUniqueID) :
			mSourceEndpoint(NULL),
			mDestinationEndpoint(NULL),
			mDisplayName(NULL),
			mUniqueID(uid) { }
		
		EndpointInfo(const EndpointInfo &info) : mDisplayName(NULL) { *this = info; }

		EndpointInfo & operator = (const EndpointInfo &a) {
			ReleaseName();
			this->mSourceEndpoint = a.mSourceEndpoint;
			this->mDestinationEndpoint = a.mDestinationEndpoint;
			this->mDisplayName = a.mDisplayName;
			if (this->mDisplayName) CFRetain(this->mDisplayName);
			this->mUniqueID = a.mUniqueID;
			
			return *this;
		}
		
		~EndpointInfo() { ReleaseName(); }
	private:
		void	ReleaseName() { if (mDisplayName) { CFRelease(mDisplayName); mDisplayName = NULL; } }
	};
	
	struct EndpointInfoList : public std::vector<EndpointInfo *> {
		~EndpointInfoList() { for (iterator it = begin(); it != end(); ++it) delete *it; }
	};

	// ____________________________________________________________________________
	// options
	enum {
		kOptSortByName						= 1,
		kOptIncludeUnconnectedExternalPorts = 2,
		kOptCombineByPort					= 4		// when multiple devices connected to a port,
													// present only a single endpoint
	};
	
	// ____________________________________________________________________________
	// methods
					CAMIDIEndpoints();
	virtual			~CAMIDIEndpoints();
	
	void			UpdateFromCurrentState();
						// call this when you get a notification that the state has changed
	
	// functions that return lists: do not dispose the pointers contained in the lists!
	EndpointInfoList *	GetSources(UInt32 opts = 0) {
							return GetEndpoints(kSources, opts);
						}
	EndpointInfoList *	GetDestinations(UInt32 opts = 0) {
							return GetEndpoints(kDestinations, opts);
						}
	EndpointInfoList *	GetEndpointPairs(UInt32 opts = 0) {
							return GetEndpoints(kPairs, opts);
						}
		// A pair of endpoints: based on being part of the same entity.
		// Considering the possibility of multi-entity USB devices, it's more robust to
		// consider entities/endpoint pairs as things to communicate bidirectionally with,
		// as opposed to devices. But in the UI, you might still treat them as "devices"
		// since most devices have only a single entity.
	
	// look up an endpoint by uniqueID in a EndpointInfo struct, return
	// an updated EndpointInfo. (### options are not currently relevant ###)
	bool				FindSource(EndpointInfo &info, UInt32 opts = 0) {
							return FindEndpoint(kSources, info, opts);
						}
	bool				FindDestination(EndpointInfo &info, UInt32 opts = 0) {
							return FindEndpoint(kDestinations, info, opts);
						}
	bool				FindPair(EndpointInfo &info, UInt32 opts = 0) {
							return FindEndpoint(kPairs, info, opts);
						}
	
protected:
	// ____________________________________________________________________________
	enum EMode { kSources, kDestinations, kPairs };
	
	// ____________________________________________________________________________
	// Corresponds to a CoreMIDI source or destination endpoint, except that
	// in the case of multiple external devices connected to the same driver port,
	// there is a separate instance for each.
	class Endpoint {
	public:
		Endpoint(MIDIEndpointRef ep, CFStringRef name, MIDIObjectRef connectedObj);
		~Endpoint();
		
		MIDIUniqueID		UniqueID() const { return mUniqueID; }
		CFStringRef			Name() const { return mName; } // do not release returned reference
		
		MIDIEndpointRef		IOEndpoint() const { return mIOEndpoint; }
		bool				DriverOwned() const { return mConnectedObj == NULL; }
		bool				EmbeddedOrVirtual() const { return mEmbeddedOrVirtual; }
		MIDIEntityRef		Entity() const { return mEntity; }
		
		void				SetNext(Endpoint *next) { mNext = next; }
		Endpoint *			Next() const { return mNext; }
		void				SetPairMate(Endpoint *e) { mPairMate = e; }
		Endpoint *			PairMate() const { return mPairMate; }
		
		bool				GetEndpointInfo(EMode mode, EndpointInfo &outInfo);
		
	protected:
		MIDIUniqueID		mUniqueID;
		MIDIEndpointRef		mIOEndpoint;		// NULL if unresolved
		CFStringRef			mName;

		MIDIEntityRef		mEntity;
		bool				mEmbeddedOrVirtual;
		
		MIDIObjectRef		mConnectedObj;
		Endpoint *			mNext;				// linked list: driver owned -> connected -> connected ...
		Endpoint *			mPairMate;
	};
	
	typedef std::vector<Endpoint *>		EndpointList;
	
	// ____________________________________________________________________________
	// members
protected:
	void				Clear();
	void				AddEndpoints(MIDIEndpointRef endpoint, EndpointList &eplist);
	EndpointInfoList *	GetEndpoints(EMode mode, UInt32 opts);
	bool				FindEndpoint(EMode mode, EndpointInfo &info, UInt32 opts);

	EndpointList		mSources, mDestinations;
};

#endif // __CAMIDIEndpoints_h__
