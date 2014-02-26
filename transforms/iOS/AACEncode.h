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
 *  iOS::AACEncode transform :: Takes a raw buffer of LPCM input and outputs AAC Packets.
 *
 *
 */


#ifndef __videocore__AACEncode__
#define __videocore__AACEncode__

#include <iostream>
#include <videocore/transforms/ITransform.hpp>
#include <AudioToolbox/AudioToolbox.h>
#include <videocore/system/Buffer.hpp>

namespace videocore { namespace iOS {
 
    class AACEncode : public ITransform
    {
    public:
        AACEncode( int frequencyInHz, int channelCount );
        ~AACEncode();
        
        void setOutput(std::shared_ptr<IOutput> output) { m_output = output; };
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
    private:
        static OSStatus ioProc(AudioConverterRef audioConverter, UInt32 *ioNumDataPackets, AudioBufferList* ioData, AudioStreamPacketDescription** ioPacketDesc, void* inUserData );
    private:
        
        AudioConverterRef       m_audioConverter;
        std::weak_ptr<IOutput>  m_output;
        size_t                  m_bytesPerSample;
        uint32_t                m_outputPacketMaxSize;
        
        Buffer                  m_outputBuffer;
        
        bool                    m_sentHeader;
    };
    
}
}
#endif /* defined(__videocore__AACEncode__) */
