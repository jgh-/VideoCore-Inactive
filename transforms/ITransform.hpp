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

#ifndef videocore_ITransform_hpp
#define videocore_ITransform_hpp

#include <videocore/transforms/IOutput.hpp>
#include <chrono>

namespace videocore {

    typedef MetaData<'tran'> TransformMetadata_t;
    
    class ITransform : public IOutput
    {
    public:
        virtual void setOutput(std::shared_ptr<IOutput> output) = 0;
        virtual ~ITransform() {};
    };
}


#endif
