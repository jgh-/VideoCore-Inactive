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

static const int kMixWindowCount = 5;
static const int kWindowBufferCount = 1;

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
        auto hash = std::hash<std::shared_ptr< ISource> >()(source);
        size_t bufferSize = (inBufferSize ? inBufferSize : (m_bytesPerSample * m_outFrequencyInHz * m_bufferDuration * 4)); // 4 frames of buffer space.

        std::unique_ptr<RingBuffer> buffer(new RingBuffer(bufferSize));

        m_inGain[hash] = 1.f;
    }
    void
    GenericAudioMixer::unregisterSource(std::shared_ptr<ISource> source)
    {
        auto hash = std::hash<std::shared_ptr< ISource> >()(source);

        auto iit = m_inGain.find(hash);
        if(iit != m_inGain.end()) {
            m_inGain.erase(iit);
        }

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
                
                const auto cMixTime = std::chrono::steady_clock::now();
                
                auto ret = resample(data, size, inMeta);
            
                if(ret->size() == 0) {
                    ret->resize(size);
                    ret->put((uint8_t*)data, size);
                }

                m_mixQueue.enqueue([=]() {
                    auto mixTime = cMixTime;
                    auto now = std::chrono::steady_clock::now();
                    
                    const float g = 0.70710678118f; // 1 / sqrt(2)
                    
                    const auto hash = std::hash<std::shared_ptr<ISource>>()(inSource.lock());
                    auto it = m_lastSampleTime.find(hash);

                    if(it != m_lastSampleTime.end() && (mixTime - it->second) < std::chrono::microseconds(int64_t(m_frameDuration * 0.5e6f ))) {
                        mixTime = it->second;
                    }
                    
                    MixWindow* window = this->m_currentWindow;
                    int oldBufferTraverseCount = 1;
                    
                    
                    size_t startOffset = 0;
                    size_t bytesLeft = ret->size();
                
                    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(mixTime - window->start).count();
                    
                    do {
                
                        if (diff < 0 && oldBufferTraverseCount < kWindowBufferCount ) {
                            window = window->prev;
                            oldBufferTraverseCount++;
                            diff = std::chrono::duration_cast<std::chrono::microseconds>(mixTime - window->start).count();
                        }
                        
                        if(diff > 0) {
                            startOffset = size_t((float(diff) / 1.0e6f) * m_outFrequencyInHz * m_bytesPerSample) & ~(m_bytesPerSample-1);
                            
                            while ( startOffset >= window->size ) {
                                //DLog("startOffset >= window->size :: %zu >= %zu", startOffset, window->size);
                                startOffset = (startOffset - window->size);
                                window = window->next;
                                
                            }
                        }
                        
                    }  while(oldBufferTraverseCount < kWindowBufferCount && diff < 0);
                    if(diff < 0) {
                       // DLog("Buffer still too far in the past! %lld [enqueue:%lld]", diff, std::chrono::duration_cast<std::chrono::microseconds>(now - cMixTime).count());
                        mixTime = window->start;
                    }
                    
                    auto sampleDuration = double(bytesLeft) / double(m_bytesPerSample * m_outFrequencyInHz);
                    
                    uint8_t* p ;
                    ret->read(&p, bytesLeft);
                    
                    if((window->size - startOffset) < bytesLeft) {
                       // DLog("window[%zu] offset[%zu] buffer[%zu] diff[%lld]", window->size, startOffset, bytesLeft, diff);
                    }
                    while(bytesLeft > 0) {
                        size_t toCopy = std::min(window->size - startOffset, bytesLeft);

                        short* mix = (short*)p;
                        short* winMix = (short*)(window->buffer+startOffset);
                        size_t count = toCopy / sizeof(short);
                        
                        const float mult = m_inGain[hash] * g;
                        
                        for ( size_t i = 0 ; i < count  ; i++ ) {
                            winMix[i]   += mix[i] * mult;
                        }
                        
                        p += toCopy;
                        bytesLeft -= toCopy;
                        if(bytesLeft) {
                            //DLog("window->next: toCopy[%zu] bytesLeft[%zu]", toCopy, bytesLeft);
                            window = window->next;
                            startOffset = 0;
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

        auto now = std::chrono::steady_clock::now();
        
        m_nextMixTime = now  + us;
        m_currentWindow->start = now;
        /*{
            std::string home = getenv("HOME");
            std::string dir = home + "/Documents/out" + std::to_string(m_outFrequencyInHz) + ".pcm";
            FILE * fp = fopen(dir.c_str(), "w+b");
            fclose(fp);
        } */
        while(!m_exiting.load()) {
            std::unique_lock<std::mutex> l(m_mixMutex);

            now = std::chrono::steady_clock::now();
            
            if( now >= m_nextMixTime) {

                m_nextMixTime += us;
                
                MixWindow* currentWindow = m_currentWindow;
                int backBufferCount = 0;
                
                MixWindow* window = m_currentWindow;
                while ( backBufferCount < (kWindowBufferCount-1)) {
                    window = window->prev;
                    ++backBufferCount;
                }
                m_currentWindow = currentWindow->next;
                m_currentWindow->start = currentWindow->start + us;

                AudioBufferMetadata md ( std::chrono::duration_cast<std::chrono::milliseconds>(m_nextMixTime - m_epoch).count() );
                std::shared_ptr<videocore::ISource> blank;
                    
                md.setData(m_outFrequencyInHz, m_outBitsPerChannel, m_outChannelCount, 0, 0, (int)window->size, false, blank);
                auto out = m_output.lock();
                if(out) {
                    out->pushBuffer(window->buffer, window->size, md);
                }
               /* {
                    std::string home = getenv("HOME");
                    std::string dir = home + "/Documents/out" + std::to_string(m_outFrequencyInHz) + ".pcm";
                    FILE * fp = fopen(dir.c_str(), "a+b");
                    fwrite(window->buffer, window->size, 1, fp);
                    fclose(fp);
                } */
                window->clear();
               
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
