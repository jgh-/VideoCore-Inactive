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
#   include <videocore/transforms/Apple/MP4Multiplexer.h>
#   include <videocore/transforms/Apple/H264Encode.h>
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

#define SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(v)  ([[[UIDevice currentDevice] systemVersion] compare:v options:NSNumericSearch] != NSOrderedAscending)


#include <sstream>

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

    VCPreviewView* _previewView;

    std::shared_ptr<videocore::simpleApi::PixelBufferOutput> m_pbOutput;
    std::shared_ptr<videocore::iOS::MicSource>               m_micSource;
    std::shared_ptr<videocore::iOS::CameraSource>            m_cameraSource;

    std::shared_ptr<videocore::Split> m_videoSplit;
    std::shared_ptr<videocore::AspectTransform>   m_aspectTransform;
    std::shared_ptr<videocore::PositionTransform> m_positionTransform;
    std::shared_ptr<videocore::IAudioMixer> m_audioMixer;
    std::shared_ptr<videocore::IVideoMixer> m_videoMixer;
    std::shared_ptr<videocore::ITransform>  m_h264Encoder;
    std::shared_ptr<videocore::ITransform>  m_aacEncoder;
    std::shared_ptr<videocore::ITransform>  m_h264Packetizer;
    std::shared_ptr<videocore::ITransform>  m_aacPacketizer;

    std::shared_ptr<videocore::Split>       m_aacSplit;
    std::shared_ptr<videocore::Split>       m_h264Split;
    std::shared_ptr<videocore::Apple::MP4Multiplexer> m_muxer;

    std::shared_ptr<videocore::IOutputSession> m_outputSession;

    // properties

    dispatch_queue_t _graphManagementQueue;
    
    CGSize _videoSize;
    int    _bitrate;
    int    _fps;
    BOOL   _useInterfaceOrientation;
    float  _videoZoomFactor;
    int    _audioChannelCount;
    float  _audioSampleRate;
    float  _micGain;

    VCCameraState _cameraState;
    VCSessionState _rtmpSessionState;
    BOOL   _torch;
}
@property (nonatomic, readwrite) VCSessionState rtmpSessionState;

- (void) setupGraph;

@end

@implementation VCSimpleSession
@dynamic videoSize;
@dynamic bitrate;
@dynamic fps;
@dynamic useInterfaceOrientation;
@dynamic torch;
@dynamic cameraState;
@dynamic rtmpSessionState;
@dynamic videoZoomFactor;
@dynamic audioChannelCount;
@dynamic audioSampleRate;
@dynamic micGain;

@dynamic previewView;
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
- (BOOL) useInterfaceOrientation
{
    return _useInterfaceOrientation;
}
- (BOOL) torch
{
    return _torch;
}
- (void) setTorch:(BOOL)torch
{
    if(m_cameraSource) {
        _torch = m_cameraSource->setTorch(torch);
    }
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
- (void) setAudioChannelCount:(int)channelCount
{
    _audioChannelCount = MIN(2, MAX(channelCount,2)); // We can only support a channel count of 2 with AAC
    
    if(m_audioMixer) {
        m_audioMixer->setChannelCount(channelCount);
    }
}
- (int) audioChannelCount
{
    return _audioChannelCount;
}
- (void) setAudioSampleRate:(float)sampleRate
{
    
    _audioSampleRate = (sampleRate > 33075 ? 44100 : 22050); // We can only support 44100 / 22050 with AAC + RTMP
    if(m_audioMixer) {
        m_audioMixer->setFrequencyInHz(sampleRate);
    }
}
- (float) audioSampleRate
{
    return _audioSampleRate;
}
- (void) setMicGain:(float)micGain
{
    if(m_audioMixer) {
        m_audioMixer->setSourceGain(m_micSource, micGain);
        _micGain = micGain;
    }
}
- (float) micGain
{
    return _micGain;
}

- (UIView*) previewView {
    return _previewView;
}
// -----------------------------------------------------------------------------
//  Public Methods
// -----------------------------------------------------------------------------
#pragma mark - Public Methods
// -----------------------------------------------------------------------------

- (instancetype) initWithVideoSize:(CGSize)videoSize
                         frameRate:(int)fps
                           bitrate:(int)bps
{
    if((self = [super init])) {
        [self initInternalWithVideoSize:videoSize
                              frameRate:fps
                                bitrate:bps
                useInterfaceOrientation:NO];
        
    }
    return self;
}

- (instancetype) initWithVideoSize:(CGSize)videoSize
                         frameRate:(int)fps
                           bitrate:(int)bps
           useInterfaceOrientation:(BOOL)useInterfaceOrientation
{
    if (( self = [super init] ))
    {
        [self initInternalWithVideoSize:videoSize
                              frameRate:fps
                                bitrate:bps
                useInterfaceOrientation:useInterfaceOrientation];
    }
    return self;
}

- (void) initInternalWithVideoSize:(CGSize)videoSize
                         frameRate:(int)fps
                           bitrate:(int)bps
           useInterfaceOrientation:(BOOL)useInterfaceOrientation
{
    self.bitrate = bps;
    self.videoSize = videoSize;
    self.fps = fps;
    _useInterfaceOrientation = useInterfaceOrientation;
    self.micGain = 1.f;
    self.audioChannelCount = 2;
    self.audioSampleRate = 44100.;
    
    _previewView = [[VCPreviewView alloc] init];
    self.videoZoomFactor = 1.f;
    
    _cameraState = VCCameraStateBack;
    
    _graphManagementQueue = dispatch_queue_create("com.videocore.session.graph", 0);

    __block VCSimpleSession* bSelf = self;
    
    dispatch_async(_graphManagementQueue, ^{
        [bSelf setupGraph];
    });
    

}

- (void) dealloc
{
    [self endRtmpSession];
    m_audioMixer.reset();
    m_videoMixer.reset();
    m_videoSplit.reset();
    m_aspectTransform.reset();
    m_positionTransform.reset();
    m_micSource.reset();
    m_cameraSource.reset();
    m_pbOutput.reset();

    [_previewView release];
    _previewView = nil;

    dispatch_release(_graphManagementQueue);
    
    [super dealloc];
}

- (void) startRtmpSessionWithURL:(NSString *)rtmpUrl
                    andStreamKey:(NSString *)streamKey
{
    std::stringstream uri ;
    uri << [rtmpUrl UTF8String] << "/" << [streamKey UTF8String];

    m_outputSession.reset(
            new videocore::RTMPSession ( uri.str(),
                                        [=](videocore::RTMPSession& session,
                                            ClientState_t state) {

        switch(state) {

            case kClientStateConnected:
                self.rtmpSessionState = VCSessionStateStarting;
                break;
            case kClientStateSessionStarted:
            {
                self.rtmpSessionState = VCSessionStateStarted;
                
                __block VCSimpleSession* bSelf = self;
                dispatch_async(_graphManagementQueue, ^{
                    [bSelf addEncodersAndPacketizers];
                });
                
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
               self.audioSampleRate,
               (self.audioChannelCount == 2));

    m_outputSession->setSessionParameters(sp);
}
- (void) endRtmpSession
{

    m_h264Packetizer.reset();
    m_aacPacketizer.reset();
    m_videoSplit->removeOutput(m_h264Encoder);
    m_h264Encoder.reset();
    m_aacEncoder.reset();

    m_outputSession.reset();

    self.rtmpSessionState = VCSessionStateEnded;
}

// -----------------------------------------------------------------------------
//  Private Methods
// -----------------------------------------------------------------------------
#pragma mark - Private Methods


- (void) setupGraph
{
    const double frameDuration = 1. / static_cast<double>(self.fps);

    {
        // Add audio mixer
        const double aacPacketTime = 1024. / self.audioSampleRate;

        m_audioMixer = std::make_shared<videocore::Apple::AudioMixer>(self.audioChannelCount,
                                                                      self.audioSampleRate,
                                                                      16,
                                                                      aacPacketTime);


        // The H.264 Encoder introduces about 2 frames of latency, so we will set the minimum audio buffer duration to 2 frames.
        m_audioMixer->setMinimumBufferDuration(frameDuration*2);
    }
#ifdef __APPLE__
#ifdef TARGET_OS_IPHONE


    {
        // Add video mixer
        m_videoMixer = std::make_shared<videocore::iOS::GLESVideoMixer>(self.videoSize.width,
                                                                        self.videoSize.height,
                                                                        frameDuration);

    }

    {
        auto videoSplit = std::make_shared<videocore::Split>();

        m_videoSplit = videoSplit;
        VCPreviewView* preview = (VCPreviewView*)self.previewView;
        
        m_pbOutput = std::make_shared<videocore::simpleApi::PixelBufferOutput>([=](const void* const data, size_t size){
            CVPixelBufferRef ref = (CVPixelBufferRef)data;
            [preview drawFrame:ref];
            if(self.rtmpSessionState == VCSessionStateNone) {
                self.rtmpSessionState = VCSessionStatePreviewStarted;
            }
        });
        
        videoSplit->setOutput(m_pbOutput);

        m_videoMixer->setOutput(videoSplit);

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
                                                                                self.videoSize.width * self.videoZoomFactor, self.videoSize.height * self.videoZoomFactor,
                                                                                self.videoSize.width, self.videoSize.height
                                                                                );

        std::dynamic_pointer_cast<videocore::iOS::CameraSource>(m_cameraSource)->setupCamera(self.fps,false,self.useInterfaceOrientation);

        m_cameraSource->setOutput(aspectTransform);
        aspectTransform->setOutput(positionTransform);
        positionTransform->setOutput(m_videoMixer);
        m_aspectTransform = aspectTransform;
        m_positionTransform = positionTransform;
    }
    {
        // Add mic source
        m_micSource = std::make_shared<videocore::iOS::MicSource>(self.audioSampleRate, self.audioChannelCount);
        m_micSource->setOutput(m_audioMixer);


    }
}
- (void) addEncodersAndPacketizers
{
    {
        // Add encoders

        m_aacEncoder = std::make_shared<videocore::iOS::AACEncode>(self.audioSampleRate, self.audioChannelCount);
        if(SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"8.0")) {
            // If >= iOS 8.0 use the VideoToolbox encoder that does not write to disk.
            m_h264Encoder = std::make_shared<videocore::Apple::H264Encode>(self.videoSize.width,
                                                                           self.videoSize.height,
                                                                           self.fps,
                                                                           self.bitrate);
        } else {
            m_h264Encoder =std::make_shared<videocore::iOS::H264Encode>(self.videoSize.width,
                                                                        self.videoSize.height,
                                                                        self.fps,
                                                                        self.bitrate);
        }
        m_audioMixer->setOutput(m_aacEncoder);
        m_videoSplit->setOutput(m_h264Encoder);

    }
    {
        m_aacSplit = std::make_shared<videocore::Split>();
        m_h264Split = std::make_shared<videocore::Split>();
        m_aacEncoder->setOutput(m_aacSplit);
        m_h264Encoder->setOutput(m_h264Split);
    }
    {
        m_h264Packetizer = std::make_shared<videocore::rtmp::H264Packetizer>();
        m_aacPacketizer = std::make_shared<videocore::rtmp::AACPacketizer>(self.audioSampleRate, self.audioChannelCount);

        m_h264Split->setOutput(m_h264Packetizer);
        m_aacSplit->setOutput(m_aacPacketizer);

    }
    {
        /*m_muxer = std::make_shared<videocore::Apple::MP4Multiplexer>();
        videocore::Apple::MP4SessionParameters_t parms(0.) ;
        std::string file = [[[self applicationDocumentsDirectory] stringByAppendingString:@"/output.mp4"] UTF8String];
        parms.setData(file, self.fps, self.videoSize.width, self.videoSize.height);
        m_muxer->setSessionParameters(parms);
        m_aacSplit->setOutput(m_muxer);
        m_h264Split->setOutput(m_muxer);*/
    }
    const auto epoch = std::chrono::steady_clock::now();

    m_audioMixer->setEpoch(epoch);
    m_videoMixer->setEpoch(epoch);

    m_h264Packetizer->setOutput(m_outputSession);
    m_aacPacketizer->setOutput(m_outputSession);


}
- (NSString *) applicationDocumentsDirectory
{
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *basePath = ([paths count] > 0) ? [paths objectAtIndex:0] : nil;
    return basePath;
}
@end
