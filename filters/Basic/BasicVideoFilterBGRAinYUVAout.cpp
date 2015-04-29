#include <videocore/filters/Basic/BasicVideoFilterBGRAinYUVAout.h>

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
    
    bool BasicVideoFilterBGRAinYUVAout::s_registered = BasicVideoFilterBGRAinYUVAout::registerFilter();
    
    bool
    BasicVideoFilterBGRAinYUVAout::registerFilter()
    {
        FilterFactory::_register("com.videocore.filters.bgra2yuva", []() { return new BasicVideoFilterBGRAinYUVAout(); });
        return true;
    }
    
    BasicVideoFilterBGRAinYUVAout::BasicVideoFilterBGRAinYUVAout()
    : IVideoFilter(), m_initialized(false), m_bound(false)
    {
        
    }
    BasicVideoFilterBGRAinYUVAout::~BasicVideoFilterBGRAinYUVAout()
    {
        glDeleteProgram(m_program);
        glDeleteVertexArraysOES(1, &m_vao);
    }
    
    const char * const
    BasicVideoFilterBGRAinYUVAout::vertexKernel() const
    {
        
        FKERNEL(GL_ES2_3, m_language,
               attribute vec2 aPos;
               attribute vec2 aCoord;
               varying vec2   vCoord;
               uniform mat4   uMat;
               void main(void) {
                   gl_Position = uMat * vec4(aPos,0.,1.);
                   vCoord = aCoord;
               }, ""
               )
        
        return nullptr;
    }
    
    const char * const
    BasicVideoFilterBGRAinYUVAout::pixelKernel() const
    {
        
        FKERNEL(GL_ES2_3, m_language,
               precision mediump float;
               varying vec2      vCoord;
               uniform sampler2D uTex0;
               const mat4 RGBtoYUV(0.257,  0.439, -0.148, 0.0,
                             0.504, -0.368, -0.291, 0.0,
                             0.098, -0.071,  0.439, 0.0,
                             0.0625, 0.500,  0.500, 1.0 );
               void main(void) {
                   gl_FragData[0] = texture2D(uTex0, vCoord) * RGBtoYUV;
               }, ""
               )
        
        return nullptr;
    }
    void
    BasicVideoFilterBGRAinYUVAout::initialize()
    {
        switch(m_language) {
            case GL_ES2_3:
            case GL_2: {
                setProgram(build_program(vertexKernel(), pixelKernel()));
                glGenVertexArraysOES(1, &m_vao);
                glBindVertexArrayOES(m_vao);
                m_uMatrix = glGetUniformLocation(m_program,  "uMat");
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
    BasicVideoFilterBGRAinYUVAout::bind()
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
                break;
        }
    }
    void
    BasicVideoFilterBGRAinYUVAout::unbind()
    {
        m_bound = false;
    }
}
}
