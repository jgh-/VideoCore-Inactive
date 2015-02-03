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
#ifndef __videocore__IThroughputAdaptation__
#define __videocore__IThroughputAdaptation__

#include <functional>
#include <videocore/system/util.h>

namespace videocore {
    
    using ThroughputCallback = std::function<void(float bitrateRecommendedVector, float predictedBytesPerSecond, int immediateBytesPerSecond)>;
    
    
    class IThroughputAdaptation {
    public:
        virtual ~IThroughputAdaptation() {};
        
        virtual void setThroughputCallback(ThroughputCallback callback) = 0;
        
        virtual void addSentBytesSample(size_t bytesSent) = 0;
        
        virtual void addBufferSizeSample(size_t bufferSize) = 0;
        
        virtual void addBufferDurationSample(int64_t bufferDuration) = 0;
        
        virtual void reset() = 0;
        
        virtual void start() = 0;
    };
}

#endif
