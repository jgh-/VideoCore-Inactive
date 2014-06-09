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
#include <videocore/sources/Apple/PixelBufferSource.h>
#include <videocore/mixers/IVideoMixer.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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
            CVPixelBufferLockBaseAddress((CVPixelBufferRef)m_pixelBuffer, 0);
            void* loc = CVPixelBufferGetBaseAddress((CVPixelBufferRef)m_pixelBuffer);
            memcpy(loc, data, size);
            CVPixelBufferUnlockBaseAddress((CVPixelBufferRef)m_pixelBuffer, 0);

            glm::mat4 mat = glm::scale(glm::mat4(1.f), glm::vec3(1.f,-1.f,1.f));
            VideoBufferMetadata md(0.);
            md.setData(0, mat, shared_from_this());
            
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