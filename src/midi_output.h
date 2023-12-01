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

#pragma once

#ifndef MODERNMIDI_MIDI_OUTPUT_H
#define MODERNMIDI_MIDI_OUTPUT_H

#include "modernmidi.h"
#include "midi_message.h"

namespace mm
{

class MidiOutput
{
    bool attached = false;

    bool sendRaw(std::vector<unsigned char> msg);

public:

    MidiOutput(const std::string & name);
    ~MidiOutput();

    bool openPort(int32_t portNumber);
    bool openPort(std::string deviceName);
    bool openDevicePort(std::string deviceFilename);
    bool openVirtualPort(std::string portName);
    void closePort();

    bool isAttached();
    
    bool send(const std::vector<uint8_t> & msg);
    bool send(const mm::MidiMessage & msg);

#ifndef MM_TINYMIDI    
    RtMidiOut * getOutputDevice() { return outputDevice.get(); }
#endif
    
    mm::MidiDeviceInfo info;

#if defined(MM_TINYMIDI)
    std::string clientName;
#else   
    std::unique_ptr<RtMidiOut> outputDevice;
#endif    
};

}

#endif
