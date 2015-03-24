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

#ifndef __videocore__GenericAudioMixer__
#define __videocore__GenericAudioMixer__

#include <iostream>
#include <videocore/mixers/IAudioMixer.hpp>
#include <videocore/system/Buffer.hpp>
#include <videocore/system/JobQueue.hpp>

#include <map>
#include <thread>
#include <mutex>

namespace videocore {

    struct MixWindow {
        MixWindow(size_t size) : start(std::chrono::steady_clock::now()) {
            buffer = new uint8_t[size]();
            this->size = size;
        }
        ~MixWindow() {
            delete [] buffer;
        }
        void clear() {
            memset(buffer, 0, size);
        }
        
        std::chrono::steady_clock::time_point start;
        size_t     size;
        MixWindow* next;
        MixWindow* prev;
        
        uint8_t*   buffer;

    };
    /*!
     *  Basic, cross-platform mixer that uses a very simple nearest neighbour resampling method
     *  and the sum of the samples to mix.  The mixer takes LPCM data from multiple sources, resamples (if needed), and
     *  mixes them to output a single LPCM stream.
     *
     *  Note that this mixer uses an extremely simple sample rate conversion algorithm that will produce undesirable
     *  noise in most cases, but it will be much less CPU intensive than more sophisticated methods.  If you are using an Apple
     *  operating system and can dedicate more CPU resources to sample rate conversion, look at videocore::Apple::AudioMixer.
     */
    class GenericAudioMixer : public IAudioMixer
    {

    public:
        /*!
         *  Constructor.
         *
         *  \param outChannelCount      number of channels to output.
         *  \param outFrequencyInHz     sampling rate to output.
         *  \param outBitsPerChannel    number of bits per channel to output
         *  \param frameDuration        The duration of a single frame of audio.  For example, AAC uses 1024 samples per frame
         *                              and therefore the duration is 1024 / sampling rate
         */
        GenericAudioMixer(int outChannelCount,
                          int outFrequencyInHz,
                          int outBitsPerChannel,
                          double frameDuration);

        /*! Destructor */
        ~GenericAudioMixer();

    public:
        /*! IMixer::registerSource */
        void registerSource(std::shared_ptr<ISource> source,
                            size_t inBufferSize = 0)  ;

        /*! IMixer::unregisterSource */
        void unregisterSource(std::shared_ptr<ISource> source);

        /*! IOutput::pushBuffer */
        void pushBuffer(const uint8_t* const data,
                        size_t size,
                        IMetadata& metadata);

        /*! ITransform::setOutput */
        void setOutput(std::shared_ptr<IOutput> output);

        /*! IAudioMixer::setSourceGain */
        void setSourceGain(std::weak_ptr<ISource> source,
                           float gain);

        /*! IAudioMixer::setChannelCount */
        void setChannelCount(int channelCount);

        /*! IAudioMixer::setFrequencyInHz */
        void setFrequencyInHz(float frequencyInHz);

        /*! IAudioMixer::setMinimumBufferDuration */
        virtual void setMinimumBufferDuration(const double duration) ;

        /*! ITransform::setEpoch */
        void setEpoch(const std::chrono::steady_clock::time_point epoch) {
            m_epoch = epoch;
            m_nextMixTime = epoch;
        };

        void start();
        
    protected:

        /*!
         *  Called to resample a buffer of audio samples.
         *
         * \param buffer    The input samples
         * \param size      The buffer size in bytes
         * \param metadata  The associated AudioBufferMetadata that specifies the properties of this buffer.
         *
         * \return An audio buffer that has been resampled to match the output properties of the mixer.
         */
        virtual std::shared_ptr<Buffer> resample(const uint8_t* const buffer,
                                                 size_t size,
                                                 AudioBufferMetadata& metadata);

        /*!
         *  Start the mixer thread.
         */
        void mixThread();

        
        void deinterleaveDefloat(float* inBuff, short* outBuff, unsigned sampleCount, unsigned channelCount);
    protected:
        
        std::vector<std::shared_ptr<MixWindow>>                m_windows;
        MixWindow*                            m_currentWindow;
        MixWindow*                            m_outgoingWindow;
        
        JobQueue                              m_mixQueue;
        
        std::chrono::steady_clock::time_point m_epoch;
        std::chrono::steady_clock::time_point m_nextMixTime;
        std::chrono::steady_clock::time_point m_lastMixTime;
        
        double m_frameDuration;
        double m_bufferDuration;

        
        std::thread m_mixThread;
        std::mutex  m_mixMutex;
        std::condition_variable m_mixThreadCond;

        std::weak_ptr<IOutput> m_output;

        std::map < std::size_t, float > m_inGain;
        std::map < std::size_t, std::chrono::steady_clock::time_point > m_lastSampleTime;
        
        int m_outChannelCount;
        int m_outFrequencyInHz;
        int m_outBitsPerChannel;
        int m_bytesPerSample;

        std::atomic<bool> m_exiting;
        
        bool m_catchingUp;

    };
}
#endif /* defined(__videocore__GenericAudioMixer__) */
