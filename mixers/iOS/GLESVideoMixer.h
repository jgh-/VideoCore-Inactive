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
/*
 *  iOS::GLESVideoMixer mixer :: Takes CVPixelBufferRef inputs and outputs a single CVPixelBufferRef that has been mixed from the various sources.
 *
 *
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
 

    class GLESVideoMixer : public IVideoMixer
    {
      
    public:
        GLESVideoMixer( int frame_w, int frame_h, double frameDuration, std::function<void(void*)> excludeContext = nullptr);
        ~GLESVideoMixer();
        
        void registerSource(std::shared_ptr<ISource> source, size_t bufferSize = 0)  ;
        void unregisterSource(std::shared_ptr<ISource> source);
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
        void setOutput(std::shared_ptr<IOutput> output);
        void setEpoch(const std::chrono::steady_clock::time_point epoch) {
            m_epoch = epoch;
            m_nextMixTime = epoch;
        };
    private:
        
        const std::size_t hash(std::weak_ptr<ISource> source) const;
        void releaseBuffer(std::weak_ptr<ISource> source);
        void mixThread();
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
    };
    
}
}
#endif /* defined(__videocore__GLESVideoMixer__) */
