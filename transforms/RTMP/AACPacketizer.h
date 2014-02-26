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

#ifndef __videocore__AACPacketizer__
#define __videocore__AACPacketizer__

#include <iostream>
#include <videocore/transforms/ITransform.hpp>
#include <Vector>

namespace videocore { namespace rtmp {
 
    class AACPacketizer : public ITransform
    {
    public:
        AACPacketizer();
        
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
        void setOutput(std::shared_ptr<IOutput> output);
        void setEpoch(const std::chrono::steady_clock::time_point epoch) { m_epoch = epoch; };

    private:
        
        std::chrono::steady_clock::time_point m_epoch;
        std::weak_ptr<IOutput> m_output;
        std::vector<uint8_t> m_outbuffer;
        
        double m_audioTs;
        bool m_sentAudioConfig;
    };
    
}
}
#endif /* defined(__videocore__AACPacketizer__) */
