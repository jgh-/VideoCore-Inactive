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
#include <videocore/transforms/RTMP/AACPacketizer.h>
#include <vector>
#include <videocore/system/Buffer.hpp>
#include <videocore/rtmp/RTMPSession.h>

namespace videocore { namespace rtmp {

    AACPacketizer::AACPacketizer(float sampleRate, int channelCount, int ctsOffset)
    : m_audioTs(0), m_sentAudioConfig(false), m_sampleRate(sampleRate), m_channelCount(channelCount), m_ctsOffset(ctsOffset)
    {
        memset(m_asc, 0, sizeof(m_asc));
    }

    void
    AACPacketizer::pushBuffer(const uint8_t* const inBuffer, size_t inSize, IMetadata& metadata)
    {
        std::vector<uint8_t> & outBuffer = m_outbuffer;

        outBuffer.clear();

        int flvStereoOrMono = (m_channelCount == 2 ? FLV_STEREO : FLV_MONO);
        int flvSampleRate = FLV_SAMPLERATE_44100HZ; // default
        if (m_sampleRate == 22050.0) {
            flvSampleRate = FLV_SAMPLERATE_22050HZ;
        }

        int flags = 0;
        const int flags_size = 2;


        int ts = metadata.timestampDelta + m_ctsOffset ;
//        DLog("AAC: %06d", ts);
        
        auto output = m_output.lock();

        RTMPMetadata_t outMeta(ts);

        if(inSize == 2 && !m_asc[0] && !m_asc[1]) {
            m_asc[0] = inBuffer[0];
            m_asc[1] = inBuffer[1];
        }

        if(output) {

            flags = FLV_CODECID_AAC | flvSampleRate | FLV_SAMPLESSIZE_16BIT | flvStereoOrMono;

            outBuffer.reserve(inSize + flags_size);

            put_byte(outBuffer, flags);
            put_byte(outBuffer, m_sentAudioConfig);

            if(!m_sentAudioConfig) {
                m_sentAudioConfig = true;
                put_buff(outBuffer, (uint8_t*)m_asc, sizeof(m_asc));

            } else {
                put_buff(outBuffer, inBuffer, inSize);
            }

            outMeta.setData(ts, static_cast<int>(outBuffer.size()), RTMP_PT_AUDIO, kAudioChannelStreamId, false);

            output->pushBuffer(&outBuffer[0], outBuffer.size(), outMeta);
        }

    }

    void
    AACPacketizer::setOutput(std::shared_ptr<IOutput> output)
    {
        m_output = output;
    }

}
}
