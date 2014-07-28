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

#define SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(v)  ([[[UIDevice currentDevice] systemVersion] compare:v options:NSNumericSearch] != NSOrderedAscending)

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
- (void)captureOutput:(AVCaptureOutput *)captureOutput
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection
{
    auto source = m_source.lock();
    if(source) {
        source->bufferCaptured(CMSampleBufferGetImageBuffer(sampleBuffer));
    }
}
- (void) captureOutput:(AVCaptureOutput *)captureOutput
   didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer
        fromConnection:(AVCaptureConnection *)connection
{
}
- (void) orientationChanged: (NSNotification*) notification
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
    
    CameraSource::CameraSource(float x,
                               float y,
                               float w,
                               float h,
                               float vw,
                               float vh,
                               float aspect)
    : m_size({x,y,w,h,vw,vh,w/h}),
    m_targetSize(m_size),
    m_captureDevice(NULL),
    m_isFirst(true),
    m_callbackSession(NULL),
    m_aspectMode(kAspectFit),
    m_fps(15),
    m_usingDeprecatedMethods(true),
    m_previewLayer(nullptr),
    m_torchOn(false),
    m_useInterfaceOrientation(false)
    {
    }
    
    CameraSource::CameraSource()
    :
    m_captureDevice(nullptr),
    m_callbackSession(nullptr),
    m_previewLayer(nullptr),
    m_matrix(glm::mat4(1.f)),
    m_usingDeprecatedMethods(false),
    m_torchOn(false),
    m_useInterfaceOrientation(false)
    {}
    
    CameraSource::~CameraSource()
    {
        
        if(m_captureSession) {
            [((AVCaptureSession*)m_captureSession) stopRunning];
            [((AVCaptureSession*)m_captureSession) release];
        }
        if(m_callbackSession) {
            [[NSNotificationCenter defaultCenter] removeObserver:(id)m_callbackSession];
            [((sbCallback*)m_callbackSession) release];
        }
        if(m_previewLayer) {
            [(id)m_previewLayer release];
        }
    }
    
    void
    CameraSource::setupCamera(int fps, bool useFront, bool useInterfaceOrientation)
    {
        m_fps = fps;
        m_useInterfaceOrientation = useInterfaceOrientation;
        
        
        @autoreleasepool {
            
            __block CameraSource* bThis = this;
            
            void (^permissions)(BOOL) = ^(BOOL granted) {
                if(granted) {

                    int position = useFront ? AVCaptureDevicePositionFront : AVCaptureDevicePositionBack;
                    
                    NSArray* devices = [AVCaptureDevice devices];
                    for(AVCaptureDevice* d in devices) {
                        if([d hasMediaType:AVMediaTypeVideo] && [d position] == position)
                        {
                            bThis->m_captureDevice = d;
                            NSError* error;
                            [d lockForConfiguration:&error];
                            [d setActiveVideoMinFrameDuration:CMTimeMake(1, fps)];
                            [d setActiveVideoMaxFrameDuration:CMTimeMake(1, fps)];
                            [d unlockForConfiguration];
                        }
                    }
                    
                    AVCaptureSession* session = [[AVCaptureSession alloc] init];
                    AVCaptureDeviceInput* input;
                    AVCaptureVideoDataOutput* output;
                    
                    NSString* preset = AVCaptureSessionPresetHigh;
                    if(bThis->m_usingDeprecatedMethods) {
                        int mult = ceil(double(bThis->m_targetSize.h) / 270.0) * 270 ;
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
                    }
                    bThis->m_captureSession = session;
                    
                    input = [AVCaptureDeviceInput deviceInputWithDevice:((AVCaptureDevice*)m_captureDevice) error:nil];
                    
                    output = [[AVCaptureVideoDataOutput alloc] init] ;
                    
                    output.videoSettings = @{(NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) };
                    if(!bThis->m_callbackSession) {
                        bThis->m_callbackSession = [[sbCallback alloc] init];
                        [((sbCallback*)bThis->m_callbackSession) setSource:shared_from_this()];
                    }
                    
                    [output setSampleBufferDelegate:((sbCallback*)bThis->m_callbackSession) queue:dispatch_get_global_queue(0, 0)];
                    
                    if([session canAddInput:input]) {
                        [session addInput:input];
                    }
                    if([session canAddOutput:output]) {
                        [session addOutput:output];
                        
                    }
                    
                    reorientCamera();
                    
                    [session startRunning];
                    
                    if(bThis->m_useInterfaceOrientation) {
                        [[NSNotificationCenter defaultCenter] addObserver:((id)bThis->m_callbackSession) selector:@selector(orientationChanged:) name:UIApplicationDidChangeStatusBarOrientationNotification object:nil];
                    } else {
                        [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
                        [[NSNotificationCenter defaultCenter] addObserver:((id)bThis->m_callbackSession) selector:@selector(orientationChanged:) name:UIDeviceOrientationDidChangeNotification object:nil];
                    }
                    [output release];
                }
            };
            
            AVAuthorizationStatus auth = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
            
            if(auth == AVAuthorizationStatusAuthorized || !SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"8.0")) {
                permissions(true);
            }
            else if(auth == AVAuthorizationStatusNotDetermined) {
                [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:permissions];
            }
            
        }
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
        if(!m_previewLayer) {
            AVCaptureSession* session = (AVCaptureSession*)m_captureSession;
            AVCaptureVideoPreviewLayer* previewLayer;
            previewLayer =  [AVCaptureVideoPreviewLayer layerWithSession:session];
            previewLayer.videoGravity = AVLayerVideoGravityResizeAspectFill;
            m_previewLayer = previewLayer;
        }
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
    bool
    CameraSource::setTorch(bool torchOn)
    {
        bool ret = false;
        if(!m_captureSession) return ret;
        
        AVCaptureSession* session = (AVCaptureSession*)m_captureSession;
        
        [session beginConfiguration];
        AVCaptureDeviceInput* currentCameraInput = [session.inputs objectAtIndex:0];
        
        if(currentCameraInput.device.torchAvailable) {
            NSError* err = nil;
            [currentCameraInput.device lockForConfiguration:&err];
            if(!err) {
                [currentCameraInput.device setTorchMode:( torchOn ? AVCaptureTorchModeOn : AVCaptureTorchModeOff ) ];
                [currentCameraInput.device unlockForConfiguration];
                ret = (currentCameraInput.device.torchMode == AVCaptureTorchModeOn);
                
            } else {
                ret = false;
            }
        }
        [session commitConfiguration];
        m_torchOn = ret;
        return ret;
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
        
        [newVideoInput release];
        
        reorientCamera();
        
    }
    
    void
    CameraSource::reorientCamera()
    {
        if(!m_captureSession) return;
        
        auto orientation = m_useInterfaceOrientation ? [[UIApplication sharedApplication] statusBarOrientation] : [[UIDevice currentDevice] orientation];
        
        // use interface orientation as fallback if device orientation is facedown, faceup or unknown
        if(orientation==UIDeviceOrientationFaceDown || orientation==UIDeviceOrientationFaceUp || orientation==UIDeviceOrientationUnknown) {
            orientation =[[UIApplication sharedApplication] statusBarOrientation];
        }
        
        bool reorient = false;
        
        AVCaptureSession* session = (AVCaptureSession*)m_captureSession;
        // [session beginConfiguration];
        
        for (AVCaptureVideoDataOutput* output in session.outputs) {
            for (AVCaptureConnection * av in output.connections) {
                
                switch (orientation) {
                        // UIInterfaceOrientationPortraitUpsideDown, UIDeviceOrientationPortraitUpsideDown
                    case UIInterfaceOrientationPortraitUpsideDown:
                        if(av.videoOrientation != AVCaptureVideoOrientationPortraitUpsideDown) {
                            av.videoOrientation = AVCaptureVideoOrientationPortraitUpsideDown;
                            reorient = true;
                        }
                        break;
                        // UIInterfaceOrientationLandscapeRight, UIDeviceOrientationLandscapeLeft
                    case UIInterfaceOrientationLandscapeRight:
                        if(av.videoOrientation != AVCaptureVideoOrientationLandscapeRight) {
                            av.videoOrientation = AVCaptureVideoOrientationLandscapeRight;
                            reorient = true;
                        }
                        break;
                        // UIInterfaceOrientationLandscapeLeft, UIDeviceOrientationLandscapeRight
                    case UIInterfaceOrientationLandscapeLeft:
                        if(av.videoOrientation != AVCaptureVideoOrientationLandscapeLeft) {
                            av.videoOrientation = AVCaptureVideoOrientationLandscapeLeft;
                            reorient = true;
                        }
                        break;
                        // UIInterfaceOrientationPortrait, UIDeviceOrientationPortrait
                    case UIInterfaceOrientationPortrait:
                        if(av.videoOrientation != AVCaptureVideoOrientationPortrait) {
                            av.videoOrientation = AVCaptureVideoOrientationPortrait;
                            reorient = true;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
        if(reorient) {
            m_isFirst = true;
        }
        //[session commitConfiguration];
        if(m_torchOn) {
            setTorch(m_torchOn);
        }
    }
    void
    CameraSource::setOutput(std::shared_ptr<IOutput> output)
    {
        m_output = output;
        
        auto mixer = std::static_pointer_cast<IVideoMixer>(output);
        
    }
    void
    CameraSource::bufferCaptured(CVPixelBufferRef pixelBufferRef)
    {
        auto output = m_output.lock();
        if(output) {
            
            if(m_usingDeprecatedMethods && m_isFirst) {
                
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
