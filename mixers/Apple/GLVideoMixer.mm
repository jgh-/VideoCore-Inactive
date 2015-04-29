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


#include <videocore/mixers/Apple/GLVideoMixer.h>
#include <videocore/sources/iOS/GLESUtil.h>
#include <videocore/filters/FilterFactory.h>

#import <Foundation/Foundation.h>
#ifdef __APPLE__
#   include <videocore/sources/iOS/GLESUtil.h>
#   include <TargetConditionals.h>
#   if (TARGET_OS_IPHONE)
#       include <OpenGLES/ES2/gl.h>
#       include <OpenGLES/ES3/gl.h>
#       import <UIKit/UIKit.h>
#   else
#       include <OpenGL/gl3.h>
#       include <OpenGL/gl3ext.h>
#       import  <OpenGL/OpenGL.h>
#       include <videocore/sources/iOS/GLESUtil.h>
#       define glDeleteVertexArraysOES glDeleteVertexArraysAPPLE
#       define glGenVertexArraysOES glGenVertexArraysAPPLE
#       define glBindVertexArrayOES glBindVertexArrayAPPLE
#       define CVOpenGLESTextureCacheFlush CVOpenGLTextureCacheFlush
#       define CVOpenGLESTextureGetName    CVOpenGLTextureGetName
#       define CVOpenGLESTextureGetTarget  CVOpenGLTextureGetTarget
#   endif
#endif

#include <CoreVideo/CoreVideo.h>

#include <glm/gtc/matrix_transform.hpp>

#define SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(v)  ([[[UIDevice currentDevice] systemVersion] compare:v options:NSNumericSearch] != NSOrderedAscending)


// Convenience macro to dispatch an OpenGL ES job to the created videocore::JobQueue
#if TARGET_OS_IPHONE
#define PERF_GL(x, dispatch) do {\
m_glJobQueue.dispatch([=](){\
EAGLContext* cur = [EAGLContext currentContext];\
if(m_glesCtx) {\
[EAGLContext setCurrentContext:(EAGLContext*)m_glesCtx];\
}\
x ;\
[EAGLContext setCurrentContext:cur];\
});\
} while(0)
#else
#define PERF_GL(x, dispatch) do {\
m_glJobQueue.dispatch([=](){\
CGLContextObj cur = CGLGetCurrentContext();\
if(m_glesCtx) {\
CGLSetCurrentContext((CGLContextObj)m_glesCtx);\
CGLLockContext((CGLContextObj)m_glesCtx);\
}\
x ;\
CGLUnlockContext((CGLContextObj)m_glesCtx);\
CGLSetCurrentContext(cur);\
});\
} while(0)
#endif
// Dispatch and execute synchronously
#define PERF_GL_sync(x) PERF_GL((x), enqueue_sync);
// Dispatch and execute asynchronously
#define PERF_GL_async(x) PERF_GL((x), enqueue);

@interface GLESObjCCallback : NSObject
{
    videocore::Apple::GLVideoMixer* _mixer;
}
- (void) setMixer: (videocore::Apple::GLVideoMixer*) mixer;
@end
@implementation GLESObjCCallback
- (instancetype) init {
    if((self = [super init])) {
#if TARGET_OS_IPHONE
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(notification:) name:UIApplicationDidEnterBackgroundNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(notification:) name:UIApplicationWillEnterForegroundNotification object:nil];
#endif
    }
    return self;
}
- (void) dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}
- (void) notification: (NSNotification*) notification {
#if TARGET_OS_IPHONE
    if([notification.name isEqualToString:UIApplicationDidEnterBackgroundNotification]) {
        
        _mixer->mixPaused(true);
        
    } else if([notification.name isEqualToString:UIApplicationWillEnterForegroundNotification]) {
        
        _mixer->mixPaused(false);
        
    }
#endif
}
- (void) setMixer: (videocore::Apple::GLVideoMixer*) mixer
{
    _mixer = mixer;
}
@end
namespace videocore { namespace Apple {
    
    // -------------------------------------------------------------------------
    //
    //  SourceBuffer::setBuffer
    //      Creates a related GLES texture and keeps track of the first-in-line
    //      texture.  Textures unused for more than 1 second will be released.
    //
    // -------------------------------------------------------------------------
    
    void
    SourceBuffer::setBuffer(Apple::PixelBufferRef ref, CVOpenGLESTextureCacheRef textureCache, JobQueue& m_glJobQueue, void* m_glesCtx)
    {
        
        bool flush = false;
        auto it = m_pixelBuffers.find(ref->cvBuffer());
        const auto now = std::chrono::steady_clock::now();
        
        if(m_currentBuffer) {
            m_currentBuffer->setState(kVCPixelBufferStateAvailable);
        }
        ref->setState(kVCPixelBufferStateAcquired);
        
        if(it == m_pixelBuffers.end()) {
            PERF_GL_async({
                
                ref->lock(true);
                OSType format = (OSType)ref->pixelFormat();
                bool is32bit = true;
                
                is32bit = (format != kCVPixelFormatType_16LE565);
                
                CVOpenGLESTextureRef texture = nullptr;
                const CVPixelBufferRef inCvpb = ref->cvBuffer();
                
                
#if TARGET_OS_IPHONE
                int w = (int)CVPixelBufferGetWidth(inCvpb);
                int h = (int)CVPixelBufferGetHeight(inCvpb);
                
                CVReturn ret = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                            textureCache,
                                                                            inCvpb,
                                                                            NULL,
                                                                            GL_TEXTURE_2D,
                                                                            is32bit ? GL_RGBA : GL_RGB,
                                                                            (GLsizei)w,
                                                                            (GLsizei)h,
                                                                            is32bit ? GL_BGRA : GL_RGB,
                                                                            is32bit ? GL_UNSIGNED_BYTE : GL_UNSIGNED_SHORT_5_6_5,
                                                                            0,
                                                                            &texture);
#else
                CVReturn ret;
                ret = CVOpenGLTextureCacheCreateTextureFromImage(kCFAllocatorDefault, textureCache, inCvpb, NULL, &texture);
                
#endif
                ref->unlock(true);
                if(ret == noErr && texture) {
                    GLint target = CVOpenGLESTextureGetTarget(texture);
                    glBindTexture(target, CVOpenGLESTextureGetName(texture));
                    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    
                    auto iit = this->m_pixelBuffers.emplace(inCvpb, ref).first;
                    iit->second.texture = texture;
                    this->m_currentBufferBacking = &iit->second;
                    this->m_currentBuffer = ref;
                    this->m_currentTexture = texture;
                    iit->second.time = now;
                    
                } else {
                    DLog("%d: Error creating texture! (%ld)", __LINE__, (long)ret);
                }
            });
            flush = true;
        } else {
            
            m_currentBufferBacking = &it->second;
            m_currentBuffer = ref;
            m_currentTexture = it->second.texture;
            it->second.time = now;
            
        }
        
        PERF_GL_async({
            //const auto currentBuffer = this->m_currentBuffer->cvBuffer();
            for ( auto it = this->m_pixelBuffers.begin() ; it != m_pixelBuffers.end() ; ) {
                
                if ( (it->second.buffer->isTemporary()) && it->second.buffer->cvBuffer() != this->m_currentBuffer->cvBuffer() ) {
                    // Buffer is temporary, release it.
                    it = this->m_pixelBuffers.erase(it);
                } else {
                    ++ it;
                }
                
            }
            if(flush) {
                CVOpenGLESTextureCacheFlush(textureCache, 0);
            }
            
        });
    }
    // -------------------------------------------------------------------------
    //
    //
    //
    //
    // -------------------------------------------------------------------------
    GLVideoMixer::GLVideoMixer( int frame_w,
                               int frame_h,
                               double frameDuration,
                               CVPixelBufferPoolRef pool,
                               std::function<void(void*)> excludeContext )
    : m_bufferDuration(frameDuration),
    m_glesCtx(nullptr),
    m_frameW(frame_w),
    m_frameH(frame_h),
    m_exiting(false),
    m_mixing(false),
    m_pixelBufferPool(pool),
    m_paused(false),
    m_glJobQueue("com.videocore.composite"),
    m_catchingUp(false),
    m_epoch(std::chrono::steady_clock::now())
    {
        PERF_GL_sync({
            
            this->setupGLES(excludeContext);
            
        });
        m_zRange.first = INT_MAX;
        m_zRange.second = INT_MIN;
        
        m_mixThreadSem = dispatch_semaphore_create(0);
        
        m_callbackSession = [[GLESObjCCallback alloc] init];
        [(GLESObjCCallback*)m_callbackSession setMixer:this];
        
    }
    
    GLVideoMixer::~GLVideoMixer()
    {
        m_output.reset();
        m_exiting = true;
        
        dispatch_semaphore_signal(m_mixThreadSem);
        DLog("GLVideoMixer::~GLVideoMixer()");
        if(m_mixThread.joinable()) {
            m_mixThread.join();
        }
        
        PERF_GL_sync({
            //glDeleteProgram(m_prog);
            glDeleteFramebuffers(kOutPBCount, m_fbo);
            glDeleteBuffers(1, &m_vbo);
            //glDeleteVertexArraysOES(1, &m_vao);
            GLuint textures[2] ;
            textures[0] = CVOpenGLESTextureGetName(m_texture[0]);
            textures[1] = CVOpenGLESTextureGetName(m_texture[1]);
            glDeleteTextures(2, textures);
            
            m_sourceBuffers.clear();
            
            CVPixelBufferRelease(m_pixelBuffer[0]);
            CVPixelBufferRelease(m_pixelBuffer[1]);
            CFRelease(m_texture[0]);
            CFRelease(m_texture[1]);
            CVOpenGLESTextureCacheFlush(m_textureCache, 0);
            CFRelease(m_textureCache);
            
            [(id)m_glesCtx release];
        });
        
        if(m_mixThread.joinable()) {
            m_mixThread.join();
        }
        m_glJobQueue.mark_exiting();
        m_glJobQueue.enqueue_sync([](){});
        
        
        [(id)m_callbackSession release];
        dispatch_release(m_mixThreadSem);
    }
    void
    GLVideoMixer::start() {
        m_mixThread = std::thread([this](){ this->mixThread(); });
    }
    void
    GLVideoMixer::setupGLES(std::function<void(void*)> excludeContext)
    {
#if TARGET_OS_IPHONE
        if(SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"7.0")) {
            m_glesCtx = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
        }
        if(!m_glesCtx) {
            m_glesCtx = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
        }
        if(!m_glesCtx) {
            std::cerr << "Error! Unable to create an OpenGL ES 2.0 or 3.0 Context!" << std::endl;
            return;
        }
        [EAGLContext setCurrentContext:nil];
        [EAGLContext setCurrentContext:(EAGLContext*)m_glesCtx];
        if(excludeContext) {
            excludeContext(m_glesCtx);
        }
#else
        CGLPixelFormatObj pix;
        CGLError errorCode;
        GLint num; // stores the number of possible pixel formats
        CGLPixelFormatAttribute attributes[] = {
            kCGLPFAAccelerated,   // no software rendering
            kCGLPFAOpenGLProfile, // core profile with the version stated below
            (CGLPixelFormatAttribute) kCGLOGLPVersion_3_2_Core,
            (CGLPixelFormatAttribute) 0
        };
        errorCode = CGLChoosePixelFormat( attributes, &pix, &num );
        CGLContextObj ctx;
        
        errorCode = CGLCreateContext(pix, nullptr, &ctx);
        CGLSetCurrentContext(ctx);
        CGLLockContext(ctx);
        m_glesCtx = (void*)ctx;
#endif
        //
        // Shared-memory FBOs
        //
        // What we are doing here is creating a couple of shared-memory textures to double-buffer the mixer and give us
        // direct access to the framebuffer data.
        //
        // There are several steps in this process:
        // 1. Create CVPixelBuffers that are created as IOSurfaces.  This is mandatory and only
        //    requires specifying the kCVPixelBufferIOSurfacePropertiesKey.
        // 2. Create a CVOpenGLESTextureCache
        // 3. Create a CVOpenGLESTextureRef for each CVPixelBuffer.
        // 4. Create an OpenGL ES Framebuffer for each CVOpenGLESTextureRef and set that texture as the color attachment.
        //
        // We may now attach these FBOs as the render target and avoid using the costly glGetPixels.
        
        @autoreleasepool {
            
            if(!m_pixelBufferPool) {
                NSDictionary* pixelBufferOptions = @{ (NSString*) kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA),
                                                      (NSString*) kCVPixelBufferWidthKey : @(m_frameW),
                                                      (NSString*) kCVPixelBufferHeightKey : @(m_frameH),
#if TARGET_OS_IPHONE
                                                      (NSString*) kCVPixelBufferOpenGLESCompatibilityKey : @YES,
                                                      (NSString*) kCVPixelBufferIOSurfacePropertiesKey : @{}
#endif
                                                      };
                
                CVPixelBufferCreate(kCFAllocatorDefault, m_frameW, m_frameH, kCVPixelFormatType_32BGRA, (CFDictionaryRef)pixelBufferOptions, &m_pixelBuffer[0]);
                CVPixelBufferCreate(kCFAllocatorDefault, m_frameW, m_frameH, kCVPixelFormatType_32BGRA, (CFDictionaryRef)pixelBufferOptions, &m_pixelBuffer[1]);
            }
            else {
                CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, m_pixelBufferPool, &m_pixelBuffer[0]);
                CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, m_pixelBufferPool, &m_pixelBuffer[1]);
            }
            
        }
        
#if TARGET_OS_IPHONE
        CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, NULL, (EAGLContext*)this->m_glesCtx, NULL, &this->m_textureCache);
#else
        CVOpenGLTextureCacheCreate(kCFAllocatorDefault,
                                   nullptr,
                                   (CGLContextObj)m_glesCtx,
                                   pix,
                                   nullptr,
                                   &m_textureCache);
        CGLDestroyPixelFormat( pix );
#endif
        
        glGenFramebuffers(2, this->m_fbo);
        for(int i = 0 ; i < 2 ; ++i) {
#if TARGET_OS_IPHONE
            CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault, this->m_textureCache, this->m_pixelBuffer[i], NULL, GL_TEXTURE_2D, GL_RGBA, m_frameW, m_frameH, GL_BGRA, GL_UNSIGNED_BYTE, 0, &m_texture[i]);
#else
            CVOpenGLTextureCacheCreateTextureFromImage(kCFAllocatorDefault, this->m_textureCache, this->m_pixelBuffer[i], NULL, &m_texture[i]);
#endif
            GLenum textarget = CVOpenGLESTextureGetTarget(m_texture[i]);
            GLint texname = CVOpenGLESTextureGetName(m_texture[i]);
            glBindTexture(textarget, texname);
            glTexParameterf(textarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameterf(textarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameterf(textarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(textarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textarget, texname, 0);

            GL_FRAMEBUFFER_STATUS(__LINE__);
        }
        
        GL_ERRORS(__LINE__);
        
        
        glGenBuffers(1, &this->m_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, this->m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(s_vbo), s_vbo, GL_STATIC_DRAW);
        
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, m_frameW,m_frameH);
        glClearColor(0.05,0.05,0.07,1.0);

        CVOpenGLESTextureCacheFlush(m_textureCache, 0);
        GL_ERRORS(__LINE__);
    }
    void
    GLVideoMixer::registerSource(std::shared_ptr<ISource> source,
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
    GLVideoMixer::releaseBuffer(std::weak_ptr<ISource> source)
    {
        DLog("GLVideoMixer::releaseBuffer");
        const auto h = hash(source);
        auto it = m_sourceBuffers.find(h) ;
        if(it != m_sourceBuffers.end()) {
            m_sourceBuffers.erase(it);
        }
        
    }
    void
    GLVideoMixer::unregisterSource(std::shared_ptr<ISource> source)
    {
        DLog("GLVideoMixer::unregisterSource");
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
    GLVideoMixer::pushBuffer(const uint8_t *const data,
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
        
        
        auto inPixelBuffer = *(Apple::PixelBufferRef*)data ;
        
        m_sourceBuffers[h].setBuffer(inPixelBuffer, this->m_textureCache, m_glJobQueue, m_glesCtx);
        m_sourceBuffers[h].setBlends(md.getData<kVideoMetadataBlends>());
        
        auto it = std::find(this->m_layerMap[zIndex].begin(), this->m_layerMap[zIndex].end(), h);
        if(it == this->m_layerMap[zIndex].end()) {
            this->m_layerMap[zIndex].push_back(h);
        }
        this->m_sourceMats[h] = mat;
    }
    void
    GLVideoMixer::setOutput(std::shared_ptr<IOutput> output)
    {
        m_output = output;
    }
    const std::size_t
    GLVideoMixer::hash(std::weak_ptr<ISource> source) const
    {
        const auto l = source.lock();
        if (l) {
            return std::hash< std::shared_ptr<ISource> >()(l);
        }
        return 0;
    }
    void
    GLVideoMixer::mixThread()
    {
        const auto us = std::chrono::microseconds(static_cast<long long>(m_bufferDuration * 1000000.));
        const auto us_25 = std::chrono::microseconds(static_cast<long long>(m_bufferDuration * 250000.));
        m_us25 = us_25;
        
        pthread_setname_np("com.videocore.compositeloop");
        
        int current_fb = 0;
        
        bool locked[2] = {false};
        
        m_nextMixTime = m_epoch;
        auto lastMixTime = m_epoch;
        while(!m_exiting.load())
        {
            const auto now = std::chrono::steady_clock::now();
            
            if(now >= (m_nextMixTime)) {
                
                auto currentTime = m_nextMixTime;
                if(!m_shouldSync) {
                    m_nextMixTime += us;
                } else {
                    m_nextMixTime = m_syncPoint > m_nextMixTime ? m_syncPoint + us : m_nextMixTime + us;
                }
                
                
                if(m_mixing.load() || m_paused.load()) {
                    continue;
                }
                
                locked[current_fb] = true;
                
                m_mixing = true;
                
                PERF_GL_sync({
                    compose(current_fb, currentTime, lastMixTime);
                });
                
                current_fb = (current_fb + 1) % kOutPBCount;
                lastMixTime = currentTime;
            }
            
            while(std::chrono::steady_clock::now() >= m_nextMixTime) {
                m_nextMixTime += us;
            }
            dispatch_semaphore_wait(m_mixThreadSem, dispatch_time(DISPATCH_TIME_NOW,
                                                                  std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                                                                                       m_nextMixTime -
                                                                                                                       std::chrono::steady_clock::now())
                                                                  .count()));
            
        }
    }
    void
    GLVideoMixer::compose(int fbo, std::chrono::steady_clock::time_point currentTime, std::chrono::steady_clock::time_point lastMixTime)
    {
        glPushGroupMarkerEXT(0, "Videocore.Mix");
        glBindFramebuffer(GL_FRAMEBUFFER, this->m_fbo[fbo]);
        
        IVideoFilter* currentFilter = nil;
        glClear(GL_COLOR_BUFFER_BIT);
        for ( int i = m_zRange.first ; i <= m_zRange.second ; ++i) {
            
            for ( auto it = this->m_layerMap[i].begin() ; it != this->m_layerMap[i].end() ; ++ it) {
                CVOpenGLESTextureRef texture = NULL;
                auto filterit = m_sourceFilters.find(*it);
                if(filterit == m_sourceFilters.end()) {
                    IFilter* filter = m_filterFactory.filter("com.videocore.filters.bgra");
                    m_sourceFilters[*it] = dynamic_cast<IVideoFilter*>(filter);
                }
                if(currentFilter != m_sourceFilters[*it]) {
                    if(currentFilter) {
                        currentFilter->unbind();
                    }
                    currentFilter = m_sourceFilters[*it];
                    
                    if(currentFilter && !currentFilter->initialized()) {
                        currentFilter->setFilterLanguage(TARGET_OS_IPHONE ? GL_ES2_3 : GL_3);
                        currentFilter->initialize();
                    }
                }
                
                auto iTex = this->m_sourceBuffers.find(*it);
                if(iTex == this->m_sourceBuffers.end()) continue;
                
                texture = iTex->second.currentTexture();
                
                // TODO: Add blending.
                if(iTex->second.blends()) {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                }
                if(texture && currentFilter) {
                    currentFilter->incomingMatrix(this->m_sourceMats[*it]);
                    currentFilter->imageDimensions(iTex->second.currentBuffer()->width(), iTex->second.currentBuffer()->height());
                    currentFilter->bind();
                    glBindTexture(CVOpenGLESTextureGetTarget(texture), CVOpenGLESTextureGetName(texture));
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                } else {
                    DLog("Null texture!");
                }
                if(iTex->second.blends()) {
                    glDisable(GL_BLEND);
                }
            }
        }
        glFlush();
        glPopGroupMarkerEXT();
        
        
        auto lout = this->m_output.lock();
        if(lout) {
            
            MetaData<'vide'> md(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_epoch).count());
            lout->pushBuffer((uint8_t*)this->m_pixelBuffer[!fbo], sizeof(this->m_pixelBuffer[!fbo]), md);
        }
        this->m_mixing = false;
        
    }
    
    void
    GLVideoMixer::mixPaused(bool paused)
    {
        m_paused = paused;
    }
    
    void
    GLVideoMixer::setSourceFilter(std::weak_ptr<ISource> source, IVideoFilter *filter) {
        auto h = hash(source);
        m_sourceFilters[h] = filter;
    }
    void
    GLVideoMixer::sync() {
        m_syncPoint = std::chrono::steady_clock::now();
        m_shouldSync = true;
        //if(m_syncPoint >= (m_nextMixTime)) {
        //    m_mixThreadCond.notify_all();
        //}
    }
}
}
