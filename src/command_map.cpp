/*
** Copyright (C) 2004 Jesse Chappell <jesse@essej.net>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/

#include "command_map.hpp"


using namespace SooperLooper;
using namespace std;

CommandMap * CommandMap::_instance = 0;

CommandMap::CommandMap()
{
	// initialize string maps

	_str_type_map["down"]  = Event::type_cmd_down;
	_str_type_map["up"]  = Event::type_cmd_up;
	_str_type_map["upforce"]  = Event::type_cmd_upforce;
	_str_type_map["hit"]  = Event::type_cmd_hit;
	_str_type_map["set"]  = Event::type_control_change;
	_str_type_map["get"]  = Event::type_control_request;
	_str_type_map["g_set"]  = Event::type_global_control_change;
	_str_type_map["sync"]  = Event::type_sync;
	
	for (StringTypeMap::iterator iter = _str_type_map.begin(); iter != _str_type_map.end(); ++iter) {
		_type_str_map[(*iter).second] = (*iter).first;
	}

	_str_cmd_map["record"]  = Event::RECORD;
	_str_cmd_map["overdub"]  = Event::OVERDUB;
	_str_cmd_map["multiply"]  = Event::MULTIPLY;
	_str_cmd_map["insert"]  = Event::INSERT;
	_str_cmd_map["replace"]  = Event::REPLACE;
	_str_cmd_map["reverse"]  = Event::REVERSE;
	_str_cmd_map["mute"]  = Event::MUTE;
	_str_cmd_map["undo"]  = Event::UNDO;
	_str_cmd_map["redo"]  = Event::REDO;
	_str_cmd_map["scratch"]  = Event::SCRATCH;
	_str_cmd_map["trigger"]  = Event::TRIGGER;
	_str_cmd_map["oneshot"]  = Event::ONESHOT;
	_str_cmd_map["substitute"]  = Event::SUBSTITUTE;
	_str_cmd_map["undo_all"]  = Event::UNDO_ALL;
	_str_cmd_map["redo_all"]  = Event::REDO_ALL;
	_str_cmd_map["mute_on"]  = Event::MUTE_ON;
	_str_cmd_map["mute_off"]  = Event::MUTE_OFF;
	
	for (StringCommandMap::iterator iter = _str_cmd_map.begin(); iter != _str_cmd_map.end(); ++iter) {
		_cmd_str_map[(*iter).second] = (*iter).first;
	}

	
	_input_controls["rec_thresh"]  = Event::TriggerThreshold;
	_input_controls["feedback"]  = Event::Feedback;
	_input_controls["use_feedback_play"]  = Event::UseFeedbackPlay;
	_input_controls["dry"]  = Event::DryLevel;
	_input_controls["wet"]  = Event::WetLevel;
	_input_controls["rate"]  = Event::Rate;
	_input_controls["scratch_pos"]  = Event::ScratchPosition;
	_input_controls["delay_trigger"]  = Event::TapDelayTrigger;
	_input_controls["quantize"]  = Event::Quantize;
	_input_controls["round"]  = Event::Round;
	_input_controls["redo_is_tap"]  = Event::RedoTap;
	_input_controls["sync"]  = Event::SyncMode;
	_input_controls["playback_sync"]  = Event::PlaybackSync;
	_input_controls["use_rate"]  = Event::UseRate;
	_input_controls["fade_samples"]  = Event::FadeSamples;
	_input_controls["use_safety_feedback"]  = Event::UseSafetyFeedback;
	_input_controls["relative_sync"]  = Event::RelativeSync;
	_input_controls["input_latency"]  = Event::InputLatency;
	_input_controls["output_latency"]  = Event::OutputLatency;
	_input_controls["trigger_latency"]  = Event::TriggerLatency;
	_input_controls["autoset_latency"]  = Event::AutosetLatency;
	_input_controls["mute_quantized"]  = Event::MuteQuantized;
	_input_controls["overdub_quantized"]  = Event::OverdubQuantized;
	//_input_controls["eighth_per_cycle_loop"] = Event::EighthPerCycleLoop;
	//_input_controls["tempo_input"] = Event::TempoInput;
	_input_controls["input_gain"]  = Event::InputGain;
	_input_controls["use_common_ins"]  = Event::UseCommonIns;
	_input_controls["use_common_outs"]  = Event::UseCommonOuts;
	_input_controls["pan_1"]  = Event::PanChannel1;
	_input_controls["pan_2"]  = Event::PanChannel2;
	_input_controls["pan_3"]  = Event::PanChannel3;
	_input_controls["pan_4"]  = Event::PanChannel4;


	_str_ctrl_map.insert (_input_controls.begin(), _input_controls.end());

	
	// outputs
	_output_controls["waiting"]  = Event::Waiting;
	_output_controls["state"]  = Event::State;
	_output_controls["next_state"]  = Event::NextState;
	_output_controls["loop_len"]  = Event::LoopLength;
	_output_controls["loop_pos"]  = Event::LoopPosition;
	_output_controls["cycle_len"]  = Event::CycleLength;
	_output_controls["free_time"]  = Event::FreeTime;
	_output_controls["total_time"]  = Event::TotalTime;
	_output_controls["rate_output"]  = Event::TrueRate;
	_output_controls["has_discrete_io"]  = Event::HasDiscreteIO;
	_output_controls["channel_count"]  = Event::ChannelCount;
	_output_controls["in_peak_meter"]  = Event::InPeakMeter;
	_output_controls["out_peak_meter"]  = Event::OutPeakMeter;

	_str_ctrl_map.insert (_output_controls.begin(), _output_controls.end());

	// control events
	_event_controls["midi_start"] = Event::MidiStart;
	_event_controls["midi_stop"] = Event::MidiStop;
	_event_controls["midi_tick"] = Event::MidiTick;

	_str_ctrl_map.insert (_event_controls.begin(), _event_controls.end());
	
	// global params
	_global_controls["tempo"] = Event::Tempo;
	_global_controls["eighth_per_cycle"] = Event::EighthPerCycle;
	_global_controls["sync_source"] = Event::SyncTo;
	_global_controls["tap_tempo"] = Event::TapTempo;
	_global_controls["save_loop"] = Event::SaveLoop;
	_global_controls["auto_disable_latency"] = Event::AutoDisableLatency;
	_global_controls["select_next_loop"] = Event::SelectNextLoop;
	_global_controls["select_prev_loop"] = Event::SelectPrevLoop;
	_global_controls["select_all_loops"] = Event::SelectAllLoops;
	_global_controls["selected_loop_num"] = Event::SelectedLoopNum;

	_str_ctrl_map.insert (_global_controls.begin(), _global_controls.end());

	// reverse it
	for (StringControlMap::iterator iter = _str_ctrl_map.begin(); iter != _str_ctrl_map.end(); ++iter) {
		_ctrl_str_map[(*iter).second] = (*iter).first;
	}

	
}

void CommandMap::get_commands (list<std::string> & cmdlist)
{
	for (StringCommandMap::iterator iter = _str_cmd_map.begin(); iter != _str_cmd_map.end(); ++iter) {
		cmdlist.push_back ((*iter).first);
	}
}

void CommandMap::get_controls (list<std::string> & ctrllist)
{
	for (StringControlMap::iterator iter = _str_ctrl_map.begin(); iter != _str_ctrl_map.end(); ++iter) {
		ctrllist.push_back ((*iter).first);
	}
}
