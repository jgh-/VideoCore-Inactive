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

#ifndef __videocore__Split__
#define __videocore__Split__

#include <iostream>
#include <videocore/transforms/ITransform.hpp>
#include <vector>

namespace videocore {
    
    class Split : public ITransform
    {
    public:
        Split();
        ~Split();
        
        void setOutput(std::shared_ptr<IOutput> output);
        void removeOutput(std::shared_ptr<IOutput> output);
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
        
    private:
        
        std::vector<std::weak_ptr<IOutput>> m_outputs;
        
    };
    
}
#endif /* defined(__videocore__Split__) */
