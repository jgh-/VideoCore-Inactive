#include <videocore/filters/Basic/BasicVideoFilterYUVAinNV12out.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#   ifdef TARGET_OS_IPHONE
#   include <OpenGLES/ES2/gl.h>
#   include <OpenGLES/ES3/gl.h>
#   include <videocore/sources/iOS/GLESUtil.h>
#   include <videocore/filters/FilterFactory.h>
#   endif
#endif

namespace videocore { namespace filters {
    
    bool BasicVideoFilterYUVAinNV12out::s_registered = BasicVideoFilterYUVAinNV12out::registerFilter();
    
    bool
    BasicVideoFilterYUVAinNV12out::registerFilter()
    {
        FilterFactory::_register("com.videocore.filters.yuva2nv12", []() { return new BasicVideoFilterYUVAinNV12out(); });
        return true;
    }
    
    BasicVideoFilterYUVAinNV12out::BasicVideoFilterYUVAinNV12out()
    : IVideoFilter(), m_initialized(false), m_bound(false)
    {
        
    }
    BasicVideoFilterYUVAinNV12out::~BasicVideoFilterYUVAinNV12out()
    {
        glDeleteProgram(m_program);
        glDeleteVertexArrays(1, &m_vao);
    }
    
    const char * const
    BasicVideoFilterYUVAinNV12out::vertexKernel() const
    {
        
        KERNEL(GL_ES2, m_language,
               attribute vec2 aPos;
               attribute vec2 aCoord;
               varying vec2   vCoord;
               uniform mat4   uMat;
               uniform vec2   uScreenSize;
               varying vec2   vScreenSize;
               varying vec2   vFragSize;
               
               void main(void) {
                   gl_Position = uMat * vec4(aPos,0.,1.);
                   vCoord = vec2(aCoord.x, aCoord.y * 1.5);
                   vFragSize = 1.0/uScreenSize;
                   vScreenSize = uScreenSize;
               }
               )
        
        return nullptr;
    }
    
    const char * const
    BasicVideoFilterYUVAinNV12out::pixelKernel() const
    {
        
        KERNEL(GL_ES2, m_language,
               precision mediump float;
               varying vec2      vCoord;
               uniform sampler2D uTex0;
               varying vec2      vScreenSize;
               varying vec2      vFragSize;
               
               void main(void) {
                   if( vCoord.y < 1.0 ) {
                       gl_FragData[0] = texture2D(uTex0, vCoord).rrrr;
                   } else {
                       vec2 coord = vec2(vCoord.x, (vCoord.y-1.0)*2.0);
                       gl_FragData[0] = (mod(gl_FragCoord.x, 2.0) == 0.0) ? texture2D(uTex0, coord).gggg : texture2D(uTex0, coord).bbbb;
                   }
                   
               }
               )
        
        return nullptr;
    }
    void
    BasicVideoFilterYUVAinNV12out::initialize()
    {
        switch(m_language) {
            case GL_ES3:
            case GL_ES2:
            case GL_2: {
                setProgram(build_program(vertexKernel(), pixelKernel()));
                glGenVertexArrays(1, &m_vao);
                glBindVertexArray(m_vao);
                m_uMatrix = glGetUniformLocation(m_program,  "uMat");
                m_uScreenSize = glGetUniformLocation(m_program, "uScreenSize");
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
    BasicVideoFilterYUVAinNV12out::bind()
    {
        switch(m_language) {
            case GL_ES3:
            case GL_ES2:
            case GL_2:
                if(!m_bound) {
                    if(!initialized()) {
                        initialize();
                    }
                    glUseProgram(m_program);
                    glBindVertexArray(m_vao);
                }
                glUniform2f(m_uScreenSize, m_dimensions.w, m_dimensions.h);
                glUniformMatrix4fv(m_uMatrix, 1, GL_FALSE, &m_matrix[0][0]);
                break;
            case GL_3:
                break;
        }
    }
    void
    BasicVideoFilterYUVAinNV12out::unbind()
    {
        m_bound = false;
    }
}
}