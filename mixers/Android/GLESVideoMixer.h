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

 #ifndef __videocore_android_GLESVideoMixer
 #define __videocore_android_GLESVideoMixer
 #include <videocore/mixers/IVideoMixer.hpp>
 #include <videocore/system/JobQueue.hpp>

 #include <videocore/system/pixelBuffer/Android/PixelBuffer.h>

 #include <EGL/egl.h>
 #include <EGL/eglext.h>

namespace videocore { namespace Android {

    struct SourceBuffer
    {
        SourceBuffer() : m_currentTexture(nullptr), m_pixelBuffers() { };
        ~SourceBuffer() { };
        void setBuffer(Android::PixelBufferRef ref, JobQueue& jobQueue);
        
        EGLImageKHR* currentTexture() const { return m_currentTexture; };
        Android::PixelBufferRef currentBuffer() const { return m_currentBuffer; };
        
    private:
        typedef struct __Buffer_ {
            __Buffer_(Android::PixelBufferRef buf) : texture(nullptr), buffer(buf) {};
            ~__Buffer_() { if(texture) { delete texture; } };
                
            Android::PixelBufferRef buffer;
            EGLImageKHR* texture;
            std::chrono::steady_clock::time_point time;
        } Buffer_;
        
        std::map< android::GraphicBuffer*, Buffer_ >   m_pixelBuffers;
        Android::PixelBufferRef 			  m_currentBuffer;
        EGLImageKHR*				          m_currentTexture;
    };

	class GLESVideoMixer : public IVideoMixer 
	{

	public:
		GLESVideoMixer();
		~GLESVideoMixer();


	private:


	};
}};

 #endif