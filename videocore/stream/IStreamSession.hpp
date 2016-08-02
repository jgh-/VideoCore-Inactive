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
#ifndef videocore_IStream_hpp
#define videocore_IStream_hpp

#include <cstddef>
#include <functional>

#include <videocore/system/util.h>

namespace videocore {
    
    enum {
        
        kStreamStatusConnected = 1,
        kStreamStatusWriteBufferHasSpace = 1 << 1,
        kStreamStatusReadBufferHasBytes = 1 << 2,
        kStreamStatusErrorEncountered = 1 << 3,
        kStreamStatusEndStream = 1 << 4
        
    } ;
    typedef long StreamStatus_T;
    
    class IStreamSession;
    
    typedef std::function<void(videocore::IStreamSession&, StreamStatus_T)> StreamSessionCallback_T;
    class IStreamSession
    {
    public:
        virtual ~IStreamSession() {};
        
        virtual void connect(const std::string& host, int port, StreamSessionCallback_T sscb) = 0;
        virtual void disconnect() = 0;
        virtual ssize_t write(uint8_t* buffer, size_t size) = 0;
        virtual ssize_t read(uint8_t* buffer, size_t size) = 0;
        virtual const StreamStatus_T status() const = 0;
                
    private:
        virtual void setStatus(StreamStatus_T,bool clear = false) = 0;
    };
    
}


#endif
