
#import <AVFoundation/AVFoundation.h>

#include <videocore/transforms/Apple/MP4Multiplexer.h>

namespace videocore { namespace apple {
 
    
    MP4Multiplexer::MP4Multiplexer() : m_assetWriter(nullptr), m_videoInput(nullptr), m_audioInput(nullptr), m_fps(30)
    {
        
    }
    MP4Multiplexer::~MP4Multiplexer()
    {
        if(m_assetWriter) {
            
        }
    }
    
    void
    MP4Multiplexer::setSessionParameters(videocore::IMetadata &parameters)
    {
        auto & parms = dynamic_cast<videocore::apple::MP4SessionParameters_t&>(parameters);
        
        auto filename = parms.getData<kMP4SessionFilename>()  ;
        m_fps = parms.getData<kMP4SessionFPS>();
        
        m_filename = filename;
        
        CMFormatDescriptionRef audioDesc, videoDesc ;

        CMFormatDescriptionCreate(kCFAllocatorDefault, kCMMediaType_Audio, 'aac ', NULL, &audioDesc);
        CMFormatDescriptionCreate(kCFAllocatorDefault, kCMMediaType_Video, 'avc1', NULL, &videoDesc);
        
        AVAssetWriterInput* video = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo outputSettings:nil sourceFormatHint:videoDesc];
        AVAssetWriterInput* audio = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeAudio outputSettings:nil sourceFormatHint:audioDesc];
        
        NSURL* fileUrl = [NSURL fileURLWithPath:[NSString stringWithUTF8String:filename.c_str()]];
        AVAssetWriter* writer = [[AVAssetWriter alloc] initWithURL:fileUrl fileType:AVFileTypeQuickTimeMovie error:nil];
        
        [writer addInput:video];
        [writer addInput:audio];
        
    //    CMTime time = {0};
   //     time.timescale = m_fps;
  //      time.flags = kCMTimeFlags_Valid;
        
//        kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms
        
        //[writer startSessionAtSourceTime:time];
        //[writer startWriting];
    }
    void
    MP4Multiplexer::pushBuffer(const uint8_t *const data, size_t size, videocore::IMetadata &metadata)
    {
        switch(metadata.type()) {
            case 'vide':
                // Process video
                pushVideoBuffer(data,size,metadata);
                
                break;
            case 'soun':
                // Process audio
                pushAudioBuffer(data,size,metadata);
                
                break;
            default:
                break;
        }
    }
    
    void
    MP4Multiplexer::pushVideoBuffer(const uint8_t* const data, size_t size, IMetadata& metadata)
    {
        
    }
    void
    MP4Multiplexer::pushAudioBuffer(const uint8_t *const data, size_t size, videocore::IMetadata &metadata)
    {
        
    }
    
}
}