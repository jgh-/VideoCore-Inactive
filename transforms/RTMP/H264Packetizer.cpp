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
#include <videocore/transforms/RTMP/H264Packetizer.h>
#include <videocore/rtmp/RTMPTypes.h>
#include <videocore/rtmp/RTMPSession.h>

namespace videocore { namespace rtmp {
    
    H264Packetizer::H264Packetizer() : m_videoTs(0), m_sentConfig(false)
    {
        
    }
    void
    H264Packetizer::setOutput(std::shared_ptr<IOutput> output)
    {
        m_output = output;
    }
    void H264Packetizer::pushBuffer(const uint8_t* const inBuffer, size_t inSize, IMetadata& inMetadata)
    {
        
      
      
        
        std::vector<uint8_t>& outBuffer = m_outbuffer;
        
        outBuffer.clear();
        
        uint8_t nal_type = inBuffer[4] & 0x1F;
        int flags = 0;
        const int flags_size = 5;
        const int ts = inMetadata.timestampDelta;

        bool is_config = (nal_type == 7 || nal_type == 8);
        
        
        flags = FLV_CODECID_H264;
        auto output = m_output.lock();
        RTMPMetadata_t outMeta(inMetadata.timestampDelta);

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
