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
#ifndef videocore_Android_GraphicBuffer_h
#define videocore_Android_GraphicBuffer_h
#include <stdint.h>
#include <ui/GraphicBuffer.h>

namespace videocore { namespace Android {

	class GraphicBuffer
	{	

	public:
		GraphicBuffer(uint32_t width, uint32_t height, android::PixelFormat format, uint32_t usage);
		~GraphicBuffer();

		android::status_t lock(uint32_t usage, void** addr);
		android::status_t unlock();
		android_native_buffer_t* getNativeBuffer() const;

		const uint32_t getWidth() const { return m_width; };
		const uint32_t getHeight() const { return m_height; };
		const android::PixelFormat getPixelFormat() const { return m_format; };

    private: 
        static void setup();
        static bool s_isSetup;

    private:
    	void* m_buffer;

    	uint32_t m_width;
    	uint32_t m_height;
    	android::PixelFormat m_format;

	};

}}

#endif