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
#include <videocore/mixers/GenericAudioMixer.h>
#include <sstream>
#include <vector>
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

    inline SInt16 TPMixSamples(SInt16 a, SInt16 b) {
        return
        // If both samples are negative, mixed signal must have an amplitude between the lesser of A and B, and the minimum permissible negative amplitude
        a < 0 && b < 0 ?
        ((int)a + (int)b) - (((int)a * (int)b)/INT16_MIN) :
        
        // If both samples are positive, mixed signal must have an amplitude between the greater of A and B, and the maximum permissible positive amplitude
        ( a > 0 && b > 0 ?
         ((int)a + (int)b) - (((int)a * (int)b)/INT16_MAX)
         
         // If samples are on opposite sides of the 0-crossing, mixed signal should reflect that samples cancel each other out somewhat
         :
         a + b);
    }
    
    GenericAudioMixer::GenericAudioMixer(int outChannelCount,
                                         int outFrequencyInHz,
                                         int outBitsPerChannel,
                                         double frameDuration)
    :
    m_bufferDuration(frameDuration),
    m_frameDuration(frameDuration),
    m_outChannelCount(outChannelCount),
    m_outFrequencyInHz(outFrequencyInHz),
    m_outBitsPerChannel(16),
    m_exiting(false)
    {
        m_bytesPerSample = outChannelCount * outBitsPerChannel / 8;

        //m_outBuffer.reset(new RingBuffer( m_bytesPerSample * outFrequencyInHz )); // 1 second ring buffer
        const int windowCount = 4;
        for ( int i = 0 ; i < windowCount ; ++i ) {
            m_windows.emplace_back(m_bytesPerSample * frameDuration * outFrequencyInHz);
        }
        for ( int i = 0 ; i < windowCount-1 ; ++i ) {
            m_windows[i].next = &m_windows[i+1];
        }
        m_windows[windowCount-1].next = &m_windows[0];
        m_currentWindow = &m_windows[0];
        m_currentWindow->start = std::chrono::steady_clock::now();
        
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
    GenericAudioMixer::setMinimumBufferDuration(const double duration)
    {
        m_bufferDuration = duration;
    }
    void
    GenericAudioMixer::registerSource(std::shared_ptr<ISource> source,
                                      size_t inBufferSize)
    {
        m_mixInProgress.lock();
        auto hash = std::hash<std::shared_ptr< ISource> >()(source);
        size_t bufferSize = (inBufferSize ? inBufferSize : (m_bytesPerSample * m_outFrequencyInHz * m_bufferDuration * 4)); // 4 frames of buffer space.

        std::unique_ptr<RingBuffer> buffer(new RingBuffer(bufferSize));

        //m_inBuffer[hash] = std::move(buffer);
        m_inGain[hash] = 1.f;
        m_mixInProgress.unlock();
    }
    void
    GenericAudioMixer::unregisterSource(std::shared_ptr<ISource> source)
    {
        m_mixInProgress.lock();
        auto hash = std::hash<std::shared_ptr< ISource> >()(source);

        /*auto it = m_inBuffer.find(hash);
        if(it != m_inBuffer.end())
        {
            m_inBuffer.erase(it);
        }*/
        auto iit = m_inGain.find(hash);
        if(iit != m_inGain.end()) {
            m_inGain.erase(iit);
        }
        m_mixInProgress.unlock();
    }
    void
    GenericAudioMixer::pushBuffer(const uint8_t* const data,
                                  size_t size,
                                  IMetadata& metadata)
    {
        AudioBufferMetadata & inMeta = static_cast<AudioBufferMetadata&>(metadata);
        
        if(inMeta.size() >= 5) {
            const auto inSource = inMeta.getData<kAudioMetadataSource>() ;
            
            auto lSource = inSource.lock();
            if(lSource) {
                
                auto mixTime = std::chrono::steady_clock::now();
                
                auto ret = resample(data, size, inMeta);
            
                if(ret->size() == 0) {
                    ret->put((uint8_t*)data, size);
                }
                const float g = 0.70710678118f; // 1 / sqrt(2)
                
                m_mixQueue.enqueue([=] {
                    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(mixTime - m_currentWindow->start).count();
                    size_t startOffset = 0;
                    size_t bytesLeft = ret->size();
                    
                    MixWindow* window = m_currentWindow;
                    if(diff > 0) {
                        startOffset = size_t((float(diff) / 1.0e6f) * m_outFrequencyInHz * m_bytesPerSample) & ~(m_bytesPerSample-1);
                        
                        if ( startOffset >= window->size ) {
                            window = window->next;
                            startOffset = 0;
                        }
                    }
                    off_t currOffset = 0;
                    
                    while(bytesLeft > 0) {
                        size_t toCopy = std::min(window->size - startOffset, bytesLeft);
                        DLog("mixing %zu", toCopy);
                        
                        uint8_t* p ;
                        ret->read(&p, toCopy);
                        p += currOffset;
                        
                        short* mix = (short*)p;
                        short* winMix = (short*)(window->buffer+startOffset);
                        size_t count = toCopy / 2;
                        
                        for ( size_t i = 0 ; i < count ; ++i ) {
                            winMix[i] = TPMixSamples(winMix[i], mix[i] * g);
                        }
                        
                        currOffset += toCopy;
                        bytesLeft -= toCopy;
                        if(bytesLeft) {
                            window = window->next;
                            startOffset = 0;
                        }
                    }
                    
                });
            }
        }
    }
    std::shared_ptr<Buffer>
    GenericAudioMixer::resample(const uint8_t* const buffer,
                                size_t size,
                                AudioBufferMetadata &metadata)
    {
        const auto inFrequncyInHz = metadata.getData<kAudioMetadataFrequencyInHz>();
        auto inBitsPerChannel = metadata.getData<kAudioMetadataBitsPerChannel>();
        const auto inChannelCount = metadata.getData<kAudioMetadataChannelCount>();
        const auto inFlags = metadata.getData<kAudioMetadataFlags>();
        const auto inNumberFrames = metadata.getData<kAudioMetadataNumberFrames>();

        if(m_outFrequencyInHz == inFrequncyInHz && m_outBitsPerChannel == inBitsPerChannel && m_outChannelCount == inChannelCount)
        {
            // No resampling necessary
            return std::make_shared<Buffer>();
        }

        int16_t (*bitconvert)(void* val) = NULL;

        //
        
        uint8_t* intBuffer = nullptr;
        uint8_t * pOutBuffer = NULL;
        uint8_t * pInBuffer = const_cast<uint8_t*>(buffer);
        
        if(inFlags & 1) {
            // Floating point lpcm
            inBitsPerChannel = m_outBitsPerChannel;
            
            intBuffer = (uint8_t*) malloc(inNumberFrames * 4);

            deinterleaveDefloat((float*)buffer, (short*)intBuffer,(int) inNumberFrames, inChannelCount);
            size = inNumberFrames * 4;
            pInBuffer = intBuffer;
            
        }

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

        const size_t sampleCount = inNumberFrames;
        const size_t outSampleCount = sampleCount / ratio;
        const size_t outBufferSize = outSampleCount * m_bytesPerSample;

        const auto outBuffer = std::make_shared<Buffer>(outBufferSize);

    
        outBuffer->read(&pOutBuffer, outBufferSize);

        int16_t* currSample = (int16_t*)(&pOutBuffer[0]);

        float currentInByteOffset = 0.;
        const float sampleStride = ratio * static_cast<float>(bytesPerSample);

        const size_t channelStride = (inChannelCount > 1) * bytesPerChannel;

        const uint8_t* pInStart = pInBuffer;
        
        for( size_t i = 0 ; i < outSampleCount ; ++i )
        {
            size_t iSample = (static_cast<size_t>(std::floor(currentInByteOffset)) + (bytesPerSample-1)) & ~(bytesPerSample-1); // get an aligned sample.


            currentInByteOffset += sampleStride;

            pInBuffer = (const_cast<uint8_t*>(pInStart)+iSample);

            int16_t sampleL = bitconvert(pInBuffer);
            int16_t sampleR = bitconvert(pInBuffer + channelStride);

            *currSample++ = sampleL;
            *currSample++ = sampleR;


        }
        if(intBuffer) {
            free(intBuffer);
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
    GenericAudioMixer::setSourceGain(std::weak_ptr<ISource> source,
                                     float gain)
    {
        auto s = source.lock();
        if(s) {
            auto hash = std::hash<std::shared_ptr<ISource>>()(s);

            m_inGain[hash] = std::max(0.f, std::min(1.f, gain));

        }
    }
    void
    GenericAudioMixer::setChannelCount(int channelCount)
    {
        m_outChannelCount = channelCount;
    }
    void
    GenericAudioMixer::setFrequencyInHz(float frequencyInHz)
    {
        m_outFrequencyInHz = frequencyInHz;
    }

    void
    GenericAudioMixer::mixThread()
    {
        const auto us = std::chrono::microseconds(static_cast<long long>(m_frameDuration * 1000000.)) ;
        const float g = 0.70710678118f; // 1 / sqrt(2)
        const size_t outSampleCount = static_cast<size_t>(m_outFrequencyInHz * m_frameDuration);
        const size_t outBufferSize = outSampleCount * m_bytesPerSample ;

        const size_t requiredSampleCount = static_cast<size_t>(m_outFrequencyInHz * m_bufferDuration);
        const size_t requiredBufferSize = requiredSampleCount * m_bytesPerSample;
        
        std::vector<short> buffer;
        std::vector<short> samples;

        buffer.resize(outBufferSize / sizeof(short));
        samples.resize(outBufferSize / sizeof(short));
        
        m_nextMixTime = std::chrono::steady_clock::now() + us;

        while(!m_exiting.load()) {
            std::unique_lock<std::mutex> l(m_mixMutex);

            const auto now = std::chrono::steady_clock::now();

            //size_t sampleBufferSize = 0;
            
            if( now >= m_nextMixTime) {

                m_nextMixTime += us;
                
                MixWindow* window = m_currentWindow;
                
                m_mixQueue.enqueue_sync([&]{
                    m_currentWindow = m_currentWindow->next;
                    m_currentWindow->start = std::chrono::steady_clock::now();
                    
                });
                
                AudioBufferMetadata md ( std::chrono::duration_cast<std::chrono::milliseconds>(m_nextMixTime - m_epoch).count() );
                std::shared_ptr<videocore::ISource> blank;
                    
                md.setData(m_outFrequencyInHz, m_outBitsPerChannel, m_outChannelCount, 0, 0, (int)window->size, false, blank);
                auto out = m_output.lock();
                if(out) {
                    out->pushBuffer(window->buffer, window->size, md);
                }
                window->clear();
               
                /*m_nextMixTime += us;

                // Mix and push
                m_mixInProgress.lock();
                m_lastMixTime = now;
                for ( auto it = m_inBuffer.begin() ; it != m_inBuffer.end() ; ++it )
                {
                    
                    auto size = it->second->get((uint8_t*)&buffer[0], outBufferSize, true);
                    
                    if(size > sampleBufferSize) {
                        sampleBufferSize = size;
                    }
                    
                    const size_t count = (size/sizeof(short));
                    const float gain = m_inGain[it->first];
                    const float mult = g*gain;
                    
                    for (size_t i = 0 ; i < count ; ++i ) {
                        samples[i] = TPMixSamples(samples[i], buffer[i] * mult);
                    }
                    
                }
                m_outBuffer->put((uint8_t*)&samples[0], sampleBufferSize);

                m_mixInProgress.unlock();
                if(m_outBuffer->size() >= requiredBufferSize) {

                    AudioBufferMetadata md ( std::chrono::duration_cast<std::chrono::milliseconds>(m_nextMixTime - m_epoch).count() );
                    std::shared_ptr<videocore::ISource> blank;

                    md.setData(m_outFrequencyInHz, m_outBitsPerChannel, m_outChannelCount, 0, 0, (int)requiredSampleCount, false, blank);

                    uint8_t outSamples[requiredBufferSize];
                    
                    m_outBuffer->get(outSamples, requiredBufferSize, true);
                    
                    auto out = m_output.lock();
                    if(out) {
                        out->pushBuffer(outSamples, requiredBufferSize, md);
                    }
                    
                }
                
                memset(&samples[0], 0, outBufferSize);
                sampleBufferSize = 0;
                */
            }
            if(!m_exiting.load()) {
                m_mixThreadCond.wait_until(l, m_nextMixTime);
            }
        }
        DLog("Exiting audio mixer...\n");
    }
    void
    GenericAudioMixer::deinterleaveDefloat(float *inBuff,
                                           short *outBuff,
                                           unsigned sampleCount,
                                           unsigned channelCount)
    {
        unsigned offset = sampleCount;


        const float mult = float(0x7FFF);
        
        if(channelCount == 2) {
            for ( int i = 0 ; i < sampleCount ; i+=2 ) {
                outBuff[i] = short(inBuff[i] * mult);
                outBuff[i+1] = short(inBuff[i+offset] * mult);
            }
        } else {
            for (int i = 0 ; i < sampleCount ; i++ ) {
                outBuff[i] = short(std::min(1.f,std::max(-1.f,inBuff[i])) * mult);
            }
        }

    }
}
