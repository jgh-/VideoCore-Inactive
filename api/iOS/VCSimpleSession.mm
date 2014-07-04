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

#import <videocore/api/iOS/VCSimpleSession.h>
#import <videocore/api/iOS/VCPreviewView.h>

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
    std::shared_ptr<videocore::iOS::CameraSource> m_cameraSource;
    
    std::weak_ptr<videocore::Split> m_videoSplit;
    std::shared_ptr<videocore::AspectTransform> m_aspectTransform;
    std::shared_ptr<videocore::PositionTransform> m_positionTransform;
    
    std::vector< std::shared_ptr<videocore::ITransform> > m_audioTransformChain;
    std::vector< std::shared_ptr<videocore::ITransform> > m_videoTransformChain;
    
    std::shared_ptr<videocore::IOutputSession> m_outputSession;

    // properties
    
    CGSize _videoSize;
    int    _bitrate;
    int    _fps;
    float  _videoZoomFactor;
    
    VCCameraState _cameraState;
    VCSessionState _rtmpSessionState;
    BOOL   _torch;
}
@property (nonatomic, readwrite) VCSessionState rtmpSessionState;
@property (nonatomic, strong, readwrite) VCPreviewView* previewView;

- (void) setupGraph;

@end

static const float kAudioRate = 44100;

@implementation VCSimpleSession
@dynamic videoSize;
@dynamic bitrate;
@dynamic fps;
@dynamic torch;
@dynamic cameraState;
@dynamic rtmpSessionState;
@dynamic videoZoomFactor;
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
    if(m_aspectTransform) {
        m_aspectTransform->setBoundingSize(videoSize.width, videoSize.height);
    }
    if(m_positionTransform) {
        m_positionTransform->setSize(videoSize.width * self.videoZoomFactor,
                                     videoSize.height * self.videoZoomFactor);
    }
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
    if(_cameraState != cameraState) {
        _cameraState = cameraState;
        if(m_cameraSource) {
            m_cameraSource->toggleCamera();
        }
    }
}
- (void) setRtmpSessionState:(VCSessionState)rtmpSessionState
{
    _rtmpSessionState = rtmpSessionState;
    [self.delegate connectionStatusChanged:rtmpSessionState];
}
- (VCSessionState) rtmpSessionState
{
    return _rtmpSessionState;
}
- (float) videoZoomFactor
{
    return _videoZoomFactor;
}
- (void) setVideoZoomFactor:(float)videoZoomFactor
{
    _videoZoomFactor = videoZoomFactor;
    if(m_positionTransform) {
        // We could use AVCaptureConnection's zoom factor, but in reality it's
        // doing the exact same thing as this (in terms of the algorithm used),
        // but it is not clear how CoreVideo accomplishes it.
        // In this case this is just modifying the matrix
        // multiplication that is already happening once per frame.
        m_positionTransform->setSize(self.videoSize.width * videoZoomFactor,
                                     self.videoSize.height * videoZoomFactor);
    }
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
        self.previewView = [[VCPreviewView alloc] init];
        self.videoZoomFactor = 1.f;
        
        _cameraState = VCCameraStateBack;
        
        [self setupGraph];
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
    std::string uri([rtmpUrl UTF8String]);
    
    m_outputSession.reset(
            new videocore::RTMPSession ( uri,
                                        [=](videocore::RTMPSession& session,
                                            ClientState_t state) {
        
        switch(state) {
                
            case kClientStateSessionStarted:
            {
                self.rtmpSessionState = VCSessionStateStarted;
                [self addEncodersAndPacketizers];
            }
                break;
            case kClientStateError:
                self.rtmpSessionState = VCSessionStateError;
                break;
            case kClientStateNotConnected:
                self.rtmpSessionState = VCSessionStateEnded;
                break;
            default:
                break;
                
        }
        
    }) );
    videocore::RTMPSessionParameters_t sp ( 0. );
    
    sp.setData(self.videoSize.width,
               self.videoSize.height,
               1. / static_cast<double>(self.fps),
               self.bitrate,
               kAudioRate);
    
    m_outputSession->setSessionParameters(sp);
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
        const double aacPacketTime = 1024. / kAudioRate;
        
        [self addTransform:std::make_shared<videocore::Apple::AudioMixer>(2,kAudioRate,16,aacPacketTime)
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
        auto videoSplit = std::make_shared<videocore::Split>();
        
        m_videoSplit = videoSplit;
        VCPreviewView* preview = (VCPreviewView*)self.previewView;
        
        m_pbOutput = std::make_shared<videocore::simpleApi::PixelBufferOutput>([=](const void* const data, size_t size){
            CVPixelBufferRef ref = (CVPixelBufferRef)data;
            [preview drawFrame:ref];
        });
        videoSplit->setOutput(m_pbOutput);
        [self addTransform:videoSplit toChain:m_videoTransformChain];
        
    }
    
#else
#endif // TARGET_OS_IPHONE
#endif // __APPLE__
    
    // Create sources
    {
        
        // Add camera source
        m_cameraSource = std::make_shared<videocore::iOS::CameraSource>();
        auto aspectTransform = std::make_shared<videocore::AspectTransform>(self.videoSize.width,self.videoSize.height,videocore::AspectTransform::kAspectFit);
        
        auto positionTransform = std::make_shared<videocore::PositionTransform>(self.videoSize.width/2, self.videoSize.height/2,
                                                                                self.videoSize.width, self.videoSize.height,
                                                                                self.videoSize.width, self.videoSize.height
                                                                                );
        
        std::dynamic_pointer_cast<videocore::iOS::CameraSource>(m_cameraSource)->setupCamera(self.fps,false);
        m_cameraSource->setOutput(aspectTransform);
        aspectTransform->setOutput(positionTransform);
        positionTransform->setOutput(m_videoTransformChain.front());
        m_aspectTransform = aspectTransform;
        m_positionTransform = positionTransform;
    }
    {
        // Add mic source
        m_micSource = std::make_shared<videocore::iOS::MicSource>();
        m_micSource->setOutput(m_audioTransformChain.front());
        
    }
}
- (void) addEncodersAndPacketizers
{
    {
        // Add encoders
        
        [self addTransform:std::make_shared<videocore::iOS::AACEncode>(kAudioRate,2)
                   toChain:m_audioTransformChain];
        [self addTransform:std::make_shared<videocore::iOS::H264Encode>(self.videoSize.width,
                                                                        self.videoSize.height,
                                                                        self.fps,
                                                                        self.bitrate)
                   toChain:m_videoTransformChain];
        
    }
    {
        [self addTransform:std::make_shared<videocore::rtmp::AACPacketizer>()
                   toChain:m_audioTransformChain];
        [self addTransform:std::make_shared<videocore::rtmp::H264Packetizer>()
                   toChain:m_videoTransformChain];
        
    }
    const auto epoch = std::chrono::steady_clock::now();
    for(auto it = m_videoTransformChain.begin() ; it != m_videoTransformChain.end() ; ++it) {
        (*it)->setEpoch(epoch);
    }
    for(auto it = m_audioTransformChain.begin() ; it != m_audioTransformChain.end() ; ++it) {
        (*it)->setEpoch(epoch);
    }
    m_videoTransformChain.back()->setOutput(m_outputSession);
    m_audioTransformChain.back()->setOutput(m_outputSession);
    

    
}
@end
