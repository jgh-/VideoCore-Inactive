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
#include <videocore/transforms/RTMP/H264Packetizer.h>
#include <videocore/rtmp/RTMPTypes.h>
#include <videocore/rtmp/RTMPSession.h>

namespace videocore { namespace rtmp {
    
    H264Packetizer::H264Packetizer() : m_videoTs(0)
    {
        
    }
    void
    H264Packetizer::setOutput(std::shared_ptr<IOutput> output)
    {
        m_output = output;
    }
    void H264Packetizer::pushBuffer(const uint8_t* const inBuffer, size_t inSize, IMetadata& inMetadata)
    {
        const auto now = std::chrono::steady_clock::now();
        const uint64_t bufferDuration(inMetadata.timestampDelta * 1000000.);
        const double nowmicros (std::chrono::duration_cast<std::chrono::microseconds>(now - m_epoch).count());
        
        const auto micros = std::nearbyint(nowmicros / double(bufferDuration)) * bufferDuration;
        
        
        
        TransformMetadata_t tmd = static_cast<TransformMetadata_t&>(inMetadata);
        
        double frameTime = tmd.timestampDelta * 1000.;

        
        std::vector<uint8_t>& outBuffer = m_outbuffer;
        
        outBuffer.clear();
        
        uint8_t nal_type = inBuffer[4] & 0x1F;
        int flags = 0;
        const int flags_size = 5;
        const int ts = micros / 1000;
        
        bool is_config = (nal_type == 7 || nal_type == 8);
        
        
        flags = FLV_CODECID_H264;
        auto output = m_output.lock();
        RTMPMetadata_t outMeta(frameTime);

        switch(nal_type) {
            case 7:
                if(m_sps.size() == 0) {
                    m_sps.resize(inSize-4);
                    memcpy(&m_sps[0], inBuffer+4, inSize-4);
                    
                }
                return;
            case 8:
                if(m_pps.size() == 0) {
                    m_pps.resize(inSize-4);
                    memcpy(&m_pps[0], inBuffer+4, inSize-4);
                }
                frameTime = 0;
                flags |= FLV_FRAME_KEY;
                break;
            case 5:
                flags |= FLV_FRAME_KEY;
                
                break;
            default:
                flags |= FLV_FRAME_INTER;
                
                
                break;
                
        }
        
        if(output) {
            std::vector<uint8_t> conf;
            
            if(is_config && m_sps.size() > 0 && m_pps.size() > 0 ) {
                conf = configurationFromSpsAndPps();
                inSize = conf.size();
            }
            outBuffer.reserve(inSize + flags_size);
            
            put_byte(outBuffer, flags);
            put_byte(outBuffer, !is_config);
            put_be24(outBuffer, 0);
            
            if(is_config) {
                // create modified SPS/PPS buffer
                if(m_sps.size() > 0 && m_pps.size() > 0 && !m_sentConfig) {
                    put_buff(outBuffer, &conf[0], conf.size());
                    m_sentConfig = true;
                } else {
                    return;
                }
            } else {
                put_buff(outBuffer, inBuffer, inSize);
            }
            
            m_videoTs += frameTime;
            static auto prev_time = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            
            auto m_micros = std::chrono::duration_cast<std::chrono::microseconds>(now - prev_time).count();
            static uint64_t total = 0;
            static uint64_t count = 0;
            
            total+=m_micros;
            count++;
            
            prev_time = now;
            outMeta.setData(ts, static_cast<int>(outBuffer.size()), FLV_TAG_TYPE_VIDEO, kVideoChannelStreamId);
            
            output->pushBuffer(&outBuffer[0], outBuffer.size(), outMeta);
        }
        
    }
    std::vector<uint8_t>
    H264Packetizer::configurationFromSpsAndPps()
    {
        std::vector<uint8_t> conf;
        
        put_byte(conf, 1); // version
        put_byte(conf, m_sps[1]); // profile
        put_byte(conf, m_sps[2]); // compat
        put_byte(conf, m_sps[3]); // level
        put_byte(conf, 0xff);   // 6 bits reserved + 2 bits nal size length - 1 (11)
        put_byte(conf, 0xe1);   // 3 bits reserved + 5 bits number of sps (00001)
        put_be16(conf, m_sps.size());
        put_buff(conf, &m_sps[0], m_sps.size());
        put_byte(conf, 1);
        put_be16(conf, m_pps.size());
        put_buff(conf, &m_pps[0], m_pps.size());
        
        return conf;
        
    }
    
}
}
