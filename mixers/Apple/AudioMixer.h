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

#ifndef __videocore__AudioMixer__
#define __videocore__AudioMixer__

#include <iostream>
#include <videocore/mixers/GenericAudioMixer.h>
#include <AudioToolbox/AudioToolbox.h>
#include <unordered_map>

namespace videocore { namespace Apple {
    /*
     *   Takes raw LPCM buffers from a variety of sources, resamples, and mixes them and ouputs a single LPCM buffer.
     *   Differs from GenericAudioMixer because it uses CoreAudio to resample.
     *
     */
    class AudioMixer : public GenericAudioMixer
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
        AudioMixer(int outChannelCount,
                   int outFrequencyInHz,
                   int outBitsPerChannel,
                   double frameDuration);
        
        /*! Destructor */
        ~AudioMixer();
        
    protected:
        
        /*!
         *  Called to resample a buffer of audio samples. You can change the quality of the resampling method
         *  by changing s_samplingRateConverterComplexity and s_samplingRateConverterQuality in Apple/AudioMixer.cpp.
         *
         * \param buffer    The input samples
         * \param size      The buffer size in bytes
         * \param metadata  The associated AudioBufferMetadata that specifies the properties of this buffer.
         *
         * \return An audio buffer that has been resampled to match the output properties of the mixer.
         */
        std::shared_ptr<Buffer> resample(const uint8_t* const buffer,
                                         size_t size,
                                         AudioBufferMetadata& metadata);

    private:
        /*! Used by AudioConverterFillComplexBuffer. Do not call manually. */
        static OSStatus ioProc(AudioConverterRef audioConverter,
                               UInt32 *ioNumDataPackets,
                               AudioBufferList* ioData,
                               AudioStreamPacketDescription** ioPacketDesc,
                               void* inUserData );
        
    private:
        
        using ConverterInst = struct { AudioStreamBasicDescription asbdIn, asbdOut; AudioConverterRef converter; };
        
        std::unordered_map<uint64_t, ConverterInst> m_converters;
        
        
    };
}
}
#endif /* defined(__videocore__AudioMixer__) */
