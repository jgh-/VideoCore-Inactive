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
#include <stdint.h>


#ifndef INT16_MAX
#define INT16_MAX 0x7FFF
#endif
#ifndef INT16_MIN
#define INT16_MIN ~INT16_MAX
#endif

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

static const int kMixWindowCount = 10;
//static const int kWindowBufferCount = 0;

static const float kE = 2.7182818284590f;

namespace videocore {

    inline int16_t TPMixSamples(int16_t a, int16_t b) {
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
    m_exiting(false),
    m_mixQueue("com.videocore.audiomix", kJobQueuePriorityHigh),
    m_outgoingWindow(nullptr),
    m_catchingUp(false),
    m_epoch(std::chrono::steady_clock::now())
    {
        m_bytesPerSample = outChannelCount * outBitsPerChannel / 8;

        
        for ( int i = 0 ; i < kMixWindowCount ; ++i ) {
            m_windows.emplace_back(std::make_shared<MixWindow>(m_bytesPerSample * frameDuration * outFrequencyInHz));
        }
        for ( int i = 0 ; i < kMixWindowCount-1 ; ++i ) {
            m_windows[i]->next = m_windows[i+1].get();
            m_windows[i+1]->prev = m_windows[i+1].get();
        }
        m_windows[kMixWindowCount-1]->next = m_windows[0].get();
        m_windows[0]->prev = m_windows[kMixWindowCount-1].get();
        
        m_currentWindow = m_windows[0].get();
        m_currentWindow->start = std::chrono::steady_clock::now();


    }
    GenericAudioMixer::~GenericAudioMixer()
    {
        m_exiting = true;
        m_mixThreadCond.notify_all();
        if(m_mixThread.joinable()) {
            m_mixThread.join();
        }
        m_mixQueue.mark_exiting();
        m_mixQueue.enqueue_sync([]() {});
    }
    void
    GenericAudioMixer::start()
    {
        m_mixThread = std::thread([this]() {
            pthread_setname_np("com.videocore.audiomixer");
            this->mixThread();
        });
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
        auto hash = std::hash<std::shared_ptr< ISource> >()(source);
        size_t bufferSize = (inBufferSize ? inBufferSize : (m_bytesPerSample * m_outFrequencyInHz * m_bufferDuration * 4)); // 4 frames of buffer space.

        std::unique_ptr<RingBuffer> buffer(new RingBuffer(bufferSize));
        
        m_inGain[hash] = 1.f;
    }
    void
    GenericAudioMixer::unregisterSource(std::shared_ptr<ISource> source)
    {
        auto hash = std::hash<std::shared_ptr< ISource> >()(source);

        m_mixQueue.enqueue([=]() {
            auto iit = m_inGain.find(hash);
            if(iit != m_inGain.end()) {
                m_inGain.erase(iit);
            }
        });

    }
    void
    GenericAudioMixer::pushBuffer(const uint8_t* const data,
                                  size_t size,
                                  IMetadata& metadata)
    {
        AudioBufferMetadata & inMeta = static_cast<AudioBufferMetadata&>(metadata);
        
        if(inMeta.size() >= 5) {
            const auto inSource = inMeta.getData<kAudioMetadataSource>() ;
            const auto cMixTime = std::chrono::steady_clock::now();
            MixWindow* currentWindow = m_currentWindow;
            auto lSource = inSource.lock();
            if(lSource) {
                
                auto ret = resample(data, size, inMeta);
            
                if(ret->size() == 0) {
                    ret->resize(size);
                    ret->put((uint8_t*)data, size);
                }
                
                
                m_mixQueue.enqueue([=]() {
                    auto mixTime = cMixTime;
                    
                    const float g = 0.70710678118f; // 1 / sqrt(2)
                    
                    const auto lSource = inSource.lock();
                    if(!lSource) return ;
                    
                    const auto hash = std::hash<std::shared_ptr<ISource>>()(lSource);
                    
                    auto it = m_lastSampleTime.find(hash);
                    
                    if(it != m_lastSampleTime.end() && (mixTime - it->second) < std::chrono::microseconds(int64_t(m_frameDuration * 0.25e6f))) {
                        mixTime = it->second;
                    }
                    
                    size_t startOffset = 0;
                
                    MixWindow* window = currentWindow;
                
                    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(mixTime - window->start).count();

                    if(diff > 0) {
                        startOffset = size_t((float(diff) / 1.0e6f) * m_outFrequencyInHz * m_bytesPerSample) & ~(m_bytesPerSample-1);
                        
                        while ( startOffset >= window->size ) {
                            startOffset = (startOffset - window->size);
                            window = window->next;
                            
                        }
                        
                    } else {
                        startOffset = 0;
                    }
                    
                    auto sampleDuration = double(ret->size()) / double(m_bytesPerSample * m_outFrequencyInHz);

                    const float mult = m_inGain[hash] * g;
                    
                    uint8_t* p ;
                    ret->read(&p, ret->size());
                    size_t bytesLeft = ret->size();

                    size_t so = startOffset;
                    
                    while(bytesLeft > 0) {
                        size_t toCopy = std::min(window->size - so, bytesLeft);

                        short* mix = (short*)p;
                        short* winMix = (short*)(window->buffer+so);
                        size_t count = toCopy / sizeof(short);
                        
                        for ( size_t i = 0 ; i < count  ; i++ ) {
                            winMix[i] = TPMixSamples(winMix[i], mix[i]*mult);
                        }
                        
                        p += toCopy;
                        bytesLeft -= toCopy;
                        
                        if(bytesLeft) {
                            window = window->next;
                            so = 0;
                        }
                    }
                    m_lastSampleTime[hash] = mixTime + std::chrono::microseconds(int64_t(sampleDuration*1.0e6));
                    
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
            size_t iSample = (static_cast<size_t>(std::floor(currentInByteOffset))) & ~(bytesPerSample-1); // get an aligned sample.


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
            
            gain = std::max(0.f, std::min(1.f, gain));
            gain = powf(gain, kE);
            m_inGain[hash] = gain;

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

        const auto start = m_epoch;
        
        m_nextMixTime = start;
        m_currentWindow->start = start;
        m_currentWindow->next->start = start + us;
        
        while(!m_exiting.load()) {
            std::unique_lock<std::mutex> l(m_mixMutex);

            auto now = std::chrono::steady_clock::now();
            
            if( now >= m_currentWindow->next->start ) {
                
                auto currentTime = m_nextMixTime;
                
                
                MixWindow* currentWindow = m_currentWindow;
                MixWindow* nextWindow = currentWindow->next;
                
                nextWindow->start = currentWindow->start + us;
                nextWindow->next->start = nextWindow->start + us;
                
                m_nextMixTime = currentWindow->start;
                
                AudioBufferMetadata md ( std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_epoch).count() );
                std::shared_ptr<videocore::ISource> blank;
                    
                md.setData(m_outFrequencyInHz, m_outBitsPerChannel, m_outChannelCount, 0, 0, (int)currentWindow->size, false, false, blank);
                auto out = m_output.lock();
                
                if(out && m_outgoingWindow) {
                    out->pushBuffer(m_outgoingWindow->buffer, m_outgoingWindow->size, md);
                    m_outgoingWindow->clear();
                }
                m_outgoingWindow = currentWindow;
               
                m_currentWindow = nextWindow;
                
            }
            if(!m_exiting.load()) {
                m_mixThreadCond.wait_until(l, m_currentWindow->next->start);
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
