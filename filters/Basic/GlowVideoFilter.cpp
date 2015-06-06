
#include <videocore/filters/Basic/GlowVideoFilter.h>

#include <TargetConditionals.h>


#ifdef TARGET_OS_IPHONE

#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES3/gl.h>
#include <videocore/sources/iOS/GLESUtil.h>
#include <videocore/filters/FilterFactory.h>

#endif

namespace videocore { namespace filters {
 
    bool GlowVideoFilter::s_registered = GlowVideoFilter::registerFilter();
    
    bool
    GlowVideoFilter::registerFilter()
    {
        FilterFactory::_register("com.videocore.filters.glow", []() { return new GlowVideoFilter(); });
        return true;
    }
    
    GlowVideoFilter::GlowVideoFilter()
    : IVideoFilter(), m_initialized(false), m_bound(false)
    {
        
    }
    GlowVideoFilter::~GlowVideoFilter()
    {
        glDeleteProgram(m_program);
        glDeleteVertexArraysOES(1, &m_vao);
    }
    
    const char * const
    GlowVideoFilter::vertexKernel() const
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
    GlowVideoFilter::pixelKernel() const
    {
        
         KERNEL(GL_ES2_3, m_language,
               precision mediump float;
               varying vec2      vCoord;
               uniform sampler2D uTex0;
               const float step_w = 0.0015625;
               const float step_h = 0.0027778;
               void main(void) {
                   vec3 t1 = texture2D(uTex0, vec2(vCoord.x - step_w, vCoord.y - step_h)).bgr;
                   vec3 t2 = texture2D(uTex0, vec2(vCoord.x, vCoord.y - step_h)).bgr;
                   vec3 t3 = texture2D(uTex0, vec2(vCoord.x + step_w, vCoord.y - step_h)).bgr;
                   vec3 t4 = texture2D(uTex0, vec2(vCoord.x - step_w, vCoord.y)).bgr;
                   vec3 t5 = texture2D(uTex0, vCoord).bgr;
                   vec3 t6 = texture2D(uTex0, vec2(vCoord.x + step_w, vCoord.y)).bgr;
                   vec3 t7 = texture2D(uTex0, vec2(vCoord.x - step_w, vCoord.y + step_h)).bgr;
                   vec3 t8 = texture2D(uTex0, vec2(vCoord.x, vCoord.y + step_h)).bgr;
                   vec3 t9 = texture2D(uTex0, vec2(vCoord.x + step_w, vCoord.y + step_h)).bgr;
                   
                   vec3 xx= t1 + 2.0*t2 + t3 - t7 - 2.0*t8 - t9;
                   vec3 yy = t1 - t3 + 2.0*t4 - 2.0*t6 + t7 - t9;
                   
                   vec3 rr = sqrt(xx * xx + yy * yy);
                   
                   gl_FragColor.a = 1.0;
                   gl_FragColor.rgb = rr * 2.0 * t5;
               }
        )
        
        return nullptr;
    }
    void
    GlowVideoFilter::initialize()
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
                break;
        }
    }
    void
    GlowVideoFilter::bind()
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
    GlowVideoFilter::unbind()
    {
        m_bound = false;
    }
}
}
