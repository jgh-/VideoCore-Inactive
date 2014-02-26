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
#ifndef videocore_IStream_hpp
#define videocore_IStream_hpp

#include <cstddef>
#include <functional>
#include <string>

namespace videocore {
    
    enum {
        
        kStreamStatusConnected = 1,
        kStreamStatusWriteBufferHasSpace = 1 << 1,
        kStreamStatusReadBufferHasBytes = 1 << 2,
        kStreamStatusErrorEncountered = 1 << 3,
        kStreamStatusEndStream = 1 << 4
        
    } ;
    typedef long StreamStatus_t;
    
    class IStreamSession;
    
    typedef std::function<void(videocore::IStreamSession&, StreamStatus_t)> StreamSessionCallback_t;
    
    class IStreamSession
    {
    public:
        virtual ~IStreamSession() {};
        
        virtual void connect(std::string host, int port, StreamSessionCallback_t ) = 0;
        virtual void disconnect() = 0;
        virtual size_t write(uint8_t* buffer, size_t size) = 0;
        virtual size_t read(uint8_t* buffer, size_t size) = 0;
        virtual const StreamStatus_t status() const = 0;
    private:
        virtual void setStatus(StreamStatus_t,bool clear = false) = 0;
    };
    
}


#endif
