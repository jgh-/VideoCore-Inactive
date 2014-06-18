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

#include <stdio.h>
#include <videocore/transforms/Apple/H264Encode.h>
#include <VideoToolbox/VideoToolbox.h>

namespace videocore { namespace Apple {

    void vtCallback(void *outputCallbackRefCon,
                    void *sourceFrameRefCon,
                    OSStatus status,
                    VTEncodeInfoFlags infoFlags,
                    CMSampleBufferRef sampleBuffer )
    {
        CMBlockBufferRef block = CMSampleBufferGetDataBuffer(sampleBuffer);
        char* bufferData;
        size_t size;
        CMBlockBufferGetDataPointer(block, 0, NULL, &size, &bufferData);
        ((H264Encode*)outputCallbackRefCon)->compressionSessionOutput((uint8_t*)bufferData,size);
        
    }
    H264Encode::H264Encode( int frame_w, int frame_h, int fps, int bitrate )
    : m_frameW(frame_w), m_frameH(frame_h), m_fps(fps), m_bitrate(bitrate)
    {
        setupCompressionSession();
    }
    H264Encode::~H264Encode()
    {
        VTCompressionSessionInvalidate((VTCompressionSessionRef)m_compressionSession);
        CFRelease((VTCompressionSessionRef)m_compressionSession);
    }
    
    void
    H264Encode::setupCompressionSession()
    {
        // Parts of this code pulled from https://github.com/galad87/HandBrake-QuickSync-Mac/blob/2c1332958f7095c640cbcbcb45ffc955739d5945/libhb/platform/macosx/encvt_h264.c
        // More info from WWDC 2014 Session 513
        
        OSStatus err = noErr;
#if !TARGET_OS_IPHONE
        /** iOS is always hardware-accelerated **/
        CFStringRef key = kVTVideoEncoderSpecification_EncoderID;
        CFStringRef value = CFSTR("com.apple.videotoolbox.videoencoder.h264.gva"); // this needs to be verified on iOS with VTCopyVideoEncoderList
        
        CFStringRef bkey = CFSTR("EnableHardwareAcceleratedVideoEncoder");
        CFBooleanRef bvalue = kCFBooleanTrue;
        
        CFStringRef ckey = CFSTR("RequireHardwareAcceleratedVideoEncoder");
        CFBooleanRef cvalue = kCFBooleanTrue;
        
        CFMutableDictionaryRef encoderSpecifications = CFDictionaryCreateMutable(
                                                                                 kCFAllocatorDefault,
                                                                                 3,
                                                                                 &kCFTypeDictionaryKeyCallBacks,
                                                                                 &kCFTypeDictionaryValueCallBacks);
        
        CFDictionaryAddValue(encoderSpecifications, bkey, bvalue);
        CFDictionaryAddValue(encoderSpecifications, ckey, cvalue);
        CFDictionaryAddValue(encoderSpecifications, key, value);
        
#endif
        
        err = VTCompressionSessionCreate(
                                         kCFAllocatorDefault,
                                         m_frameW,
                                         m_frameH,
                                         kCMVideoCodecType_H264,
                                         encoderSpecifications,
                                         NULL,
                                         NULL,
                                         &vtCallback,
                                         this,
                                         &session);
        if(err == noErr) {
            m_session = session;
            const int v = m_fps * 2; // 2-second kfi
            err = VTSessionSetProperty(session, kVTCompressionPropertyKey_MaxKeyFrameInterval, &v);
        }
        if(err == noErr) {
            const int v = m_fps / 2; // limit the number of frames kept in the buffer
            err = VTSessionSetProperty(session, kVTCompressionPropertyKey_MaxFrameDelayCount, &v);
        }
        if(err == noErr) {
            const int v = m_fps;
            err = VTSessionSetProperty(session, kVTCompressionPropertyKey_ExpectedFrameRate, &v);
        }
        if(err == noErr) {
            const int v = 0;
            err = VTSessionSetProperty(session , kVTCompressionPropertyKey_AllowFrameReordering, &v);
        }
        if(err == noErr) {
            const int v = m_bitrate;
            err = VTSessionSetProperty(session, kVTCompressionPropertyKey_AverageBitRate, &v);
        }
        if(err == noErr) {
            const int v = 1;
            err = VTSessionSetProperty(session, kVTCompressionPropertyKey_RealTime, &v);
        }
    }
    
    void
    H264Encode::compressionSessionOutput(const uint8_t *data, size_t size)
    {
        
    }
}
}