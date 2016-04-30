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
#   include <videocore/sources/Apple/PixelBufferSource.h>
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


static const int kMinVideoBitrate = 32000;

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
    std::shared_ptr<videocore::Apple::PixelBufferSource>     m_pixelBufferSource;
    std::shared_ptr<videocore::AspectTransform>              m_pbAspect;
    std::shared_ptr<videocore::PositionTransform>            m_pbPosition;
    
    std::shared_ptr<videocore::Split> m_videoSplit;
    std::shared_ptr<videocore::AspectTransform>   m_aspectTransform;
    videocore::AspectTransform::AspectMode m_aspectMode;
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
    int    _bpsCeiling;
    int    _estimatedThroughput;

    BOOL   _useInterfaceOrientation;
    float  _videoZoomFactor;
    int    _audioChannelCount;
    float  _audioSampleRate;
    float  _micGain;

    VCCameraState _cameraState;
    VCAspectMode _aspectMode;
    VCSessionState _rtmpSessionState;
    BOOL   _orientationLocked;
    BOOL   _torch;

    BOOL _useAdaptiveBitrate;
    BOOL _continuousAutofocus;
    BOOL _continuousExposure;
    CGPoint _focusPOI;
    CGPoint _exposurePOI;
    
    VCFilter _filter;
}
@property (nonatomic, readwrite) VCSessionState rtmpSessionState;

- (void) setupGraph;

@end

@implementation VCSimpleSession
@dynamic videoSize;
@dynamic bitrate;
@dynamic fps;
@dynamic useInterfaceOrientation;
@dynamic orientationLocked;
@dynamic torch;
@dynamic cameraState;
@dynamic aspectMode;
@dynamic rtmpSessionState;
@dynamic videoZoomFactor;
@dynamic audioChannelCount;
@dynamic audioSampleRate;
@dynamic micGain;
@dynamic continuousAutofocus;
@dynamic continuousExposure;
@dynamic focusPointOfInterest;
@dynamic exposurePointOfInterest;
@dynamic useAdaptiveBitrate;
@dynamic estimatedThroughput;

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
- (BOOL) orientationLocked
{
    return _orientationLocked;
}
- (void) setOrientationLocked:(BOOL)orientationLocked
{
    _orientationLocked = orientationLocked;
    if(m_cameraSource) {
        m_cameraSource->setOrientationLocked(orientationLocked);
    }
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
- (void) setAspectMode:(VCAspectMode)aspectMode
{
    _aspectMode = aspectMode;
    switch (aspectMode) {
        case VCAscpectModeFill:
            m_aspectMode = videocore::AspectTransform::AspectMode::kAspectFill;
            break;
        case VCAspectModeFit:
            m_aspectMode = videocore::AspectTransform::AspectMode::kAspectFit;
            break;
        default:
            break;
    }
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
    if (NSOperationQueue.currentQueue != NSOperationQueue.mainQueue) {
        dispatch_async(dispatch_get_main_queue(), ^{
            // trigger in main thread, avoid autolayout engine exception
            if(self.delegate) {
                [self.delegate connectionStatusChanged:rtmpSessionState];
            }
        });
    } else {
        if (self.delegate) {
            [self.delegate connectionStatusChanged:rtmpSessionState];
        }
    }
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
    _audioChannelCount = MAX(1, MIN(channelCount, 2));

    if(m_audioMixer) {
        m_audioMixer->setChannelCount(_audioChannelCount);
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

- (void) setContinuousAutofocus:(BOOL)continuousAutofocus
{
    _continuousAutofocus = continuousAutofocus;
    if( m_cameraSource ) {
        m_cameraSource->setContinuousAutofocus(continuousAutofocus);
    }
}
- (BOOL) continuousAutofocus {
    return _continuousAutofocus;
}
- (void) setContinuousExposure:(BOOL)continuousExposure
{
    _continuousExposure = continuousExposure;
    if(m_cameraSource) {
        m_cameraSource->setContinuousExposure(continuousExposure);
    }
}

- (void) setFocusPointOfInterest:(CGPoint)focusPointOfInterest {
    _focusPOI = focusPointOfInterest;

    if(m_cameraSource) {
        m_cameraSource->setFocusPointOfInterest(focusPointOfInterest.x, focusPointOfInterest.y);
    }
}
- (CGPoint) focusPointOfInterest {
    return _focusPOI;
}
- (void) setExposurePointOfInterest:(CGPoint)exposurePointOfInterest
{
    _exposurePOI = exposurePointOfInterest;
    if(m_cameraSource) {
        m_cameraSource->setExposurePointOfInterest(exposurePointOfInterest.x, exposurePointOfInterest.y);
    }
}
- (CGPoint) exposurePointOfInterest {
    return _exposurePOI;
}

- (BOOL) useAdaptiveBitrate {
    return _useAdaptiveBitrate;
}
- (void) setUseAdaptiveBitrate:(BOOL)useAdaptiveBitrate {
    _useAdaptiveBitrate = useAdaptiveBitrate;
    _bpsCeiling = _bitrate;
}
- (int) estimatedThroughput {
    return _estimatedThroughput;
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
                useInterfaceOrientation:NO
                            cameraState:VCCameraStateBack
                             aspectMode:VCAspectModeFit];

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
                useInterfaceOrientation:useInterfaceOrientation
                            cameraState:VCCameraStateBack
                             aspectMode:VCAspectModeFit];
    }
    return self;
}

- (instancetype) initWithVideoSize:(CGSize)videoSize
                         frameRate:(int)fps
                           bitrate:(int)bps
           useInterfaceOrientation:(BOOL)useInterfaceOrientation
                       cameraState:(VCCameraState) cameraState
{
    if (( self = [super init] ))
    {
        [self initInternalWithVideoSize:videoSize
                              frameRate:fps
                                bitrate:bps
                useInterfaceOrientation:useInterfaceOrientation
                            cameraState:cameraState
                             aspectMode:VCAspectModeFit];
    }
    return self;
}

- (instancetype) initWithVideoSize:(CGSize)videoSize
                         frameRate:(int)fps
                           bitrate:(int)bps
           useInterfaceOrientation:(BOOL)useInterfaceOrientation
                       cameraState:(VCCameraState) cameraState
                        aspectMode:(VCAspectMode)aspectMode
{
    if (( self = [super init] ))
    {
        [self initInternalWithVideoSize:videoSize
                              frameRate:fps
                                bitrate:bps
                useInterfaceOrientation:useInterfaceOrientation
                            cameraState:cameraState
                             aspectMode:aspectMode];
    }
    return self;
}



- (void) initInternalWithVideoSize:(CGSize)videoSize
                         frameRate:(int)fps
                           bitrate:(int)bps
           useInterfaceOrientation:(BOOL)useInterfaceOrientation
                       cameraState:(VCCameraState) cameraState
                        aspectMode:(VCAspectMode)aspectMode
{
    self.bitrate = bps;
    self.videoSize = videoSize;
    self.fps = fps;
    _useInterfaceOrientation = useInterfaceOrientation;
    self.micGain = 1.f;
    self.audioChannelCount = 2;
    self.audioSampleRate = 44100.;
    self.useAdaptiveBitrate = NO;
    self.aspectMode = aspectMode;

    _previewView = [[VCPreviewView alloc] init];
    self.videoZoomFactor = 1.f;

    _cameraState = cameraState;
    _exposurePOI = _focusPOI = CGPointMake(0.5f, 0.5f);
    _continuousExposure = _continuousAutofocus = YES;

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

    __block VCSimpleSession* bSelf = self;

    dispatch_async(_graphManagementQueue, ^{
        [bSelf startSessionInternal:rtmpUrl streamKey:streamKey];
    });
}
- (void) startSessionInternal: (NSString*) rtmpUrl
                    streamKey: (NSString*) streamKey
{
    std::stringstream uri ;
    uri << (rtmpUrl ? [rtmpUrl UTF8String] : "") << "/" << (streamKey ? [streamKey UTF8String] : "");
    
    m_outputSession.reset(
                          new videocore::RTMPSession ( uri.str(),
                                                      [=](videocore::RTMPSession& session,
                                                          ClientState_t state) {

                                                          DLog("ClientState: %d\n", state);

                                                          switch(state) {

                                                              case kClientStateConnected:
                                                                  self.rtmpSessionState = VCSessionStateStarting;
                                                                  break;
                                                              case kClientStateSessionStarted:
                                                              {

                                                                  __block VCSimpleSession* bSelf = self;
                                                                  dispatch_async(_graphManagementQueue, ^{
                                                                      [bSelf addEncodersAndPacketizers];
                                                                  });
                                                              }
                                                                  self.rtmpSessionState = VCSessionStateStarted;

                                                                  break;
                                                              case kClientStateError:
                                                                  self.rtmpSessionState = VCSessionStateError;
                                                                  [self endRtmpSession];
                                                                  break;
                                                              case kClientStateNotConnected:
                                                                  self.rtmpSessionState = VCSessionStateEnded;
                                                                  [self endRtmpSession];
                                                                  break;
                                                              default:
                                                                  break;

                                                          }

                                                      }) );
    VCSimpleSession* bSelf = self;

    _bpsCeiling = _bitrate;

    if ( self.useAdaptiveBitrate ) {
        _bitrate = 500000;
    }

    m_outputSession->setBandwidthCallback([=](float vector, float predicted, int inst)
                                          {

                                              bSelf->_estimatedThroughput = predicted;
                                              auto video = std::dynamic_pointer_cast<videocore::IEncoder>( bSelf->m_h264Encoder );
                                              auto audio = std::dynamic_pointer_cast<videocore::IEncoder>( bSelf->m_aacEncoder );
                                              if(video && audio && bSelf.useAdaptiveBitrate) {

                                                  if ([bSelf.delegate respondsToSelector:@selector(detectedThroughput:)]) {
                                                      [bSelf.delegate detectedThroughput:predicted];
                                                  }
                                                  if ([bSelf.delegate respondsToSelector:@selector(detectedThroughput:videoRate:)]) {
                                                      [bSelf.delegate detectedThroughput:predicted videoRate:video->bitrate()];
                                                  }


                                                  int videoBr = 0;

                                                  if(vector != 0) {

                                                      vector = vector < 0 ? -1 : 1 ;

                                                      videoBr = video->bitrate();

                                                      if (audio) {

                                                          if ( videoBr > 500000 ) {
                                                              audio->setBitrate(128000);
                                                          } else if (videoBr <= 500000 && videoBr > 250000) {
                                                              audio->setBitrate(96000);
                                                          } else {
                                                              audio->setBitrate(80000);
                                                          }
                                                      }


                                                      if(videoBr > 1152000) {
                                                          video->setBitrate(std::min(int((videoBr / 384000 + vector )) * 384000, bSelf->_bpsCeiling) );
                                                      }
                                                      else if( videoBr > 512000 ) {
                                                          video->setBitrate(std::min(int((videoBr / 128000 + vector )) * 128000, bSelf->_bpsCeiling) );
                                                      }
                                                      else if( videoBr > 128000 ) {
                                                          video->setBitrate(std::min(int((videoBr / 64000 + vector )) * 64000, bSelf->_bpsCeiling) );
                                                      }
                                                      else {
                                                          video->setBitrate(std::max(std::min(int((videoBr / 32000 + vector )) * 32000, bSelf->_bpsCeiling), kMinVideoBitrate) );
                                                      }
                                                      DLog("\n(%f) AudioBR: %d VideoBR: %d (%f)\n", vector, audio->bitrate(), video->bitrate(), predicted);
                                                  } /* if(vector != 0) */

                                              } /* if(video && audio && m_adaptiveBREnabled) */


                                          });

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

    _bitrate = _bpsCeiling;

    self.rtmpSessionState = VCSessionStateEnded;
}
- (void) getCameraPreviewLayer:(AVCaptureVideoPreviewLayer **)previewLayer {
    if(m_cameraSource) {
        m_cameraSource->getPreviewLayer((void**)previewLayer);
    }
}

//Set property filter for the new enum + set dynamically the sourceFilter for the video mixer
- (void)setFilter:(VCFilter)filterToChange {
        NSString *filterName = @"com.videocore.filters.bgra";
        
        switch (filterToChange) {
            case VCFilterNormal:
                filterName = @"com.videocore.filters.bgra";
                break;
            case VCFilterGray:
                filterName = @"com.videocore.filters.grayscale";
                break;
            case VCFilterInvertColors:
                filterName = @"com.videocore.filters.invertColors";
                break;
            case VCFilterSepia:
                filterName = @"com.videocore.filters.sepia";
                break;
            case VCFilterFisheye:
                filterName = @"com.videocore.filters.fisheye";
                break;
            case VCFilterGlow:
                filterName = @"com.videocore.filters.glow";
                break;
            default:
                break;
        }
        
        _filter = filterToChange;
        NSLog(@"FILTER IS : [%d]", (int)_filter);
        std::string convertString([filterName UTF8String]);
        m_videoMixer->setSourceFilter(m_cameraSource, dynamic_cast<videocore::IVideoFilter*>(m_videoMixer->filterFactory().filter(convertString))); // default is com.videocore.filters.bgra
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
        m_cameraSource->setOrientationLocked(self.orientationLocked);
        auto aspectTransform = std::make_shared<videocore::AspectTransform>(self.videoSize.width,self.videoSize.height,m_aspectMode);

        auto positionTransform = std::make_shared<videocore::PositionTransform>(self.videoSize.width/2, self.videoSize.height/2,
                                                                                self.videoSize.width * self.videoZoomFactor, self.videoSize.height * self.videoZoomFactor,
                                                                                self.videoSize.width, self.videoSize.height
                                                                                );


        std::dynamic_pointer_cast<videocore::iOS::CameraSource>(m_cameraSource)->setupCamera(self.fps,(self.cameraState == VCCameraStateFront),self.useInterfaceOrientation,nil,^{
            m_cameraSource->setContinuousAutofocus(true);
            m_cameraSource->setContinuousExposure(true);

            m_cameraSource->setOutput(aspectTransform);

            m_videoMixer->setSourceFilter(m_cameraSource, dynamic_cast<videocore::IVideoFilter*>(m_videoMixer->filterFactory().filter("com.videocore.filters.bgra")));
            _filter = VCFilterNormal;
            aspectTransform->setOutput(positionTransform);
            positionTransform->setOutput(m_videoMixer);
            m_aspectTransform = aspectTransform;
            m_positionTransform = positionTransform;

            // Inform delegate that camera source has been added
            if ([_delegate respondsToSelector:@selector(didAddCameraSource:)]) {
                [_delegate didAddCameraSource:self];
            }
        });
    }
    {
        // Add mic source
        m_micSource = std::make_shared<videocore::iOS::MicSource>(self.audioSampleRate, self.audioChannelCount);
        m_micSource->setOutput(m_audioMixer);

        const auto epoch = std::chrono::steady_clock::now();

        m_audioMixer->setEpoch(epoch);
        m_videoMixer->setEpoch(epoch);

        m_audioMixer->start();
        m_videoMixer->start();

    }
}
- (void) addEncodersAndPacketizers
{
    int ctsOffset = 2000 / self.fps; // 2 * frame duration
    {
        // Add encoders

        m_aacEncoder = std::make_shared<videocore::iOS::AACEncode>(self.audioSampleRate, self.audioChannelCount, 96000);
        if(SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"8.0")) {
            // If >= iOS 8.0 use the VideoToolbox encoder that does not write to disk.
            m_h264Encoder = std::make_shared<videocore::Apple::H264Encode>(self.videoSize.width,
                                                                           self.videoSize.height,
                                                                           self.fps,
                                                                           self.bitrate,
                                                                           false,
                                                                           ctsOffset);
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
        m_h264Packetizer = std::make_shared<videocore::rtmp::H264Packetizer>(ctsOffset);
        m_aacPacketizer = std::make_shared<videocore::rtmp::AACPacketizer>(self.audioSampleRate, self.audioChannelCount, ctsOffset);

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


    m_h264Packetizer->setOutput(m_outputSession);
    m_aacPacketizer->setOutput(m_outputSession);

    
}
- (void) addPixelBufferSource: (UIImage*) image
                     withRect:(CGRect)rect {
    CGImageRef ref = [image CGImage];
    
    m_pixelBufferSource = std::make_shared<videocore::Apple::PixelBufferSource>(CGImageGetWidth(ref),
                                                                                CGImageGetHeight(ref),
                                                                                'BGRA');
    
    NSUInteger width = CGImageGetWidth(ref);
    NSUInteger height = CGImageGetHeight(ref);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    unsigned char *rawData = (unsigned char*) calloc(height * width * 4, sizeof(unsigned char));
    NSUInteger bytesPerPixel = 4;
    NSUInteger bytesPerRow = bytesPerPixel * width;
    NSUInteger bitsPerComponent = 8;
    CGContextRef context = CGBitmapContextCreate(rawData, width, height,
                                                 bitsPerComponent, bytesPerRow, colorSpace,
                                                 kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(colorSpace);
    
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), ref);
    CGContextRelease(context);
    
    m_pbAspect = std::make_shared<videocore::AspectTransform>(rect.size.width,rect.size.height,videocore::AspectTransform::kAspectFit);
    
    m_pbPosition = std::make_shared<videocore::PositionTransform>(rect.origin.x, rect.origin.y,
                                                                  rect.size.width, rect.size.height,
                                                                  self.videoSize.width, self.videoSize.height
                                                                            );
    m_pixelBufferSource->setOutput(m_pbAspect);
    m_pbAspect->setOutput(m_pbPosition);
    m_pbPosition->setOutput(m_videoMixer);
    m_videoMixer->registerSource(m_pixelBufferSource);
    m_pixelBufferSource->pushPixelBuffer(rawData, width * height * 4);
    
    free(rawData);
    
}
- (NSString *) applicationDocumentsDirectory
{
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *basePath = ([paths count] > 0) ? [paths objectAtIndex:0] : nil;
    return basePath;
}
@end
