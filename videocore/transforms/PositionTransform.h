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

#ifndef __videocore__PositionTransform__
#define __videocore__PositionTransform__

#include <iostream>
#include <videocore/transforms/ITransform.hpp>
#include <glm/glm.hpp>

namespace videocore {

    class PositionTransform : public ITransform
    {
    public:
        
        /*! Constructor.
         *
         *  \param x                The x position of the image in the video context.
         *  \param y                The y position of the image in the video context.
         *  \param width            The width of the image.
         *  \param height           The height of the image.
         *  \param contextWidth     The width of the video context.
         *  \param contextHeight    The height of the video context.
         */
        PositionTransform(int x,
                          int y,
                          int width,
                          int height,
                          int contextWidth,
                          int contextHeight);
        
        /*! Destructor */
        ~PositionTransform();
        
        /*!
         *  Change the position of the image in the video context.
         *
         *  \param x  The x position of the image in the video context.
         *  \param y  The y position of the image in the video context.
         */
        void setPosition(int x,
                         int y);
        
        /*!
         *  Change the size of the image in the video context.
         *
         *  \param width            The width of the image.
         *  \param height           The height of the image.
         */
        void setSize(int width,
                     int height);
        
    public:
        
        /*! ITransform::setOutput */
        void setOutput(std::shared_ptr<IOutput> output);
        
        /*! IOutput::pushBuffer */
        void pushBuffer(const uint8_t* const data,
                        size_t size,
                        IMetadata& metadata);
        
    private:
        
        glm::mat4 m_matrix;
        
        std::weak_ptr<IOutput> m_output;
        
        int  m_posX;
        int  m_posY;
        int  m_width;
        int  m_height;
        int  m_contextWidth;
        int  m_contextHeight;
        
        bool m_positionIsDirty;
    };
}

#endif
