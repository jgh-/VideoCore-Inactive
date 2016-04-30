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
            
            void connect(const std::string& host, int port, StreamSessionCallback_T) override;
            void disconnect() override;
            
            ssize_t write(uint8_t* buffer, size_t size) override;
            ssize_t read(uint8_t* buffer, size_t size) override;
                        
            const StreamStatus_T status() const override {
                return m_status;
            };
        public:
            void nsStreamCallback(void* stream, unsigned event);
            
        private:
            void setStatus(StreamStatus_T status, bool clear = false) override;
            void startNetwork();
        private:
            void*              m_inputStream;
            void*             m_outputStream;
            void*                  m_runLoop;
            void*           m_streamCallback;
                        
            StreamSessionCallback_T     m_callback;
            StreamStatus_T              m_status;
          
            int m_outSocket;
            
        };
    }
}



#endif /* defined(__videocore__StreamSession__) */
