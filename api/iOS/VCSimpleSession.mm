//
//  VCSimpleSession.m
//  VideoCoreDevelopment
//
//  Created by James Hurley on 7/1/14.
//  Copyright (c) 2014 VideoCore. All rights reserved.
//

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
    
    using PixelBufferCallback = std::function<void(const uint8_t* const data, size_t size)> ;
    
    class PixelBufferOutput : public IOutput
    {
    public:
        PixelBufferOutput(PixelBufferCallback callback) : m_callback(callback) {};
        
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata)
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
    std::shared_ptr<videocore::Split> m_videoSplit;
    
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
- (void) setupGraph
{
    
}
@end
