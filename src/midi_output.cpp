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

#include "midi_output.h"

using namespace mm;

MidiOutput::MidiOutput(const std::string & name) 
{
#if defined(MM_TINYMIDI)
    clientName=name;
#else    
    outputDevice.reset(new RtMidiOut(RtMidi::UNSPECIFIED, name));
#endif    
}

MidiOutput::~MidiOutput() 
{
    closePort();
}

bool MidiOutput::openPort(int32_t portNumber) 
{
#ifndef MM_TINYMIDI    
    if (attached) throw std::runtime_error("device is already attached to a port");
    try 
    {
        closePort();
        outputDevice->openPort(portNumber, std::to_string(portNumber));
        attached = true;
    }
    catch(RtMidiError & e) 
    {
        std::cerr << e.what() << std::endl;
        return false;
    }

    info = {portNumber, false, outputDevice->getPortName(portNumber)};
#endif    
    return true;
}

bool MidiOutput::openPort(std::string deviceName) 
{
    int port = -1;
#ifndef MM_TINYMIDI    
    for (uint32_t i = 0; i < outputDevice->getPortCount(); ++i) 
    {
        std::string name = outputDevice->getPortName(i);
        if(name == deviceName) 
        {
            port = i;
            break;
        }
    }
    if (port == -1) 
    {
        std::cerr << "Port not available" << std::endl;
        return false;
    }
#endif    
    return openPort(port);
}

bool MidiOutput::openDevicePort(std::string deviceFilename)
{
    info = {0, false, ""};
    info.device = deviceFilename;
    if (rawmidi_hw_open(0, &rawmidi_output, deviceFilename.c_str(), clientName.c_str(), 0) < 0)
        throw std::runtime_error("Could not open midi output for writing");
    std::cout << "rawmidi_hw_open returned" << std::endl;
    attached = true;
    return true;
}

bool MidiOutput::openVirtualPort(std::string portName) 
{
#if defined (MM_PLATFORM_WINDOWS)
    throw std::runtime_error("virtual ports are unsupported on windows");
#endif
#ifndef MM_TINYMIDI
    if (attached) throw std::runtime_error("device is already attached to a port");
    try 
    {
        closePort();
        outputDevice->openVirtualPort(portName);
        attached = true;
    }
    catch(RtMidiError & e) 
    {
        std::cerr << e.what() << std::endl;
        return false;
    }

    info = {-1, true, portName};
#endif    
    return true;
}

void MidiOutput::closePort() 
{
#if defined(MM_TINYMIDI)
    if (rawmidi_output)
        rawmidi_hw_close(rawmidi_output);
#else
    outputDevice->closePort();
#endif
    info = {-1, false, ""};
    attached = false;
}

bool MidiOutput::isAttached()
{
    return attached;
}

bool MidiOutput::sendRaw(std::vector<unsigned char> msg)
{
#ifndef MM_TINYMIDI    
    if (!outputDevice) throw std::runtime_error("output device not initialized");
#endif    
    if (!attached) throw std::runtime_error("interface not bound to a port");
#if defined(MM_TINYMIDI)
    rawmidi_hw_write(rawmidi_output, msg.data(), msg.size());
#else    
    try 
    {
        // std::cout << "sending message: ";
        // for(unsigned char i: msg)
        //     std::cout << std::hex << (unsigned int)i << ' ';
        // std::cout << std::endl;
        outputDevice->sendMessage(&msg);
    }
    catch(RtMidiError & e) 
    {
        std::cerr << e.what() << std::endl;
        return false;
    }
#endif    
    return true;
}

bool MidiOutput::send(const std::vector<uint8_t> & msg)
{
    return sendRaw(static_cast<std::vector<unsigned char>>(msg));
}

bool MidiOutput::send(const mm::MidiMessage & msg)
{
    return sendRaw(static_cast<std::vector<unsigned char>>(msg.data));
}

