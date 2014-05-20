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
        
        // note: For now channelCount = 2, bitsPerChannel = 16.
        GenericAudioMixer(int outChannelCount, int outFrequencyInHz, int outBitsPerChannel, double outBufferDuration);
        ~GenericAudioMixer();
        
    public:
        void registerSource(std::shared_ptr<ISource> source, size_t inBufferSize = 0)  ;
        void unregisterSource(std::shared_ptr<ISource> source);
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
        void setOutput(std::shared_ptr<IOutput> output);
        void setSourceGain(std::weak_ptr<ISource> source, float gain);
        void setEpoch(const std::chrono::steady_clock::time_point epoch) {
            m_epoch = epoch;
        };
        
    protected:
        
        virtual std::shared_ptr<Buffer> resample(const uint8_t* const buffer, size_t size, AudioBufferMetadata& metadata);
        
        void mixThread();
        
    protected:
        std::chrono::steady_clock::time_point m_epoch;
        
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
