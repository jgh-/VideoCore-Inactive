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
 *  GenericAudioMixer mixer :: Takes raw LPCM buffers from a variety of sources, resamples, and mixes them and ouputs a single LPCM buffer.
 *
 *
 */
#ifndef __videocore__GenericAudioMixer__
#define __videocore__GenericAudioMixer__

#include <iostream>
#include <videocore/mixers/IAudioMixer.hpp>
#include <videocore/system/Buffer.hpp>
#include <map>
#include <thread>
#include <mutex>

namespace videocore {
    class GenericAudioMixer : public IAudioMixer
    {
      
    public:
        /*
         *
         */
        GenericAudioMixer(int outChannelCount, int outFrequencyInHz, int outBitsPerChannel, double frameDuration);
        ~GenericAudioMixer();
        
        virtual void setMinimumBufferDuration(const double duration) ;
        
    public:
        void registerSource(std::shared_ptr<ISource> source, size_t inBufferSize = 0)  ;
        void unregisterSource(std::shared_ptr<ISource> source);
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
        void setOutput(std::shared_ptr<IOutput> output);
        void setSourceGain(std::weak_ptr<ISource> source, float gain);
        void setEpoch(const std::chrono::steady_clock::time_point epoch) {
            m_epoch = epoch;
            m_nextMixTime = epoch;
        };
        
    protected:
        
        virtual std::shared_ptr<Buffer> resample(const uint8_t* const buffer, size_t size, AudioBufferMetadata& metadata);
        
        void mixThread();
        
    protected:
        std::chrono::steady_clock::time_point m_epoch;
        std::chrono::steady_clock::time_point m_nextMixTime;
        
        double m_frameDuration;
        double m_bufferDuration;
        
        std::thread m_mixThread;
        std::mutex  m_mixMutex;
        std::condition_variable m_mixThreadCond;
        
        std::weak_ptr<IOutput> m_output;
        std::map < std::size_t, std::unique_ptr<RingBuffer> > m_inBuffer;
        std::map < std::size_t, float > m_inGain;
        
        int m_outChannelCount;
        int m_outFrequencyInHz;
        int m_outBitsPerChannel;
        int m_bytesPerSample;
        
        std::atomic<bool> m_exiting;
        
    };
}
#endif /* defined(__videocore__GenericAudioMixer__) */
