
#include <videocore/filters/Basic/InvertColorsVideoFilter.h>

#include <TargetConditionals.h>


#ifdef TARGET_OS_IPHONE

#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES3/gl.h>
#include <videocore/sources/iOS/GLESUtil.h>
#include <videocore/filters/FilterFactory.h>

#endif

namespace videocore { namespace filters {
 
    bool InvertColorsVideoFilter::s_registered = InvertColorsVideoFilter::registerFilter();
    
    bool
    InvertColorsVideoFilter::registerFilter()
    {
        FilterFactory::_register("com.videocore.filters.invertColors", []() { return new InvertColorsVideoFilter(); });
        return true;
    }
    
    InvertColorsVideoFilter::InvertColorsVideoFilter()
    : IVideoFilter(), m_initialized(false), m_bound(false)
    {
        
    }
    InvertColorsVideoFilter::~InvertColorsVideoFilter()
    {
        glDeleteProgram(m_program);
        glDeleteVertexArrays(1, &m_vao);
    }
    
    const char * const
    InvertColorsVideoFilter::vertexKernel() const
    {
        
        KERNEL(GL_ES2_3, m_language,
               attribute vec2 aPos;
               attribute vec2 aCoord;
               varying vec2   vCoord;
               uniform mat4   uMat;
               void main(void) {
                gl_Position = uMat * vec4(aPos,0.,1.);
                vCoord = aCoord;
               }
        )
        
        return nullptr;
    }
    
    const char * const
    InvertColorsVideoFilter::pixelKernel() const
    {
        
         KERNEL(GL_ES2_3, m_language,
               precision mediump float;
               varying vec2      vCoord;
               uniform sampler2D uTex0;
               void main(void) {
                   vec4 color = texture2D(uTex0, vCoord);
                   gl_FragColor = vec4(1.0 - color.r, 1.0 - color.g, 1.0 - color.b, color.a);
               }
        )
        
        return nullptr;
    }
    void
    InvertColorsVideoFilter::initialize()
    {
        switch(m_language) {
            case GL_ES2_3:
            case GL_2: {
                setProgram(build_program(vertexKernel(), pixelKernel()));
                glGenVertexArrays(1, &m_vao);
                glBindVertexArray(m_vao);
                m_uMatrix = glGetUniformLocation(m_program, "uMat");
                int attrpos = glGetAttribLocation(m_program, "aPos");
                int attrtex = glGetAttribLocation(m_program, "aCoord");
                int unitex = glGetUniformLocation(m_program, "uTex0");
                glUniform1i(unitex, 0);
                glEnableVertexAttribArray(attrpos);
                glEnableVertexAttribArray(attrtex);
                glVertexAttribPointer(attrpos, BUFFER_SIZE_POSITION, GL_FLOAT, GL_FALSE, BUFFER_STRIDE, BUFFER_OFFSET_POSITION);
                glVertexAttribPointer(attrtex, BUFFER_SIZE_POSITION, GL_FLOAT, GL_FALSE, BUFFER_STRIDE, BUFFER_OFFSET_TEXTURE);
                m_initialized = true;
            }
                break;
            case GL_3:
                break;
        }
    }
    void
    InvertColorsVideoFilter::bind()
    {
        switch(m_language) {
            case GL_ES2_3:
            case GL_2:
                if(!m_bound) {
                    if(!initialized()) {
                        initialize();
                    }
                    glUseProgram(m_program);
                    glBindVertexArray(m_vao);
                }
                glUniformMatrix4fv(m_uMatrix, 1, GL_FALSE, &m_matrix[0][0]);
                break;
            case GL_3:
                break;
        }
    }
    void
    InvertColorsVideoFilter::unbind()
    {
        m_bound = false;
    }
}
}