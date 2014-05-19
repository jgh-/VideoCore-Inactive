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
#include <videocore/rtmp/RTMPSession.h>

#ifdef __APPLE__
#include <videocore/stream/Apple/StreamSession.h>
#endif

#include <boost/tokenizer.hpp>
#include <stdlib.h>
#include <algorithm>
#include <sstream>

namespace videocore
{
    RTMPSession::RTMPSession(std::string uri, RTMPSessionStateCallback_t callback)
    : m_streamOutRemainder(65536),m_streamInBuffer(new RingBuffer(4096)), m_uri(http::ParseHttpUrl(uri)), m_callback(callback),  m_previousTimestamp(0), m_currentChunkSize(128), m_streamId(0),  m_numberOfInvokes(0), m_state(kClientStateNone)
    {
#ifdef __APPLE__
        m_streamSession.reset(new Apple::StreamSession());
#endif
        boost::char_separator<char> sep("/");
        boost::tokenizer<boost::char_separator<char> > tokens(m_uri.path, sep );
        
        auto itr = tokens.begin();
        std::stringstream pp;
        {
            std::stringstream app;
            for (; itr != tokens.end() ; ++itr) {
                auto peek = itr;
                ++peek;
                if (peek == tokens.end()) {
                    pp << *itr;
                    break;
                }
                app << *itr <<  "/";
            }
            m_app = app.str();
            m_app.pop_back();
        }
        {
            if(m_uri.search.length() > 0) {
                pp << m_playPath << "?" << m_uri.search;
                m_playPath = pp.str();
            }
        }
        long port = (m_uri.port > 0) ? m_uri.port : 1935;
        
        m_jobQueue.set_name("com.videocore.rtmp");
        
        m_streamSession->connect(m_uri.host, static_cast<int>(port), [&](IStreamSession& session, StreamStatus_t status) {
            streamStatusChanged(status);
        });
        
    }
    RTMPSession::~RTMPSession()
    {
        if(m_state == kClientStateConnected) {
            sendDeleteStream();
        }
        m_jobQueue.enqueue_sync([]() {});
    }
    void
    RTMPSession::setSessionParameters(videocore::IMetadata &parameters)
    {
        
        RTMPSessionParameters_t& parms = dynamic_cast<RTMPSessionParameters_t&>(parameters);
        m_bitrate = parms.getData<kRTMPSessionParameterVideoBitrate>();
        m_frameDuration = parms.getData<kRTMPSessionParameterFrameDuration>();
        m_frameHeight = parms.getData<kRTMPSessionParameterHeight>();
        m_frameWidth = parms.getData<kRTMPSessionParameterWidth>();
        
    }
    void
    RTMPSession::pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata)
    {
        
        std::shared_ptr<Buffer> buf = std::make_shared<Buffer>(size);
        buf->put(const_cast<uint8_t*>(data), size);
        
        const RTMPMetadata_t inMetadata = static_cast<const RTMPMetadata_t&>(metadata);
        
        m_jobQueue.enqueue([&,buf,inMetadata]() {
            
            std::vector<uint8_t> chunk;
            std::vector<uint8_t> & outb = this->m_outBuffer;
            size_t len = buf->size();
            size_t tosend = std::min(len, m_currentChunkSize);
            uint8_t* p;
            buf->read(&p, buf->size());
            uint64_t ts = inMetadata.getData<kRTMPMetadataTimestamp>();
            
            //if(ts < m_previousTimestamp) { ts = m_previousTimestamp; };
            
            m_previousTimestamp = ts;
            const int streamId = inMetadata.getData<kRTMPMetadataMsgStreamId>();
            put_byte(chunk, ( streamId & 0x1F));
            put_be24(chunk, static_cast<uint32_t>(ts));
            put_be24(chunk, inMetadata.getData<kRTMPMetadataMsgLength>());
            put_byte(chunk, inMetadata.getData<kRTMPMetadataMsgTypeId>());
            put_buff(chunk, (uint8_t*)&m_streamId, sizeof(int32_t)); // msg stream id is little-endian
            put_buff(chunk, p, tosend);
            
            outb.insert(outb.end(), chunk.begin(), chunk.end());
            
            
            len -= tosend;
            p += tosend;
            
            while(len > 0) {
                tosend = std::min(len, m_currentChunkSize);
                p[-1] = RTMP_CHUNK_TYPE_3 | (inMetadata.getData<kRTMPMetadataMsgStreamId>() & 0x1F);
                
                outb.insert(outb.end(), p-1, p+tosend);
                p+=tosend;
                len-=tosend;
                if(outb.size() > 3072) {
                    
                    this->write(&outb[0], outb.size());
                    outb.clear();
                }
                
            }
            if(this->m_state != kClientStateConnected || outb.size() > 3072) {
                this->write(&outb[0], outb.size());
                outb.clear();
            }
            
        });
        
    }
    void
    RTMPSession::sendPacket(uint8_t* data, size_t size, RTMPChunk_0 metadata)
    {
        RTMPMetadata_t md(0.);
        
        md.setData(metadata.timestamp.data, metadata.msg_length.data, metadata.msg_type_id, metadata.msg_stream_id);
        
        pushBuffer(data, size, md);
    }
    void
    RTMPSession::write(uint8_t* data, size_t size)
    {
        if(size > 0) {
            std::shared_ptr<Buffer> buf = std::make_shared<Buffer>(size);
            buf->put(data, size);
            m_streamOutQueue.push(buf);
        }
        if((m_streamSession->status() & kStreamStatusWriteBufferHasSpace) && m_streamOutRemainder.size()) {
            
            uint8_t* buffer;
            size_t size = m_streamOutRemainder.size();
            
            m_streamOutRemainder.read(&buffer, size, false); // Read the entire buffer, but do not advance the read pointer.
            //printf("Remain %8zu\n ", size);
            size_t sent = m_streamSession->write(buffer, size);
            
            m_streamOutRemainder.read(&buffer, sent, true); // Advance the read pointer as far as we were able to send.
        }
        
        while((m_streamSession->status() & kStreamStatusWriteBufferHasSpace) && m_streamOutQueue.size() > 0) {
            //printf("StreamQueue: %zu\n", m_streamOutQueue.size());
            std::shared_ptr<Buffer> front = m_streamOutQueue.front();
            m_streamOutQueue.pop();
            uint8_t* buf;
            size_t size = front->size();
            front->read(&buf, size);
            size_t sent = m_streamSession->write(buf, size);
            
            if(sent < size) {
                m_streamOutRemainder.put(buf+sent, size-sent);
                break;
            }
        }
    }
    void
    RTMPSession::dataReceived()
    {
        
        static uint8_t buffer[4096] = {0};
        bool stop1 = false;
        bool stop2 = false;
        do {
            
            size_t maxlen = m_streamInBuffer->total() - m_streamInBuffer->size();
            size_t len = m_streamSession->read(buffer, maxlen);
            
            m_streamInBuffer->put(&buffer[0], len);
            
            while(m_streamInBuffer->size() > 0 && !stop1) {
                
                switch(m_state) {
                    case kClientStateHandshake1s0:
                    {
                        uint8_t s0 ;
                        m_streamInBuffer->get(&s0, 1);
                        
                        if(s0 == 0x03) {
                            setClientState(kClientStateHandshake1s1);
                        }
                    }
                        break;
                        
                    case kClientStateHandshake1s1:
                    {
                        if(m_streamInBuffer->size() >= kRTMPSignatureSize) {
                            
                            uint8_t* buf;
                            size_t size = m_streamInBuffer->read(&buf, kRTMPSignatureSize);
                            m_s1.resize(size);
                            m_s1.put(buf, size);
                            handshake();
                        } else {
                            stop1 = true;
                        }
                    }
                        break;
                    case kClientStateHandshake2:
                    {
                        if(m_streamInBuffer->size() >= kRTMPSignatureSize) {
                            uint8_t* buf;
                            m_streamInBuffer->read(&buf, kRTMPSignatureSize);
                            
                            setClientState(kClientStateInitializing0);
                            handshake();
                            sendConnectPacket();
                        } else {
                            stop1 = true;
                        }
                    }
                        break;
                    default:
                    {
                        if(!parseCurrentData()) {
                            // the buffer seems corrupted.
                            stop1 = stop2 = true;
                        }
                    }
                }
            }
            
        } while((m_streamSession->status() & kStreamStatusReadBufferHasBytes) && !stop2);
    }
    void
    RTMPSession::setClientState(ClientState_t state)
    {
        printf("RTMPStatus: %d\n", state);
        
        m_state = state;
        m_callback(*this, state);
    }
    void
    RTMPSession::streamStatusChanged(StreamStatus_t status)
    {
        if(status & kStreamStatusConnected && m_state < kClientStateConnected) {
            setClientState(kClientStateConnected);
        }
        if(status & kStreamStatusReadBufferHasBytes) {
            dataReceived();
        }
        if(status & kStreamStatusWriteBufferHasSpace) {
            if(m_state < kClientStateSessionStarted) {
                handshake();
            } else {
                m_jobQueue.enqueue([this]() {
                    this->write(nullptr, 0);
                });
            }
        }
        if(status & kStreamStatusEndStream) {
            setClientState(kClientStateNotConnected);
        }
        if(status & kStreamStatusErrorEncountered) {
            printf("kStreamStatusErrorEncountered\n");
            setClientState(kClientStateError);
        }
    }
    
    // RTMP
    
    void
    RTMPSession::handshake()
    {
        switch(m_state) {
            case kClientStateConnected:
                handshake0();
                break;
            case kClientStateHandshake0:
                handshake1();
                break;
            case kClientStateHandshake1s1:
                handshake2();
                break;
            default:
                m_c1.resize(0);
                m_s1.resize(0);
                break;
        }
    }
    void
    RTMPSession::handshake0()
    {
        char c0 = 0x03;
        
        setClientState(kClientStateHandshake0);
        
        write((uint8_t*)&c0, 1);
        
        handshake();
    }
    void
    RTMPSession::handshake1()
    {
        setClientState(kClientStateHandshake1s0);
        
        m_c1.resize(kRTMPSignatureSize);
        uint8_t* p;
        m_c1.read(&p, kRTMPSignatureSize);
        uint64_t zero = 0;
        m_c1.put((uint8_t*)&zero, sizeof(uint64_t));
        
        write(p, kRTMPSignatureSize);
        
    }
    void
    RTMPSession::handshake2()
    {
        setClientState(kClientStateHandshake2);
        
        write(m_s1(), m_s1.size());
    }
    
    void
    RTMPSession::sendConnectPacket()
    {
        RTMPChunk_0 metadata = {{0}};
        metadata.msg_stream_id = kControlChannelStreamId;
        metadata.msg_type_id = FLV_TAG_TYPE_INVOKE;
        std::vector<uint8_t> buff;
        std::stringstream url ;
        if(m_uri.port > 0) {
            url << m_uri.protocol << "://" << m_uri.host << ":" << m_uri.port << "/" << m_app;
        } else {
            url << m_uri.protocol << "://" << m_uri.host << "/" << m_app;
        }
        put_string(buff, "connect");
        put_double(buff, ++m_numberOfInvokes);
        put_byte(buff, kAMFObject);
        put_named_string(buff, "app", m_app.c_str());
        put_named_string(buff,"type", "nonprivate");
        put_named_string(buff, "tcUrl", url.str().c_str());
        put_named_bool(buff, "fpad", false);
        put_named_double(buff, "capabilities", 15.);
        put_named_double(buff, "audioCodecs", 10. );
        put_named_double(buff, "videoCodecs", 7.);
        put_named_double(buff, "videoFunction", 1.);
        put_be16(buff, 0);
        put_byte(buff, kAMFObjectEnd);
        
        metadata.msg_length.data = static_cast<int>( buff.size() );
        sendPacket(&buff[0], buff.size(), metadata);
    }
    void
    RTMPSession::sendReleaseStream()
    {
        RTMPChunk_0 metadata = {{0}};
        metadata.msg_stream_id = kControlChannelStreamId;
        metadata.msg_type_id = FLV_TAG_TYPE_INVOKE;
        std::vector<uint8_t> buff;
        put_string(buff, "releaseStream");
        put_double(buff, ++m_numberOfInvokes);
        put_byte(buff, kAMFNull);
        put_string(buff, m_playPath);
        metadata.msg_length.data = static_cast<int> (buff.size());
        
        sendPacket(&buff[0], buff.size(), metadata);
    }
    void
    RTMPSession::sendFCPublish()
    {
        RTMPChunk_0 metadata = {{0}};
        metadata.msg_stream_id = kControlChannelStreamId;
        metadata.msg_type_id = FLV_TAG_TYPE_INVOKE;
        std::vector<uint8_t> buff;
        put_string(buff, "FCPublish");
        put_double(buff, ++m_numberOfInvokes);
        put_byte(buff, kAMFNull);
        put_string(buff, m_playPath);
        metadata.msg_length.data = static_cast<int>( buff.size() );
        
        sendPacket(&buff[0], buff.size(), metadata);
    }
    void
    RTMPSession::sendCreateStream()
    {
        RTMPChunk_0 metadata = {{0}};
        metadata.msg_stream_id = kControlChannelStreamId;
        metadata.msg_type_id = FLV_TAG_TYPE_INVOKE;
        std::vector<uint8_t> buff;
        put_string(buff, "createStream");
        put_double(buff, ++m_numberOfInvokes);
        put_byte(buff, kAMFNull);
        metadata.msg_length.data = static_cast<int>( buff.size() );
        
        sendPacket(&buff[0], buff.size(), metadata);
    }
    void
    RTMPSession::sendPublish()
    {
        RTMPChunk_0 metadata = {{0}};
        metadata.msg_stream_id = kAudioChannelStreamId;
        metadata.msg_type_id = FLV_TAG_TYPE_INVOKE;
        std::vector<uint8_t> buff;
        std::vector<uint8_t> chunk;
        
        put_string(buff, "publish");
        put_double(buff, ++m_numberOfInvokes);
        put_byte(buff, kAMFNull);
        put_string(buff, m_playPath);
        put_string(buff, "live");
        metadata.msg_length.data = static_cast<int>( buff.size() );
        
        sendPacket(&buff[0], buff.size(), metadata);
    }
    void
    RTMPSession::sendHeaderPacket()
    {
        std::vector<uint8_t> outBuffer;
        
        std::vector<uint8_t> enc;
        RTMPChunk_0 metadata = {{0}};
        
        put_string(enc, "@setDataFrame");
        put_string(enc, "onMetaData");
        put_byte(enc, kAMFEMCAArray);
        put_be32(enc, 5+5+2); // videoEnabled + audioEnabled + 2
        
        put_named_double(enc, "duration", 0.0);
        put_named_double(enc, "width", m_frameWidth);
        put_named_double(enc, "height", m_frameHeight);
        put_named_double(enc, "videodatarate", static_cast<double>(m_bitrate) / 1024.);
        put_named_double(enc, "framerate", m_frameDuration);
        put_named_double(enc, "videocodecid", 7.);
        
        
        put_named_double(enc, "audiodatarate", 128.);
        put_named_double(enc, "audiosamplerate", 44100.);
        put_named_double(enc, "audiosamplesize", 16);
        put_named_bool(enc, "stereo", true);
        put_named_double(enc, "audiocodecid", 10.);
        
        
        put_named_double(enc, "filesize", 0.);
        put_be16(enc, 0);
        put_byte(enc, kAMFObjectEnd);
        size_t len = enc.size();
        
        
        put_buff(outBuffer, (uint8_t*)&enc[0], static_cast<size_t>(len));
        
        
        metadata.msg_type_id = FLV_TAG_TYPE_META;
        metadata.msg_stream_id = kAudioChannelStreamId;
        metadata.msg_length.data = static_cast<int>( outBuffer.size() );
        metadata.timestamp.data = 0;
        
        sendPacket(&outBuffer[0], outBuffer.size(), metadata);
        
    }
    void
    RTMPSession::sendDeleteStream()
    {
        RTMPChunk_0 metadata = {{0}};
        metadata.msg_stream_id = kControlChannelStreamId;
        metadata.msg_type_id = FLV_TAG_TYPE_INVOKE;
        std::vector<uint8_t> buff;
        put_string(buff, "deleteStream");
        put_double(buff, ++m_numberOfInvokes);
        put_byte(buff, kAMFNull);
        put_double(buff, m_streamId);
        
        metadata.msg_length.data = static_cast<int>( buff.size() );
        
        sendPacket(&buff[0], buff.size(), metadata);
        
    }
    bool
    RTMPSession::parseCurrentData()
    {
        uint8_t* p, *start ;
        m_streamInBuffer->read(&p, m_streamInBuffer->size(), false);
        start = p;
        
        if(!p) return false;
        
        int header_type = (p[0] & 0xC0) >> 6;
        p++;
        switch(header_type) {
            case 0:
            {
                RTMPChunk_0 chunk;
                memcpy(&chunk, p, sizeof(RTMPChunk_0));
                chunk.msg_length.data = get_be24((uint8_t*)&chunk.msg_length);
                
                p+=sizeof(chunk);
                
                switch(chunk.msg_type_id) {
                    case 1:
                    {
                        // printf("Current chunk size %zu ->", m_currentChunkSize );
                        // m_currentChunkSize = get_be32(p);
                        // printf(" %zu\n", m_currentChunkSize);
                    }
                        break;
                    case FLV_TAG_TYPE_INVOKE:
                    {
                        if(m_state == kClientStateInitializing0) {
                            sendReleaseStream();
                            sendFCPublish();
                            sendCreateStream();
                            setClientState(kClientStateInitializing1);
                            
                        } else if(m_state == kClientStateInitializing1) {
                            // send publish
                            uint8_t* start = p;
                            
                            std::string result = get_string(p);
                            if(result.length() < 0xFFFF) {
                                p+=3+result.length();
                            } else {
                                p+=5+result.length();
                            }
                            if(result == "_result") {
                                p+=11;
                                
                                double streamId = get_double(p);
                                m_streamId = streamId;
                                sendPublish();
                                setClientState(kClientStateInitializing2);
                            }
                            p = start;
                            
                        } else if(m_state == kClientStateInitializing2) {
                            sendHeaderPacket();
                            setClientState(kClientStateSessionStarted);
                            
                        }
                    }
                        break;
                }
                
                p+=chunk.msg_length.data;
            }
                break;
            case 1:
            {
                RTMPChunk_1 chunk;
                memcpy(&chunk, p, sizeof(RTMPChunk_1));
                p+=sizeof(chunk)+m_currentChunkSize;
            }
                break;
            case 2:
            {
                RTMPChunk_2 chunk;
                memcpy(&chunk, p, sizeof(RTMPChunk_2));
                p+=sizeof(chunk)+m_currentChunkSize;
            }
                break;
            case 3:
            {
                p+=m_currentChunkSize;
            }
                break;
            default:
                return false;
        }
        m_streamInBuffer->read(&p, p-start);
        return true;
        
    }
}
