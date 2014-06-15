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
    : m_size({x,y,w,h,vw,vh,aspect}), m_targetSize(m_size), m_captureDevice(NULL),  m_isFirst(true), m_callbackSession(NULL), m_aspectMode(kAspectFit), m_fps(15)
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
        
        int mult = ceil(double(m_targetSize.h) / 270.0) * 270 ;
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
        AVCaptureVideoPreviewLayer* previewLayer;
        previewLayer =  [AVCaptureVideoPreviewLayer layerWithSession:session];
        previewLayer.videoGravity = AVLayerVideoGravityResizeAspectFill;

        m_previewLayer = previewLayer;
        
        [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
        
        [[NSNotificationCenter defaultCenter] addObserver:((id)m_callbackSession) selector:@selector(deviceOrientationChanged:) name:UIDeviceOrientationDidChangeNotification object:nil];
        
        [output release];

    }
    void
    CameraSource::setAspectMode(AspectMode aspectMode)
    {
        m_aspectMode = aspectMode;
        m_isFirst = true;           // Force the transformation matrix to be re-generated.
    }
    void
    CameraSource::getPreviewLayer(void** outAVCaptureVideoPreviewLayer)
    {
        if(outAVCaptureVideoPreviewLayer) {
            *outAVCaptureVideoPreviewLayer = m_previewLayer;
        }
    }
    void*
    CameraSource::cameraWithPosition(int pos)
    {
        AVCaptureDevicePosition position = (AVCaptureDevicePosition)pos;
        
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
            newCamera = (AVCaptureDevice*)cameraWithPosition(AVCaptureDevicePositionFront);
        }
        else
        {
            newCamera = (AVCaptureDevice*)cameraWithPosition(AVCaptureDevicePositionBack);
        }
        
        AVCaptureDeviceInput *newVideoInput = [[AVCaptureDeviceInput alloc] initWithDevice:newCamera error:nil];
        
        [session addInput:newVideoInput];
        
        [session commitConfiguration];
        
    }
    
    void
    CameraSource::reorientCamera()
    {
        if(!m_captureSession) return;
        
        const auto orientation = [[UIDevice currentDevice] orientation];
        
        bool reorient = false;
        
        AVCaptureSession* session = (AVCaptureSession*)m_captureSession;
        for (AVCaptureVideoDataOutput* output in session.outputs) {
            for (AVCaptureConnection * av in output.connections) {
               
                switch (orientation) {
                        // NOTE: device orientation and capture orientation for landscape
                        //       left vs. right are swapped
                    case UIDeviceOrientationPortraitUpsideDown:
                        if(av.videoOrientation != AVCaptureVideoOrientationPortraitUpsideDown) {
                            av.videoOrientation = AVCaptureVideoOrientationPortraitUpsideDown;
                            reorient = true;
                        }
                        break;
                    case UIDeviceOrientationLandscapeLeft:
                        if(av.videoOrientation != AVCaptureVideoOrientationLandscapeRight) {
                            av.videoOrientation = AVCaptureVideoOrientationLandscapeRight;
                            reorient = true;
                        }
                        break;
                    case UIDeviceOrientationLandscapeRight:
                        if(av.videoOrientation != AVCaptureVideoOrientationLandscapeLeft) {
                            av.videoOrientation = AVCaptureVideoOrientationLandscapeLeft;
                            reorient = true;
                        }
                        break;
                    case UIDeviceOrientationPortrait:
                        if(av.videoOrientation != AVCaptureVideoOrientationPortrait) {
                            av.videoOrientation = AVCaptureVideoOrientationPortrait;
                            reorient = true;
                        }
                        break;
                    default:    // Don't do anything for faceup/facedown
                        break;
                }
                
                
            }
        }
        if(reorient) {
            [session stopRunning];
            [session startRunning];
            m_isFirst = true;
        }
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
                
                
                m_isFirst = false;
                
                m_size.w = float(CVPixelBufferGetWidth(pixelBufferRef));
                m_size.h = float(CVPixelBufferGetHeight(pixelBufferRef));
                
                const float wfac = m_targetSize.w / m_size.w;
                const float hfac = m_targetSize.h / m_size.h;
                
                const float mult = (m_aspectMode == kAspectFit ? (wfac < hfac) : (wfac > hfac)) ? wfac : hfac;
                
                m_size.w *= mult;
                m_size.h *= mult;
                
                glm::mat4 mat(1.f);
                
                mat = glm::translate(mat,
                                     glm::vec3((m_size.x / m_targetSize.vw) * 2.f - 1.f,   // The compositor uses normalized device co-ordinates.
                                               (m_size.y / m_targetSize.vh) * 2.f - 1.f,   // i.e. [ -1 .. 1 ]
                                               0.f));
                
                mat = glm::scale(mat,
                                 glm::vec3(m_size.w / m_targetSize.vw, //
                                           m_size.h / m_targetSize.vh, // size is a percentage for scaling.
                                           1.f));
                
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
