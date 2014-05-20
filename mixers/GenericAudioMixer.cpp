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
#include <videocore/mixers/GenericAudioMixer.h>
#include <sstream>

//#include <arm_neon.h>

static inline int16_t b8_to_b16(void* v) {
    int16_t val = *(int8_t*)v;
    return val * 0xFF;
}
static inline int16_t b16_to_b16(void*  v) {
    return *(int16_t*)v;
}

static inline int16_t b32_to_b16(void*  v) {
    int16_t val = (*(int32_t*)v / 0xFFFF);
    return val;
}
static inline int16_t b24_to_b16(void* v) {

    static const int m = 1U << 23;
    
    uint8_t* p = (uint8_t*)v;

    union {
        int32_t x;
        uint8_t in[4];
    };
    in[0] = p[0];
    in[1] = p[1];
    in[2] = p[2];
    in[3] = 0;
    
    int32_t r =  (x ^ m) - m;
    
    return b32_to_b16(&r);
    
}
extern std::string g_tmpFolder;

namespace videocore {
    static inline int cubicInterp(int y0, int y1, int y2, int y3, double mu)
    {
        double a[4], mu2;
        
        mu2 = mu*mu;
        a[0] = y3 - y2 - y0 + y1;
        a[1] = y0 - y1 - a[0];
        a[2] = y2 - y0;
        a[3] = y1;
        
        return (a[0]*mu*mu2+a[1]*mu2+a[2]*mu+a[3]);
        
    }
    GenericAudioMixer::GenericAudioMixer(int outChannelCount, int outFrequencyInHz, int outBitsPerChannel, double outBufferDuration)
    : m_bufferDuration(outBufferDuration), m_outChannelCount(2), m_outFrequencyInHz(outFrequencyInHz), m_outBitsPerChannel(16),  m_exiting(false)
    {
        m_bytesPerSample = outChannelCount * outBitsPerChannel / 8;
        
        m_mixThread = std::thread([this]() {
            pthread_setname_np("com.videocore.audiomixer");
            this->mixThread();
        });
    }
    GenericAudioMixer::~GenericAudioMixer()
    {
        m_exiting = true;
        m_mixThreadCond.notify_all();
        m_mixThread.join();
    }
    void
    GenericAudioMixer::registerSource(std::shared_ptr<ISource> source, size_t inBufferSize)
    {
        auto hash = std::hash<std::shared_ptr< ISource> >()(source);
        size_t bufferSize = (inBufferSize ? inBufferSize : (m_bytesPerSample * m_outFrequencyInHz * m_bufferDuration * 4));
        
        std::unique_ptr<RingBuffer> buffer(new RingBuffer(bufferSize));
        
        m_inBuffer[hash] = std::move(buffer);
        m_inGain[hash] = 1.f;
    }
    void
    GenericAudioMixer::unregisterSource(std::shared_ptr<ISource> source)
    {
        auto hash = std::hash<std::shared_ptr< ISource> >()(source);
        
        auto it = m_inBuffer.find(hash);
        if(it != m_inBuffer.end())
        {
            m_inBuffer.erase(it);
        }
        auto iit = m_inGain.find(hash);
        if(iit != m_inGain.end()) {
            m_inGain.erase(iit);
        }
    }
    void
    GenericAudioMixer::pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata)
    {
        AudioBufferMetadata & inMeta = static_cast<AudioBufferMetadata&>(metadata);
        
        if(inMeta.size() >= 5) {
            const auto inSource = inMeta.getData<kAudioMetadataSource>() ;
            
            auto lSource = inSource.lock();
            if(lSource) {
                
                auto hash = std::hash<std::shared_ptr<ISource> > ()(lSource);
                
                auto ret = resample(data, size, inMeta);
                if(ret->size() > 0) {
                    // push buffer
                    uint8_t *p;
                    size_t rsize = ret->read(&p, ret->size());
                    m_inBuffer[hash]->put(p, rsize);
                    
                } else {
                    // use data provided
                    m_inBuffer[hash]->put(const_cast<uint8_t*>(data), size);
                    
                }
                
            }
        }
    }
    std::shared_ptr<Buffer>
    GenericAudioMixer::resample(const uint8_t* const buffer, size_t size, AudioBufferMetadata &metadata)
    {
        const auto inFrequncyInHz = metadata.getData<kAudioMetadataFrequencyInHz>();
        const auto inBitsPerChannel = metadata.getData<kAudioMetadataBitsPerChannel>();
        const auto inChannelCount = metadata.getData<kAudioMetadataChannelCount>();
        //auto inLoops = metadata.getData<kAudioMetadataLoops>();
        
        if(m_outFrequencyInHz == inFrequncyInHz && m_outBitsPerChannel == inBitsPerChannel && m_outChannelCount == inChannelCount)
        {
            // No resampling necessary
            return std::make_shared<Buffer>();
        }
        
        int16_t (*bitconvert)(void* val) = NULL;
        
        switch(inBitsPerChannel)
        {
            case 8:
                bitconvert = b8_to_b16;
                break;
            case 16:
                bitconvert = b16_to_b16;
                break;
            case 24:
                bitconvert = b24_to_b16;
                break;
            case 32:
                bitconvert = b32_to_b16;
                break;
            default:
                bitconvert = b16_to_b16;
                break;
        }
        

        const size_t bytesPerChannel = inBitsPerChannel / 8;
        const size_t bytesPerSample = inChannelCount * bytesPerChannel;
        
        const double ratio = static_cast<double>(inFrequncyInHz) / static_cast<double>(m_outFrequencyInHz);
        
        const size_t sampleCount = size / bytesPerSample;
        const size_t outSampleCount = sampleCount / ratio;
        const size_t outBufferSize = outSampleCount * m_bytesPerSample;
        
        const auto outBuffer = std::make_shared<Buffer>(outBufferSize);
        
        uint8_t * pOutBuffer = NULL;
        uint8_t * pInBuffer = const_cast<uint8_t*>(buffer);

        outBuffer->read(&pOutBuffer, outBufferSize);

        int16_t* currSample = (int16_t*)(&pOutBuffer[0]);
        
        double currentInByteOffset = 0.;
        const double sampleStride = ratio * static_cast<double>(bytesPerSample);
        
        const size_t channelStride = (inChannelCount > 1) * bytesPerChannel;
        
        // Sample rate conversion is achieved by simple lerp for now.
        for( size_t i = 0 ; i < outSampleCount ; ++i )
        {
            size_t iSample = (static_cast<size_t>(std::floor(currentInByteOffset)) + (bytesPerSample-1)) & ~(bytesPerSample-1); // get an aligned sample.
            
            
            currentInByteOffset += sampleStride;
            
            pInBuffer = (const_cast<uint8_t*>(buffer)+iSample);
            
            int16_t sampleL = bitconvert(pInBuffer);
            int16_t sampleR = bitconvert(pInBuffer + channelStride);
            
            *currSample++ = sampleL;
            *currSample++ = sampleR;

            
        }
        outBuffer->setSize(outBufferSize);
        return outBuffer;
    }
    void
    GenericAudioMixer::setOutput(std::shared_ptr<IOutput> output)
    {
        m_output = output;
    }
    void
    GenericAudioMixer::setSourceGain(std::weak_ptr<ISource> source, float gain)
    {
        auto s = source.lock();
        if(s) {
            auto hash = std::hash<std::shared_ptr<ISource>>()(s);
            
            m_inGain[hash] = std::max(0.f, std::min(1.f, gain));
            
        }
    }
    
    void
    GenericAudioMixer::mixThread()
    {
        const auto us = std::chrono::microseconds(static_cast<long long>(m_bufferDuration * 1000000.)) ;
        const float g = 0.70710678118f; // 1 / sqrt(2)
        const size_t outSampleCount = static_cast<size_t>(m_outFrequencyInHz * m_bufferDuration);
        const size_t outBufferSize = outSampleCount * m_bytesPerSample ;
        const std::unique_ptr<short[]> buffer(new short[outBufferSize / sizeof(short)]);
        const std::unique_ptr<short[]> samples(new short[outBufferSize / sizeof(short)]);
        
        auto nextMixTime = std::chrono::high_resolution_clock::now();

        while(!m_exiting.load()) {
            std::unique_lock<std::mutex> l(m_mixMutex);
            
            if(std::chrono::high_resolution_clock::now() > nextMixTime) {
                nextMixTime = std::chrono::high_resolution_clock::now() + us;
                size_t sampleBufferSize = 0;
                
                // Mix and push
                for ( auto it = m_inBuffer.begin() ; it != m_inBuffer.end() ; ++it )
                {
                    auto size = it->second->get((uint8_t*)&buffer[0], outBufferSize);
                    if(size > sampleBufferSize) {
                        sampleBufferSize = size;
                    }
                    const size_t count = (size/sizeof(short));
                    const float gain = m_inGain[it->first];
                    const float mult = g*gain;
                    const short div ( 1.f / mult );
                    for ( size_t i = 0 ; i <  count ; i+=8) {
                        samples[i] += buffer[i] / div;
                        samples[i+1] += buffer[i+1] / div;
                        samples[i+2] += buffer[i+2] / div;
                        samples[i+3] += buffer[i+3] / div;
                        samples[i+4] += buffer[i+4] / div;
                        samples[i+5] += buffer[i+5] / div;
                        samples[i+6] += buffer[i+6] / div;
                        samples[i+7] += buffer[i+7] / div;
                    }
                    
                }
                
                if(sampleBufferSize) {
                    MetaData<'soun'> md ( m_bufferDuration );
                    
                    auto out = m_output.lock();
                    if(out) {
                        out->pushBuffer((uint8_t*)&samples[0], sampleBufferSize, md);
                    }
                }
                
                memset(samples.get(), 0, outBufferSize);
            }
            usleep(100);
        }

    }
}
