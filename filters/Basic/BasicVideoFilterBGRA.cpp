
#include <videocore/filters/Basic/BasicVideoFilterBGRA.h>




#ifdef __APPLE__
#   include <TargetConditionals.h>
#   if (TARGET_OS_IPHONE)
#       include <OpenGLES/ES2/gl.h>
#       include <OpenGLES/ES3/gl.h>
#       include <videocore/sources/iOS/GLESUtil.h>
#   else
#       include <OpenGL/gl3.h>
#       include <OpenGL/gl3ext.h>
#       include <videocore/sources/iOS/GLESUtil.h>
#       define glDeleteVertexArraysOES glDeleteVertexArrays
#       define glGenVertexArraysOES glGenVertexArrays
#       define glBindVertexArrayOES glBindVertexArray
#   endif
#endif


#include <videocore/filters/FilterFactory.h>

namespace videocore { namespace filters {
 
    bool BasicVideoFilterBGRA::s_registered = BasicVideoFilterBGRA::registerFilter();
    
    bool
    BasicVideoFilterBGRA::registerFilter()
    {
        FilterFactory::_register("com.videocore.filters.bgra", []() { return new BasicVideoFilterBGRA(); });
        return true;
    }
    
    BasicVideoFilterBGRA::BasicVideoFilterBGRA()
    : IVideoFilter(), m_initialized(false), m_bound(false)
    {
        
    }
    BasicVideoFilterBGRA::~BasicVideoFilterBGRA()
    {
        if(m_initialized) {
            glDeleteProgram(m_program);
            glDeleteVertexArraysOES(1, &m_vao);
        }
    }
    
    const char * const
    BasicVideoFilterBGRA::vertexKernel() const
    {
        
        FKERNEL(GL_ES2_3, m_language,
               attribute vec2 aPos;
               attribute vec2 aCoord;
               varying vec2   vCoord;
               uniform mat4   uMat;
               void main(void) {
                gl_Position = uMat * vec4(aPos,0.,1.);
                vCoord = aCoord;
               },
                ""
        )
        FKERNEL(GL_3, m_language,
                in vec2 aPos;
                in vec2 aCoord;
                out vec2   vCoord;
                uniform mat4   uMat;
                void main(void) {
                    gl_Position = uMat * vec4(aPos,0.,1.);
                    vCoord = aCoord;
                },
                "#version 150"
        )
        return nullptr;
    }
    
    const char * const
    BasicVideoFilterBGRA::pixelKernel() const
    {
        
         FKERNEL(GL_ES2_3, m_language,
               precision mediump float;
               varying vec2      vCoord;
               uniform sampler2D uTex0;
               void main(void) {
                   gl_FragData[0] = texture2D(uTex0, vCoord);
               },
                 ""
        )
        FKERNEL(GL_3, m_language,
                
                in vec2                 vCoord;
                uniform sampler2DRect   uTex0;
                out vec4                fragColor;
                uniform vec2            uImageSize;
                void main(void) {

                    vec4 color = texture(uTex0, vCoord * uImageSize);
                    fragColor = color;
                },
                "#version 150"
        )
        
        return nullptr;
    }
    void
    BasicVideoFilterBGRA::initialize()
    {
        switch(m_language) {
            case GL_ES2_3:
            case GL_2: {
                setProgram(build_program(vertexKernel(), pixelKernel()));
                glGenVertexArraysOES(1, &m_vao);
                glBindVertexArrayOES(m_vao);
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
            {
                setProgram(build_program(vertexKernel(), pixelKernel()));
              
                glGenVertexArraysOES(1, &m_vao);
                glBindVertexArrayOES(m_vao);
                glUseProgram(m_program);
                m_uMatrix = glGetUniformLocation(m_program, "uMat");
                int attrpos = glGetAttribLocation(m_program, "aPos");
                int attrtex = glGetAttribLocation(m_program, "aCoord");
                int unitex = glGetUniformLocation(m_program, "uTex0");
                m_uImageSize = glGetUniformLocation(m_program, "uImageSize");

                glUniform1i(unitex, 0);
                glEnableVertexAttribArray(attrpos);
                glEnableVertexAttribArray(attrtex);
                glVertexAttribPointer(attrpos, BUFFER_SIZE_POSITION, GL_FLOAT, GL_FALSE, BUFFER_STRIDE, BUFFER_OFFSET_POSITION);
                glVertexAttribPointer(attrtex, BUFFER_SIZE_POSITION, GL_FLOAT, GL_FALSE, BUFFER_STRIDE, BUFFER_OFFSET_TEXTURE);
                m_initialized = true;

            }
                break;
        }
    }
    void
    BasicVideoFilterBGRA::bind()
    {
        switch(m_language) {
            case GL_ES2_3:
            case GL_2:
                if(!m_bound) {
                    if(!initialized()) {
                        initialize();
                    }
                    glUseProgram(m_program);
                    glBindVertexArrayOES(m_vao);
                }
                glUniformMatrix4fv(m_uMatrix, 1, GL_FALSE, &m_matrix[0][0]);
                break;
            case GL_3:
                if(!m_bound) {
                    if(!initialized()) {
                        initialize();
                    }
                    glUseProgram(m_program);
                    glBindVertexArrayOES(m_vao);
                }
                glUniformMatrix4fv(m_uMatrix, 1, GL_FALSE, &m_matrix[0][0]);
                glUniform2f(m_uImageSize, m_dimensions.w, m_dimensions.h);
                break;
        }
    }
    void
    BasicVideoFilterBGRA::unbind()
    {
        m_bound = false;
    }
}
}
