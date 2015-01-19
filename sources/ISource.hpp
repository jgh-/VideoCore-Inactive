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

#ifndef videocore_ISource_hpp
#define videocore_ISource_hpp

#include <videocore/transforms/IOutput.hpp>
#include <videocore/filters/IFilter.hpp>

namespace videocore
{
    /*!
     *  ISource interface.  Defines the interface for sources of data into a graph.
     */
    class ISource
    {
    public:
        /*!
         *  Set the output for the source.
         *
         *  \param output a component that conforms to the videocore::IOutput interface and is compatible with the
         *                data being vended by the source.
         */
        virtual void setOutput(std::shared_ptr<IOutput> output) = 0;
        
        virtual void setFilter(std::shared_ptr<IFilter>) {} ;
        virtual IFilter* const filter() { return nullptr; };
        
        /*! Virtual destructor */
        virtual ~ISource() {};
    };
    
}

#endif
