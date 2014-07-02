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

#import "VCSimpleSession.h"

#include <videocore/rtmp/RTMPSession.h>
#include <videocore/transforms/RTMP/AACPacketizer.h>
#include <videocore/transforms/RTMP/H264Packetizer.h>
#include <videocore/transforms/Split.h>
#include <videocore/transforms/AspectTransform.h>
#include <videocore/transforms/PositionTransform.h>

#ifdef __APPLE__
#   include <videocore/mixers/Apple/AudioMixer.h>
#   ifdef TARGET_OS_IPHONE
#       include <videocore/sources/iOS/CameraSource.h>
#       include <videocore/sources/iOS/MicSource.h>
#       include <videocore/mixers/iOS/GLESVideoMixer.h>
#       include <videocore/transforms/iOS/AACEncode.h>
#       include <videocore/transforms/iOS/H264Encode.h>

#   else /* OS X */

#   endif
#else
#   include <videocore/mixers/GenericAudioMixer.h>
#endif

namespace videocore { namespace simpleApi {
    
    using PixelBufferCallback = std::function<void(const uint8_t* const data,
                                                   size_t size)> ;
    
    class PixelBufferOutput : public IOutput
    {
    public:
        PixelBufferOutput(PixelBufferCallback callback)
        : m_callback(callback) {};
        
        void pushBuffer(const uint8_t* const data,
                        size_t size,
                        IMetadata& metadata)
        {
            m_callback(data, size);
        }
        
    private:
        
        PixelBufferCallback m_callback;
    };
}
}

@interface VCSimpleSession()
{
    std::shared_ptr<videocore::simpleApi::PixelBufferOutput> m_pbOutput;
    std::shared_ptr<videocore::ISource> m_micSource;
    std::shared_ptr<videocore::ISource> m_cameraSource;
    
    std::weak_ptr<videocore::Split> m_videoSplit;
    
    std::vector< std::shared_ptr<videocore::ITransform> > m_audioTransformChain;
    std::vector< std::shared_ptr<videocore::ITransform> > m_videoTransformChain;
    
    std::shared_ptr<videocore::IOutputSession> m_outputSession;

    // properties
    
    CGSize _videoSize;
    int    _bitrate;
    int    _fps;
    VCCameraState _cameraState;
    BOOL   _torch;
}
@property (nonatomic, readwrite) VCSessionState rtmpSessionState;
@property (nonatomic, strong, readwrite) UIView* previewView;

- (void) setupGraph;

@end

@implementation VCSimpleSession
@dynamic videoSize;
@dynamic bitrate;
@dynamic fps;
@dynamic torch;
@dynamic cameraState;
// -----------------------------------------------------------------------------
//  Properties Methods
// -----------------------------------------------------------------------------
#pragma mark - Properties
- (CGSize) videoSize
{
    return _videoSize;
}
- (void) setVideoSize:(CGSize)videoSize
{
    _videoSize = videoSize;
}
- (int) bitrate
{
    return _bitrate;
}
- (void) setBitrate:(int)bitrate
{
    _bitrate = bitrate;
}
- (int) fps
{
    return _fps;
}
- (void) setFps:(int)fps
{
    _fps = fps;
}
- (BOOL) torch
{
    return _torch;
}
- (void) setTorch:(BOOL)torch
{
    _torch = torch;
}
- (VCCameraState) cameraState
{
    return _cameraState;
}
- (void) setCameraState:(VCCameraState)cameraState
{
    _cameraState = cameraState;
}
// -----------------------------------------------------------------------------
//  Public Methods
// -----------------------------------------------------------------------------
#pragma mark - Public Methods
- (instancetype) initWithVideoSize:(CGSize)videoSize
                         frameRate:(int)fps
                           bitrate:(int)bps
{
    if (( self = [super init] ))
    {
        self.bitrate = bps;
        self.videoSize = videoSize;
        self.fps = fps;
    }
    return self;
}

- (void) dealloc
{
    [self endRtmpSession];
}

- (void) startRtmpSessionWithURL:(NSString *)rtmpUrl
                    andStreamKey:(NSString *)streamKey
{
    
}
- (void) endRtmpSession
{
    m_outputSession.reset();
}

// -----------------------------------------------------------------------------
//  Private Methods
// -----------------------------------------------------------------------------
#pragma mark - Private Methods

- (void) addTransform: (std::shared_ptr<videocore::ITransform>) transform
              toChain:(std::vector<std::shared_ptr<videocore::ITransform> > &) chain
{
    if( chain.size() > 0 ) {
        chain.back()->setOutput(transform);
    }
    chain.push_back(transform);
}

- (void) setupGraph
{
    m_audioTransformChain.clear();
    m_videoTransformChain.clear();
    
    const double frameDuration = 1. / static_cast<double>(self.fps);
    
    {
        // Add audio mixer
        const double aacPacketTime = 1024. / 44100.0;
        
        [self addTransform:std::make_shared<videocore::Apple::AudioMixer>(2,44100,16,aacPacketTime)
                   toChain:m_audioTransformChain];

        
        // The H.264 Encoder introduces about 2 frames of latency, so we will set the minimum audio buffer duration to 2 frames.
        std::dynamic_pointer_cast<videocore::IAudioMixer>(m_audioTransformChain.back())->setMinimumBufferDuration(frameDuration*2);
    }
#ifdef __APPLE__
#ifdef TARGET_OS_IPHONE
    
    
    {
        // Add video mixer
        
        auto mixer = std::make_shared<videocore::iOS::GLESVideoMixer>(self.videoSize.width,
                                                                      self.videoSize.height,
                                                                      frameDuration);

        [self addTransform:mixer toChain:m_videoTransformChain];
        
        
    }
    
    {
        // [Optional] add splits
        // Splits would be used to add different graph branches at various
        // stages.  For example, if you wish to record an MP4 file while
        // streaming to RTMP.
        
        auto videoSplit = std::make_shared<videocore::Split>();
        
        m_videoSplit = videoSplit;
        m_pbOutput = std::make_shared<videocore::simpleApi::PixelBufferOutput>([&self](const void* const data, size_t size){
            
        });
        videoSplit->setOutput(m_pbOutput);
        [self addTransform:videoSplit toChain:m_videoTransformChain];
        
    }
    
    {
        // Add encoders
        
        [self addTransform:std::make_shared<videocore::iOS::AACEncode>(44100,2)
                   toChain:m_audioTransformChain];
        [self addTransform:std::make_shared<videocore::iOS::H264Encode>(self.videoSize.width,
                                                                        self.videoSize.height,
                                                                        self.fps,
                                                                        self.bitrate)
                   toChain:m_videoTransformChain];
        
    }
    
#else
#endif // TARGET_OS_IPHONE
#endif // __APPLE__
    
    [self addTransform:std::make_shared<videocore::rtmp::AACPacketizer>()
               toChain:m_audioTransformChain];
    [self addTransform:std::make_shared<videocore::rtmp::H264Packetizer>()
               toChain:m_videoTransformChain];

    
    
    const auto epoch = std::chrono::steady_clock::now();
    for(auto it = m_videoTransformChain.begin() ; it != m_videoTransformChain.end() ; ++it) {
        (*it)->setEpoch(epoch);
    }
    for(auto it = m_audioTransformChain.begin() ; it != m_audioTransformChain.end() ; ++it) {
        (*it)->setEpoch(epoch);
    }
    
    
    // Create sources
    {
        
        // Add camera source
        m_cameraSource = std::make_shared<videocore::iOS::CameraSource>();
        auto aspectTransform = std::make_shared<videocore::AspectTransform>(self.videoSize.width,self.videoSize.height,videocore::AspectTransform::kAspectFill);
        std::dynamic_pointer_cast<videocore::iOS::CameraSource>(m_cameraSource)->setupCamera(false);
        m_cameraSource->setOutput(aspectTransform);
        aspectTransform->setOutput(m_videoTransformChain.front());
    }
    {
        // Add mic source
        m_micSource = std::make_shared<videocore::iOS::MicSource>();
        m_micSource->setOutput(m_audioTransformChain.front());
        
    }
}
@end
