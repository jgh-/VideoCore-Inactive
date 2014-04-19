//
//  PixelBufferSource.cpp
//  mobcrush
//
//  Created by James Hurley on 4/19/14.
//  Copyright (c) 2014 Raster Multimedia. All rights reserved.
//

#include <videocore/sources/Apple/PixelBufferSource.h>
#include <videocore/mixers/IVideoMixer.hpp>

#include <CoreVideo/CoreVideo.h>

namespace videocore { namespace Apple {
 
    PixelBufferSource::PixelBufferSource(int width, int height, OSType pixelFormat )
    : m_pixelBuffer(nullptr), m_width(width), m_height(height), m_pixelFormat(pixelFormat)
    {
        CVPixelBufferRef pb = nullptr;
        
        CVReturn ret = CVPixelBufferCreate(kCFAllocatorDefault, width, height, pixelFormat, NULL, &pb);
        
        if(!ret) {
            m_pixelBuffer = pb;
        } else {
            throw std::runtime_error("PixelBuffer creation failed");
        }
    }
    PixelBufferSource::~PixelBufferSource()
    {
        if(m_pixelBuffer) {
            CVPixelBufferRelease((CVPixelBufferRef)m_pixelBuffer);
            m_pixelBuffer = nullptr;
        }
    }
    
    void
    PixelBufferSource::pushPixelBuffer(void *data, size_t size)
    {
        
        auto outp = m_output.lock();
        
        if(outp) {
            void* loc = CVPixelBufferGetBaseAddress((CVPixelBufferRef)m_pixelBuffer);
            CVPixelBufferLockBaseAddress((CVPixelBufferRef)m_pixelBuffer, 0);
            memcpy(loc, data, size);
            CVPixelBufferUnlockBaseAddress((CVPixelBufferRef)m_pixelBuffer, 0);
            
            VideoBufferMetadata md(0.);
            md.setData(kLayerGame, shared_from_this());
            
            outp->pushBuffer((const uint8_t*)m_pixelBuffer, sizeof(CVPixelBufferRef), md);
        }
    }
    void
    PixelBufferSource::setOutput(std::shared_ptr<IOutput> output)
    {
        m_output = output;
    }
    
}
}