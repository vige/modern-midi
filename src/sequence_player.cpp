/*
Copyright (c) 2015, Dimitri Diakopoulos All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "modernmidi.h"
#include "sequence_player.h"
#include "midi_message.h"
#include "midi_utils.h"
#include "timer.h"

using namespace mm;

MidiSequencePlayer::MidiSequencePlayer(MidiOutput & output) : output(output), shouldSequence(false)
{

}
    
MidiSequencePlayer::~MidiSequencePlayer()
{
    shouldSequence = false;

    if (sequencerThread.joinable())
        sequencerThread.join();
}

double MidiSequencePlayer::ticksToSeconds(int ticks)
{
    double beats = (double) ticks / ticksPerBeat;
    double seconds = beats / (beatsPerMinute / 60.0);
    return seconds;
}

int MidiSequencePlayer::secondsToTicks(float seconds)
{
    double ticksPerSecond = (beatsPerMinute * ticksPerBeat) / 60.0;
    double ticks = ticksPerSecond * seconds;
    return (int) ticks;
}

void MidiSequencePlayer::loadSingleTrack(int trackNumber, const MidiTrack & track)
{
    int localElapsedTicks = 0;

    // Events in track
    for (auto m : track)
    {
        localElapsedTicks += m->tick;

        addTickEvent(trackNumber, localElapsedTicks, m); // already checks if non-meta message

        std::cout << "Added event " << StringFromMessageType(m->m->getMessageType()) << std::endl;
    }
}

void MidiSequencePlayer::loadMultipleTracks(const std::vector<MidiTrack> & tracks, double ticksPerBeat, double beatsPerMinute)
{
    reset();
    this->ticksPerBeat = ticksPerBeat; 
    this->beatsPerMinute = beatsPerMinute;
    for(unsigned int i = 0; i < tracks.size(); i++) {
        eventList.push_back({});
        loadSingleTrack(i, tracks[i]);
    }
}

void MidiSequencePlayer::start()
{
    if (sequencerThread.joinable()) 
        sequencerThread.join();

    shouldSequence = true;
    sequencerThread = std::thread(&MidiSequencePlayer::run, this);

#if defined(MM_PLATFORM_WINDOWS)
    HANDLE threadHandle = sequencerThread.native_handle();
    int err = (int)SetThreadPriority(threadHandle, THREAD_PRIORITY_TIME_CRITICAL);
    if (err == 0)
    {
        std::cerr << "SetThreadPriority() failed: " << GetLastError() << std::endl;
    }

    err = (int)SetThreadAffinityMask(threadHandle, 1);
    if (err == 0)
    {
        std::cerr<< "SetThreadAffinityMask() failed: " << GetLastError() << std::endl;
    }
#endif
    
    //sequencerThread.detach();

    if (startedEvent)
        startedEvent();
}
    
void MidiSequencePlayer::run()
{
    int numTracks = eventList.size();
    std::vector<size_t> eventCursors(numTracks);
    bool running = true;
    int timerStartTick = 0;
    u_int8_t runningStatus = 0;

    PlatformTimer timer;
    timer.start();

//    while (eventCursor < eventList.size())
    while(running)
    {
        // find next event
        int min_tick = 0;
        MidiPlayerEvent* nextEvent = nullptr;
        for (int i = 0; i < numTracks; i++)
        {
            EventList& track = eventList[i];
            if (eventCursors[i] < track.size()) {
                if (track[eventCursors[i]].tick < min_tick
                    || !nextEvent) {
                    nextEvent = &track[eventCursors[i]];
                    min_tick = nextEvent->tick;
                }
            }
        }

        if (!nextEvent) {
            running = false;
        } else {
            // calculate next event time
            double seconds = ticksToSeconds(min_tick - timerStartTick);
            double runningTimeNow = timer.running_time_s();
            while((timer.running_time_s()) <= seconds || timer.running_time_s() <= runningTimeNow + 0.001)
            {
                continue;
            }
            const mm::MidiMessage& msg = *(nextEvent->msg);
            if (msg.isMetaEvent() && msg.getMetaEventSubtype() == MetaEventType::TEMPO_CHANGE) {
                const uint8_t *pdata = &(msg.data[3]);
                uint32_t us_per_quarter_note = read_uint24_be(pdata);
                std::cout << "Tempo us/quarter note: " << us_per_quarter_note << std::endl;
                this->beatsPerMinute = 60000000/us_per_quarter_note;
                std::cout << "BPM: " << this->beatsPerMinute << std::endl;
                timerStartTick = min_tick;
                timer.start();

            } else {
                if (msg.data[0] == runningStatus) {
//                    std::cout << "sending running status event: ";
//                    for(unsigned char i: msg.data)
//                       std::cout << std::hex << (unsigned int)i << ' ';
//                    std::cout << std::endl;
                    output.sendRunningStatus(msg);
                } else {
                    std::cout << "sending event: ";
//                    for(unsigned char i: msg.data)
//                       std::cout << std::hex << (unsigned int)i << ' ';
//                    std::cout << std::endl;
                    output.send(msg);
                    runningStatus = msg.data[0];
                }
            }
            if (shouldSequence == false) {
                for(int i = 0; i < 0x0F; i++){
                    MidiMessage allSoundOff(0xB0|i,120,0, 0.0);
                    output.send(allSoundOff);
                }
                        
                break;
            }
            eventCursors[nextEvent->trackIdx]++;
        }
    }

    timer.stop(); 

    if (loop && shouldSequence == true) 
        run();
        
    if (stoppedEvent)
        stoppedEvent();
}

void MidiSequencePlayer::stop()
{
    shouldSequence = false;
}
    
void MidiSequencePlayer::addTickEvent(int track, int when, std::shared_ptr<TrackEvent> ev)
{
    if (ev->m->isMetaEvent() == false || ev->m->getMetaEventSubtype() == MetaEventType::TEMPO_CHANGE)
    {
        eventList[track].push_back(MidiPlayerEvent(when, ev->m, track));
    }
}

float MidiSequencePlayer::length() const
{
    return playTimeSeconds;
}

void MidiSequencePlayer::setLooping(bool newState)
{
    loop = newState;
}

void MidiSequencePlayer::reset()
{
    eventList.clear();
    eventCursor = 0;
    startTime = 0;
    playTimeSeconds = 0;
}
