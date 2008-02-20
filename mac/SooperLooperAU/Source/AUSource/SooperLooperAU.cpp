/*
*	File:		SooperLooperAU.cpp
*	
*	Version:	1.0
* 
*	Created:	7/2/05
*	
*	Copyright:  Copyright © 2005 Jesse Chappell, All Rights Reserved
* 
*/
#include "SooperLooperAU.h"
#include "SLproperties.h"


#include "AudioToolbox/AudioUnitUtilities.h"

#include "engine.hpp"
#include "midi_bridge.hpp"
#include "control_osc.hpp"
#include "command_map.hpp"
#include "plugin.hpp"

#include <pbd/transmitter.h>
using namespace PBD;

using namespace SooperLooper;

#include <iostream>
#include <sstream>
using namespace std;

#include <unistd.h>

// needed so linking to libpbd will work
Transmitter  error (Transmitter::Error);
Transmitter  info (Transmitter::Info);
Transmitter  fatal (Transmitter::Fatal);
Transmitter  warning (Transmitter::Warning);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

COMPONENT_ENTRY(SooperLooperAU)


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	SooperLooperAU::SooperLooperAU
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
SooperLooperAU::SooperLooperAU(AudioUnit component)
	: AUMIDIEffectBase(component, false), _engine(0), _midi_bridge(0)
{
	CreateElements();
	//Globals()->UseIndexedParameters(kNumberOfParameters);
	//SetParameter(kParam_LoopCount, kDefaultValue_LoopCount );
    //SetParameter(kParam_LoopSecs, 40.0);
    
	SetBusCount (kAudioUnitScope_Input, SL_MAXLOOPS);
	SetBusCount (kAudioUnitScope_Output, SL_MAXLOOPS);		
	
#if AU_DEBUG_DISPATCHER
	mDebugDispatcher = new AUDebugDispatcher (this);
#endif
	
	// SL stuff
	_in_channel_id = 0;
	_out_channel_id = 0;
	_engine_thread = 0;
	_last_framepos = 0;
	_last_rendered_frames = 0;
	_stay_on_top = 0;
	
	//sl_init();
	
	_engine = new SooperLooper::Engine();
	_midi_bridge = new SooperLooper::MidiBridge("AUmidi");
	
	_engine->set_midi_bridge (_midi_bridge);
	_engine->set_ignore_quit (true);
	_engine->ParamChanged.connect(slot(*this, &SooperLooperAU::parameter_changed));
	_engine->LoopAdded.connect(slot(*this, &SooperLooperAU::loop_added));
	_engine->LoopRemoved.connect(slot(*this, &SooperLooperAU::loop_removed));	
	
	memset(_currStates, 0, sizeof(int) * SL_MAXLOOPS);
}

SooperLooperAU::~SooperLooperAU () 
{ 
#if AU_DEBUG_DISPATCHER
	delete mDebugDispatcher; 
#endif
	
	//cerr << "SOOP DESTRCUT" << endl;
	
	if (_alive) {
		_alive = false;
		_engine->quit(true);
	
		void * status;
		if (_engine_thread) {
				pthread_join (_engine_thread, &status);
		}
	}
	
	delete _engine;
	delete _midi_bridge;
	
	//sl_fini();
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	SooperLooperAU::GetParameterValueStrings
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult		SooperLooperAU::GetParameterValueStrings(AudioUnitScope		inScope,
                                                                AudioUnitParameterID	inParameterID,
                                                                CFArrayRef *		outStrings)
{
        
    return kAudioUnitErr_InvalidProperty;
}


void SooperLooperAU::setup_params()
{
	// query the engine's command map for all the params
	// keep global ones separate
	
	// our convention here will be
	// global controls are left alone
	// (i+1)*10000 is added to all control #s where i is the loop index
	// (i+1)*10000 + 500 is added to all command #s where i is the loop index
	
	// for SELECTED loop, the offset will be 1000
	
	int loopcount = (int) _engine->loop_count();

	list<string> ctrls;
	SooperLooper::CommandMap & cmdmap = SooperLooper::CommandMap::instance();
	cmdmap.get_controls(ctrls);
	
	for (list<string>::iterator iter = ctrls.begin(); iter != ctrls.end(); ++iter) {
		Event::control_t ctrl = cmdmap.to_control_t(*iter);
		if (cmdmap.is_global_control(*iter)) {
			Globals()->SetParameter(ctrl, _engine->get_control_value(ctrl, -2));
		}
		else {

			Globals()->SetParameter(ctrl + (1000), _engine->get_control_value(ctrl, -3)); // SEL
					
			for (int i=0; i < loopcount; ++i) {
				Globals()->SetParameter(ctrl + ((i+1)*10000), _engine->get_control_value(ctrl, i));
			}
			
		}
	}
	
	list<string> cmds;	
    cmdmap.get_commands(cmds);
	
	for (list<string>::iterator iter = cmds.begin(); iter != cmds.end(); ++iter) {
		Event::command_t cmd = cmdmap.to_command_t(*iter);

		Globals()->SetParameter(cmd + (1000) + 500, 0); // selected
			
		for (int i=0; i < loopcount; ++i) {
			Globals()->SetParameter(cmd + ((i+1)*10000) + 500, 0);
		}
		
	}
	
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	SooperLooperAU::GetParameterInfo
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult		SooperLooperAU::GetParameterInfo(AudioUnitScope		inScope,
                                                        AudioUnitParameterID	inParameterID,
                                                        AudioUnitParameterInfo	&outParameterInfo )
{
	ComponentResult result = noErr;
	int instance;
	int ctrl;
	string ctrlstr;
	int flags;
	char prebuf[12];
	char postbuf[12];	

	SooperLooper::CommandMap::ControlInfo ctrlinfo;
	SooperLooper::CommandMap & cmdmap = SooperLooper::CommandMap::instance();

	outParameterInfo.flags = 	kAudioUnitParameterFlag_IsWritable
						|		kAudioUnitParameterFlag_IsReadable;
    
    if (inScope == kAudioUnitScope_Global) {
        switch(inParameterID)
        {
			/*
            case kParam_LoopCount:
                AUBase::FillInParameterName (outParameterInfo, kParameterLoopCountName, false);
                outParameterInfo.unit = kAudioUnitParameterUnit_Indexed;
                outParameterInfo.minValue = 1;
                outParameterInfo.maxValue = 16;
                outParameterInfo.defaultValue = kDefaultValue_LoopCount;
                break;
            case kParam_LoopSecs:
                AUBase::FillInParameterName (outParameterInfo, kParameterLoopSecsName, false);
                outParameterInfo.unit = kAudioUnitParameterUnit_Seconds;
                outParameterInfo.minValue = 4.0;
                outParameterInfo.maxValue = 600.0;
                outParameterInfo.defaultValue = 40.0;
                break;
				*/
			case kParam_OSCPort:
                AUBase::FillInParameterName (outParameterInfo, kParameterOSCPortName, false);
                outParameterInfo.unit = kAudioUnitParameterUnit_Indexed;                
				outParameterInfo.flags = kAudioUnitParameterFlag_IsReadable;
				outParameterInfo.minValue = 0;
                outParameterInfo.maxValue = 3000000;
				outParameterInfo.defaultValue = 0;
                break;
				
			default:

				// anything else is either a global control, or a loop instance control
					
		
				
				if (inParameterID >= 10000) {
					// instance
					instance = (inParameterID / 10000) - 1;
					ctrl = inParameterID % 10000;
					snprintf(prebuf, sizeof(prebuf), "%d-", instance+1);
					strncpy(postbuf, "", sizeof(postbuf));					
				}
				else if (inParameterID >= 1000) {
					// selected
					instance = -3;
					ctrl = inParameterID - 1000;
					strncpy(prebuf, "", sizeof(prebuf));
					snprintf(postbuf, sizeof(postbuf), " (sel)");
				}
				else {
					// global
					instance = -2;
					ctrl = inParameterID;
					strncpy(prebuf, "", sizeof(prebuf));
					strncpy(postbuf, "", sizeof(postbuf));					
				}
				
				if (ctrl >= 500) {
					// command acting as a control
					ctrl -= 500;
					ctrlstr = cmdmap.to_command_str((SooperLooper::Event::command_t)ctrl);
					flags = kAudioUnitParameterFlag_IsReadable|kAudioUnitParameterFlag_IsWritable;
					ctrlinfo.unit = SooperLooper::CommandMap::UnitBoolean;
					ctrlinfo.minValue = 0;
					ctrlinfo.maxValue = 1;
					ctrlinfo.defaultValue = 0;
					ctrlstr = prebuf + ctrlstr + postbuf + " [cmd]";
				}
				else {
					// normal control
					ctrlstr = cmdmap.to_control_str((SooperLooper::Event::control_t)ctrl);
					flags = cmdmap.is_output_control(ctrlstr) ? kAudioUnitParameterFlag_IsReadable : kAudioUnitParameterFlag_IsReadable|kAudioUnitParameterFlag_IsWritable;

					if (cmdmap.get_control_info(ctrlstr, ctrlinfo)) {
						//cerr << "control info: " << ctrlstr << "  minval: " << ctrlinfo.minValue << "  maxval: " << ctrlinfo.maxValue << endl;
					}
					ctrlstr = prebuf + ctrlstr + postbuf;
				}
				
				
				
				CFStringRef statstr = CFStringCreateWithCString(0, ctrlstr.c_str(), CFStringGetSystemEncoding());
				AUBase::FillInParameterName (outParameterInfo, statstr, false);
				CFRelease(statstr);
				
				switch (ctrlinfo.unit)
				{
					case SooperLooper::CommandMap::UnitSeconds:
						outParameterInfo.unit = kAudioUnitParameterUnit_Seconds;
						break;
					case SooperLooper::CommandMap::UnitIndexed:
						outParameterInfo.unit = kAudioUnitParameterUnit_Indexed;
						break;
					case SooperLooper::CommandMap::UnitGain:
						outParameterInfo.unit = kAudioUnitParameterUnit_MixerFaderCurve1;
						break;
					case SooperLooper::CommandMap::UnitRatio:
						outParameterInfo.unit = kAudioUnitParameterUnit_Ratio;
						break;
					case SooperLooper::CommandMap::UnitSemitones:
						outParameterInfo.unit = kAudioUnitParameterUnit_RelativeSemiTones;
						break;
					case SooperLooper::CommandMap::UnitBoolean:
						outParameterInfo.unit = kAudioUnitParameterUnit_Boolean;
						break;
					case SooperLooper::CommandMap::UnitSamples:
						outParameterInfo.unit = kAudioUnitParameterUnit_SampleFrames;
						break;
					case SooperLooper::CommandMap::UnitInteger:
						outParameterInfo.unit = kAudioUnitParameterUnit_Indexed;
						break;
					case SooperLooper::CommandMap::UnitTempo:
						outParameterInfo.unit = kAudioUnitParameterUnit_BPM;
						break;
					default:
						outParameterInfo.unit = kAudioUnitParameterUnit_Generic;
				}
				
				outParameterInfo.flags = flags;
				outParameterInfo.minValue = ctrlinfo.minValue;
				outParameterInfo.maxValue = ctrlinfo.maxValue;
				outParameterInfo.defaultValue = ctrlinfo.defaultValue;
				
                //result = kAudioUnitErr_InvalidParameter;
                break;
            }
	} else {
        result = kAudioUnitErr_InvalidParameter;
    }
    


	return result;
}

/*! @method Initialize */
ComponentResult		SooperLooperAU::Initialize()
{
	// get our current numChannels for input and output
	SInt16 auNumInputs = (SInt16) GetInput(0)->GetStreamFormat().mChannelsPerFrame;
	SInt16 auNumOutputs = (SInt16) GetOutput(0)->GetStreamFormat().mChannelsPerFrame;
	
	if ((auNumOutputs != auNumInputs) || (auNumOutputs == 0))
	{
		return kAudioUnitErr_FormatNotSupported;
	}

	// start thread for main loop
	_alive = true;
	pthread_create (&_engine_thread, NULL, &SooperLooperAU::_engine_mainloop, this);
	
	
	_chancnt = auNumInputs;
		
	//cerr << "Initializing engine with " << auNumInputs << " inputs and " << auNumOutputs << " outputs" << endl;
	
	_engine->initialize (this, auNumInputs, 10051);

	Globals()->SetParameter(kParam_OSCPort, _engine->get_control_osc()->get_server_port());
	
	if (_pending_restore.empty()) {
		int numloops = 1;
		float loopsecs = 40.0f;
	    //cerr << "NOT PENDING restore" << endl;
		//numloops = GetParameter(kParam_LoopCount);
		//loopsecs = GetParameter(kParam_LoopSecs);
		// allocate separate buses for each loop
		
		for (int n=0; n < numloops; ++n) {
			_engine->add_loop (_chancnt, loopsecs, true);
		}		
	}
	else {
			// do a state restore
		//cerr << "doing state restore with: " << _pending_restore << endl;
		_engine->load_session ("", &_pending_restore);
	}
	
	for (size_t n=0; n < SL_MAXLOOPS; ++n) {
		_in_buflist[n] = 0;	
		_out_buflist[n] = 0;			
	}
	
	setup_params();
	
	return noErr;
}

/*! @method Cleanup */
void				SooperLooperAU::Cleanup()
{
	cerr << "SLAU cleanup called" << endl;
	// totally kill the engine
	if (_alive) {
		_alive = false;	
		_engine->quit(true);
	
		void * status;
		if (_engine_thread) {
			pthread_join (_engine_thread, &status);
		}
	
		_engine->cleanup();
	}
	
	_pending_restore = "";
	_in_channel_id = 0;
	_out_channel_id = 0;
}


/*! @method Reset */
ComponentResult		SooperLooperAU::Reset(AudioUnitScope 				inScope,
										  AudioUnitElement 			inElement)
{
	
	return noErr;
}

/*
ComponentResult		SooperLooperAU::RenderBus(				AudioUnitRenderActionFlags &	ioActionFlags,
													const AudioTimeStamp &			inTimeStamp,
													UInt32							inBusNumber,
													UInt32							inNumberFrames)
{
	if (NeedsToRender(inTimeStamp.mSampleTime)) {
		_mainbus_called = true;
		return Render(ioActionFlags, inTimeStamp, inNumberFrames);
	}
	else {
		_mainbus_called = false;
		cerr << "renderbus: " << inBusNumber << endl;
		return Render(ioActionFlags, inTimeStamp, inNumberFrames);		
	}
	return noErr;	// was presumably already rendered via another bus
}
*/

ComponentResult 	SooperLooperAU::Render(	AudioUnitRenderActionFlags &ioActionFlags,
											const AudioTimeStamp &		inTimeStamp,
											UInt32						nFrames)
{
		// save the timestamp
	_curr_stamp = inTimeStamp;
	
	if (_last_rendered_frames != nFrames) {
		_last_rendered_frames = nFrames;
		ConnectionsChanged(); // emit
	}
	
	// if we're bypassed we need to passthru the other buses
	if (ShouldBypassEffect())
	{
		
		for (size_t n=0; n < SL_MAXLOOPS; ++n) 
		{
			try {
				AUOutputElement *theOutput = GetOutput(n+1);	// throws if error
				AUInputElement *theInput = GetInput(n+1);
				if (theOutput && theInput) {
					OSStatus result = theInput->PullInput(ioActionFlags, _curr_stamp, 0 /* element */, nFrames);
					
					if (result == noErr) {
						if(ProcessesInPlace() )
						{
							theOutput->SetBufferList(theInput->GetBufferList() );
						}
						else {
							theOutput->PrepareBuffer(nFrames);
						}
						
						// leave silence bit alone
						if(!ProcessesInPlace() )
						{
							theInput->CopyBufferContentsTo (theOutput->GetBufferList());
						}
					}
				}
			} catch (...) {
				//cerr << "got exception: " << endl;	
			}
		}
		
	}
	
	return AUMIDIEffectBase::Render (ioActionFlags, inTimeStamp, nFrames);
}


OSStatus			SooperLooperAU::ProcessBufferLists(
											   AudioUnitRenderActionFlags &	ioActionFlags,
											   const AudioBufferList &			inBuffer,
											   AudioBufferList &				outBuffer,
											   UInt32							inFramesToProcess )
{	
	// deinterleaved		
	//const AudioBuffer *srcBuffer = inBuffer.mBuffers;
	//AudioBuffer *destBuffer = outBuffer.mBuffers;
	
		
	if (inBuffer.mNumberBuffers == 1) {
		if (_chancnt > 1) {
			//cerr << "interleaved " << _chancnt << endl;
			ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
			return kAudioUnitErr_InvalidParameter;
		}
	}
	
	_in_buflist[0] = (AudioBufferList *) &inBuffer;
	_out_buflist[0] = &outBuffer;

	// this is called with the main bus (0) buffers
	// we really should subclass Render here, but instead
	// we'll just get the other busses data too

	for (size_t n=0;  n < SL_MAXLOOPS; ++n) 
	{
		try {
			AUOutputElement *theOutput = GetOutput(n+1);	// throws if error
			AUInputElement *theInput = GetInput(n+1);
			if (theOutput && theInput) {
				OSStatus result = theInput->PullInput(ioActionFlags, _curr_stamp, 0 /* element */, inFramesToProcess);
			
				if (result == noErr) {
					if(ProcessesInPlace() )
					{
						theOutput->SetBufferList(theInput->GetBufferList() );
					}
					else {
						theOutput->PrepareBuffer(inFramesToProcess);
						if (n > _engine->loop_count()) {
							// zero the buffer
							AUBufferList::ZeroBuffer(theOutput->GetBufferList());
						}
					}

					_in_buflist[n+1] = &theInput->GetBufferList();
					_out_buflist[n+1] = &theOutput->GetBufferList();
					//cerr << "got bus input: " << n+1 <<  "  " << _out_buflist[n+1] << endl;
				}
			}
		} catch (...) {
				//cerr << "got exception: " << endl;	
		}
	}
		

	// actually do the work

	_engine->process (inFramesToProcess);
	
	_in_buflist[0] = 0;
	_out_buflist[0] = 0;
	
	for (size_t n=0; n < _engine->loop_count() && n < SL_MAXLOOPS; ++n) {
		_in_buflist[n+1] = 0;	
		_out_buflist[n+1] = 0;			
	}
	
	return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AUMIDIBase::HandleMidiEvent
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus 	SooperLooperAU::HandleMidiEvent(UInt8 inStatus, UInt8 inChannel, UInt8 inData1, UInt8 inData2, long inStartFrame)
{
	if (!IsInitialized()) return kAudioUnitErr_Uninitialized;
	
	UInt8 chcmd = inStatus | inChannel;
	UInt8 data1 = inData1;
	UInt8 data2 = inData2;
	
	_midi_bridge->inject_midi (chcmd, data1, data2, inStartFrame);
	
	return noErr;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	SooperLooperAU::GetPropertyInfo
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult		SooperLooperAU::GetPropertyInfo (AudioUnitPropertyID	inID,
                                                        AudioUnitScope		inScope,
                                                        AudioUnitElement	inElement,
                                                        UInt32 &		outDataSize,
                                                        Boolean &		outWritable)
{
	if (inID == kSLguiAppPathProperty) {
		outDataSize = _guiapp_path.size() + 1;
		outWritable = true;
		return noErr;
	}
	else if (inID == kSLguiStayOnTopProperty) {
		outDataSize = sizeof(short);
		outWritable = true;
		return noErr;
	}

	
	return AUMIDIEffectBase::GetPropertyInfo (inID, inScope, inElement, outDataSize, outWritable);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	SooperLooperAU::GetProperty
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult		SooperLooperAU::GetProperty(	AudioUnitPropertyID inID,
                                                        AudioUnitScope 		inScope,
                                                        AudioUnitElement 	inElement,
                                                        void *			outData )
{
		
	if (inID == kSLguiAppPathProperty) {
		strcpy ((char *)outData, _guiapp_path.c_str());
		return noErr;
	}
	else if (inID == kSLguiStayOnTopProperty) {
		*((short *)outData) = _stay_on_top;
		return noErr;
	}
	return AUMIDIEffectBase::GetProperty (inID, inScope, inElement, outData);
}

/*! @method SetProperty */
ComponentResult		SooperLooperAU::SetProperty(AudioUnitPropertyID 		inID,
										AudioUnitScope 				inScope,
										AudioUnitElement 			inElement,
										const void *				inData,
										UInt32 						inDataSize)
{
	if (inID == kSLguiAppPathProperty) {
		_guiapp_path = (char *) inData;
		//cerr << "set app path in AU to: " << _guiapp_path << endl;
		return noErr;
	}
	else if (inID == kSLguiStayOnTopProperty) {
		_stay_on_top = *((short *) inData);
		//cerr << "set app path in AU to: " << _guiapp_path << endl;
		return noErr;
	}
	return AUMIDIEffectBase::SetProperty (inID, inScope, inElement, inData, inDataSize);
}


//_____________________________________________________________________________
//
ComponentResult 	SooperLooperAU::GetParameter(	AudioUnitParameterID			inID,
													AudioUnitScope 					inScope,
													AudioUnitElement 				inElement,
													Float32 &						outValue)
{
	if (inScope == kAudioUnitScope_Group) {
		return GetGroupParameter (inID, inElement, outValue);
	}
	
	if (inID ==  kParam_OSCPort) {
	    //cerr << "outvalue for port is: " << Globals()->GetParameter(inID) << endl;
		outValue = Globals()->GetParameter(inID);
		return noErr;
	}
	
	int instance;
	int ctrl;
	
	//AUElement *elem = SafeGetElement(inScope, inElement);
	//outValue = elem->GetParameter(inID);

	if (inID >= 1000) {
		// loop specific
		
		if (inID >= 10000) {
			// normal instance
			instance = (inID / 10000) - 1;
			ctrl = inID % 10000;
		}
		else {
			instance = -3; // selected
			ctrl = inID - 1000;
		}
		
		if (ctrl >= 500) {
			// command acting as control, treat specially
			// by converting the current state into the value here
			
			SooperLooper::Event::command_t cmd = (SooperLooper::Event::command_t) (ctrl - 500);
			int state = (int) _engine->get_control_value(SooperLooper::Event::State, instance);
			switch(cmd) {
				case SooperLooper::Event::RECORD:
					outValue = (state == LooperStateRecording);
					break;
				case SooperLooper::Event::OVERDUB:
					outValue = (state == LooperStateOverdubbing);
					break;
				case SooperLooper::Event::MULTIPLY:
					outValue = (state == LooperStateMultiplying);
					break;	
				case SooperLooper::Event::INSERT:
					outValue = (state == LooperStateInserting);
					break;	
				case SooperLooper::Event::SUBSTITUTE:
					outValue = (state == LooperStateSubstitute);
					break;	
				case SooperLooper::Event::REPLACE:
					outValue = (state == LooperStateReplacing);
					break;	
				case SooperLooper::Event::MUTE:
					outValue = (state == LooperStateMuted);
					break;	
				case SooperLooper::Event::DELAY:
					outValue = (state == LooperStateDelay);
					break;	
				case SooperLooper::Event::ONESHOT:
					outValue = (state == LooperStateOneShot);
					break;				
				case SooperLooper::Event::REVERSE:
					outValue = (_engine->get_control_value(SooperLooper::Event::TrueRate, instance) < 0);
					break;		
				default:
					outValue = 0.0f;
			}
			
			return noErr;
		}
	}
	else {
		// global
		instance = -2;
		ctrl = inID;
	}
	//cerr << "getparam: " << inID << endl;
	outValue = _engine->get_control_value((SooperLooper::Event::control_t) ctrl, instance);
	
	return noErr;
}

											
//_____________________________________________________________________________
//
ComponentResult  SooperLooperAU::SetParameter(			AudioUnitParameterID			inID,
													AudioUnitScope 					inScope,
													AudioUnitElement 				inElement,
													Float32							inValue,
													UInt32							inBufferOffsetInFrames)
{
	if (inScope == kAudioUnitScope_Group) {
		return SetGroupParameter (inID, inElement, inValue, inBufferOffsetInFrames);
	}
	
	AUElement *elem = SafeGetElement(inScope, inElement);
	elem->SetParameter(inID, inValue);
	
	int instance;
	int ctrl;

	if (inID >= 1000) {
		// loop specific
		if (inID >= 10000) {
			instance = (inID / 10000) - 1;
			ctrl = inID % 10000;
		}
		else {
			// selected
			instance = -3;
			ctrl = inID - 1000;
		}
		
		if (ctrl >= 500) {
			// command acting as control, treat specially
			// current logic is that any change will trigger a hit command for now
			SooperLooper::Event::command_t cmd = (SooperLooper::Event::command_t) (ctrl - 500);
			
			_engine->push_command_event(SooperLooper::Event::type_cmd_hit, cmd, instance);
			
			return noErr;
		}
	}
	else {
		// global
		instance = -2;
		ctrl = inID;
	}

	//cerr << "set param: " << inID << endl;
	_engine->push_control_event (SooperLooper::Event::type_control_change, (SooperLooper::Event::control_t)ctrl, inValue, instance);
	
	return noErr;
}

void SooperLooperAU::parameter_changed(int ctrl_id, int instance)
{
	int paramid;
	int selected_loop = (int) _engine->get_control_value(SooperLooper::Event::SelectedLoopNum, -2);
	bool selected = (selected_loop == instance || selected_loop == -1 || _engine->loop_count()==1);
	
	if (instance == -3) {
		// selected
		paramid = ctrl_id + 1000;
	}
	else if (instance < 0) {
		// global
		paramid = ctrl_id;
	} else {
		// instance
		paramid = (instance+1)*10000 + ctrl_id;
	}
   

	AudioUnitParameter changedUnit;	
	changedUnit.mAudioUnit = GetComponentInstance();
	//changedUnit.mParameterID = kAUParameterListener_AnyParameter;
	changedUnit.mScope = kAudioUnitScope_Global;
	changedUnit.mElement = 0;
	
	if (ctrl_id == SooperLooper::Event::State)
	{
		if (instance < 0) {
			instance = 0;
		}
		// update state of command controls
		int currstate = (int) _engine->get_control_value(SooperLooper::Event::State, instance);
		int cmdid = (instance+1)*10000 + 500;
		if (_currStates[instance] != currstate) {
			// just notify them all, what the hell
			for (int i=0; i < SooperLooper::Event::LAST_COMMAND; ++i) {
				changedUnit.mParameterID = cmdid + i;	
				AUParameterListenerNotify (NULL, NULL, &changedUnit);
				if (selected) {
					changedUnit.mParameterID = 1500 + i;	
					AUParameterListenerNotify (NULL, NULL, &changedUnit);
				}		
			}
			_currStates[instance] = currstate;
	
			//cerr << "notifying for state change on: " << ctrl_id << "  inst: " << instance << " selloop: " << selected_loop << " sel: " << selected << endl;
		}
	
	}
	else {
	
		if (instance == -1) {
			// all loops
			for (size_t i=0; i < _engine->loop_count(); ++i) {
				paramid = (i+1)*10000 + ctrl_id;
				changedUnit.mParameterID = paramid;
				AUParameterListenerNotify (NULL, NULL, &changedUnit);
			}
			changedUnit.mParameterID = 1000 + ctrl_id;	
			AUParameterListenerNotify (NULL, NULL, &changedUnit);
		}
		else {
			// normal control
			changedUnit.mParameterID = paramid;
			AUParameterListenerNotify (NULL, NULL, &changedUnit);
			if (selected) {
				changedUnit.mParameterID = 1000 + ctrl_id;	
				AUParameterListenerNotify (NULL, NULL, &changedUnit);
			}
		}
		//cerr << "notifying for param change on: " << ctrl_id << "  inst: " << instance << endl;
	}

}

void SooperLooperAU::loop_added(int index, bool huh)
{
	loops_changed();
}

void SooperLooperAU::loop_removed()
{
	loops_changed();
}


// just leave this in for reference in case I need it
void SooperLooperAU::loops_changed()
{
	setup_params();

	AudioUnitEvent myEvent;
    myEvent.mEventType = kAudioUnitEvent_PropertyChange;
    myEvent.mArgument.mProperty.mAudioUnit = GetComponentInstance();
    myEvent.mArgument.mProperty.mPropertyID = kAudioUnitProperty_ParameterInfo;
    myEvent.mArgument.mProperty.mScope = kAudioUnitScope_Global;
    myEvent.mArgument.mProperty.mElement = 0;
    AUEventListenerNotify(NULL, NULL, &myEvent);
}


// SL audio driver stuff

void * SooperLooperAU::_engine_mainloop(void * arg)
{
	SooperLooperAU * slau = (SooperLooperAU *) arg;

	while (slau->_alive) {
		slau->_engine->mainloop();
		usleep(10000);
	} 

	return 0;
}

bool SooperLooperAU::initialize(std::string client_name)
{

	return true;
}


bool  SooperLooperAU::create_input_port (std::string name, port_id_t & port_id) 
{
	// this is a terrible hack, but generate the port_ids
	// from string matching the name
	int loopnum, chan;
	
	if (name.find("common", 0) == 0) {
		if (sscanf(name.c_str(), "common_in_%d", &chan) == 1) {
			port_id = chan;
			//cerr << "portid for " << name << " = " << port_id << endl;			
			return true;
		}
	}
	else if (name.find("loop", 0) == 0) {
		if (sscanf(name.c_str(), "loop%d_in_%d", &loopnum, &chan) == 2) {
			port_id = _chancnt + ((loopnum)*_chancnt) + (chan);
			//cerr << "portid for " << name << " = " << port_id << endl;
			return true;
		}		
	}
	
	/*
	if (_in_channel_id >= _chancnt) return false;
	
	// just increment input port count
	port_id = _in_channel_id++;
	 */
	 
	return false;
}

bool  SooperLooperAU::create_output_port (std::string name, port_id_t & port_id) 
{
	// this is a terrible hack, but generate the port_ids
	// from string matching the name
	int loopnum, chan;
	
	if (name.find("common", 0) == 0) {
		if (sscanf(name.c_str(), "common_out_%d", &chan) == 1) {
			port_id = chan;
			//cerr << "portid for " << name << " = " << port_id << endl;			
			return true;
		}
	}
	else if (name.find("loop", 0) == 0) {
		if (sscanf(name.c_str(), "loop%d_out_%d", &loopnum, &chan) == 2) {
			port_id = _chancnt + ((loopnum)*_chancnt) + (chan);
			//cerr << "portid for " << name << " = " << port_id << endl;
			return true;
		}		
	}
	
	/*
	if (_out_channel_id >= _chancnt) return false;
	
	port_id = _out_channel_id++;
	 */
	
	return false;
}

bool SooperLooperAU::destroy_input_port (port_id_t portid) 
{
	return true;
}

bool SooperLooperAU::destroy_output_port (port_id_t portid) 
{
	return true;
}

sample_t * SooperLooperAU::get_input_port_buffer (port_id_t port, nframes_t nframes) 
{
	// only works within a call to processbufferlists
	// port starts at 1
	if (port <= 0) return 0;
	
	port--;
	
	int bus = port / _chancnt;
	int chan = port % _chancnt;
	
	if (bus < SL_MAXLOOPS && _in_buflist[bus]) {
		//cerr << "returning input" << _in_buflist[bus] <<  " for " << bus << "  " << chan << "  port: " << port << endl;
		return (sample_t *) _in_buflist[bus]->mBuffers[chan].mData;
	}
	
	//cerr << "null input for " << port << endl;	
	/*
	if (!_in_buflist) return 0;
	
	if (port < _chancnt) {
		//cerr << "port " << port << " input 0 is: " << (sample_t *)_in_buflist->mBuffers[port].mData << " output is: " << 
		//(sample_t *)_out_buflist->mBuffers[port].mData << endl;
		return (sample_t *) _in_buflist->mBuffers[port].mData;
	}
	*/
	
	return 0;
}

sample_t * SooperLooperAU::get_output_port_buffer (port_id_t port, nframes_t nframes)
{
	// port starts at 1
	if (port <= 0) return 0;
	
	port--;
	
	int bus = port / _chancnt;
	int chan = port % _chancnt;
	
	if (bus < SL_MAXLOOPS && _out_buflist[bus]) {
		//cerr << "returning input" << _in_buflist[bus] <<  " for " << bus << "  " << chan << "  port: " << port << endl;
		return (sample_t *) _out_buflist[bus]->mBuffers[chan].mData;
	}
	
	//cerr << "null output for " << port << endl;
/*	
	if (!_out_buflist) return 0;
	
	if (port < _chancnt) {
		return (sample_t *)_out_buflist->mBuffers[port].mData;
	}
*/	
	return 0;
}

nframes_t SooperLooperAU::get_input_port_latency (port_id_t portid)
{
	// looks like the best we can do is report buffer size as an estimate.
	// it may not always be the case, but this is only used for auto-setting latency compensation
	return _last_rendered_frames;
}

nframes_t SooperLooperAU::get_output_port_latency (port_id_t portid)
{
	// looks like the best we can do is report buffer size as an estimate.
	// it may not always be the case, but this is only used for auto-setting latency compensation
	return _last_rendered_frames;
}

bool
SooperLooperAU::get_transport_info (TransportInfo &info)
{
	// the host tempo expressed in beats per minute
	Float64 tempo = info.bpm;
	// the current beat in the song, counting from 0 at the start of the song
	Float64 beat = 0;
	
	info.state = TransportInfo::STOPPED;
	
	if (mHostCallbackInfo.beatAndTempoProc != NULL )
	{
		
		if ( mHostCallbackInfo.beatAndTempoProc(mHostCallbackInfo.hostUserData, &beat, &tempo) == noErr )
		{
			// do something with tempo and beat values here
			info.bpm = tempo;
		}
	}
	
	if (tempo <= 0.0) {
		return false;
	}
	
	Float64 sampsbeat = GetSampleRate() / (tempo / 60.0);
	
	// the number of samples until the next beat from the start sample of the current rendering buffer
	UInt32 sampleOffsetToNextBeat = 0;
	
	// the number of beats of the denominator value that contained in the current measure
	Float32 timeSigNumerator = 4;
	// music notational conventions (4 is a quarter note, 8 an eigth note, etc)
	UInt32 timeSigDenominator = 4;
	// the beat that corresponds to the downbeat (first beat) of the current measure
	Float64 currentMeasureDownBeat;
	nframes_t currpos = _last_framepos;
	
	if (mHostCallbackInfo.musicalTimeLocationProc != NULL) {
			if ( mHostCallbackInfo.musicalTimeLocationProc(mHostCallbackInfo.hostUserData, &sampleOffsetToNextBeat, 
													   &timeSigNumerator, &timeSigDenominator, &currentMeasureDownBeat) == noErr )
			{
				// do something with beat position and time signature values here
				
				//info.framepos = (nframes_t) ((beat * sampsbeat) + (sampsbeat - sampleOffsetToNextBeat)); 
				currpos = (nframes_t) (beat * sampsbeat);
				//cerr << "musicaltime: " << sampsbeat << " beat: " << beat << "  frame: " << info.framepos << endl;
			}
	}
	
	if (currpos != _last_framepos) {
		info.framepos = currpos;
		_last_framepos = currpos;
	}
	else {
		info.framepos = (nframes_t) _curr_stamp.mSampleTime;					
	}
				
	info.state = TransportInfo::ROLLING;

	
	return true;
}


ComponentResult SooperLooperAU::SaveState(CFPropertyListRef *outData)
{
	ComponentResult result = AUBase::SaveState(outData);
	if (result != noErr)
		return result;
	
	CFMutableDictionaryRef dict = (CFMutableDictionaryRef) *outData;
	
	
	// save session state as string
	string sess_str;
	if (_engine->save_session("", false, &sess_str)) { 
		CFMutableDataRef cfdata = CFDataCreateMutable(NULL, 0);	
		unsigned long plaindatasize;
		const UInt8 * plaindata = (const UInt8 *) sess_str.c_str();
		plaindatasize = sess_str.size();
		//cerr << "returning state as: " << sess_str << endl;
		CFDataAppendBytes(cfdata, plaindata, plaindatasize);
		CFDictionarySetValue(dict, CFSTR("SLSessionXML"), cfdata);
		CFRelease(cfdata);
	}
	
	
	// save midi bindings as string
	MidiBindings & bindings = _midi_bridge->bindings();
	stringstream midisstr;
	if (bindings.save_bindings(midisstr)) {
		CFMutableDataRef cfdata = CFDataCreateMutable(NULL, 0);	
		unsigned long plaindatasize;
		const string & midistr = midisstr.str();
		const UInt8 * plaindata = (const UInt8 *) midistr.c_str();
		plaindatasize = midistr.size();
		
		CFDataAppendBytes(cfdata, plaindata, plaindatasize);
		CFDictionarySetValue(dict, CFSTR("SLMidiBindings"), cfdata);
		CFRelease(cfdata);
	}

	// save guiapp path
	{
		CFMutableDataRef cfdata = CFDataCreateMutable(NULL, 0);	
		unsigned long plaindatasize;
		const UInt8 * plaindata = (const UInt8 *) _guiapp_path.c_str();
		plaindatasize = _guiapp_path.size();
		
		CFDataAppendBytes(cfdata, plaindata, plaindatasize);
		CFDictionarySetValue(dict, CFSTR("SLAppPath"), cfdata);
		CFRelease(cfdata);	
	}
		
	// save guiapp path
	{
		CFDataRef cfdata = CFDataCreate(NULL, (const UInt8 *) &_stay_on_top, sizeof(short));	
		CFDictionarySetValue(dict, CFSTR("SLstayOnTop"), cfdata);
		CFRelease(cfdata);	
		//cerr << "saved stay on top as " << _stay_on_top << endl;		
	}
		
	*outData = dict;
	
	return noErr;
}

ComponentResult SooperLooperAU::RestoreState(CFPropertyListRef inData)
{
	ComponentResult result = AUBase::RestoreState(inData);
	if (result != noErr)
		return result;

	// session data
	
	CFDataRef cfdata =
		reinterpret_cast<CFDataRef>(CFDictionaryGetValue((CFDictionaryRef)inData,
														 CFSTR("SLSessionXML")));
	if (cfdata != NULL) 
	{
		const UInt8 * plaindata = CFDataGetBytePtr(cfdata);
		unsigned long plaindatasize = CFDataGetLength(cfdata);
									
		string sess_str((const char *)plaindata, plaindatasize);

		if (_engine && _engine->is_ok()) {
			//cerr << "RestoreState: " << sess_str << endl;
			_engine->load_session ("", &sess_str);
		}
		else {
			//cerr << "pending restore, will use: " << sess_str << endl;
			_pending_restore = sess_str;
		}
	}
	
	if (_engine && _engine->get_control_osc()) {
		Globals()->SetParameter(kParam_OSCPort, _engine->get_control_osc()->get_server_port());
	}
	
	// restore midi bindings
	
	CFDataRef midicfdata =
		reinterpret_cast<CFDataRef>(CFDictionaryGetValue((CFDictionaryRef)inData,
														 CFSTR("SLMidiBindings")));
	if (midicfdata != NULL)
	{
		const UInt8 * plaindata = CFDataGetBytePtr(midicfdata);
		unsigned long plaindatasize = CFDataGetLength(midicfdata);
		stringstream midisstr;
		midisstr.write((const char *) plaindata, plaindatasize);
		if (_midi_bridge) {
			_midi_bridge->bindings().load_bindings (midisstr);
		}

	}
	
	// restore app path
	CFDataRef app_pathdata =
		reinterpret_cast<CFDataRef>(CFDictionaryGetValue((CFDictionaryRef)inData,
														 CFSTR("SLAppPath")));
	if (app_pathdata != NULL)
	{
		const UInt8 * plaindata = CFDataGetBytePtr(app_pathdata);
		unsigned long plaindatasize = CFDataGetLength(app_pathdata);

		_guiapp_path.assign((const char *) plaindata, plaindatasize);
	}
	
	// restore stay on top
	CFDataRef stayontop_data =
		reinterpret_cast<CFDataRef>(CFDictionaryGetValue((CFDictionaryRef)inData,
														 CFSTR("SLstayOnTop")));
	if (stayontop_data != NULL)
	{
		const UInt8 * plaindata = CFDataGetBytePtr(stayontop_data);
		_stay_on_top = *((short*)plaindata);
		//cerr << "restored stay on top as " << _stay_on_top << endl;
	}

	
	return noErr;
}
