/***************************************************************************
 *                                                                         *
 *   LinuxSampler - modular, streaming capable sampler                     *
 *                                                                         *
 *   Copyright (C) 2003, 2004 by Benno Senoner and Christian Schoenebeck   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston,                 *
 *   MA  02111-1307  USA                                                   *
 ***************************************************************************/

#ifndef __LS_EVENT_H__
#define __LS_EVENT_H__

#include "../../common/global.h"

namespace LinuxSampler {

    // just symbol prototyping
    class Event;

    /**
     * Generates Event objects and is responsible for resolving the position
     * in the current audio fragment each Event actually belongs to.
     */
    class EventGenerator {
        public:
            EventGenerator(uint SampleRate);
            void UpdateFragmentTime(uint SamplesToProcess);
            Event CreateEvent();
        protected:
            typedef uint32_t time_stamp_t; ///< We read the processor's cycle count register as a reference for the real time. These are of course only abstract values with arbitrary time entity, but that's not a problem as we calculate relatively.
            inline uint ToFragmentPos(time_stamp_t TimeStamp) {
                return uint ((TimeStamp - FragmentTime.begin) * FragmentTime.sample_ratio);
            }
            friend class Event;
        private:
            uint uiSampleRate;
            uint uiSamplesProcessed;
            struct __FragmentTime__ {
                time_stamp_t begin;        ///< Real time stamp of the beginning of this audio fragment cycle.
                time_stamp_t end;          ///< Real time stamp of the end of this audio fragment cycle.
                float        sample_ratio; ///< (Samples per cycle) / (Real time duration of cycle)
            } FragmentTime;
            time_stamp_t CreateTimeStamp();
    };

    /**
     * Events are usually caused by a MIDI source or an internal modulation
     * controller like LFO or EG. An event can only be created by an
     * EventGenerator.
     *
     * @see EventGenerator
     */
    class Event {
        public:
            Event(){}
            enum type_t {
                type_note_on,
                type_note_off,
                type_pitchbend,
                type_control_change,
                type_cancel_release,  ///< transformed either from a note-on or sustain-pedal-down event
                type_release          ///< transformed either from a note-off or sustain-pedal-up event
            } Type;
            enum destination_t {
                destination_vca,   ///< Volume level
                destination_vco,   ///< Pitch depth
                destination_vcfc,  ///< Filter curoff frequency
                destination_vcfr,  ///< Filter resonance
                destination_count  ///< Total number of modulation destinations (this has to stay the last element in the enum)
            };
            union {
                uint8_t Key;          ///< MIDI key number for note-on and note-off events.
                uint8_t Controller;   ///< MIDI controller number for control change events.
            };
            union {
                uint8_t Velocity;     ///< Trigger or release velocity for note-on or note-off events.
                uint8_t Value;        ///< Value for control change events.
            };
            int16_t Pitch;            ///< Pitch value for pitchbend events.

            inline uint FragmentPos() {
                if (iFragmentPos >= 0) return (uint) iFragmentPos;
                return (uint) (iFragmentPos = pEventGenerator->ToFragmentPos(TimeStamp));
            }
        protected:
            typedef EventGenerator::time_stamp_t time_stamp_t;
            Event(EventGenerator* pGenerator, EventGenerator::time_stamp_t Time);
            friend class EventGenerator;
        private:
            EventGenerator* pEventGenerator; ///< Creator of the event.
            time_stamp_t    TimeStamp;       ///< Time stamp of the event's occurence.
            int             iFragmentPos;    ///< Position in the current fragment this event refers to.
    };

} // namespace LinuxSampler

#endif // __LS_EVENT_H__
