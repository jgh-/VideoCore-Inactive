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
        
        AudioMixer(int outChannelCount, int outFrequencyInHz, int outBitsPerChannel, double outBufferDuration);
        ~AudioMixer();
        
    protected:
        
        std::shared_ptr<Buffer> resample(const uint8_t* const buffer, size_t size, AudioBufferMetadata& metadata);

    private:
        static OSStatus ioProc(AudioConverterRef audioConverter, UInt32 *ioNumDataPackets, AudioBufferList* ioData, AudioStreamPacketDescription** ioPacketDesc, void* inUserData );
        
    };
}
}
#endif /* defined(__videocore__AudioMixer__) */
