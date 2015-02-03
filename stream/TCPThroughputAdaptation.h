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
#ifndef __videocore__TCPThroughputAdaptation__
#define __videocore__TCPThroughputAdaptation__

#include <videocore/stream/IThroughputAdaptation.h>
#include <videocore/system/JobQueue.hpp>
#include <vector>
#include <deque>
#include <thread>
#include <condition_variable>
#include <mutex>
namespace videocore {
    class TCPThroughputAdaptation : public IThroughputAdaptation
    {
    public:
        TCPThroughputAdaptation();
        ~TCPThroughputAdaptation();
        
    public:
        
        void setThroughputCallback(ThroughputCallback callback);
        
        void addSentBytesSample(size_t bytesSent);
        
        void addBufferSizeSample(size_t bufferSize);
        
        void addBufferDurationSample(int64_t bufferDuration);
        
        void reset();
        void start();
    private:
        void sampleThread();
        
    private:
        
        std::chrono::steady_clock::time_point m_previousTurndown;
        std::chrono::steady_clock::time_point m_previousIncrease;
        
        std::thread             m_thread;
        std::condition_variable m_cond;
        std::mutex              m_sentMutex;
        std::mutex              m_buffMutex;
        std::mutex              m_durMutex;
        
        std::vector<size_t> m_sentSamples;
        std::vector<size_t> m_bufferSizeSamples;
        std::vector<int64_t> m_bufferDurationSamples;
        
        std::deque<float> m_bwSamples;
        std::deque<int> m_buffGrowth;
        std::deque<float> m_turnSamples;
        std::vector<float> m_bwWeights;
        
        ThroughputCallback m_callback;
        
        int  m_bwSampleCount;
        int  m_negSampleCount;
        
        float m_previousVector;
        
        bool m_started;
        bool m_exiting;
        bool m_hasFirstTurndown;
        
    };
}

#endif
