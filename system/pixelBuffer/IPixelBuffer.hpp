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
#ifndef videocore_IPixelBuffer_hpp
#define videocore_IPixelBuffer_hpp

#include <stdint.h>

namespace videocore {

    enum {
		kVCPixelBufferFormat32BGRA = 'bgra',
		kVCPixelBufferFormat32RGBA = 'rgba',
		kVCPixelBufferFormatL565 = 'L565',
		kCVPixelBufferFormat420v = '420v',
	} PixelBufferFormatType_;
    
    typedef uint32_t PixelBufferFormatType;
    
    typedef enum {
        kVCPixelBufferStateAvailable,
        kVCPixelBufferStateDequeued,
        kVCPixelBufferStateEnqueued,
        kVCPixelBufferStateAcquired
    } PixelBufferState;
    
	class IPixelBuffer {
	public:
		virtual ~IPixelBuffer() {};

		virtual const int   width() const = 0;
		virtual const int   height() const = 0;

		virtual const PixelBufferFormatType pixelFormat() const = 0;

		virtual const void* baseAddress() const = 0;

		virtual void  lock(bool readOnly = false) = 0;
		virtual void  unlock(bool readOnly = false) = 0;
        
        virtual void setState(const PixelBufferState state) = 0;
        virtual const PixelBufferState state() const = 0;
        
        virtual const bool isTemporary() const = 0; /* mark if the pixel buffer needs to disappear soon */
        virtual void setTemporary(const bool temporary) = 0;
	};
}

 #endif