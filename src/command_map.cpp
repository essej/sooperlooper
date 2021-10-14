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
	_str_cmd_map["mute_trigger"]  = Event::MUTE_TRIGGER;
	_str_cmd_map["pause"]  = Event::PAUSE;
	_str_cmd_map["pause_on"]  = Event::PAUSE_ON;
	_str_cmd_map["pause_off"]  = Event::PAUSE_OFF;
	_str_cmd_map["solo"]  = Event::SOLO;
	_str_cmd_map["solo_next"]  = Event::SOLO_NEXT;
	_str_cmd_map["solo_prev"]  = Event::SOLO_PREV;
	_str_cmd_map["record_solo"]  = Event::RECORD_SOLO;	
	_str_cmd_map["record_solo_next"]  = Event::RECORD_SOLO_NEXT;	
	_str_cmd_map["record_solo_prev"]  = Event::RECORD_SOLO_PREV;	
	_str_cmd_map["set_sync_pos"]  = Event::SET_SYNC_POS;
	_str_cmd_map["reset_sync_pos"]  = Event::RESET_SYNC_POS;
	_str_cmd_map["record_or_overdub"]  = Event::RECORD_OR_OVERDUB;
	_str_cmd_map["record_exclusive"]  = Event::RECORD_EXCLUSIVE;	
	_str_cmd_map["record_exclusive_next"]  = Event::RECORD_EXCLUSIVE_NEXT;	
	_str_cmd_map["record_exclusive_prev"]  = Event::RECORD_EXCLUSIVE_PREV;
	_str_cmd_map["record_or_overdub_excl"]  = Event::RECORD_OR_OVERDUB_EXCL;
	_str_cmd_map["record_or_overdub_excl_next"]  = Event::RECORD_OR_OVERDUB_EXCL_NEXT;
	_str_cmd_map["record_or_overdub_excl_prev"]  = Event::RECORD_OR_OVERDUB_EXCL_PREV;
	_str_cmd_map["record_or_overdub_solo"]  = Event::RECORD_OR_OVERDUB_SOLO;
	_str_cmd_map["record_or_overdub_solo_next"]  = Event::RECORD_OR_OVERDUB_SOLO_NEXT;
	_str_cmd_map["record_or_overdub_solo_prev"]  = Event::RECORD_OR_OVERDUB_SOLO_PREV;
	_str_cmd_map["record_or_overdub_solo_trig"]  = Event::RECORD_OR_OVERDUB_SOLO_TRIG;
	_str_cmd_map["record_overdub_end_solo"]  = Event::RECORD_OVERDUB_END_SOLO;
	_str_cmd_map["record_overdub_end_solo_trig"]  = Event::RECORD_OVERDUB_END_SOLO_TRIG;
	
	for (StringCommandMap::iterator iter = _str_cmd_map.begin(); iter != _str_cmd_map.end(); ++iter) {
		_cmd_str_map[(*iter).second] = (*iter).first;
	}

	add_input_control("rec_thresh", Event::TriggerThreshold, UnitGain, 0.0f, 1.0f, 0.0f);
	add_input_control("feedback", Event::Feedback, UnitPercent, 0.0f, 1.0f, 1.0f);
	add_input_control("use_feedback_play", Event::UseFeedbackPlay, UnitBoolean, 0.0f, 1.0f, 0.0f);
	add_input_control("dry", Event::DryLevel, UnitGain, 0.0f, 1.0f, 0.0f);
	add_input_control("wet", Event::WetLevel, UnitGain, 0.0f, 1.0f, 1.0f);
	add_input_control("rate", Event::Rate, UnitRatio, 0.25f, 4.0f, 1.0f);
	add_input_control("scratch_pos", Event::ScratchPosition, UnitRatio, 0.0f, 1.0f, 0.0f);
	add_input_control("delay_trigger",  Event::TapDelayTrigger, UnitGeneric, 0.0f, 1.0f, 0.0f);
	add_input_control("quantize", Event::Quantize, UnitIndexed, 0.0f, 6.0f, 0.0f);
	add_input_control("round", Event::Round, UnitBoolean);
	add_input_control("redo_is_tap", Event::RedoTap, UnitBoolean);
	add_input_control("sync", Event::SyncMode, UnitBoolean);
	add_input_control("playback_sync", Event::PlaybackSync, UnitBoolean);
	add_input_control("use_rate", Event::UseRate, UnitBoolean);
	add_input_control("fade_samples", Event::FadeSamples, UnitSamples, 0.0f, 4096.0f, 64.0f);
	add_input_control("use_safety_feedback", Event::UseSafetyFeedback, UnitBoolean, 0.0f, 1.0f, 1.0f);
	add_input_control("relative_sync", Event::RelativeSync, UnitBoolean);
	add_input_control("input_latency", Event::InputLatency, UnitSamples, 0.0f, 32768.0f, 0.0f);
	add_input_control("output_latency", Event::OutputLatency, UnitSamples, 0.0f, 32768.0f, 0.0f);
	add_input_control("trigger_latency", Event::TriggerLatency, UnitSamples, 0.0f, 32768.0f, 0.0f);
	add_input_control("autoset_latency", Event::AutosetLatency, UnitBoolean, 0.0f, 1.0f, 1.0f);
    add_input_control("discrete_prefader", Event::DiscretePreFader, UnitBoolean, 0.0f, 1.0f, 0.0f);
	add_input_control("mute_quantized", Event::MuteQuantized, UnitBoolean);
	add_input_control("overdub_quantized", Event::OverdubQuantized, UnitBoolean);
	add_input_control("replace_quantized", Event::ReplaceQuantized, UnitBoolean);
	//_input_controls["eighth_per_cycle_loop"] = Event::EighthPerCycleLoop;
	//_input_controls["tempo_input"] = Event::TempoInput;
	add_input_control("input_gain", Event::InputGain, UnitGain, 0.0f, 1.0f, 1.0f);
	add_input_control("use_common_ins", Event::UseCommonIns, UnitBoolean, 0.0f, 1.0f, 1.0f);
	add_input_control("use_common_outs", Event::UseCommonOuts, UnitBoolean, 0.0f, 1.0f, 1.0f);
	add_input_control("pan_1", Event::PanChannel1, UnitGeneric, 0.0f, 1.0f, 0.5f);
	add_input_control("pan_2", Event::PanChannel2, UnitGeneric, 0.0f, 1.0f, 0.5f);
	add_input_control("pan_3", Event::PanChannel3, UnitGeneric, 0.0f, 1.0f, 0.5f);
	add_input_control("pan_4", Event::PanChannel4, UnitGeneric, 0.0f, 1.0f, 0.5f);
	add_input_control("stretch_ratio", Event::StretchRatio, UnitRatio, 0.5f, 4.0f, 1.0f);
	add_input_control("pitch_shift", Event::PitchShift, UnitSemitones, -12.0f, 12.0f, 0.0f); 
	add_input_control("tempo_stretch", Event::TempoStretch, UnitBoolean);
	add_input_control("round_integer_tempo", Event::RoundIntegerTempo, UnitBoolean);
	add_input_control("jack_timebase_master", Event::JackTimebaseMaster, UnitBoolean);

	_str_ctrl_map.insert (_input_controls.begin(), _input_controls.end());

	
	// outputs
	add_output_control("waiting", Event::Waiting, UnitBoolean);
	add_output_control("state", Event::State, UnitIndexed, -1.0f, 20.0f);
	add_output_control("next_state", Event::NextState, UnitIndexed, -1.0f, 20.0f);
	add_output_control("loop_len", Event::LoopLength, UnitSeconds, 0.0f, 1e6);
	add_output_control("loop_pos", Event::LoopPosition,  UnitSeconds, 0.0f, 1e6);
	add_output_control("cycle_len", Event::CycleLength,  UnitSeconds, 0.0f, 1e6);
	add_output_control("free_time", Event::FreeTime,  UnitSeconds, 0.0f, 1e6);
	add_output_control("total_time", Event::TotalTime,  UnitSeconds, 0.0f, 1e6);
	add_output_control("rate_output", Event::TrueRate, UnitGeneric, 0.0f, 8.0f);
	add_output_control("has_discrete_io", Event::HasDiscreteIO, UnitBoolean);
	add_output_control("channel_count", Event::ChannelCount, UnitInteger, 0.0f, 16.0f);
	add_output_control("in_peak_meter", Event::InPeakMeter, UnitGeneric, 0.0f, 4.0f);
	add_output_control("out_peak_meter", Event::OutPeakMeter, UnitGeneric, 0.0f, 4.0f);
	add_output_control("is_soloed", Event::IsSoloed, UnitBoolean);

	_str_ctrl_map.insert (_output_controls.begin(), _output_controls.end());

	// control events
	_event_controls["midi_start"] = Event::MidiStart;
	_event_controls["midi_stop"] = Event::MidiStop;
	_event_controls["midi_tick"] = Event::MidiTick;

	_str_ctrl_map.insert (_event_controls.begin(), _event_controls.end());
	
	// global params
	add_global_control("tempo", Event::Tempo, UnitTempo, 0.0f, 1000.0f, 120.0f);
	add_global_control("eighth_per_cycle", Event::EighthPerCycle, UnitIndexed, 0.0, 2048.0f);
	add_global_control("sync_source", Event::SyncTo, UnitIndexed, -5.0f, 16.0f);
	add_global_control("tap_tempo", Event::TapTempo, UnitGeneric);
	add_global_control("save_loop", Event::SaveLoop);
	add_global_control("auto_disable_latency", Event::AutoDisableLatency, UnitBoolean, 0.0f, 1.0f, 1.0f);
	add_global_control("select_next_loop", Event::SelectNextLoop);
	add_global_control("select_prev_loop", Event::SelectPrevLoop);
	add_global_control("select_all_loops", Event::SelectAllLoops);
	add_global_control("selected_loop_num", Event::SelectedLoopNum, UnitIndexed, -1.0f, 32.0f);
	add_global_control("output_midi_clock", Event::OutputMidiClock, UnitBoolean, 0.0f, 1.0f, 1.0f);
	add_global_control("smart_eighths", Event::SmartEighths, UnitBoolean, 0.0f, 1.0f, 1.0f);
	add_global_control("use_midi_start", Event::UseMidiStart, UnitBoolean, 0.0f, 1.0f, 1.0f);
	add_global_control("use_midi_stop", Event::UseMidiStop, UnitBoolean, 0.0f, 1.0f, 1.0f);
	add_global_control("send_midi_start_on_trigger", Event::SendMidiStartOnTrigger, UnitBoolean, 0.0f, 1.0f, 1.0f);
	add_global_control("global_cycle_len", Event::GlobalCycleLen, UnitSeconds, 0.0f, 1e6);
	add_global_control("global_cycle_pos", Event::GlobalCyclePos, UnitSeconds, 0.0f, 1e6);
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

void CommandMap::get_global_controls (list<std::string> & ctrllist)
{
	for (StringControlMap::iterator iter = _global_controls.begin(); iter != _global_controls.end(); ++iter) {
		ctrllist.push_back ((*iter).first);
	}
}

bool CommandMap::get_control_info(const std::string & ctrl, ControlInfo & info)
{
	ControlInfoMap::iterator found = _ctrl_info_map.find(ctrl);
	if (found != _ctrl_info_map.end()) {
		info = found->second;
		return true;
	}
	return false;
}

void CommandMap::add_input_control(const std::string & name, Event::control_t ctrl, 
				   ControlUnit unit, float minVal, float maxVal, float defaultVal)
{
	_input_controls[name] = ctrl;
	_ctrl_info_map[name] = ControlInfo(name, ctrl, unit, minVal, maxVal, defaultVal);
}

void CommandMap::add_output_control(const std::string & name, Event::control_t ctrl, 
			       ControlUnit unit, float minVal, float maxVal, float defaultVal)
{
	_output_controls[name] = ctrl;
	_ctrl_info_map[name] = ControlInfo(name, ctrl, unit, minVal, maxVal, defaultVal);
}

void CommandMap::add_global_control(const std::string & name, Event::control_t ctrl, 
				   ControlUnit unit, float minVal, float maxVal, float defaultVal)
{
	_global_controls[name] = ctrl;
	_ctrl_info_map[name] = ControlInfo(name, ctrl, unit, minVal, maxVal, defaultVal);
}
