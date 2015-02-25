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

//
//  and whatever you do, dont forget to smile, because if you don't smile,
//  then the execution would smell like funky drawers
//

#ifndef videocore_IVideoFilter_hpp
#define videocore_IVideoFilter_hpp

#include <videocore/filters/IFilter.hpp>
#include <glm/glm.hpp>

#define KERNEL(_language, _target, _kernelstr) if(_language == _target){ do { return \
"const mediump mat4 RGBtoYUV = mat4(0.257,  0.439, -0.148, 0.0," \
"0.504, -0.368, -0.291, 0.0," \
"0.098, -0.071,  0.439, 0.0," \
"0.0625, 0.500,  0.500, 1.0 ); " # _kernelstr ; } while(0); }


namespace videocore {

    enum FilterLanguage {
        GL_ES3,
        GL_ES2,
        GL_2,
        GL_3
    };
    
    class IVideoFilter : public IFilter {
        
    public:
        
        virtual ~IVideoFilter() {} ;

        virtual const char * const vertexKernel() const = 0;
        virtual const char * const pixelKernel() const = 0;

        
    public:
        
        void setIncomingMatrix(glm::mat4& matrix) {  m_matrix = matrix; };
        void setImageDimensions(float w, float h) { m_dimensions.w = w; m_dimensions.h = h; };
        
        void setFilterLanguage(FilterLanguage language) { m_language = language ; };
        void setProgram(int program) { m_program = program; };
        const int program() const { return m_program; };
   
     protected:
        IVideoFilter() : m_program(0), m_matrix(1.f), m_dimensions({ 1.f, 1.f }), m_language(GL_ES2) {};
      
        glm::mat4 m_matrix;
        struct { float w, h ; } m_dimensions;
        int m_program;
        
        FilterLanguage m_language;
        
    };
}

#endif
