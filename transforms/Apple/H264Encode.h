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
#include <videocore/transforms/ITransform.hpp>
#include <videocore/system/JobQueue.hpp>
#include <videocore/system/Buffer.hpp>
#include <deque>

namespace videocore { namespace Apple {
 
    class H264Encode : public ITransform
    {
    public:
        H264Encode( int frame_w, int frame_h, int fps, int bitrate );
        ~H264Encode();
        
        void setOutput(std::shared_ptr<IOutput> output) { m_output = output; };
        
        // Input is expecting a CVPixelBufferRef
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
        
    public:
        void compressionSessionOutput(const uint8_t* data, size_t size, uint64_t ts);
        
    private:
        void setupCompressionSession();
        
        
    private:
        
        std::weak_ptr<IOutput> m_output;
        void*                  m_compressionSession;
        int                    m_frameW;
        int                    m_frameH;
        int                    m_fps;
        int                    m_bitrate;
        
    };
}
}