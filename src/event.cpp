/*
** Copyright (C) 2004 Jesse Chappell <jesse@essej.net>
**              and Benno Senoner and Christian Schoenebeck
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

#include "event.hpp"
#include <iostream>
#include <stdint.h>
#include <sys/time.h>

using namespace std;

namespace SooperLooper {

    /**
     * Create an EventGenerator.
     *
     * @param SampleRate - sample rate of the sampler engine's audio output
     *                     signal (in Hz)
     */
    EventGenerator::EventGenerator(uint32_t sampleRate) {
        uiSampleRate       = sampleRate;
        uiSamplesProcessed = 0;
        fragmentTime.end   = createTimeStamp();
    }

    /**
     * Updates the time stamps for the beginning and end of the current audio
     * fragment. This is needed to be able to calculate the respective sample
     * point later to which an event belongs to.
     *
     * @param SamplesToProcess - number of sample points to process in this
     *                           audio fragment cycle
     */
    void EventGenerator::updateFragmentTime(uint32_t samplesToProcess)
    {
        // update time stamp for this audio fragment cycle
        fragmentTime.begin = fragmentTime.end;
        fragmentTime.end   = createTimeStamp();
        // recalculate sample ratio for this audio fragment
        time_stamp_t fragmentDuration = fragmentTime.end - fragmentTime.begin;
        fragmentTime.sample_ratio = (double) uiSamplesProcessed / (double) fragmentDuration;

	//cerr << "begin: " << fragmentTime.begin << " end: " << fragmentTime.end << "  ratio: " << fragmentTime.sample_ratio << endl;
	// store amount of samples to process for the next cycle
        uiSamplesProcessed = samplesToProcess;
    }

    /**
     * Create a new event with the current time as time stamp.
     */
	Event EventGenerator::createEvent(long fragTime)
	{
		if (fragTime < 0) {
			return Event(this, createTimeStamp());
		}
		else {
			return Event(this, (int) fragTime);
		}
	}

	Event EventGenerator::createTimestampedEvent(time_stamp_t timeStamp)
	{
		return Event(this, timeStamp);
	}

	
// 	Event EventGenerator::createEvent()
//     {
// 	    time_stamp_t ts = createTimeStamp();
// 	    cerr << "ts is : " << ts << endl;
// 	    uint32_t pos = toFragmentPos(ts);
// 	    cerr << "event pos: " << pos << endl;
// 	    return Event(pos);
//     }

    /**
     * Creates a real time stamp for the current moment.
     */
    EventGenerator::time_stamp_t EventGenerator::createTimeStamp()
    {
	    struct timeval tv;
	    gettimeofday(&tv, NULL);

	    return (time_stamp_t)(tv.tv_usec * 1e-6) + (time_stamp_t)(tv.tv_sec);
    }

    /**
     * Will be called by an EventGenerator to create a new Event.
     */
    Event::Event(EventGenerator* pGenerator, time_stamp_t Time) {
        pEventGenerator = pGenerator;
        TimeStamp       = Time;
        iFragmentPos    = -1;
    }

    Event::Event(EventGenerator* pGenerator, int fragmentpos) {
        pEventGenerator = pGenerator;
        iFragmentPos    = fragmentpos;
    }
	
} // namespace LinuxSampler
