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
#import "VCPreviewView.h"

#include <videocore/sources/iOS/GLESUtil.h>

#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>

#import <glm/glm.hpp>
#import <glm/gtc/matrix_transform.hpp>

#include <atomic>

@interface VCPreviewView()
{
    GLuint _renderBuffer;
    GLuint _shaderProgram;
    GLuint _vbo;
    GLuint _fbo;
    GLuint _vao;
    GLuint _matrixPos;
    
    int _currentBuffer;
    
    std::atomic<bool> _paused;
    
    CVPixelBufferRef _currentRef[2];
    CVOpenGLESTextureCacheRef _cache;
    CVOpenGLESTextureRef _texture[2];
    
}
@property (nonatomic, strong) EAGLContext* context;
@property (nonatomic) CAEAGLLayer* glLayer;
@end
@implementation VCPreviewView

#pragma mark - UIView overrides

+ (Class) layerClass
{
    return [CAEAGLLayer class];
}

- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        
        
    }
    return self;
}
- (instancetype) init {
    if ((self = [super init])) {
        [self initInternal];
    }
    return self;
}
- (void) awakeFromNib {
    [self initInternal];
}
- (void) initInternal {
    // Initialization code
    self.glLayer = (CAEAGLLayer*)self.layer;
    
    NSLog(@"Creating context");
    _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    
    if(!self.context) {
        NSLog(@"Context creation failed");
    }
    self.autoresizingMask = 0xFF;
    
    _currentRef [0] = _currentRef[1] = nil;
    _texture[0] = _texture[1] = nil;
    _shaderProgram = 0;
    _renderBuffer = 0;
    _currentBuffer = 1;
    
    __block VCPreviewView* bSelf = self;
    
    _paused = NO;
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [bSelf setupGLES];
    });
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(notification:) name:UIApplicationDidEnterBackgroundNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(notification:) name:UIApplicationWillEnterForegroundNotification object:nil];
}
- (void) dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    if(_texture[0]) {
        CFRelease(_texture[0]);
    }
    if(_texture[1]) {
        CFRelease(_texture[1]);
    }
    if(_currentRef[0]) {
        CVPixelBufferRelease(_currentRef[0]);
    }
    if(_currentRef[1]) {
        CVPixelBufferRelease(_currentRef[1]);
    }
    if(_shaderProgram) {
        glDeleteProgram(_shaderProgram);
    }
    if(_vbo) {
        glDeleteBuffers(1, &_vbo);
    }
    if(_vao) {
        glDeleteVertexArrays(1, &_vao);
    }
    if(_fbo) {
        glDeleteFramebuffers(1, &_fbo);
    }
    if(_renderBuffer) {
        glDeleteRenderbuffers(1, &_renderBuffer);
    }
    CVOpenGLESTextureCacheFlush(_cache,0);
    CFRelease(_cache);
    
    self.context = nil;
    [super dealloc];
}
- (void) layoutSubviews
{
    self.backgroundColor = [UIColor blackColor];
    [self generateGLESBuffers];
}
- (void) notification: (NSNotification*) notification {
    if([notification.name isEqualToString:UIApplicationDidEnterBackgroundNotification]) {
        _paused = true;
    } else if([notification.name isEqualToString:UIApplicationWillEnterForegroundNotification]) {
        _paused = false;
    }
}
#pragma mark - Public Methods

- (void) drawFrame:(CVPixelBufferRef)pixelBuffer
{
    
    if(_paused) return;
    
    bool updateTexture = false;
    
    if(pixelBuffer != _currentRef[_currentBuffer]) {
        // not found, swap buffers.
        _currentBuffer = !_currentBuffer;
    }
    
    if(pixelBuffer != _currentRef[_currentBuffer]) {
        // Still not found, update the texture for this buffer.
        if(_currentRef[_currentBuffer]){
            CVPixelBufferRelease(_currentRef[_currentBuffer]);
        }
        
        _currentRef[_currentBuffer] = CVPixelBufferRetain(pixelBuffer);
        updateTexture = true;
        
    }
    int currentBuffer = _currentBuffer;
    __block VCPreviewView* bSelf = self;
    
    dispatch_async(dispatch_get_main_queue(), ^{
        
        EAGLContext* current = [EAGLContext currentContext];
        [EAGLContext setCurrentContext:bSelf.context];
        
        if(updateTexture) {
            // create a new texture
            if(bSelf->_texture[currentBuffer]) {
                CFRelease(bSelf->_texture[currentBuffer]);
            }
            
            CVPixelBufferLockBaseAddress(bSelf->_currentRef[currentBuffer],
                                         kCVPixelBufferLock_ReadOnly);
            CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                         bSelf->_cache,
                                                         bSelf->_currentRef[currentBuffer],
                                                         NULL,
                                                         GL_TEXTURE_2D,
                                                         GL_RGBA,
                                                         (GLsizei)CVPixelBufferGetWidth(bSelf->_currentRef[currentBuffer]),
                                                         (GLsizei)CVPixelBufferGetHeight(bSelf->_currentRef[currentBuffer]),
                                                         GL_BGRA,
                                                         GL_UNSIGNED_BYTE,
                                                         0,
                                                         &bSelf->_texture[currentBuffer]);
            
            glBindTexture(GL_TEXTURE_2D,
                          CVOpenGLESTextureGetName(bSelf->_texture[currentBuffer]));
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            
            CVPixelBufferUnlockBaseAddress(bSelf->_currentRef[currentBuffer],
                                           kCVPixelBufferLock_ReadOnly);
            
        }
        
        // draw
        glBindFramebuffer(GL_FRAMEBUFFER, bSelf->_fbo);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindTexture(GL_TEXTURE_2D, CVOpenGLESTextureGetName(bSelf->_texture[currentBuffer]));
        
        glm::mat4 matrix(1.f);
        float width = CVPixelBufferGetWidth(bSelf->_currentRef[currentBuffer]);
        float height = CVPixelBufferGetHeight(bSelf->_currentRef[currentBuffer]);
        
        
        float wfac = float(bSelf.bounds.size.width) / width;
        float hfac = float(bSelf.bounds.size.height) / height;
        
        bool aspectFit = false;
        
        const float mult = (aspectFit ? (wfac < hfac) : (wfac > hfac)) ? wfac : hfac;
        
        wfac = width*mult / float(bSelf.bounds.size.width);
        hfac = height*mult / float(bSelf.bounds.size.height);
        
        matrix = glm::scale(matrix, glm::vec3(1.f * wfac,-1.f * hfac,1.f));
        
        glUniformMatrix4fv(bSelf->_matrixPos, 1, GL_FALSE, &matrix[0][0]);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        GL_ERRORS(__LINE__)
        glBindRenderbuffer(GL_RENDERBUFFER, bSelf->_renderBuffer);
        if(!bSelf->_paused.load()) {
            [self.context presentRenderbuffer:GL_RENDERBUFFER];
        }
        [EAGLContext setCurrentContext:current];
        CVOpenGLESTextureCacheFlush(_cache,0);
    });
    
}
#pragma mark - Private Methods

- (void) generateGLESBuffers
{
    EAGLContext* current = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:self.context];
    
    if(_renderBuffer) {
        glDeleteRenderbuffers(1, &_renderBuffer);
    }
    if(_fbo) {
        glDeleteFramebuffers(1, &_fbo);
    }
    glGenRenderbuffers(1, &_renderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _renderBuffer);
    
    [self.context renderbufferStorage:GL_RENDERBUFFER fromDrawable:self.glLayer];
    
    GL_ERRORS(__LINE__)
    
    glGenFramebuffers(1, &_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    int width, height;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _renderBuffer);
    GL_ERRORS(__LINE__)
    
    glClearColor(0.f, 0.f, 0.f, 1.f);
    
    glViewport(0, 0, self.glLayer.bounds.size.width, self.glLayer.bounds.size.height);
    
    [EAGLContext setCurrentContext:current];
}
- (void) setupGLES
{
    EAGLContext* current = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:self.context];
    CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, NULL, self.context, NULL, &_cache);
    
    glGenVertexArraysOES(1, &_vao);
    glBindVertexArrayOES(_vao);
    
    glGenBuffers(1, &_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_vbo), s_vbo, GL_STATIC_DRAW);
    
    _shaderProgram = build_program(s_vs_mat, s_fs);
    glUseProgram(_shaderProgram);
    
    int attrpos = glGetAttribLocation(_shaderProgram, "aPos");
    int attrtex = glGetAttribLocation(_shaderProgram, "aCoord");
    int unitex = glGetUniformLocation(_shaderProgram, "uTex0");
    
    _matrixPos = glGetUniformLocation(_shaderProgram, "uMat");
    
    glUniform1i(unitex, 0);
    
    glEnableVertexAttribArray(attrpos);
    glEnableVertexAttribArray(attrtex);
    glVertexAttribPointer(attrpos, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, BUFFER_OFFSET(0));
    glVertexAttribPointer(attrtex, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, BUFFER_OFFSET(8));
    
    [EAGLContext setCurrentContext:current];
}

@end
