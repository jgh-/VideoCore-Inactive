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


#include <videocore/mixers/Android/GLESVideoMixer.h>

#include <glm/gtc/matrix_transform.hpp>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#define SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(v)  ([[[UIDevice currentDevice] systemVersion] compare:v options:NSNumericSearch] != NSOrderedAscending)

// Convenience macro to dispatch an OpenGL ES job to the created videocore::JobQueue
#define PERF_GL(x, dispatch) do {\
m_glJobQueue.dispatch([=](){\
	EGLContext _ctx_current = eglGetCurrentContext();\
	EGLDisplay _dsp_current = eglGetCurrentDisplay();\
	EGLSurface _rsf_current = eglGetCurrentSurface(EGL_READ);\
	EGLSurface _dsf_current = eglGetCurrentSurface(EGL_DRAW);\
	if(m_context) {\
		eglMakeCurrent(m_display, m_surface, m_surface, m_context);\
	}\
x ;\
	eglMakeCurrent(_dsp_current, _dsf_current, _rsf_current, _ctx_current);\
});\
} while(0);
// Dispatch and execute synchronously
#define PERF_GL_sync(x) PERF_GL((x), enqueue_sync);
// Dispatch and execute asynchronously
#define PERF_GL_async(x) PERF_GL((x), enqueue);


const EGLint eglImgAttrs[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE };


namespace videocore { namespace Android {
    
    // -------------------------------------------------------------------------
    //
    //  SourceBuffer::setBuffer
    //      Creates a related GLES texture and keeps track of the first-in-line
    //      texture.  Textures unused for more than 1 second will be released.
    //
    // -------------------------------------------------------------------------
    
    void
    SourceBuffer::setBuffer(Android::PixelBufferRef ref, JobQueue& m_glJobQueue, void* m_display, void* m_surface, void* m_context)
    {
        
        bool flush = false;
        auto it = m_pixelBuffers.find(ref->gBuffer());
        const auto now = std::chrono::steady_clock::now();
       	
        if(it == m_pixelBuffers.end()) {
            PERF_GL_async({
                
                //ref->lock(true);
                //auto format = ref->pixelFormat();
                //bool is32bit = true;
                
                //is32bit = (format != kVCPixelBufferFormatL565);
     			

     			/* eglCreateImageKHR (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);*/
     			EGLClientBuffer buffer = (EGLClientBuffer)ref->gBuffer()->getNativeBuffer();
               	EGLImageKHR image = eglCreateImageKHR(eglGetCurrentDisplay(), EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, buffer, eglImgAttrs );
                GLuint texture;

                glGenTextures(1, &texture);

                //ref->unlock(true);
                if(texture) {
                    glBindTexture(GL_TEXTURE_2D, texture);
                    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    
                    auto iit = this->m_pixelBuffers.emplace(ref->gBuffer(), ref).first;
                    iit->second.texture = texture;
                    iit->second.image = image;

                    if(this->m_currentBuffer) {
                        this->m_currentBuffer->setState(kVCPixelBufferStateAvailable);
                    }
                    this->m_currentBuffer = ref;
                    this->m_currentTexture = texture;
                    iit->second.time = now;
                    
                } else {
                    DLog("%d: Error creating texture! (%p)", __LINE__, image);
                }
            });
       
        } else {
            if(m_currentBuffer) {
                m_currentBuffer->setState(kVCPixelBufferStateAvailable);
            }
            m_currentBuffer = ref;
            m_currentTexture = it->second.texture;
            it->second.time = now;
            
        }
        
        ref->setState(kVCPixelBufferStateAcquired);
        
        PERF_GL_async({
            //const auto currentBuffer = this->m_currentBuffer->cvBuffer();
            for ( auto it = this->m_pixelBuffers.begin() ; it != m_pixelBuffers.end() ; ) {
                
                if ( (it->second.buffer->isTemporary()) && it->second.buffer->gBuffer() != this->m_currentBuffer->gBuffer() ) {
                    // Buffer is temporary, release it.
                    it = this->m_pixelBuffers.erase(it);
                } else {
                    ++ it;
                }
                
            }
            
        });
    }
    // -------------------------------------------------------------------------
    //
    //
    //
    //
    // -------------------------------------------------------------------------
    GLESVideoMixer::GLESVideoMixer( int frame_w,
                                   int frame_h,
                                   double frameDuration,
                                   std::function<void(void*)> excludeContext )
    : m_bufferDuration(frameDuration),
    m_frameW(frame_w),
    m_frameH(frame_h),
    m_exiting(false),
    m_mixing(false),
    m_paused(false),
    m_context(nullptr),
    m_glJobQueue("com.videocore.composite")
    {

        PERF_GL_sync({
            
            this->setupGLES(excludeContext);
            
        });
        m_zRange.first = INT_MAX;
        m_zRange.second = INT_MIN;
        m_mixThread = std::thread([this](){ this->mixThread(); });
        
    }
    
    GLESVideoMixer::~GLESVideoMixer()
    {
        m_output.reset();
        m_exiting = true;
        m_mixThreadCond.notify_all();
        DLog("GLESVideoMixer::~GLESVideoMixer()");
        PERF_GL_sync({
            glDeleteProgram(m_prog);
            glDeleteFramebuffers(2, m_fbo);
            glDeleteRenderbuffers(2, m_rbo);
            glDeleteBuffers(1, &m_vbo);
           	
           	for( int i = 0 ; i < 2 ; ++ i ) {
           		eglDestroyImageKHR(m_display, m_texture[i]);
           	}

            m_sourceBuffers.clear();

        });
        m_glJobQueue.mark_exiting();
        m_glJobQueue.enqueue_sync([](){});
        m_mixThread.join();
    }
    void
    GLESVideoMixer::setupGLES(std::function<void(void*)> excludeContext)
    {
        // Create off-screen context
        const EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

        DLog("Getting display\n");
        auto eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if(eglDisplay == EGL_NO_DISPLAY) {
        	DLog("No display returned! (wryyyy?)\n");
        }
        m_display = eglDisplay;
        DLog("Initializing EGL\n");
        EGLBoolean ret = eglInitialize(eglDisplay, 0, 0);
        if(ret == EGL_FALSE) {
        	DLog("eglInitialize failed!\n");
        }

        const EGLint configAttribs[] = {
    		 EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    		 EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
   			 EGL_RED_SIZE, 8,
    		 EGL_GREEN_SIZE, 8,
    		 EGL_BLUE_SIZE, 8,
    		 EGL_ALPHA_SIZE, 8,
    		 EGL_NONE };

        int configCount;
        DLog("Choosing config\n");
        ret = eglChooseConfig(m_display, configAttribs, &m_config, 1, &configCount);
        if(configCount != 1) {
        	DLog("Couldn't find a config?! (%d)\n", configCount);
        }
        DLog("Creating context\n");
        m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, contextAttribs );

        DLog("[%d] Creating pixelbuffer surface (%d)\n", ret, configCount);
        EGLint SurfaceAttribs[] = { EGL_WIDTH, m_frameW,
								    EGL_HEIGHT, m_frameH,
									EGL_NONE};
        m_surface = eglCreatePbufferSurface(m_display, m_config, SurfaceAttribs);

        DLog("Making EGL environment current\n");
        eglMakeCurrent(m_display, m_surface, m_surface, m_context);

        DLog("Setting up framebuffers/renderbuffers\n");
        // Create renderbuffers
        glGenFramebuffers(2, this->m_fbo);
        glGenRenderbuffers(2, this->m_rbo);
        for ( int i = 0 ; i < 2 ; ++ i ) {
        	GraphicBuffer* buffer = new GraphicBuffer(m_frameW, m_frameH, android::PIXEL_FORMAT_RGBA_8888, android::GraphicBuffer::USAGE_SW_READ_OFTEN |
        																								  android::GraphicBuffer::USAGE_HW_RENDER | 
        																								  android::GraphicBuffer::USAGE_HW_VIDEO_ENCODER);
        	m_pixelBuffer[i].reset(new PixelBuffer(buffer));
        	EGLImageKHR image = eglCreateImageKHR(m_display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, buffer->getNativeBuffer(), eglImgAttrs );
        	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo[i]);
        	glBindRenderbuffer(GL_RENDERBUFFER, m_rbo[i]);
        	glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, image);
        	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_rbo[i]);

        	m_texture[i] = image;
        }

        GL_FRAMEBUFFER_STATUS(__LINE__);
        DLog("Setting up draw buffers\n");
        glGenBuffers(1, &this->m_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, this->m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(s_vbo), s_vbo, GL_STATIC_DRAW);

        this->m_prog = build_program(s_vs_mat, s_fs);
        glUseProgram(this->m_prog);

        
        this->m_uMat = glGetUniformLocation(this->m_prog, "uMat");
        
        m_vertexAttribPosition = glGetAttribLocation(this->m_prog, "aPos");
        m_vertexAttribTexCoord = glGetAttribLocation(this->m_prog, "aCoord");
        int unitex = glGetUniformLocation(this->m_prog, "uTex0");
        glUniform1i(unitex, 0);
        glEnableVertexAttribArray(m_vertexAttribPosition);
        glEnableVertexAttribArray(m_vertexAttribTexCoord);
        glVertexAttribPointer(m_vertexAttribPosition, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, BUFFER_OFFSET(0));
        glVertexAttribPointer(m_vertexAttribTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, BUFFER_OFFSET(8));
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, m_frameW,m_frameH);
        glClearColor(0.05,0.05,0.07,1.0);

    }
    void
    GLESVideoMixer::registerSource(std::shared_ptr<ISource> source,
                                   size_t bufferSize)
    {
        const auto hash = std::hash< std::shared_ptr<ISource> > () (source);
        bool registered = false;
        
        for ( auto it : m_sources) {
            auto lsource = it.lock();
            if(lsource) {
                const auto shash = std::hash< std::shared_ptr<ISource> >() (lsource);
                if(shash == hash) {
                    registered = true;
                    break;
                }
            }
        }
        if(!registered)
        {
            m_sources.push_back(source);
        }
    }
    void
    GLESVideoMixer::releaseBuffer(std::weak_ptr<ISource> source)
    {
        DLog("GLESVideoMixer::releaseBuffer");
        const auto h = hash(source);
        auto it = m_sourceBuffers.find(h) ;
        if(it != m_sourceBuffers.end()) {
            m_sourceBuffers.erase(it);
        }
        
    }
    void
    GLESVideoMixer::unregisterSource(std::shared_ptr<ISource> source)
    {
        DLog("GLESVideoMixer::unregisterSource");
        releaseBuffer(source);
        
        auto it = m_sources.begin();
        const auto h = std::hash<std::shared_ptr<ISource> >()(source);
        for ( ; it != m_sources.end() ; ++it ) {
            
            const auto shash = hash(*it);
            
            if(h == shash) {
                m_sources.erase(it);
                break;
            }
            
        }
        {
            auto iit = m_sourceBuffers.find(h);
            if(iit != m_sourceBuffers.end()) {
                m_sourceBuffers.erase(iit);
            }
        }
        for ( int i = m_zRange.first ; i <= m_zRange.second ; ++i )
        {
            for ( auto iit = m_layerMap[i].begin() ; iit!= m_layerMap[i].end() ; ++iit) {
                if((*iit) == h) {
                    m_layerMap[i].erase(iit);
                    break;
                }
            }
        }
        
    }
    void
    GLESVideoMixer::pushBuffer(const uint8_t *const data,
                               size_t size,
                               videocore::IMetadata &metadata)
    {
        if(m_paused.load()) {
            return;
        }
        
        VideoBufferMetadata & md = dynamic_cast<VideoBufferMetadata&>(metadata);
        const int zIndex = md.getData<kVideoMetadataZIndex>();
        
        const glm::mat4 mat = md.getData<kVideoMetadataMatrix>();
        
        if(zIndex < m_zRange.first) {
            m_zRange.first = zIndex;
        }
        if(zIndex > m_zRange.second){
            m_zRange.second = zIndex;
        }
        
        std::weak_ptr<ISource> source = md.getData<kVideoMetadataSource>();
        
        const auto h = hash(source);
    
        
        auto inPixelBuffer = *(Android::PixelBufferRef*)data ;
        
        m_sourceBuffers[h].setBuffer(inPixelBuffer, m_glJobQueue, m_display, m_surface, m_context);
        auto it = std::find(this->m_layerMap[zIndex].begin(), this->m_layerMap[zIndex].end(), h);
        if(it == this->m_layerMap[zIndex].end()) {
            this->m_layerMap[zIndex].push_back(h);
        }
        this->m_sourceMats[h] = mat;
        
    }
    void
    GLESVideoMixer::setOutput(std::shared_ptr<IOutput> output)
    {
        m_output = output;
    }
    const std::size_t
    GLESVideoMixer::hash(std::weak_ptr<ISource> source) const
    {
        const auto l = source.lock();
        if (l) {
            return std::hash< std::shared_ptr<ISource> >()(l);
        }
        return 0;
    }
    void
    GLESVideoMixer::mixThread()
    {
        const auto us = std::chrono::microseconds(static_cast<long long>(m_bufferDuration * 1000000.));
        
        pthread_setname_np("com.videocore.compositeloop");
        
        int current_fb = 0;
        
        bool locked[2] = {false};
        
        m_nextMixTime = std::chrono::steady_clock::now();
        
        while(!m_exiting.load())
        {
            std::unique_lock<std::mutex> l(m_mutex);
            const auto now = std::chrono::steady_clock::now();
            
            if(now >= m_nextMixTime) {
                
                m_nextMixTime += us;
                
                if(m_mixing.load() || m_paused.load()) {
                    continue;
                }
                
                locked[current_fb] = true;
                
                m_mixing = true;
                PERF_GL_async({
                    
                    glBindFramebuffer(GL_FRAMEBUFFER, this->m_fbo[current_fb]);
                    
                    glClear(GL_COLOR_BUFFER_BIT);
                    glBindBuffer(GL_ARRAY_BUFFER, this->m_vbo);

                    glUseProgram(this->m_prog);
                    
			        glEnableVertexAttribArray(m_vertexAttribPosition);
			        glEnableVertexAttribArray(m_vertexAttribTexCoord);
			        glVertexAttribPointer(m_vertexAttribPosition, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, BUFFER_OFFSET(0));
			        glVertexAttribPointer(m_vertexAttribTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, BUFFER_OFFSET(8));
                    for ( int i = m_zRange.first ; i <= m_zRange.second ; ++i) {
                        
                        for ( auto it = this->m_layerMap[i].begin() ; it != this->m_layerMap[i].end() ; ++ it) {

                            auto iTex = this->m_sourceBuffers.find(*it);
                            if(iTex == this->m_sourceBuffers.end()) continue;
                            
                            auto texture = iTex->second.currentTexture();
                            
                            if(texture) {
                                glUniformMatrix4fv(m_uMat, 1, GL_FALSE, &this->m_sourceMats[*it][0][0]);
                                glBindTexture(GL_TEXTURE_2D, texture);
                                glDrawArrays(GL_TRIANGLES, 0, 6);
                            } else {
                                DLog("Null texture!");
                            }
                        }
                    }
                    glFlush();

                    
                    auto lout = this->m_output.lock();
                    if(lout) {
                        
                        MetaData<'vide'> md(std::chrono::duration_cast<std::chrono::milliseconds>(m_nextMixTime - m_epoch).count());
                        lout->pushBuffer((uint8_t*)this->m_pixelBuffer[!current_fb].get(), sizeof(this->m_pixelBuffer[!current_fb].get()), md);
                    }
                    this->m_mixing = false;
                });
                current_fb = !current_fb;
            }
            m_mixThreadCond.wait_until(l, m_nextMixTime);
                
        }
    }
   
}
}
