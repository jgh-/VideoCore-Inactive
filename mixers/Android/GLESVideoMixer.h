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

 #ifndef __videocore_android_GLESVideoMixer
 #define __videocore_android_GLESVideoMixer
 #include <videocore/mixers/IVideoMixer.hpp>
 #include <videocore/system/JobQueue.hpp>

 #include <videocore/system/GLESUtil.h>
 #include <videocore/system/pixelBuffer/Android/PixelBuffer.h>

 #include <EGL/egl.h>
 #include <EGL/eglext.h>

 #include <unordered_map>
 #include <map>
 #include <thread>
 #include <chrono>
 #include <atomic>

extern "C" {
    EGLAPI EGLImageKHR EGLAPIENTRY eglCreateImageKHR (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
    EGLAPI EGLBoolean EGLAPIENTRY  eglDestroyImageKHR (EGLDisplay dpy, EGLImageKHR image);
    GL_APICALL void GL_APIENTRY glEGLImageTargetTexture2DOES (GLenum target, GLeglImageOES image);
    GL_APICALL void GL_APIENTRY glEGLImageTargetRenderbufferStorageOES (GLenum target, GLeglImageOES image);
}

namespace videocore { namespace Android {

    struct SourceBuffer
    {
        SourceBuffer() : m_currentTexture(0), m_pixelBuffers() { };
        ~SourceBuffer() { };
        void setBuffer(Android::PixelBufferRef ref, JobQueue& jobQueue, void* display, void* surface, void* context);
        
        GLuint                  currentTexture() const { return m_currentTexture; };
        Android::PixelBufferRef currentBuffer() const { return m_currentBuffer; };
        
    private:
        typedef struct __Buffer_ {
            __Buffer_(Android::PixelBufferRef buf) : image(nullptr), texture(0), buffer(buf) {};
            ~__Buffer_() { if(texture) { glDeleteTextures(1,&texture); } if(image) { eglDestroyImageKHR(eglGetCurrentDisplay(), image); } };
                
            Android::PixelBufferRef buffer;
            EGLImageKHR  image;
            GLuint       texture;
            std::chrono::steady_clock::time_point time;
        } Buffer_;
        
        std::map<const GraphicBuffer*, Buffer_ >   m_pixelBuffers;
        Android::PixelBufferRef 			       m_currentBuffer;
        GLuint  				                   m_currentTexture;
    };

	class GLESVideoMixer : public IVideoMixer 
	{

	public:
		GLESVideoMixer(int frame_w, int frame_h, double frame_duration, std::function<void(void*)> exclude_context);
		~GLESVideoMixer();

		/*! IMixer::registerSource */
        void registerSource(std::shared_ptr<ISource> source,
                            size_t bufferSize = 0)  ;
        
        /*! IMixer::unregisterSource */
        void unregisterSource(std::shared_ptr<ISource> source);
        
        /*! IOutput::pushBuffer */
        void pushBuffer(const uint8_t* const data,
                        size_t size,
                        IMetadata& metadata);
        
        /*! ITransform::setOutput */
        void setOutput(std::shared_ptr<IOutput> output);
        
        /*! ITransform::setEpoch */
        void setEpoch(const std::chrono::steady_clock::time_point epoch) {
            m_epoch = epoch;
            m_nextMixTime = epoch;
        };

        /*! IVideoMixer::setSourceFilter */
        void setSourceFilter(std::weak_ptr<ISource> source, IVideoFilter *filter);
        
        FilterFactory& filterFactory() { return m_filterFactory; };

	private:

		/*!
         * Hash a smart pointer to a source.
         * 
         * \return a hash based on the smart pointer to a source.
         */
        const std::size_t hash(std::weak_ptr<ISource> source) const;
        
        /*!
         *  Release a currently-retained buffer from a source.
         *  
         *  \param source  The source that created the buffer to be released.
         */
        void releaseBuffer(std::weak_ptr<ISource> source);
        
        /*! Start the compositor thread */
        void mixThread();
        
        /*! 
         * Setup the OpenGL ES context, shaders, and state.
         *
         * \param excludeContext An optional lambda method that is called when the mixer generates its GL ES context.
         *                       The parameter of this method will be a pointer to its GL context.  This is useful for
         *                       applications that may be capturing GLES data and do not wish to capture the mixer.
         */
        void setupGLES(std::function<void(void*)> excludeContext);

        const int availableColorAttachments() const ;
    private:
        JobQueue m_glJobQueue;
        FilterFactory m_filterFactory;

        double m_bufferDuration;
        
        std::weak_ptr<IOutput> m_output;
        std::vector< std::weak_ptr<ISource> > m_sources;
        
        std::thread m_mixThread;
        std::mutex  m_mutex;
        std::condition_variable m_mixThreadCond;
       
        std::shared_ptr<PixelBuffer>   m_pixelBuffer[2];
        EGLImageKHR			           m_texture[2];

        unsigned  				  m_vbo, 
        						  m_vao, 
        						  m_fbo[2], 
                                  m_rbo[2],
        						  m_prog, 
        						  m_uMat;
        
        
        int m_frameW;
        int m_frameH;
        
        std::pair<int, int> m_zRange;
        std::map<int, std::vector< std::size_t >> m_layerMap;
        
        std::map< std::size_t, glm::mat4 >          m_sourceMats;
        
        std::unordered_map<std::size_t, IVideoFilter*>   m_sourceFilters;
        std::unordered_map<std::size_t, SourceBuffer> m_sourceBuffers;
        
        std::chrono::steady_clock::time_point m_epoch;
        std::chrono::steady_clock::time_point m_nextMixTime;
        
        std::atomic<bool> m_exiting;
        std::atomic<bool> m_mixing;
        std::atomic<bool> m_paused;

        EGLDisplay m_display;
        EGLContext m_context;
        EGLConfig  m_config;
        EGLSurface m_surface;

        GLuint m_vertexAttribPosition;
        GLuint m_vertexAttribTexCoord;
        
	};
}};

 #endif