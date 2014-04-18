/*
 
 Video Core
 Copyright (C) 2014 James G. Hurley
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 USA
 
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
        void setSourceProperties(std::weak_ptr<ISource> source, videocore::SourceProperties properties);
        
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
        
        std::vector< std::size_t > m_layerMap[VideoLayer_Count];
        
        std::map< std::size_t, glm::mat4 >          m_sourceMats;
        std::map< std::size_t, CVPixelBufferRef >   m_sourceBuffers;
        std::map< std::size_t, CVOpenGLESTextureRef > m_sourceTextures;
        std::map< std::size_t, SourceProperties >    m_sourceProperties;
        std::atomic<bool> m_exiting;
    };
    
}
}
#endif /* defined(__videocore__GLESVideoMixer__) */
