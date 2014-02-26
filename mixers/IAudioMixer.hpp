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
#ifndef videocore_IAudioMixer_hpp
#define videocore_IAudioMixer_hpp

#include <videocore/system/Buffer.hpp>
#include <videocore/mixers/IMixer.hpp>
#include <videocore/transforms/IMetadata.hpp>

namespace videocore {
    
    enum {
        kAudioMetadataFrequencyInHz,
        kAudioMetadataBitsPerChannel,
        kAudioMetadataChannelCount,
        kAudioMetadataLoops,
        kAudioMetadataSource
    };
    typedef MetaData<'soun', int, int, int, bool, std::weak_ptr<ISource> > AudioBufferMetadata;
    
    class ISource;
    
    class IAudioMixer : public IMixer
    {
    public:
        virtual ~IAudioMixer() {};
        virtual void setSourceGain(std::weak_ptr<ISource> source, float gain) = 0;
    };
}

#endif
