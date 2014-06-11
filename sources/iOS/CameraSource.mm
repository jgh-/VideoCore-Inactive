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

#include <videocore/sources/iOS/CameraSource.h>
#include <videocore/mixers/IVideoMixer.hpp>

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

@interface sbCallback: NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>
{
    std::weak_ptr<videocore::iOS::CameraSource> m_source;
}
- (void) setSource:(std::weak_ptr<videocore::iOS::CameraSource>) source;
@end

@implementation sbCallback
-(void) setSource:(std::weak_ptr<videocore::iOS::CameraSource>)source
{
    m_source = source;
}
- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
{
    auto source = m_source.lock();
    if(source) {
        source->bufferCaptured(CMSampleBufferGetImageBuffer(sampleBuffer));
    }
}
- (void) captureOutput:(AVCaptureOutput *)captureOutput didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
{
}
- (void) deviceOrientationChanged: (NSNotification*) notification
{
    auto source = m_source.lock();
    if(source) {
        dispatch_async(dispatch_get_global_queue(0, 0), ^{
            source->reorientCamera();
        });
    }
}
@end
namespace videocore { namespace iOS {
    
    CameraSource::CameraSource(float x, float y, float w, float h, float vw, float vh, float aspect)
    : m_size({x,y,w,h,vw,vh,aspect}), m_target_size(m_size), m_captureDevice(NULL),  m_isFirst(true), m_callbackSession(NULL),m_preview(NULL),m_fps(15)
    {
    }
    
    CameraSource::~CameraSource()
    {
        [[NSNotificationCenter defaultCenter] removeObserver:(id)m_callbackSession];
        [((AVCaptureSession*)m_captureSession) stopRunning];
        [((AVCaptureSession*)m_captureSession) release];
        [((sbCallback*)m_callbackSession) release];
    }
    
    void
    CameraSource::setupCamera(int fps, bool useFront)
    {
        m_fps = fps;
        int position = useFront ? AVCaptureDevicePositionFront : AVCaptureDevicePositionBack;
        
        for(AVCaptureDevice* d in [AVCaptureDevice devices]) {
            if([d hasMediaType:AVMediaTypeVideo] && [d position] == position)
            {
                m_captureDevice = d;
                NSError* error;
                [d lockForConfiguration:&error];
                [d setActiveVideoMinFrameDuration:CMTimeMake(1, fps)];
                [d setActiveVideoMaxFrameDuration:CMTimeMake(1, fps)];
                [d unlockForConfiguration];
            }
        }
        
        int mult = ceil(double(m_target_size.h) / 270.0) * 270 ;
        AVCaptureSession* session = [[AVCaptureSession alloc] init];
        AVCaptureDeviceInput* input;
        AVCaptureVideoDataOutput* output;
        
        NSString* preset = nil;
        
        switch(mult) {
            case 270:
                preset = AVCaptureSessionPresetLow;
                break;
            case 540:
                preset = AVCaptureSessionPresetMedium;
                break;
            default:
                preset = AVCaptureSessionPresetHigh;
                break;
        }
        session.sessionPreset = preset;
        m_captureSession = session;
        
        input = [AVCaptureDeviceInput deviceInputWithDevice:((AVCaptureDevice*)m_captureDevice) error:nil];
        
        output = [[AVCaptureVideoDataOutput alloc] init] ;
        
        output.videoSettings = @{(NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) };
        
        if(!m_callbackSession) {
            m_callbackSession = [[sbCallback alloc] init];
        }
        
        [output setSampleBufferDelegate:((sbCallback*)m_callbackSession) queue:dispatch_get_global_queue(0, 0)];
        
        if([session canAddInput:input]) {
            [session addInput:input];
        }
        if([session canAddOutput:output]) {
            [session addOutput:output];
        }
        
        reorientCamera();

        m_preview =  [AVCaptureVideoPreviewLayer layerWithSession:session];
        m_preview.videoGravity = AVLayerVideoGravityResizeAspectFill;

        [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
        
        [[NSNotificationCenter defaultCenter] addObserver:((id)m_callbackSession) selector:@selector(deviceOrientationChanged:) name:UIDeviceOrientationDidChangeNotification object:nil];
        
        [output release];

    }
    AVCaptureVideoPreviewLayer*
    CameraSource::getPreviewLayer()
    {
        return m_preview;
    }
    AVCaptureDevice*
    CameraSource::cameraWithPosition(AVCaptureDevicePosition position)
    {
        NSArray *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
        for (AVCaptureDevice *device in devices)
        {
            if ([device position] == position) return device;
        }
        return nil;
    
    }
    void
    CameraSource::toggleCamera()
    {
        
        if(!m_captureSession) return;
        
        AVCaptureSession* session = (AVCaptureSession*)m_captureSession;
        
        [session beginConfiguration];
        
        AVCaptureInput* currentCameraInput = [session.inputs objectAtIndex:0];
        
        [session removeInput:currentCameraInput];
        
        AVCaptureDevice *newCamera = nil;
        if(((AVCaptureDeviceInput*)currentCameraInput).device.position == AVCaptureDevicePositionBack)
        {
            newCamera = cameraWithPosition(AVCaptureDevicePositionFront);
        }
        else
        {
            newCamera = cameraWithPosition(AVCaptureDevicePositionBack);
        }
        
        AVCaptureDeviceInput *newVideoInput = [[AVCaptureDeviceInput alloc] initWithDevice:newCamera error:nil];
        
        [session addInput:newVideoInput];
        
        [session commitConfiguration];
        
    }
    
    void
    CameraSource::reorientCamera()
    {
        if(!m_captureSession) return;
        
        AVCaptureSession* session = (AVCaptureSession*)m_captureSession;
        for (AVCaptureVideoDataOutput* output in session.outputs) {
            for (AVCaptureConnection * av in output.connections) {
                switch ([UIApplication sharedApplication].statusBarOrientation) {
                        
                    case UIInterfaceOrientationPortraitUpsideDown:
                        av.videoOrientation = AVCaptureVideoOrientationPortraitUpsideDown;
                        break;
                    case UIInterfaceOrientationLandscapeLeft:
                        av.videoOrientation = AVCaptureVideoOrientationLandscapeLeft;
                        break;
                    case UIInterfaceOrientationLandscapeRight:
                        av.videoOrientation = AVCaptureVideoOrientationLandscapeRight;
                        break;
                    default:
                        av.videoOrientation = AVCaptureVideoOrientationPortrait;
                        break;
                }
                
            }
        }
        [session stopRunning];
        [session startRunning];
        m_isFirst = true;
    }
    void
    CameraSource::setOutput(std::shared_ptr<IOutput> output)
    {
        m_output = output;
        
        auto mixer = std::static_pointer_cast<IVideoMixer>(output);
        
        [((sbCallback*)m_callbackSession) setSource:shared_from_this()];
    }
    void
    CameraSource::bufferCaptured(CVPixelBufferRef pixelBufferRef)
    {
        auto output = m_output.lock();
        if(output) {
            if(m_isFirst) {
                const float aspect = float(CVPixelBufferGetWidth(pixelBufferRef)) / float(CVPixelBufferGetHeight(pixelBufferRef));
                const float inp_aspect = m_target_size.a;
                //const float diff = inp_aspect / aspect;
                if( aspect < inp_aspect ) {
                    m_size.w = m_target_size.h * aspect;
                } else {
                    m_size.h = m_target_size.w / aspect;
                }
                m_isFirst = false;
                glm::mat4 mat(1.f);
                mat = glm::translate(mat, glm::vec3((m_size.x / m_target_size.vw) * 2.f - 1.f, (m_size.y / m_target_size.vh) * 2.f - 1.f, 0.f));
                mat = glm::scale(mat, glm::vec3(m_size.w / m_target_size.vw, m_size.h / m_target_size.vh, 1.f));
                m_matrix = mat;
            }

            VideoBufferMetadata md(1.f / float(m_fps));
            
            md.setData(1, m_matrix, shared_from_this());
            
            CVPixelBufferRetain(pixelBufferRef);
            output->pushBuffer((uint8_t*)pixelBufferRef, sizeof(pixelBufferRef), md);
            CVPixelBufferRelease(pixelBufferRef);
        }
    }
    
}
}
