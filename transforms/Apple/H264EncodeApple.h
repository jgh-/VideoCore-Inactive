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
#include <videocore/transforms/IEncoder.hpp>
#include <videocore/system/Buffer.hpp>
#include <deque>

#include <CoreVideo/CoreVideo.h>

namespace videocore { namespace Apple {
 
    class H264EncodeApple : public IEncoder
    {
    public:
        H264EncodeApple( int frame_w, int frame_h, int fps, int bitrate, bool useBaseline = true, int ctsOffset = 0 );
        ~H264EncodeApple();
        
        CVPixelBufferPoolRef pixelBufferPool();
        
    public:
        /*! ITransform */
        void setOutput(std::shared_ptr<IOutput> output) { m_output = output; };
        
        // Input is expecting a CVPixelBufferRef
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
        
    public:
        /*! IEncoder */
        void setBitrate(int bitrate) ;
        
        const int bitrate() const { return m_bitrate; };
        
        void requestKeyframe();
        
    public:
        void compressionSessionOutput(const uint8_t* data, size_t size, uint64_t pts, uint64_t dts);
        
    private:
        void setupCompressionSession( bool useBaseline );
        void teardownCompressionSession();
        
    private:
        
    
        
        std::mutex             m_encodeMutex;
        std::weak_ptr<IOutput> m_output;
        void*                  m_compressionSession;
        int                    m_frameW;
        int                    m_frameH;
        int                    m_fps;
        int                    m_bitrate;
        
        int                    m_ctsOffset;

        bool                   m_baseline;
        bool                   m_forceKeyframe;
    };
}
}