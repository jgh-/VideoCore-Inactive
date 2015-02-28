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
#ifndef videocore_IVideoMixer_hpp
#define videocore_IVideoMixer_hpp

#include <videocore/mixers/IMixer.hpp>
#include <videocore/filters/IVideoFilter.hpp>
#include <videocore/filters/FilterFactory.h>

#include <glm/glm.hpp>

namespace videocore
{
    /*! Enum values for the VideoBufferMetadata tuple */
    enum {
        kVideoMetadataZIndex, /*!< Specifies the z-Index the buffer (lower is farther back) */
        kVideoMetadataMatrix, /*!< Specifies the transformation matrix to use. Pass an Identity matrix if no transformation is to be applied.
                                   Note that the compositor operates using homogeneous coordinates (-1 to 1) unless otherwise specified. */
        kVideoMetadataSource  /*!< Specifies a smart pointer to the source */
    };
    
    /*!
     *  Specifies the properties of the incoming image buffer.
     */
    typedef MetaData<'vide', int, glm::mat4, std::weak_ptr<ISource>> VideoBufferMetadata;
    
    /*! IAudioMixer interface.  Defines the required interface methods for Video mixers (compositors). */
    class IVideoMixer : public IMixer
    {
    public:
        virtual ~IVideoMixer() {};
        virtual void setSourceFilter(std::weak_ptr<ISource> source, IVideoFilter* filter)=0;
        virtual FilterFactory& filterFactory() = 0;
        virtual void sync() = 0;
    };
}

#endif
