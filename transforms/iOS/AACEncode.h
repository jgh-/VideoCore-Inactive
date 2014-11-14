/*

 Video Core
 Copyright (c) 2014 James G. Hurley

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.

 */

/*
 *  iOS::AACEncode transform :: Takes a raw buffer of LPCM input and outputs AAC Packets.
 *
 *
 */


#ifndef __videocore__AACEncode__
#define __videocore__AACEncode__

#include <iostream>
#include <videocore/transforms/IEncoder.hpp>
#include <AudioToolbox/AudioToolbox.h>
#include <videocore/system/Buffer.hpp>

namespace videocore { namespace iOS {

    class AACEncode : public IEncoder
    {
    public:

        AACEncode(int frequencyInHz, int channelCount, int averageBitrate);

        ~AACEncode();

        void setOutput(std::shared_ptr<IOutput> output) { m_output = output; };
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
        
        void setBitrate(int bitrate);
        const int bitrate() const { return m_bitrate; };
        
    private:
        static OSStatus ioProc(AudioConverterRef audioConverter, UInt32 *ioNumDataPackets, AudioBufferList* ioData, AudioStreamPacketDescription** ioPacketDesc, void* inUserData );
        void makeAsc(uint8_t sampleRateIndex, uint8_t channelCount);
    private:

        AudioStreamBasicDescription m_in, m_out;
        
        std::mutex              m_converterMutex;
        AudioConverterRef       m_audioConverter;
        std::weak_ptr<IOutput>  m_output;
        size_t                  m_bytesPerSample;
        uint32_t                m_outputPacketMaxSize;

        Buffer                  m_outputBuffer;

        int m_bitrate;
        uint8_t m_asc[2];
        bool    m_sentConfig;

    };

}
}
#endif /* defined(__videocore__AACEncode__) */
