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
#include "CAAUConnections.h"

OSStatus	CAAUConnections::Find (const AUGraph &inGraph, const CAAudioUnit &inUnit, AudioUnitScope inScope)
{
	if (!mConns.empty()) mConns.clear();

	AudioUnit theUnit;
	OSStatus result = AUGraphGetNodeInfo (inGraph, inUnit.GetAUNode(), NULL, NULL, NULL, &theUnit);
	if ((theUnit != inUnit.AU()) || result) return (result ? result : -1);
		
	UInt32 numConns = 0;
	if ((result = AUGraphCountNodeConnections (inGraph, inUnit.GetAUNode(), &numConns))) return result;
	
	if (numConns) {
		AudioUnitNodeConnection s_conns[4];
		AudioUnitNodeConnection *conns = (numConns > 4 ? new AudioUnitNodeConnection[numConns] : s_conns);
	
		require_noerr (result = AUGraphGetNodeConnections (inGraph, inUnit.GetAUNode(), conns, &numConns), home);
		
		for (unsigned int i = 0; i < numConns; ++i) {
			if (inScope == kAudioUnitScope_Output && conns[i].sourceNode == inUnit.GetAUNode()) {
				AudioUnit otherAU;
				require_noerr (result = AUGraphGetNodeInfo (inGraph, conns[i].destNode, NULL, NULL, NULL, &otherAU), home);
				mConns.push_back (AUConn (conns[i].sourceOutputNumber, otherAU, conns[i].destNode, conns[i].destInputNumber));
			}
			if (inScope == kAudioUnitScope_Input && conns[i].destNode == inUnit.GetAUNode()) {
				AudioUnit otherAU;
				require_noerr (result = AUGraphGetNodeInfo (inGraph, conns[i].sourceNode, NULL, NULL, NULL, &otherAU), home);
				mConns.push_back (AUConn (conns[i].destInputNumber, otherAU, conns[i].sourceNode, conns[i].sourceOutputNumber));
			}
		}
	home:
		if (numConns > 4) delete [] conns;
	}
	return result;
}
	
bool		CAAUConnections::Connection (UInt32				inIndex, 
									AudioUnitElement		&outMyEl, 
									CAAudioUnit				&outOtherAU, 
									AudioUnitElement		&outOtherAUEl)
{
	if (inIndex < Size()) {
		outMyEl = mConns[inIndex].mMyEl;
		outOtherAU = mConns[inIndex].mOtherAU;
		outOtherAUEl = mConns[inIndex].mOtherEl;
		return true;
	}
	return false;
}

bool		CAAUConnections::IsMember (AudioUnitElement inMyEl) const
{
	for (AUConns::const_iterator iter = mConns.begin(); iter < mConns.end(); ++iter) {
		const AUConn &conn = (*iter);
		if (inMyEl == conn.mMyEl);
			return true;
	}
	return false;
}

void		CAAUConnections::Print (FILE* file) const
{
	for (AUConns::const_iterator iter = mConns.begin(); iter < mConns.end(); ++iter) {
		const AUConn &conn = (*iter);
		fprintf (file, "El: %d, To:", (int)conn.mMyEl);
		conn.mOtherAU.Print (file);
		fprintf (file, ", Other El: %d\n", (int)conn.mOtherEl); 
	}
}
