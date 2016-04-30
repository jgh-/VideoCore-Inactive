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

/*
 *  iOS::H264Encode transform :: Takes a CVPixelBufferRef input and outputs H264 NAL Units.
 *
 *
 */

#ifndef __videocore__H264Encode__
#define __videocore__H264Encode__

#include <iostream>
#include <videocore/transforms/IEncoder.hpp>
#include <videocore/system/JobQueue.hpp>
#include <videocore/system/Buffer.hpp>
#include <deque>

namespace videocore { namespace iOS {
 
    class H264Encode : public IEncoder
    {
    public:
        H264Encode( int frame_w, int frame_h, int fps, int bitrate );
        ~H264Encode();
        
        void setOutput(std::shared_ptr<IOutput> output) { m_output = output; };
        
        // Input is expecting a CVPixelBufferRef
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
      
    public:
        /*! IEncoder */
        void setBitrate(int bitrate) ;
        
        const int bitrate() const { return m_bitrate; };
        
    private:
        bool setupWriters();
        bool setupWriter(int writer);
        void swapWriters(bool force = false);
        void teardownWriter(int writer);
        void extractSpsAndPps(int writer);
        void releaseFrame(videocore::IMetadata& metadata);
        bool isFirstSlice(uint8_t* nalu, size_t size);


        
    private:
        void*                  m_assetWriters[2];
        void*                  m_pixelBuffers[2];
        
        Buffer                 m_sps;
        Buffer                 m_pps;
        
        std::deque<std::unique_ptr<Buffer>>     m_frameQueue;
        
        JobQueue               m_queue;
        
        std::string            m_tmpFile[2];
        
        std::weak_ptr<IOutput> m_output;
        
        long                   m_lastFilePos;
        
        uint32_t               m_frameCount;
        
        int                    m_frameW;
        int                    m_frameH;
        int                    m_fps;
        int                    m_bitrate;

        bool                   m_currentWriter;
    };
}
}
#endif /* defined(__videocore__H264Encode__) */
