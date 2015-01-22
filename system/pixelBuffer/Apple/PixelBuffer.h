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
#ifndef videocore_PixelBuffer_hpp
#define videocore_PixelBuffer_hpp

#include <CoreVideo/CoreVideo.h>
#include <videocore/system/pixelBuffer/IPixelBuffer.hpp>
#include <memory>

namespace videocore { namespace Apple {
 
    
    class PixelBuffer : public IPixelBuffer
    {
        
    public:
        
        PixelBuffer(CVPixelBufferRef pb, bool temporary = false);
        ~PixelBuffer();
        
        static const size_t hash(std::shared_ptr<PixelBuffer> buf) { return std::hash<std::shared_ptr<PixelBuffer>>()(buf); };
        
    public:
        const int   width() const  { return (int)CVPixelBufferGetWidth(m_pixelBuffer); };
        const int   height() const { return (int)CVPixelBufferGetHeight(m_pixelBuffer); };
        const void* baseAddress() const { return CVPixelBufferGetBaseAddress(m_pixelBuffer); };
        
        const PixelBufferFormatType pixelFormat() const { return m_pixelFormat; };
        
        void  lock(bool readOnly = false);
        void  unlock(bool readOnly = false);
        
        void  setState(const PixelBufferState state) { m_state = state; };
        const PixelBufferState state() const { return m_state; };
        
        const bool isTemporary() const { return m_temporary; };
        void setTemporary(const bool temporary) { m_temporary = temporary; };
        
    public:
        const CVPixelBufferRef cvBuffer() const { return m_pixelBuffer; };
        
    private:
        
        CVPixelBufferRef m_pixelBuffer;
        PixelBufferFormatType m_pixelFormat;
        PixelBufferState m_state;
        
        bool m_locked;
        bool m_temporary;
    };
    
    typedef std::shared_ptr<PixelBuffer> PixelBufferRef;
    
}
}

#endif
