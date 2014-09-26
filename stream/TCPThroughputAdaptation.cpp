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
#include <videocore/stream/TCPThroughputAdaptation.h>
#include <chrono>
#include <cmath>
#include <stdlib.h>

namespace videocore {
    
    static const float kPI_2 = M_PI_2;
    static const float kWeight = 0.75f;
    static const int   kPivotSamples = 5;
    static const int   kMeasurementDelay = 2; // seconds - represents the time between measurements when increasing or decreasing bitrate
    static const int   kSettlementDelay  = 60; // seconds - represents time to wait after a bitrate decrease before attempting to increase again
    static const int   kIncreaseDelta    = 20; // seconds - number of seconds to wait between increase vectors (after initial ramp up)
    
    TCPThroughputAdaptation::TCPThroughputAdaptation()
    : m_callback(nullptr), m_exiting(false), m_hasFirstTurndown(false), m_bwSampleCount(30), m_previousVector(0.f)
    {
        float v = (1.f - powf(kWeight, m_bwSampleCount)) / (1.f - kWeight) ;
        for ( int i = 0 ; i < m_bwSampleCount ; ++i ) {
            m_bwWeights.push_back(powf(kWeight, i) / v);
        }
        
        m_thread = std::thread([&]{
            this->sampleThread();
        });
    }
    TCPThroughputAdaptation::~TCPThroughputAdaptation()
    {
        m_exiting = true;
        m_cond.notify_all();
        m_thread.join();
    }

    void
    TCPThroughputAdaptation::setThroughputCallback(ThroughputCallback callback)
    {
        m_callback = callback;
    }
    void
    TCPThroughputAdaptation::sampleThread()
    {
        std::mutex m;
        auto prev = std::chrono::steady_clock::now();
        //char* home = getenv("HOME");
        //char* folder = "/Library/Documents";
        
        while(!m_exiting) {
            std::unique_lock<std::mutex> l(m);
            auto now = std::chrono::steady_clock::now();
            auto diff = now - prev;
            auto previousTurndownDiff = std::chrono::duration_cast<std::chrono::seconds>(now - m_previousTurndown).count();
            auto previousIncreaseDiff = std::chrono::duration_cast<std::chrono::seconds>(now - m_previousIncrease).count();
            prev = now;
            m_sentMutex.lock();
            m_buffMutex.lock();
            
            size_t totalSent = 0;
            
            for ( auto & samp : m_sentSamples )
            {
                totalSent += samp;
            }
            
            const float timeDelta            = float(std::chrono::duration_cast<std::chrono::microseconds>(diff).count()) / 1.0e6f;
            const float detectedBytesPerSec  = float(totalSent) / timeDelta;
            float vec = 0.f;
            float avg = 0.f;
            float turnAvg = 0.f;
            
            m_bwSamples.push_front(detectedBytesPerSec);
            if(m_bwSamples.size() > m_bwSampleCount) {
                m_bwSamples.pop_back();
            }
            for( int i = 0 ; i < m_bwSamples.size() ; ++i ) {
                avg += m_bwSamples[i] * m_bwWeights[i];
            }
            
            
            if(!m_bufferSizeSamples.empty()) {
                bool noBuffer = true;
              
                float frontAvg = 0.f;
                float backAvg = 0.f;
                int frontCount = 0;
                int backCount = 0;
                
                for (int i = 0 ; i < m_bufferSizeSamples.size() ; ++i) {

                    const float s1 = m_bufferSizeSamples[i];
                    if(s1>0) noBuffer = false;
                    
                    if ( i < m_bufferSizeSamples.size() / 2 ) {
                        frontAvg += s1;
                        frontCount++;
                    } else {
                        backAvg += s1;
                        backCount++;
                    }
                }
                frontAvg /= float(frontCount);
                backAvg /= float(backCount);
                
                if(noBuffer && m_bufferSizeSamples.back() > 0) noBuffer = false;
                
                //DLog("NB: %d, FT: %f BK: %f\n", noBuffer, frontAvg, backAvg);
                if(noBuffer && m_bufferSizeSamples.back() == 0 && (!m_hasFirstTurndown || (previousTurndownDiff > kSettlementDelay && previousIncreaseDiff > kIncreaseDelta))) {
                    vec = 1.f;
                }
                else if(!noBuffer && backAvg > frontAvg) {
                    vec = -1.f;

                    m_previousTurndown = now;
                    m_hasFirstTurndown = true;
                }

                
                if(m_previousVector < 0 && vec >= 0) {
                    m_turnSamples.push_front(m_bwSamples.front());
                    if(m_turnSamples.size() > kPivotSamples) {
                        m_turnSamples.pop_back();
                    }
                }

                if(m_turnSamples.size() > 0) {
                    
                    
                    for ( int i = 0 ; i < m_turnSamples.size() ; ++i ) {
                        turnAvg += m_turnSamples[i];
                    }
                    turnAvg /= m_turnSamples.size();
                    
                    if(detectedBytesPerSec > turnAvg) {
                        m_turnSamples.push_front(detectedBytesPerSec);
                        if(m_turnSamples.size() > kPivotSamples) {
                            m_turnSamples.pop_back();
                        }
                    }
                    
                    float a = (detectedBytesPerSec - avg) / turnAvg ;
                    float slope = 3.f * powf(a,2.f);
                    
                    vec *= std::min(1.f,std::max(atanf(slope) / kPI_2, 0.1f));
                }

                m_previousVector = vec;

            }
            m_sentSamples.clear();
            m_bufferSizeSamples.clear();
            m_bufferDurationSamples.clear();
            m_sentMutex.unlock();
            m_buffMutex.unlock();
            
            if(m_callback) {
                if(vec > 0.f) {
                    m_previousIncrease = now;
                }
                m_callback(vec, turnAvg);
            }
            
            if(!m_exiting) {
                
                m_cond.wait_until(l, now + std::chrono::seconds(kMeasurementDelay));
            }
        }
    }
    void
    TCPThroughputAdaptation::addBufferSizeSample(size_t bufferSize)
    {
        m_buffMutex.lock();
        m_bufferSizeSamples.push_back(bufferSize);
        m_buffMutex.unlock();
    }
    void
    TCPThroughputAdaptation::addSentBytesSample(size_t bytesSent)
    {
        m_sentMutex.lock();
        m_sentSamples.push_back(bytesSent);
        m_sentMutex.unlock();
    }
    void
    TCPThroughputAdaptation::addBufferDurationSample(int64_t bufferDuration)
    {
        m_durMutex.lock();
        m_bufferDurationSamples.push_back(bufferDuration);
        m_durMutex.unlock();
        
    }
}