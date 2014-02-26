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
        
        MicSource();
        ~MicSource();
        
        
    public:
        void setOutput(std::shared_ptr<IOutput> output);
        
        void inputCallback(uint8_t* data, size_t data_size);
        
        const AudioUnit& audioUnit() const { return m_audioUnit; };
        
    private:
        
        AudioComponentInstance m_audioUnit;
        AudioComponent         m_component;
        
        std::weak_ptr<IOutput> m_output;
    };
    
}
}
#endif /* defined(__videocore__MicSource__) */
