//
//  PixelBufferSource.h
//  mobcrush
//
//  Created by James Hurley on 4/19/14.
//  Copyright (c) 2014 Raster Multimedia. All rights reserved.
//

#ifndef __mobcrush__PixelBufferSource__
#define __mobcrush__PixelBufferSource__

#include <iostream>
#include <videocore/sources/ISource.hpp>
#include <videocore/transforms/IOutput.hpp>
#include <MacTypes.h>

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

#endif /* defined(__mobcrush__PixelBufferSource__) */
