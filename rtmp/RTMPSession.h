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
#ifndef __videocore__RTMSession__
#define __videocore__RTMSession__

#include <iostream>
#include <videocore/stream/IStreamSession.hpp>
#include <UriParser/UriParser.hpp>

#include <functional>
#include <queue>
#include <map>

#include <videocore/system/JobQueue.hpp>
#include <cstdlib>

#include <videocore/rtmp/RTMPTypes.h>
#include <videocore/system/Buffer.hpp>
#include <videocore/transforms/IOutputSession.hpp>

namespace videocore
{
    class RTMPSession;

    enum {
        kRTMPSessionParameterWidth=0,
        kRTMPSessionParameterHeight,
        kRTMPSessionParameterFrameDuration,
        kRTMPSessionParameterVideoBitrate,
        kRTMPSessionParameterAudioFrequency
    };
    typedef MetaData<'rtmp', int32_t, int32_t, double, int32_t, double> RTMPSessionParameters_t;
    enum {
        kRTMPMetadataTimestamp=0,
        kRTMPMetadataMsgLength,
        kRTMPMetadataMsgTypeId,
        kRTMPMetadataMsgStreamId
    };
    
    typedef MetaData<'rtmp', int32_t, int32_t, uint8_t, int32_t> RTMPMetadata_t;

    typedef std::function<void(RTMPSession& session, ClientState_t state)> RTMPSessionStateCallback_t;
      
    class RTMPSession : public IOutputSession
    {
    public:
        RTMPSession(std::string uri, RTMPSessionStateCallback_t callback);
        ~RTMPSession();
        
    public:
        
        // Requires RTMPMetadata_t
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
        
        void setSessionParameters(IMetadata& parameters);

        
    private:
        
        // Deprecate sendPacket
        void sendPacket(uint8_t* data, size_t size, RTMPChunk_0 metadata);
        
        
        
        void streamStatusChanged(StreamStatus_t status);
        void write(uint8_t* data, size_t size);
        void dataReceived();
        void setClientState(ClientState_t state);
        void handshake();
        void handshake0();
        void handshake1();
        void handshake2();
        
        void sendConnectPacket();
        void sendReleaseStream();
        void sendFCPublish();
        void sendCreateStream();
        void sendPublish();
        void sendHeaderPacket();
        
        void sendDeleteStream();
        
        bool parseCurrentData();
    private:
        
        JobQueue            m_jobQueue;
       
        RingBuffer          m_streamOutRemainder;
        Buffer              m_s1, m_c1;
        
        std::queue<std::shared_ptr<Buffer> > m_streamOutQueue;
        
        std::map<int, uint64_t>                  m_previousChunkData;
        std::unique_ptr<RingBuffer>         m_streamInBuffer;
        std::unique_ptr<IStreamSession>     m_streamSession;
        std::vector<uint8_t> m_outBuffer;
        http::url                       m_uri;
        
        RTMPSessionStateCallback_t      m_callback;
        std::string                     m_playPath;
        std::string                     m_app;
	
        int64_t         m_previousTimestamp;        
        size_t          m_currentChunkSize;
        int32_t         m_streamId;
        int32_t         m_numberOfInvokes;
        int32_t         m_frameWidth;
        int32_t         m_frameHeight;
        int32_t         m_bitrate;
        double          m_frameDuration;
        double          m_audioSampleRate;
        
        ClientState_t  m_state;
    };
}

#endif /* defined(__videocore__RTMSession__) */
