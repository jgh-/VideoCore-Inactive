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

#ifndef videocore_ISource_hpp
#define videocore_ISource_hpp

#include <videocore/transforms/IOutput.hpp>

namespace videocore
{
    class ISource
    {
    public:
        virtual void setOutput(std::shared_ptr<IOutput> output) = 0;
        virtual ~ISource() {};
    };
    
    // CRTP used to provide a weak_ptr to the class upon instantiation.
    template <typename Derived>
    class StaticSource : public ISource
    {
    public:
        static std::shared_ptr<Derived> createInstance()
        {
            return Derived::staticCreateInstance();
        }
    };
}

#endif
