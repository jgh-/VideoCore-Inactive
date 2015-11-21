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
#include <videocore/rtmp/RTMPSession.h>

#ifdef __APPLE__
#include <videocore/stream/Apple/StreamSession.h>
#endif

#ifndef DLOG_LEVEL_DEF
#define DLOG_LEVEL_DEF DLOG_LEVEL_VERBOSE
#endif
#include <videocore/system/Logger.hpp>

#include <boost/tokenizer.hpp>
#include <stdlib.h>
#include <algorithm>
#include <sstream>

namespace videocore
{
    static const size_t kMaxSendbufferSize = 10 * 1024 * 1024; // 10 MB
    
    RTMPSession::RTMPSession(std::string uri, RTMPSessionStateCallback callback)
    : m_streamOutRemainder(65536)
    , m_streamInBuffer(new PreallocBuffer(4096))
    , m_callback(callback)
    , m_bandwidthCallback(nullptr)
    , m_outChunkSize(128)
    , m_inChunkSize(128)
    , m_bufferSize(0)
    , m_streamId(0)
    , m_numberOfInvokes(0)
    , m_state(kClientStateNone)
    , m_ending(false)
    , m_jobQueue("com.videocore.rtmp")
    , m_networkQueue("com.videocore.rtmp.network")
    , m_previousTs(0)
    , m_clearing(false)
    {
        m_previousChunk.msg_length.data = 0;
        m_previousChunk.msg_stream_id = 0;
        m_previousChunk.msg_type_id = 0;
#ifdef __APPLE__
        m_streamSession.reset(new Apple::StreamSession());
        m_networkWaitSemaphore = dispatch_semaphore_create(0);
#endif
        boost::char_separator<char> sep("/");
        boost::tokenizer<boost::char_separator<char>> uri_tokens(uri, sep);
        
        // http::ParseHttpUrl is destructive to the parameter passed in.
        std::string uri_cpy(uri);
        m_uri = http::ParseHttpUrl(uri_cpy);
        boost::tokenizer<boost::char_separator<char> > tokens(m_uri.path, sep );
        
        
        int tokenCount = 0;
        std::stringstream pp;
        for ( auto it = uri_tokens.begin() ; it != uri_tokens.end() ; ++it) {
            if(tokenCount++ < 2) { // skip protocol and host/port
                continue;
            }
            if(tokenCount == 3) {
                m_app = *it;
            } else {
                pp << *it << "/";
            }
        }
        m_playPath = pp.str();
        m_playPath.pop_back();
        
        connectServer();
    }
    RTMPSession::~RTMPSession()
    {
        DLog("~RTMPSession");
        if(m_state == kClientStateConnected) {
            sendDeleteStream();
        }
        
        m_ending = true;
        m_jobQueue.mark_exiting();
        m_jobQueue.enqueue_sync([]() {});
        m_networkQueue.mark_exiting();
        m_networkQueue.enqueue_sync([]() {});
#ifdef __APPLE__
        dispatch_release(m_networkWaitSemaphore);
#endif
    }
    void
    RTMPSession::connectServer() {
        // reset the stream buffer.
        m_streamInBuffer->reset();
        int port = (m_uri.port > 0) ? m_uri.port : 1935;
        DLog("Connecting:%s:%d, stream name:%s\n", m_uri.host.c_str(), port, m_playPath.c_str());
        m_streamSession->connect(m_uri.host, port, [&](IStreamSession& session, StreamStatus_T status) {
            streamStatusChanged(status);
        });
    }
    void
    RTMPSession::setSessionParameters(videocore::IMetadata &parameters)
    {
        
        RTMPSessionParameters_t& parms = dynamic_cast<RTMPSessionParameters_t&>(parameters);
        m_bitrate = parms.getData<kRTMPSessionParameterVideoBitrate>();
        m_frameDuration = parms.getData<kRTMPSessionParameterFrameDuration>();
        m_frameHeight = parms.getData<kRTMPSessionParameterHeight>();
        m_frameWidth = parms.getData<kRTMPSessionParameterWidth>();
        m_audioSampleRate = parms.getData<kRTMPSessionParameterAudioFrequency>();
        m_audioStereo = parms.getData<kRTMPSessionParameterStereo>();
    }
    void
    RTMPSession::setBandwidthCallback(BandwidthCallback callback)
    {
        m_bandwidthCallback = callback;
        m_throughputSession.setThroughputCallback(callback);
    }
    void
    RTMPSession::pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata)
    {
        if(m_ending) {
            return ;
        }
        
        // make the lamdba capture the data
        std::shared_ptr<Buffer> buf = std::make_shared<Buffer>(size);
        buf->put(const_cast<uint8_t*>(data), size);
        
        const RTMPMetadata_t inMetadata = static_cast<const RTMPMetadata_t&>(metadata);
        
        m_jobQueue.enqueue([=]() {
            if(!this->m_ending) {
                static int c_count = 0;
                c_count ++;
                
                auto packetTime = std::chrono::steady_clock::now();
                
                std::vector<uint8_t> chunk;
                chunk.reserve(size+64);
                size_t len = buf->size();
                size_t tosend = std::min(len, m_outChunkSize);
                uint8_t* p;
                buf->read(&p, buf->size());
                uint64_t ts = inMetadata.getData<kRTMPMetadataTimestamp>() ;
                const int streamId = inMetadata.getData<kRTMPMetadataMsgStreamId>();
                
#ifndef RTMP_CHUNK_TYPE_0_ONLY
                auto it = m_previousChunkData.find(streamId);
                if(it == m_previousChunkData.end()) {
#endif
                    // Type 0.
                    put_byte(chunk, ( streamId & 0x1F));
                    put_be24(chunk, static_cast<uint32_t>(ts));
                    put_be24(chunk, inMetadata.getData<kRTMPMetadataMsgLength>());
                    put_byte(chunk, inMetadata.getData<kRTMPMetadataMsgTypeId>());
                    put_buff(chunk, (uint8_t*)&m_streamId, sizeof(int32_t)); // msg stream id is little-endian
#ifndef RTMP_CHUNK_TYPE_0_ONLY
                } else {
                    // Type 1.
                    put_byte(chunk, RTMP_CHUNK_TYPE_1 | (streamId & 0x1F));
                    put_be24(chunk, static_cast<uint32_t>(ts - it->second)); // timestamp delta
                    put_be24(chunk, inMetadata.getData<kRTMPMetadataMsgLength>());
                    put_byte(chunk, inMetadata.getData<kRTMPMetadataMsgTypeId>());
                }
#endif
                m_previousChunkData[streamId] = ts;
                put_buff(chunk, p, tosend);
                
                len -= tosend;
                p += tosend;
                
                while(len > 0) {
                    tosend = std::min(len, m_outChunkSize);
                    p[-1] = RTMP_CHUNK_TYPE_3 | (streamId & 0x1F);
                    
                    put_buff(chunk, p-1, tosend+1);
                    p+=tosend;
                    len-=tosend;
                }
                this->write(&chunk[0], chunk.size(), packetTime, inMetadata.getData<kRTMPMetadataIsKeyframe>() );
            }
        });
    }
    void
    RTMPSession::sendPacket(uint8_t* data, size_t size, RTMPChunk_0 metadata)
    {
        RTMPMetadata_t md(0.);
        
        md.setData(metadata.timestamp.data, metadata.msg_length.data, metadata.msg_type_id, metadata.msg_stream_id, false);
        
        pushBuffer(data, size, md);
    }
    void
    RTMPSession::increaseBuffer(int64_t size) {
        m_bufferSize = std::max(m_bufferSize + size, 0LL);
    }
    void
    RTMPSession::write(uint8_t* data, size_t size, std::chrono::steady_clock::time_point packetTime, bool isKeyframe)
    {
        if(size > 0) {
            std::shared_ptr<Buffer> buf = std::make_shared<Buffer>(size);
            buf->put(data, size);
            
            m_throughputSession.addBufferSizeSample(m_bufferSize);
            
            increaseBuffer(size);
            if(isKeyframe) {
                m_sentKeyframe = packetTime;
            }
            if(m_bufferSize > kMaxSendbufferSize && isKeyframe) {
                m_clearing = true;
            }
            m_networkQueue.enqueue([=]() {
                size_t tosend = size;
                uint8_t* p ;
                buf->read(&p, size);
                
                while(tosend > 0 && !this->m_ending && (!this->m_clearing || this->m_sentKeyframe == packetTime)) {
                    this->m_clearing = false;
                    size_t sent = m_streamSession->write(p, tosend);
                    p += sent;
                    tosend -= sent;
                    this->m_throughputSession.addSentBytesSample(sent);
                    if( sent == 0 ) {
#ifdef __APPLE__
                        dispatch_semaphore_wait(m_networkWaitSemaphore, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)));
#else
                        std::unique_lock<std::mutex> l(m_networkMutex);
                        m_networkCond.wait_until(l, std::chrono::steady_clock::now() + std::chrono::milliseconds(1000));
                        
                        l.unlock();
#endif
                    }
                }
                this->increaseBuffer(-int64_t(size));
            });
        }
        
    }
    void
    RTMPSession::dataReceived()
    {
        bool stop1 = false;
        bool stop2 = false;
        while ((m_streamSession->status() & kStreamStatusReadBufferHasBytes) && !stop2) {
            size_t maxlen = m_streamInBuffer->availableSpace();
            if (maxlen > 0) {
                ssize_t len = m_streamSession->read(m_streamInBuffer->writeBuffer(), maxlen);
                DLogVerbose("Want read:%zd, read:%zd\n", maxlen, len);
                
                if (len <= 0) {
                    DLogError("Read from stream error:%ld\n", len);
                    stop2 = true;
                    break;
                }
                m_streamInBuffer->didWrite(len);
            }
            else {
                DLogDebug("Stream in buffer full\n");
            }
            
            while(m_streamInBuffer->availableBytes() > 0 && !stop1) {
                switch(m_state) {
                    case kClientStateHandshake1s0:
                    {
                        uint8_t s0;
                        memcpy(&s0, m_streamInBuffer->readBuffer(), 1);
                        if(s0 == 0x03) {
                            setClientState(kClientStateHandshake1s1);
                            m_streamInBuffer->didRead(1);
                        }
                        else {
                            DLogError("Want s0, but not:0x%X\n", static_cast<int>(s0));
                            // do remove data from buffer??
                            stop1 = true;
                        }
                    }
                        break;
                        
                    case kClientStateHandshake1s1:
                    {
                        if(m_streamInBuffer->availableBytes() >= kRTMPSignatureSize) {
                            uint8_t buf[kRTMPSignatureSize];
                            memcpy(buf, m_streamInBuffer->readBuffer(), kRTMPSignatureSize);
                            m_streamInBuffer->didRead(kRTMPSignatureSize);
                            m_s1.resize(kRTMPSignatureSize);
                            m_s1.put(buf, kRTMPSignatureSize);
                            handshake();
                        }
                        else {
                            DLogDebug("Not enough s1 size\n");
                            stop1 = true;
                        }
                    }
                        break;
                    case kClientStateHandshake2:
                    {
                        if(m_streamInBuffer->availableBytes() >= kRTMPSignatureSize) {
                            // we don't care about s2 data, so did read directly
                            m_streamInBuffer->didRead(kRTMPSignatureSize);
                            setClientState(kClientStateHandshakeComplete);
                            handshake();
                            sendConnectPacket();
                        }
                        else {
                            DLogDebug("Not enough s2 size\n");
                            stop1 = true;
                        }
                    }
                        break;
                    default:
                    {
                        if(!parseCurrentData()) {
                            m_streamInBuffer->dumpInfo();
                            stop1 = true;
                        }
                    }
                }
            }
        };
    }
    void
    RTMPSession::setClientState(ClientState_t state)
    {
        m_state = state;
        m_callback(*this, state);
    }
    void
    RTMPSession::streamStatusChanged(StreamStatus_T status)
    {
        if(status & kStreamStatusConnected && m_state < kClientStateConnected) {
            setClientState(kClientStateConnected);
        }
        if(status & kStreamStatusReadBufferHasBytes) {
            dataReceived();
        }
        if(status & kStreamStatusWriteBufferHasSpace) {
            if(m_state < kClientStateHandshakeComplete) {
                handshake();
            } else {
                
#ifdef __APPLE__
                dispatch_semaphore_signal(m_networkWaitSemaphore);
#else
                m_networkMutex.unlock();
                m_networkCond.notify_one();
#endif
            }
        }
        if(status & kStreamStatusEndStream) {
            setClientState(kClientStateNotConnected);
        }
        if(status & kStreamStatusErrorEncountered) {
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
        uint8_t* p;
        m_s1.read(&p, kRTMPSignatureSize);
        p += 4;
        uint32_t zero = 0;
        memcpy(p, &zero, sizeof(uint32_t));
        
        write(m_s1(), m_s1.size());
    }
    
    void
    RTMPSession::sendConnectPacket()
    {
        RTMPChunk_0 metadata = {{0}};
        metadata.msg_stream_id = kControlChannelStreamId;
        metadata.msg_type_id = RTMP_PT_INVOKE;
        std::vector<uint8_t> buff;
        std::stringstream url ;
        if(m_uri.port > 0) {
            url << m_uri.protocol << "://" << m_uri.host << ":" << m_uri.port << "/" << m_app;
        } else {
            url << m_uri.protocol << "://" << m_uri.host << "/" << m_app;
        }
        put_string(buff, "connect");
        put_double(buff, trackCommand("connect"));
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
        metadata.msg_type_id = RTMP_PT_NOTIFY;
        std::vector<uint8_t> buff;
        put_string(buff, "releaseStream");
        put_double(buff, trackCommand("releaseStream"));
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
        metadata.msg_type_id = RTMP_PT_NOTIFY;
        std::vector<uint8_t> buff;
        put_string(buff, "FCPublish");
        put_double(buff, trackCommand("FCPublish"));
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
        metadata.msg_type_id = RTMP_PT_INVOKE;
        std::vector<uint8_t> buff;
        put_string(buff, "createStream");
        put_double(buff, trackCommand("createStream"));
        put_byte(buff, kAMFNull);
        metadata.msg_length.data = static_cast<int>( buff.size() );
        
        sendPacket(&buff[0], buff.size(), metadata);
    }
    void
    RTMPSession::sendPublish()
    {
        RTMPChunk_0 metadata = {{0}};
        metadata.msg_stream_id = kAudioChannelStreamId;
        metadata.msg_type_id = RTMP_PT_INVOKE;
        std::vector<uint8_t> buff;
        std::vector<uint8_t> chunk;
        
        put_string(buff, "publish");
        put_double(buff, trackCommand("publish"));
        put_byte(buff, kAMFNull);
        put_string(buff, m_playPath);
        put_string(buff, "live");
        metadata.msg_length.data = static_cast<int>( buff.size() );
        
        sendPacket(&buff[0], buff.size(), metadata);
    }
    
    void
    RTMPSession::sendHeaderPacket()
    {
        DLog("send header packet\n");
        
        std::vector<uint8_t> enc;
        RTMPChunk_0 metadata = {{0}};
        
        put_string(enc, "@setDataFrame");
        put_string(enc, "onMetaData");
        put_byte(enc, kAMFObject);
        //put_be32(enc, 5+5+2); // videoEnabled + audioEnabled + 2
        
        //put_named_double(enc, "duration", 0.0);
        put_named_double(enc, "width", m_frameWidth);
        put_named_double(enc, "height", m_frameHeight);
        put_named_double(enc, "displaywidth", m_frameWidth);
        put_named_double(enc, "displayheight", m_frameHeight);
        put_named_double(enc, "framewidth", m_frameWidth);
        put_named_double(enc, "frameheight", m_frameHeight);
        put_named_double(enc, "videodatarate", static_cast<double>(m_bitrate) / 1024.);
        put_named_double(enc, "videoframerate", 1. / m_frameDuration);
        
        put_named_string(enc, "videocodecid", "avc1");
        {
            put_name(enc, "trackinfo");
            put_byte(enc, kAMFStrictArray);
            put_be32(enc, 2);
            
            //
            // Audio stream metadata
            put_byte(enc, kAMFObject);
            put_named_string(enc, "type", "audio");
            {
                std::stringstream ss;
                ss << "{AACFrame: codec:AAC, channels: " << m_audioStereo+1 << ", frequency:" << m_audioSampleRate << ", samplesPerFrame:1024, objectType:LC}";
                put_named_string(enc, "description", ss.str());
            }
            put_named_double(enc, "timescale", 1000.);
            
            put_name(enc, "sampledescription");
            put_byte(enc, kAMFStrictArray);
            put_be32(enc, 1);
            put_byte(enc, kAMFObject);
            put_named_string(enc, "sampletype", "mpeg4-generic");
            put_byte(enc, 0);
            put_byte(enc, 0);
            put_byte(enc, kAMFObjectEnd);
            
            put_named_string(enc, "language", "eng");
            
            put_byte(enc, 0);
            put_byte(enc, 0);
            put_byte(enc, kAMFObjectEnd);
            
            //
            // Video stream metadata
            
            put_byte(enc, kAMFObject);
            put_named_string(enc, "type", "video");
            put_named_double(enc, "timescale", 1000.);
            put_named_string(enc, "language", "eng");
            put_name(enc, "sampledescription");
            put_byte(enc, kAMFStrictArray);
            put_be32(enc, 1);
            put_byte(enc, kAMFObject);
            put_named_string(enc, "sampletype", "H264");
            put_byte(enc, 0);
            put_byte(enc, 0);
            put_byte(enc, kAMFObjectEnd);
            put_byte(enc, 0);
            put_byte(enc, 0);
            put_byte(enc, kAMFObjectEnd);
        }
        put_be16(enc, 0);
        put_byte(enc, kAMFObjectEnd);
        put_named_double(enc, "audiodatarate", 131152. / 1024.);
        put_named_double(enc, "audiosamplerate", m_audioSampleRate);
        put_named_double(enc, "audiosamplesize", 16);
        put_named_double(enc, "audiochannels", m_audioStereo + 1);
        put_named_string(enc, "audiocodecid", "mp4a");
        
        put_be16(enc, 0);
        put_byte(enc, kAMFObjectEnd);
        size_t len = enc.size();
        
        
        //        put_buff(outBuffer, (uint8_t*)&enc[0], static_cast<size_t>(len));
        
        
        metadata.msg_type_id = FLV_TAG_TYPE_META;
        metadata.msg_stream_id = kAudioChannelStreamId;
        //        metadata.msg_length.data = static_cast<int>( outBuffer.size() );
        metadata.msg_length.data = static_cast<int>( len );
        metadata.timestamp.data = 0;
        
        sendPacket(&enc[0], len, metadata);
        
        //        sendPacket(&outBuffer[0], outBuffer.size(), metadata);
    }
    void
    RTMPSession::sendDeleteStream()
    {
        DLog("send delete stream\n");
        
        RTMPChunk_0 metadata = {{0}};
        metadata.msg_stream_id = kControlChannelStreamId;
        metadata.msg_type_id = RTMP_PT_INVOKE;
        std::vector<uint8_t> buff;
        put_string(buff, "deleteStream");
        put_double(buff, ++m_numberOfInvokes);
        m_trackedCommands[m_numberOfInvokes] = "deleteStream";
        put_byte(buff, kAMFNull);
        put_double(buff, m_streamId);
        
        metadata.msg_length.data = static_cast<int>( buff.size() );
        
        sendPacket(&buff[0], buff.size(), metadata);
        
    }
    void
    RTMPSession::sendSetChunkSize(int32_t chunkSize)
    {
        m_jobQueue.enqueue([&, chunkSize] {
            DLog("send set chunk size:%d\n", chunkSize);
            int streamId = 0;
            
            std::vector<uint8_t> buff;
            
            put_byte(buff, 2); // chunk stream ID 2
            put_be24(buff, 0); // ts
            put_be24(buff, 4); // size (4 bytes)
            put_byte(buff, RTMP_PT_CHUNK_SIZE); // chunk type
            
            put_buff(buff, (uint8_t*)&streamId, sizeof(int32_t)); // msg stream id is little-endian
            
            put_be32(buff, chunkSize);
            
            write(&buff[0], buff.size());
            
            m_outChunkSize = chunkSize;
        });
        
    }
    void
    RTMPSession::sendPong()
    {
        m_jobQueue.enqueue([&] {
            DLog("send pong\n")
            
            int streamId = 0;
            
            std::vector<uint8_t> buff;
            
            put_byte(buff, 2); // chunk stream ID 2
            put_be24(buff, 0); // ts
            put_be24(buff, 6); // size (6 bytes)
            put_byte(buff, RTMP_PT_PING); // chunk type
            
            put_buff(buff, (uint8_t*)&streamId, sizeof(int32_t)); // msg stream id is little-endian
            put_be16(buff, 7);
            put_be16(buff, 0);
            put_be16(buff, 0);
            
            write(&buff[0], buff.size());
        });
    }
    void
    RTMPSession::sendSetBufferTime(int milliseconds)
    {
        m_jobQueue.enqueue([=]{
            DLog("send ping\n")
            
            int streamId = 0;
            
            std::vector<uint8_t> buff;
            
            put_byte(buff, 2);
            put_be24(buff, 0);
            put_be24(buff, 10);
            put_byte(buff, RTMP_PT_PING);
            put_buff(buff, (uint8_t*)&streamId, sizeof(int32_t));
            
            put_be16(buff, 3); // SetBufferTime
            put_be32(buff, m_streamId);
            put_be32(buff, milliseconds);
            
            write(&buff[0], buff.size());
        });
    }    bool
    RTMPSession::handleMessage(uint8_t *p, uint8_t msgTypeId)
    {
        bool ret = true;
        DLogDebug("Handle message:%d\n", (int)msgTypeId);
        switch(msgTypeId) {
            case RTMP_PT_BYTES_READ:
            {
                //DLog("received bytes read: %d\n", get_be32(p));
            }
                break;
                
            case RTMP_PT_CHUNK_SIZE:
            {
                unsigned long newChunkSize = get_be32(p);
                DLog("Request to change incoming chunk size from %zu -> %zu\n", m_inChunkSize, newChunkSize);
                m_inChunkSize = newChunkSize;
            }
                break;
                
            case RTMP_PT_PING:
            {
                DLog("Received ping, sending pong.\n");
                sendPong();
            }
                break;
                
            case RTMP_PT_SERVER_WINDOW:
            {
                DLog("Received server window size: %d\n", get_be32(p));
            }
                break;
                
            case RTMP_PT_PEER_BW:
            {
                DLog("Received peer bandwidth limit: %d type: %d\n", get_be32(p), p[4]);
            }
                break;
                
            case RTMP_PT_INVOKE:
            {
                DLog("Received invoke\n");
                handleInvoke(p);
            }
                break;
            case RTMP_PT_VIDEO:
            {
                DLog("Received video\n");
            }
                break;
                
            case RTMP_PT_AUDIO:
            {
                DLog("Received audio\n");
            }
                break;
                
            case RTMP_PT_METADATA:
            {
                DLog("Received metadata\n");
            }
                break;
                
            case RTMP_PT_NOTIFY:
            {
                DLog("Received notify\n");
            }
                break;
                
            default:
            {
                DLog("Received unknown packet type: 0x%02X\n", msgTypeId);
                ret = false;
            }
                break;
        }
        return ret;
    }
    
    int  RTMPSession::tryReadOneMessage(uint8_t *msg, int msgsize, int from_offset){
        int full_msg_length = msgsize;
        if (msgsize > m_inChunkSize) {
            // multiple chunk
            int remain = msgsize;
            while (remain > m_inChunkSize) {
                remain -= m_inChunkSize;
                full_msg_length++; // addn the chunk seperator 0xC?(0xC3 specially) count.
            }
        }
        
        // because we do not confirm the header length, so check with header length.
        if (m_streamInBuffer->availableBytes() >= from_offset + full_msg_length) {
            int msg_offset = 0;             // where to write
            int buf_offset = from_offset;   // where read for write
            int remain = msgsize;
            while (remain > m_inChunkSize) {
                memcpy(msg+msg_offset, m_streamInBuffer->readBuffer()+buf_offset, m_inChunkSize);
                msg_offset += m_inChunkSize;
                buf_offset += m_inChunkSize+1;
                remain -= m_inChunkSize;
            }
            if (remain > 0) {
                memcpy(msg+msg_offset, m_streamInBuffer->readBuffer()+buf_offset, remain);
            }
            
            return full_msg_length;
        }
        return -1;
    }
    
    // Parse only one message every time, loop in the caller
    // If data not enough for one message, return false, else return true;
    bool
    RTMPSession::parseCurrentData()
    {
        //        Logger::dumpBuffer("dataReceived", m_streamInBuffer->readBuffer(), m_streamInBuffer->availableBytes());
        DLogVerbose("Steam in buffer size:%zd\n", m_streamInBuffer->availableBytes());
        if (m_streamInBuffer->availableBytes() <= 0) {
            DLogDebug("No data in buffer\n");
            return false;
        }
        
        uint8_t first_byte;
        // at least one byte in current buffer.
        memcpy(&first_byte, m_streamInBuffer->readBuffer(), 1);
        int header_type = (first_byte & 0xC0) >> 6;
        DLogVerbose("First byte:0x%X, header type:%d\n", (int)first_byte, header_type);
        switch(header_type) {
            case RTMP_HEADER_TYPE_FULL:
            {
                RTMPChunk_0 chunk;
                // at least a full header bytes in current buffer
                if (m_streamInBuffer->availableBytes() >= 1+sizeof(RTMPChunk_0)) {
                    memcpy(&chunk, m_streamInBuffer->readBuffer()+1, sizeof(RTMPChunk_0));
                    chunk.msg_length.data = get_be24((uint8_t*)&chunk.msg_length);
                    if (chunk.msg_length.data < 0) {
                        DLogDebug("ERROR: Invalid header length\n");
                        Logger::dumpBuffer("RTMPChunk_0 ERROR", m_streamInBuffer->readBuffer(), m_streamInBuffer->availableBytes());
                        // FIXME: Clear the stream in buffer ?
                        return false;
                    }
                    if(chunk.msg_length.data > 65535) {
                        DLogDebug("Length too large ???:%d\n", chunk.msg_length.data);
                    }
                    std::vector<uint8_t> msg(chunk.msg_length.data);
                    int  full_msgsize = tryReadOneMessage(&msg[0], chunk.msg_length.data, 1+sizeof(RTMPChunk_0));
                    if (full_msgsize > 0) {
                        m_streamInBuffer->didRead(1+sizeof(RTMPChunk_0) + full_msgsize);
                        
                        handleMessage(&msg[0], chunk.msg_type_id);
                        m_previousChunk = chunk;
                        return true;
                    }
                    else {
                        DLogDebug("Not enough one message in buffer\n");
                        return false;
                    }
                }
                else {
                    DLogDebug("Not enough a header\n");
                    // DEBUG only
                    Logger::dumpBuffer("RTMPChunk_0", m_streamInBuffer->readBuffer(), m_streamInBuffer->availableBytes());
                    return false;
                }
            }
                break;
                
            case RTMP_HEADER_TYPE_NO_MSG_STREAM_ID:
            {
                RTMPChunk_1 chunk;
                if (m_streamInBuffer->availableBytes() >= 1+sizeof(RTMPChunk_1)) {
                    memcpy(&chunk, m_streamInBuffer->readBuffer()+1, sizeof(RTMPChunk_1));
                    chunk.msg_length.data = get_be24((uint8_t*)&chunk.msg_length);
                    
                    if (chunk.msg_length.data < 0) {
                        DLogDebug("ERROR: Invalid header length");
                        Logger::dumpBuffer("RTMPChunk_1 ERROR", m_streamInBuffer->readBuffer(), m_streamInBuffer->availableBytes());
                        // FIXME: Clear the stream in buffer ?
                        return false;
                    }
                    
                    if(chunk.msg_length.data > 65535) {
                        DLogDebug("Length too large ???:%d\n", chunk.msg_length.data);
                    }
                    
                    std::vector<uint8_t> msg(chunk.msg_length.data);
                    int full_msgsize = tryReadOneMessage(&msg[0], chunk.msg_length.data, 1+sizeof(RTMPChunk_1));
                    if (full_msgsize > 0) {
                        m_streamInBuffer->didRead(1+sizeof(RTMPChunk_1) + full_msgsize);
                        
                        handleMessage(&msg[0], chunk.msg_type_id);
                        
                        m_previousChunk.msg_type_id = chunk.msg_type_id;
                        m_previousChunk.msg_length = chunk.msg_length;
                        return true;
                    }
                    else {
                        DLogDebug("Not enough one message in buffer\n");
                        return false;
                    }
                }
                else {
                    DLogDebug("Not enough a header\n");
                    // DEBUG only
                    Logger::dumpBuffer("RTMPChunk_1", m_streamInBuffer->readBuffer(), m_streamInBuffer->availableBytes());
                    return false;
                }
            }
                break;
                
            case RTMP_HEADER_TYPE_TIMESTAMP:
            {
                // the message length is the same as previous message.
                DLogDebug("Previous chunk length:%d, msgid:%d, streamid:%d\n", m_previousChunk.msg_length.data, m_previousChunk.msg_type_id, m_previousChunk.msg_stream_id);
                RTMPChunk_2 chunk;
                if (m_streamInBuffer->availableBytes() >= 1+sizeof(RTMPChunk_2)) {
                    memcpy(&chunk, m_streamInBuffer->readBuffer()+1, sizeof(RTMPChunk_2));
                    std::vector<uint8_t> msg(m_previousChunk.msg_length.data);
                    int full_msgsize = tryReadOneMessage(&msg[0], m_previousChunk.msg_length.data, 1+sizeof(RTMPChunk_2));
                    if (full_msgsize > 0) {
                        m_streamInBuffer->didRead(1+sizeof(RTMPChunk_2) + full_msgsize);
                        handleMessage(&msg[0], m_previousChunk.msg_type_id);
                        return true;
                    }
                    else {
                        DLogDebug("Not enough one message in buffer\n");
                        return false;
                    }
                }
                else {
                    DLogDebug("Not enough a header\n");
                    // DEBUG only
                    Logger::dumpBuffer("RTMPChunk_2", m_streamInBuffer->readBuffer(), m_streamInBuffer->availableBytes());
                    return false;
                }
            }
                break;
                
            case RTMP_HEADER_TYPE_ONLY:
            {
                m_streamInBuffer->didRead(1);
                return true;
            }
                break;
                
            default:
                DLogError("Invalid header type:%d\n", header_type);
                // FIXME: Maybe we shoult close the connection and reopen it
                m_networkQueue.enqueue([=]{
                    connectServer();
                });
                return false;
        }
        return false;
    }
    
    void
    RTMPSession::handleInvoke(uint8_t* p)
    {
        int buflen=0;
        std::string command = get_string(p, buflen);
        
        DLog("Received invoke %s\n", command.c_str());
        
        if (command == "_result") {
            int32_t pktId = int32_t(get_double(p+11));
            // 找回result对应的command
            std::string trackedCommand;
            auto it = m_trackedCommands.find(pktId) ;
            
            if(it != m_trackedCommands.end()) {
                trackedCommand = it->second;
            }
            
            DLog("Find command: %s for ID:%d\n", trackedCommand.c_str(), (int)pktId);
            if (trackedCommand == "connect") {
                
                sendReleaseStream();
                sendFCPublish();
                sendCreateStream();
                setClientState(kClientStateFCPublish);
                
            } else if (trackedCommand == "createStream") {
                if (p[10] || p[19] != 0x05 || p[20]) {
                    DLog("RTMP: Unexpected reply on connect()\n");
                } else {
                    m_streamId = get_double(p+21);
                }
                sendPublish();
                setClientState(kClientStateReady);
            }
            // FIXME: 需要清理一下m_trackedCommands的记录吗？
            
        } else if (command == "onStatus") {
            std::string code = parseStatusCode(p + 3 + command.length());
            DLog("code : %s\n", code.c_str());
            if (code == "NetStream.Publish.Start") {
                
                sendHeaderPacket();
                
                sendSetChunkSize(getpagesize());
                // sendSetBufferTime(0);
                setClientState(kClientStateSessionStarted);
                
                m_throughputSession.start();
            }
        }
        
    }
    
    std::string RTMPSession::parseStatusCode(uint8_t *p) {
        //uint8_t *start = p;
        std::map<std::string, std::string> props;
        
        // skip over the packet id
        get_double(p+1); // num
        p += sizeof(double) + 1;
        
        // keep reading until we find an AMF Object
        bool foundObject = false;
        while (!foundObject) {
            if (p[0] == AMF_DATA_TYPE_OBJECT) {
                p += 1;
                foundObject = true;
                continue;
            } else {
                p += amfPrimitiveObjectSize(p);
            }
        }
        
        // read the properties of the object
        uint16_t nameLen, valLen;
        char propName[128], propVal[128];
        do {
            nameLen = get_be16(p);
            p += sizeof(nameLen);
            strncpy(propName, (char*)p, nameLen);
            propName[nameLen] = '\0';
            p += nameLen;
            if (p[0] == AMF_DATA_TYPE_STRING) {
                valLen = get_be16(p+1);
                p += sizeof(valLen) + 1;
                strncpy(propVal, (char*)p, valLen);
                propVal[valLen] = '\0';
                p += valLen;
                props[propName] = propVal;
            } else {
                // treat non-string property values as empty
                p += amfPrimitiveObjectSize(p);
                props[propName] = "";
            }
            // Fix large AMF object may break to multiple packets
            // that crash us.
            if (strcmp(propName, "code") == 0) {
                break;
            }
        } while (get_be24(p) != AMF_DATA_TYPE_OBJECT_END);
        
        //p = start;
        return props["code"];
    }
    
    int32_t RTMPSession::amfPrimitiveObjectSize(uint8_t* p) {
        switch(p[0]) {
            case AMF_DATA_TYPE_NUMBER:       return 9;
            case AMF_DATA_TYPE_BOOL:         return 2;
            case AMF_DATA_TYPE_NULL:         return 1;
            case AMF_DATA_TYPE_STRING:       return 3 + get_be16(p);
            case AMF_DATA_TYPE_LONG_STRING:  return 5 + get_be32(p);
        }
        return -1; // not a primitive, likely an object
    }
    int32_t RTMPSession::trackCommand(const std::string& cmd) {
        ++m_numberOfInvokes;
        m_trackedCommands[m_numberOfInvokes] = cmd;
        DLog("Tracking command(%d, %s)\n", m_numberOfInvokes, cmd.c_str());
        return m_numberOfInvokes;
    }
}
