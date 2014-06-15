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
        virtual void setMinimumBufferDuration(const double duration) = 0;
    };
}

#endif
