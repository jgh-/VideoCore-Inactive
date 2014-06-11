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
#ifndef __videocore__MicSource__
#define __videocore__MicSource__

#include <iostream>
#import <CoreAudio/CoreAudioTypes.h>
#import <AudioToolbox/AudioToolbox.h>
#include <videocore/sources/ISource.hpp>
#include <videocore/transforms/IOutput.hpp>


namespace videocore { namespace iOS {

    class MicSource : public ISource, public std::enable_shared_from_this<MicSource>
    {
    public:
        
        MicSource(double audioSampleRate = 44100., std::function<void(AudioUnit&)> excludeAudioUnit = nullptr);
        ~MicSource();
        
        
    public:
        void setOutput(std::shared_ptr<IOutput> output);
        
        void inputCallback(uint8_t* data, size_t data_size);
        
        const AudioUnit& audioUnit() const { return m_audioUnit; };
        
    private:
        
        AudioComponentInstance m_audioUnit;
        AudioComponent         m_component;
    
        double m_audioSampleRate;
        
        std::weak_ptr<IOutput> m_output;
    
    };
    
}
}
#endif /* defined(__videocore__MicSource__) */
