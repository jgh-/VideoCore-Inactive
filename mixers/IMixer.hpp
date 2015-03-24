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
#ifndef videocore_IMixer_hpp
#define videocore_IMixer_hpp

#include <videocore/transforms/ITransform.hpp>

namespace videocore
{
    class ISource;
    
    /*!
     *  IMixer interface.  Defines the interface for registering and unregistering sources with mixers.
     */
    class IMixer : public ITransform
    {
    public:
        /*!
         *  Register a source with the mixer.  There may be intermediate transforms between the source and
         *  the mixer.
         *
         *  \param source A smart pointer to the source being registered.
         *  \param inBufferSize an optional parameter to specify the expected buffer size from the source. Only useful if
         *         the buffer size is always the same.
         */
        virtual void registerSource(std::shared_ptr<ISource> source,
                                    size_t inBufferSize = 0) = 0;
        /*!
         *  Unregister a source with the mixer.
         *
         *  \param source  A smart pointer to the source being unregistered.
         */
        virtual void unregisterSource(std::shared_ptr<ISource> source) = 0;
        
        
        virtual void start() = 0;
        
        /*! Virtual destructor */
        virtual ~IMixer() {};
        
    };
}

#endif
