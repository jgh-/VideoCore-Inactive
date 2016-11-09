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
#ifndef __videocore__AspectTransform__
#define __videocore__AspectTransform__

#include <iostream>
#include <videocore/transforms/ITransform.hpp>
#include <glm/glm.hpp>

namespace videocore {
    
    /*!
     *
     */
    class AspectTransform : public ITransform
    {
    public:
        enum AspectMode
        {
            kAspectFit,  /*!< An aspect mode which shrinks the incoming video to fit in the supplied boundaries. */
            kAspectFill  /*!< An aspect mode which scales the video to fill the supplied boundaries and maintain aspect ratio. */
        };
        
        /*! Constructor.
         *
         *  \param boundingWidth  The width ouf the bounding box.
         *  \param boundingHeight The height of the bounding box.
         *  \param aspectMode     The aspectMode to use.
         */
        AspectTransform(int boundingWidth,
                        int boundingHeight,
                        AspectMode aspectMode);
        
        /*! Destructor */
        ~AspectTransform();
        
        /*! 
         *  Change the size of the target bounding box.
         *
         *  \param boundingWidth  The width ouf the bounding box.
         *  \param boundingHeight The height of the bounding box.
         */
        void setBoundingSize(int boundingWidth,
                             int boundingHeight);
        
        /*!
         *  Change the aspect mode
         *
         *  \param aspectMode The aspectMode to use.
         */
        void setAspectMode(AspectMode aspectMode);
        
        /*! 
         *  Mark the bounding box as dirty and force a refresh.  This may be useful if the
         *  dimensions of the pixel buffer change.
         */
        void setBoundingBoxDirty() ;
        
    public:
        
        /*! ITransform::setOutput */
        void setOutput(std::shared_ptr<IOutput> output);
        
        /*! IOutput::pushBuffer */
        void pushBuffer(const uint8_t* const data,
                        size_t size,
                        IMetadata& metadata);

        
    private:
        
        glm::vec3 m_scale;
        
        std::weak_ptr<IOutput> m_output;
        
        int m_boundingWidth;
        int m_boundingHeight;
        int m_prevWidth;
        int m_prevHeight;
        AspectMode m_aspectMode;
        
        bool m_boundingBoxDirty;
        
    };
}

#endif 
