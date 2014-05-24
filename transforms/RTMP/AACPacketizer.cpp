/*
 
 Video Core
 Copyright (C) 2014 James G. Hurley
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 USA
 
 */
#include <videocore/transforms/RTMP/AACPacketizer.h>
#include <vector>
#include <videocore/system/Buffer.hpp>
#include <videocore/rtmp/RTMPSession.h>

namespace videocore { namespace rtmp {
 
    AACPacketizer::AACPacketizer()
    : m_audioTs(0), m_sentAudioConfig(false)
    {
        memset(m_asc, 0, sizeof(m_asc));
    }
    
    void
    AACPacketizer::pushBuffer(const uint8_t* const inBuffer, size_t inSize, IMetadata& metadata)
    {
        std::vector<uint8_t> & outBuffer = m_outbuffer;
        
        outBuffer.clear();
        
        int flags = 0;
        const int flags_size = 2;
    
        
        int ts = metadata.timestampDelta;
        
        auto output = m_output.lock();
        
        RTMPMetadata_t outMeta(metadata.timestampDelta);
        
        if(inSize == 2 && !m_asc[0] && !m_asc[1]) {
            m_asc[0] = inBuffer[0];
            m_asc[1] = inBuffer[1];
        }
        
        if(output) {
        
            flags = FLV_CODECID_AAC | FLV_SAMPLERATE_44100HZ | FLV_SAMPLESSIZE_16BIT | FLV_STEREO;
           
            outBuffer.reserve(inSize + flags_size);
            
            put_byte(outBuffer, flags);
            put_byte(outBuffer, m_sentAudioConfig);

            if(!m_sentAudioConfig) {
                m_sentAudioConfig = true;
                put_buff(outBuffer, (uint8_t*)m_asc, sizeof(m_asc));
                
            } else {
                
                put_buff(outBuffer, inBuffer, inSize);
                m_audioTs += metadata.timestampDelta;
                
            }

            outMeta.setData(ts, static_cast<int>(outBuffer.size()), FLV_TAG_TYPE_AUDIO, kAudioChannelStreamId);
            
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
