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
 *  Apple::AudioMixer mixer :: Takes raw LPCM buffers from a variety of sources, resamples, and mixes them and ouputs a single LPCM buffer.
 *                             Differs from GenericAudioMixer because it uses CoreAudio to resample
 *
 */
#ifndef __videocore__AudioMixer__
#define __videocore__AudioMixer__

#include <iostream>
#include <videocore/mixers/GenericAudioMixer.h>
#include <AudioToolbox/AudioToolbox.h>

namespace videocore { namespace Apple {
 
    class AudioMixer : public GenericAudioMixer
    {
    public:
        
        AudioMixer(int outChannelCount, int outFrequencyInHz, int outBitsPerChannel, double frameDuration);
        ~AudioMixer();
        
    protected:
        
        std::shared_ptr<Buffer> resample(const uint8_t* const buffer, size_t size, AudioBufferMetadata& metadata);

    private:
        static OSStatus ioProc(AudioConverterRef audioConverter, UInt32 *ioNumDataPackets, AudioBufferList* ioData, AudioStreamPacketDescription** ioPacketDesc, void* inUserData );
        
    };
}
}
#endif /* defined(__videocore__AudioMixer__) */
