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
#include <Availability.h>
#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>

#if TARGET_OS_IPHONE
#if defined(__IPHONE_8_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_8_0
#define VERSION_OK 1
#else
#define VERSION_OK 0
#endif
#else
#define VERSION_OK (1090 <= MAC_OS_X_VERSION_MAX_ALLOWED)
#endif

#include <stdio.h>
#include <videocore/transforms/Apple/H264Encode.h>
#include <videocore/mixers/IVideoMixer.hpp>

#if VERSION_OK==1
#include <VideoToolbox/VideoToolbox.h>
#endif

namespace videocore { namespace Apple {
    
    static CMTimeValue s_forcedKeyframePTS = 0;
    
#if VERSION_OK
    void vtCallback(void *outputCallbackRefCon,
                    void *sourceFrameRefCon,
                    OSStatus status,
                    VTEncodeInfoFlags infoFlags,
                    CMSampleBufferRef sampleBuffer )
    {
        CMBlockBufferRef block = CMSampleBufferGetDataBuffer(sampleBuffer);
        CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
        CMTime pts = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
        CMTime dts = CMSampleBufferGetDecodeTimeStamp(sampleBuffer);
        
        //printf("status: %d\n", (int) status);
        bool isKeyframe = false;
        if(attachments != NULL) {
            CFDictionaryRef attachment;
            CFBooleanRef dependsOnOthers;
            attachment = (CFDictionaryRef)CFArrayGetValueAtIndex(attachments, 0);
            dependsOnOthers = (CFBooleanRef)CFDictionaryGetValue(attachment, kCMSampleAttachmentKey_DependsOnOthers);
            isKeyframe = (dependsOnOthers == kCFBooleanFalse);
        }
        
        if(isKeyframe) {
            
            // Send the SPS and PPS.
            CMFormatDescriptionRef format = CMSampleBufferGetFormatDescription(sampleBuffer);
            size_t spsSize, ppsSize;
            size_t parmCount;
            const uint8_t* sps, *pps;
            
            CMVideoFormatDescriptionGetH264ParameterSetAtIndex(format, 0, &sps, &spsSize, &parmCount, nullptr );
            CMVideoFormatDescriptionGetH264ParameterSetAtIndex(format, 1, &pps, &ppsSize, &parmCount, nullptr );
            
            std::unique_ptr<uint8_t[]> sps_buf (new uint8_t[spsSize + 4]) ;
            std::unique_ptr<uint8_t[]> pps_buf (new uint8_t[ppsSize + 4]) ;
            
            memcpy(&sps_buf[4], sps, spsSize);
            spsSize+=4 ;
            memcpy(&sps_buf[0], &spsSize, 4);
            memcpy(&pps_buf[4], pps, ppsSize);
            ppsSize += 4;
            memcpy(&pps_buf[0], &ppsSize, 4);
            
            ((H264Encode*)outputCallbackRefCon)->compressionSessionOutput((uint8_t*)sps_buf.get(),spsSize, pts.value, dts.value);
            ((H264Encode*)outputCallbackRefCon)->compressionSessionOutput((uint8_t*)pps_buf.get(),ppsSize, pts.value, dts.value);
        }
        
        char* bufferData;
        size_t size;
        CMBlockBufferGetDataPointer(block, 0, NULL, &size, &bufferData);
        
        ((H264Encode*)outputCallbackRefCon)->compressionSessionOutput((uint8_t*)bufferData,size, pts.value, dts.value);
        
    }
#endif
    H264Encode::H264Encode( int frame_w, int frame_h, int fps, int bitrate, bool useBaseline, int ctsOffset)
    : m_frameW(frame_w), m_frameH(frame_h), m_fps(fps), m_bitrate(bitrate), m_forceKeyframe(false), m_ctsOffset(ctsOffset)
    {
        setupCompressionSession( useBaseline );
    }
    H264Encode::~H264Encode()
    {
        teardownCompressionSession();
    }
    void
    H264Encode::pushBuffer(const uint8_t *const data, size_t size, videocore::IMetadata &metadata)
    {
#if VERSION_OK
        if(m_compressionSession) {
            m_encodeMutex.lock();
            VTCompressionSessionRef session = (VTCompressionSessionRef)m_compressionSession;

            CMTime pts = CMTimeMake(metadata.timestampDelta + m_ctsOffset, 1000.); // timestamp is in ms.
            CMTime dur = CMTimeMake(1, m_fps);
            VTEncodeInfoFlags flags;
            
            
            CFMutableDictionaryRef frameProps = NULL;
            
            if(m_forceKeyframe) {
                s_forcedKeyframePTS = pts.value;
                
                frameProps = CFDictionaryCreateMutable(kCFAllocatorDefault, 1,&kCFTypeDictionaryKeyCallBacks,                                                            &kCFTypeDictionaryValueCallBacks);
                
                
                CFDictionaryAddValue(frameProps, kVTEncodeFrameOptionKey_ForceKeyFrame, kCFBooleanTrue);
            }
            
            VTCompressionSessionEncodeFrame(session, (CVPixelBufferRef)data, pts, dur, frameProps, NULL, &flags);
            
            if(m_forceKeyframe) {
                CFRelease(frameProps);
                m_forceKeyframe = false;
            }
            
            m_encodeMutex.unlock();
        }
#endif
    }
    void
    H264Encode::setupCompressionSession( bool useBaseline )
    {
        m_baseline = useBaseline;
        
#if VERSION_OK
        // Parts of this code pulled from https://github.com/galad87/HandBrake-QuickSync-Mac/blob/2c1332958f7095c640cbcbcb45ffc955739d5945/libhb/platform/macosx/encvt_h264.c
        // More info from WWDC 2014 Session 513
        
        m_encodeMutex.lock();
        OSStatus err = noErr;
        CFMutableDictionaryRef encoderSpecifications = nullptr;
        
#if !TARGET_OS_IPHONE
        /** iOS is always hardware-accelerated **/
        CFStringRef key = kVTVideoEncoderSpecification_EncoderID;
        CFStringRef value = CFSTR("com.apple.videotoolbox.videoencoder.h264.gva");
        
        CFStringRef bkey = CFSTR("EnableHardwareAcceleratedVideoEncoder");
        CFBooleanRef bvalue = kCFBooleanTrue;
        
        CFStringRef ckey = CFSTR("RequireHardwareAcceleratedVideoEncoder");
        CFBooleanRef cvalue = kCFBooleanTrue;
        
        encoderSpecifications = CFDictionaryCreateMutable(
                                                          kCFAllocatorDefault,
                                                          3,
                                                          &kCFTypeDictionaryKeyCallBacks,
                                                          &kCFTypeDictionaryValueCallBacks);
        
        CFDictionaryAddValue(encoderSpecifications, bkey, bvalue);
        CFDictionaryAddValue(encoderSpecifications, ckey, cvalue);
        CFDictionaryAddValue(encoderSpecifications, key, value);
        
#endif
        VTCompressionSessionRef session = nullptr;
        @autoreleasepool {
            SInt32 cvPixelFormatTypeValue = ::kCVPixelFormatType_32BGRA;
            SInt8  boolYESValue = 0xFF;

            CFDictionaryRef emptyDict = ::CFDictionaryCreate(kCFAllocatorDefault, nil, nil, 0, nil, nil);
            CFNumberRef cvPixelFormatType = ::CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, (const void*)(&(cvPixelFormatTypeValue)));
            CFNumberRef frameW = ::CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, (const void*)(&(m_frameW)));
            CFNumberRef frameH = ::CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, (const void*)(&(m_frameH)));
            CFNumberRef boolYES = ::CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt8Type, (const void*)(&(boolYESValue)));
            
            const void *pixelBufferOptionsDictKeys[] = { kCVPixelBufferPixelFormatTypeKey, kCVPixelBufferWidthKey,  kCVPixelBufferHeightKey, kCVPixelBufferOpenGLESCompatibilityKey, kCVPixelBufferIOSurfacePropertiesKey};
            const void *pixelBufferOptionsDictValues[] = { cvPixelFormatType,  frameW, frameH, boolYES, emptyDict};
                CFDictionaryRef pixelBufferOptions = ::CFDictionaryCreate(kCFAllocatorDefault, pixelBufferOptionsDictKeys, pixelBufferOptionsDictValues, 5, nil, nil);
            
            err = VTCompressionSessionCreate(
                                             kCFAllocatorDefault,
                                             m_frameW,
                                             m_frameH,
                                             kCMVideoCodecType_H264,
                                             encoderSpecifications,
                                             pixelBufferOptions,
                                             NULL,
                                             &vtCallback,
                                             this,
                                             &session);
            
            CFRelease(emptyDict);
            CFRelease(cvPixelFormatType);
            CFRelease(frameW);
            CFRelease(frameH);
            CFRelease(boolYES);
            CFRelease(pixelBufferOptions);
            
        }
        
        if(err == noErr) {
            m_compressionSession = session;
            
            const int32_t v = m_fps * 2; // 2-second kfi
            
            CFNumberRef ref = CFNumberCreate(NULL, kCFNumberSInt32Type, &v);
            err = VTSessionSetProperty(session, kVTCompressionPropertyKey_MaxKeyFrameInterval, ref);
            CFRelease(ref);
        }
        
        if(err == noErr) {
            const int v = m_fps;
            CFNumberRef ref = CFNumberCreate(NULL, kCFNumberSInt32Type, &v);
            err = VTSessionSetProperty(session, kVTCompressionPropertyKey_ExpectedFrameRate, ref);
            CFRelease(ref);
        }
        
        if(err == noErr) {
            CFBooleanRef allowFrameReodering = useBaseline ? kCFBooleanFalse : kCFBooleanTrue;
            err = VTSessionSetProperty(session , kVTCompressionPropertyKey_AllowFrameReordering, allowFrameReodering);
        }
        
        if(err == noErr) {
            const int v = m_bitrate;
            CFNumberRef ref = CFNumberCreate(NULL, kCFNumberSInt32Type, &v);
            err = VTSessionSetProperty(session, kVTCompressionPropertyKey_AverageBitRate, ref);
            CFRelease(ref);
        }
        
        if(err == noErr) {
            err = VTSessionSetProperty(session, kVTCompressionPropertyKey_RealTime, kCFBooleanTrue);
        }
        
        if(err == noErr) {
            CFStringRef profileLevel = useBaseline ? kVTProfileLevel_H264_Baseline_AutoLevel : kVTProfileLevel_H264_Main_AutoLevel;
            
            err = VTSessionSetProperty(session, kVTCompressionPropertyKey_ProfileLevel, profileLevel);
        }
        if(!useBaseline) {
            VTSessionSetProperty(session, kVTCompressionPropertyKey_H264EntropyMode, kVTH264EntropyMode_CABAC);
        }
        if(err == noErr) {
            VTCompressionSessionPrepareToEncodeFrames(session);
        }
#endif
        m_encodeMutex.unlock();
        
    }
    void
    H264Encode::teardownCompressionSession()
    {
#if VERSION_OK
        if(m_compressionSession) {
            VTCompressionSessionInvalidate((VTCompressionSessionRef)m_compressionSession);
            CFRelease((VTCompressionSessionRef)m_compressionSession);
        }
#endif
    }
    void
    H264Encode::compressionSessionOutput(const uint8_t *data, size_t size, uint64_t pts, uint64_t dts)
    {
#if VERSION_OK
        auto l = m_output.lock();
        if(l && data && size > 0) {
            videocore::VideoBufferMetadata md(pts, dts);
            l->pushBuffer(data, size, md);
        }
#endif
        
    }
    void
    H264Encode::requestKeyframe()
    {
        m_forceKeyframe = true;
    }
    void
    H264Encode::setBitrate(int bitrate)
    {
#if VERSION_OK
        if(bitrate == m_bitrate) {
            return;
        }
        m_bitrate = bitrate;
        
        if(m_compressionSession) {
            m_encodeMutex.lock();
            
            int v = m_bitrate ;
            CFNumberRef ref = CFNumberCreate(NULL, kCFNumberSInt32Type, &v);
            
            //VTCompressionSessionCompleteFrames((VTCompressionSessionRef)m_compressionSession, kCMTimeInvalid);
            
            OSStatus ret = VTSessionSetProperty((VTCompressionSessionRef)m_compressionSession, kVTCompressionPropertyKey_AverageBitRate, ref);
            
            if(ret != noErr) {
                DLog("H264Encode::setBitrate Error setting bitrate! %d", (int) ret);
            }
            CFRelease(ref);
            ret = VTSessionCopyProperty((VTCompressionSessionRef)m_compressionSession, kVTCompressionPropertyKey_AverageBitRate, kCFAllocatorDefault, &ref);
            
            if(ret == noErr && ref) {
                SInt32 br = 0;
                
                CFNumberGetValue(ref, kCFNumberSInt32Type, &br);
                
                m_bitrate = br;
                CFRelease(ref);
            } else {
                m_bitrate = v;
            }
            v = bitrate / 8;
            CFNumberRef bytes = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &v);
            v = 1;
            CFNumberRef duration = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &v);
            
            CFMutableArrayRef limit = CFArrayCreateMutable(kCFAllocatorDefault, 2, &kCFTypeArrayCallBacks);

            CFArrayAppendValue(limit, bytes);
            CFArrayAppendValue(limit, duration);

            VTSessionSetProperty((VTCompressionSessionRef)m_compressionSession, kVTCompressionPropertyKey_DataRateLimits, limit);
            CFRelease(bytes);
            CFRelease(duration);
            CFRelease(limit);

            m_encodeMutex.unlock();
            
        }
#endif
    }
    
    CVPixelBufferPoolRef
    H264Encode::pixelBufferPool() {
        if(m_compressionSession) {
            return VTCompressionSessionGetPixelBufferPool((VTCompressionSessionRef)m_compressionSession);
        }
        return nullptr;
    }
}
}