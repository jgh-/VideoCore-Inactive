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
 *  Split transform :: push a buffer to multiple outputs.
 *
 *
 */
#include <videocore/transforms/Split.h>

namespace videocore {
    Split::Split() {}
    Split::~Split() {}
    
    void
    Split::setOutput(std::shared_ptr<IOutput> output)
    {
        const auto inHash = std::hash<std::shared_ptr<IOutput>>()(output);
        bool duplicate = false;
        for ( auto & it : m_outputs) {
            auto outp = it.lock();
            if(outp) {
                auto hash = std::hash<std::shared_ptr<IOutput>>()(outp);
                duplicate |= (hash == inHash);
            }
        }
        if(!duplicate) {
            m_outputs.push_back(output);
        }
    }
    void
    Split::removeOutput(std::shared_ptr<IOutput> output)
    {
        const auto inHash = std::hash<std::shared_ptr<IOutput>>()(output);
        
        auto it = std::remove_if(m_outputs.begin(), m_outputs.end(), [=](const std::weak_ptr<IOutput>& rhs) {
           
            auto outp = rhs.lock();
            if(outp) {
                return inHash == std::hash<std::shared_ptr<IOutput>>()(outp);
            }
            return true;
        });
        m_outputs.resize(std::distance(m_outputs.begin(), it));
    }
    void
    Split::pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata)
    {
        for ( auto it = m_outputs.begin() ; it != m_outputs.end() ; ++it ) {
            auto outp = it->lock();
            if(outp) {
                outp->pushBuffer(data, size, metadata);
            }
        }
    }
}