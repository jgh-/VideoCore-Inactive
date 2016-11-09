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


#include <videocore/transforms/PositionTransform.h>
#include <glm/gtc/matrix_transform.hpp>
#include <videocore/mixers/IVideoMixer.hpp>
namespace videocore {
    
    PositionTransform::PositionTransform(int x,
                                         int y,
                                         int width,
                                         int height,
                                         int contextWidth,
                                         int contextHeight)
    : m_posX(x),
    m_posY(y),
    m_width(width),
    m_height(height),
    m_contextWidth(contextWidth),
    m_contextHeight(contextHeight),
    m_positionIsDirty(true)
    {
        
    }
    
    PositionTransform::~PositionTransform()
    {
        
    }
    
    void
    PositionTransform::setOutput(std::shared_ptr<IOutput> output)
    {
        m_output = output;
    }
    
    void
    PositionTransform::setPosition(int x,
                                   int y)
    {
        m_posX = x;
        m_posY = y;
        m_positionIsDirty = true;
    }
    void
    PositionTransform::setSize(int width,
                               int height)
    {
        m_width = width;
        m_height = height;
        m_positionIsDirty = true;
    }
    
    void
    PositionTransform::pushBuffer(const uint8_t *const data,
                                  size_t size,
                                  videocore::IMetadata &metadata)
    {
        auto output = m_output.lock();
        
        if(output) {

            if(m_positionIsDirty) {
                glm::mat4 mat(1.f);
                const float x (m_posX), y(m_posY), cw(m_contextWidth), ch(m_contextHeight), w(m_width), h(m_height);
                
                mat = glm::translate(mat,
                                     glm::vec3((x / cw) * 2.f - 1.f,   // The compositor uses homogeneous coordinates.
                                               (y / ch) * 2.f - 1.f,   // i.e. [ -1 .. 1 ]
                                               0.f));
                
                mat = glm::scale(mat,
                                 glm::vec3(w / cw, //
                                           h / ch, // size is a percentage for scaling.
                                           1.f));
                
                m_matrix = mat;

                m_positionIsDirty = false;
            }
            videocore::VideoBufferMetadata& md = dynamic_cast<videocore::VideoBufferMetadata&>(metadata);
            glm::mat4 & mat = md.getData<videocore::kVideoMetadataMatrix>();
            
            mat = mat * m_matrix;
            
            output->pushBuffer(data, size, metadata);
        }
    }
    
}