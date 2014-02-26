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
#ifndef __videocore__StreamSession__
#define __videocore__StreamSession__

#include "IStreamSession.hpp"

namespace videocore {
    namespace Apple {
        
        class StreamSession : public IStreamSession
        {
        public:
            StreamSession();
            ~StreamSession();
            
            void connect(std::string host, int port, StreamSessionCallback_t);
            void disconnect();
            
            size_t write(uint8_t* buffer, size_t size);
            size_t read(uint8_t* buffer, size_t size);
            
            const StreamStatus_t status() const {
                return m_status;
            };
        public:
            void nsStreamCallback(void* stream, unsigned event);
            
        private:
            void setStatus(StreamStatus_t status, bool clear = false);
            void startNetwork();
        private:
            void*              m_inputStream;
            void*             m_outputStream;
            void*                  m_runLoop;
            void*           m_streamCallback;
                        
            StreamSessionCallback_t     m_callback;
            StreamStatus_t              m_status;
            
        };
    }
}



#endif /* defined(__videocore__StreamSession__) */
