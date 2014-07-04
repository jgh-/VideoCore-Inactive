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


#ifndef __videocore__GLESVideoMixer__
#define __videocore__GLESVideoMixer__

#include <iostream>
#include <videocore/mixers/IVideoMixer.hpp>
#include <videocore/system/JobQueue.hpp>

#include <map>
#include <thread>
#include <mutex>
#include <glm/glm.hpp>
#include <CoreVideo/CoreVideo.h>
#include <vector>

namespace videocore { namespace iOS {
 
    /*
     *  Takes CVPixelBufferRef inputs and outputs a single CVPixelBufferRef that has been composited from the various sources.
     *  Sources must output VideoBufferMetadata with their buffers. This compositor uses homogeneous coordinates.
     */
    class GLESVideoMixer : public IVideoMixer
    {
      
    public:
        /*! Constructor.
         *
         *  \param frame_w          The width of the output frame
         *  \param frame_h          The height of the output frame
         *  \param frameDuration    The duration of time a frame is presented, in seconds. 30 FPS would be (1/30)
         *  \param excludeContext   An optional lambda method that is called when the mixer generates its GL ES context.
         *                          The parameter of this method will be a pointer to its EAGLContext.  This is useful for
         *                          applications that may be capturing GLES data and do not wish to capture the mixer.
         */
        GLESVideoMixer(int frame_w,
                       int frame_h,
                       double frameDuration,
                       std::function<void(void*)> excludeContext = nullptr);
        
        /*! Destructor */
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
        
    public:
        
        void mixPaused(bool paused);
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
         *                       The parameter of this method will be a pointer to its EAGLContext.  This is useful for
         *                       applications that may be capturing GLES data and do not wish to capture the mixer.
         */
        void setupGLES(std::function<void(void*)> excludeContext);
        
        
        
    private:
        
        JobQueue m_glJobQueue;
        
        double m_bufferDuration;
        
        std::weak_ptr<IOutput> m_output;
        std::vector< std::weak_ptr<ISource> > m_sources;
        
        std::thread m_mixThread;
        std::mutex  m_mutex;
        std::condition_variable m_mixThreadCond;
        
        
        CVPixelBufferRef m_pixelBuffer[2];
        CVOpenGLESTextureCacheRef m_textureCache;
        CVOpenGLESTextureRef      m_texture[2];
        
        void*       m_callbackSession;
        void*       m_glesCtx;
        unsigned    m_vbo, m_vao, m_fbo[2], m_prog, m_uMat;
        
        
        int m_frameW;
        int m_frameH;
        
        std::pair<int, int> m_zRange;
        std::map<int, std::vector< std::size_t >> m_layerMap;
        
        std::map< std::size_t, glm::mat4 >          m_sourceMats;
        std::map< std::size_t, CVPixelBufferRef >   m_sourceBuffers;
        std::map< std::size_t, CVOpenGLESTextureRef > m_sourceTextures;
        
        std::chrono::steady_clock::time_point m_epoch;
        std::chrono::steady_clock::time_point m_nextMixTime;
        
        std::atomic<bool> m_exiting;
        std::atomic<bool> m_mixing;
        std::atomic<bool> m_paused;
    };
    
}
}
#endif /* defined(__videocore__GLESVideoMixer__) */
