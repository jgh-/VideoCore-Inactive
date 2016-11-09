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
#ifndef __videocore__PixelBufferSource__
#define __videocore__PixelBufferSource__

#include <iostream>
#include <videocore/sources/ISource.hpp>
#include <videocore/transforms/IOutput.hpp>

#ifdef __APPLE__
#   include <MacTypes.h>
#else
#   include <stdint.h>

    typedef uint32_t OSType;

#endif

namespace videocore { namespace Apple {

    class PixelBufferSource : public ISource, public std::enable_shared_from_this<PixelBufferSource>
    {
        
    public:
        
        /*
         *  @param width            The desired width of the pixel buffer
         *  @param height           The desired height of the pixel buffer
         *  @param pixelFormat      The FourCC format of the pixel data.
         */
        PixelBufferSource(int width, int height, OSType pixelFormat );
        ~PixelBufferSource();
        
        void setOutput(std::shared_ptr<IOutput> output);
        
    public:
        
        void pushPixelBuffer(void* data, size_t size);
        
    private:
        std::weak_ptr<IOutput> m_output;
        void*                  m_pixelBuffer;
        int                    m_width;
        int                    m_height;
        OSType                 m_pixelFormat;
        
    };
}
}

#endif /* defined(__videocore__PixelBufferSource__) */
