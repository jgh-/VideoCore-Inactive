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
#include <videocore/system/pixelBuffer/Apple/PixelBuffer.h>

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
    if(source && !source->orientationLocked()) {
        dispatch_async(dispatch_get_global_queue(0, 0), ^{
            source->reorientCamera();
        });
    }
}
@end
namespace videocore { namespace iOS {
    

    
    CameraSource::CameraSource()
    :
    m_captureDevice(nullptr),
    m_callbackSession(nullptr),
    m_previewLayer(nullptr),
    m_matrix(glm::mat4(1.f)),
    m_orientationLocked(false),
    m_torchOn(false),
    m_useInterfaceOrientation(false),
    m_captureSession(nullptr)
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
    CameraSource::setupCamera(int fps, bool useFront, bool useInterfaceOrientation, NSString* sessionPreset, void (^callbackBlock)(void))
    {
        m_fps = fps;
        m_useInterfaceOrientation = useInterfaceOrientation;
        
        __block CameraSource* bThis = this;
        
        void (^permissions)(BOOL) = ^(BOOL granted) {
            @autoreleasepool {
                if(granted) {
                    
                    int position = useFront ? AVCaptureDevicePositionFront : AVCaptureDevicePositionBack;
                    
                    NSArray* devices = [AVCaptureDevice devices];
                    for(AVCaptureDevice* d in devices) {
                        if([d hasMediaType:AVMediaTypeVideo] && [d position] == position)
                        {
                            bThis->m_captureDevice = d;
                            NSError* error;
                            [d lockForConfiguration:&error];
                            if(SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"7.0")) {
                                [d setActiveVideoMinFrameDuration:CMTimeMake(1, fps)];
                                [d setActiveVideoMaxFrameDuration:CMTimeMake(1, fps)];
                            }
                            [d unlockForConfiguration];
                        }
                    }
                    
                    AVCaptureSession* session = [[AVCaptureSession alloc] init];
                    AVCaptureDeviceInput* input;
                    AVCaptureVideoDataOutput* output;
                    if(sessionPreset) {
                        session.sessionPreset = (NSString*)sessionPreset;
                    }
                    bThis->m_captureSession = session;
                    
                    input = [AVCaptureDeviceInput deviceInputWithDevice:((AVCaptureDevice*)m_captureDevice) error:nil];
                    
                    output = [[AVCaptureVideoDataOutput alloc] init] ;
                    
                    output.videoSettings = @{(NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) };
                    
                    if(!SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"7.0")) {
                        AVCaptureConnection* conn = [output connectionWithMediaType:AVMediaTypeVideo];
                        if([conn isVideoMinFrameDurationSupported]) {
                            [conn setVideoMinFrameDuration:CMTimeMake(1, fps)];
                        }
                        if([conn isVideoMaxFrameDurationSupported]) {
                            [conn setVideoMaxFrameDuration:CMTimeMake(1, fps)];
                        }
                    }
                    if(!bThis->m_callbackSession) {
                        bThis->m_callbackSession = [[sbCallback alloc] init];
                        [((sbCallback*)bThis->m_callbackSession) setSource:shared_from_this()];
                    }
                    dispatch_queue_t camQueue = dispatch_queue_create("com.videocore.camera", 0);
                    
                    [output setSampleBufferDelegate:((sbCallback*)bThis->m_callbackSession) queue:camQueue];
                    
                    dispatch_release(camQueue);
                    
                    if([session canAddInput:input]) {
                        [session addInput:input];
                    }
                    if([session canAddOutput:output]) {
                        [session addOutput:output];
                        
                    }
                    
                    reorientCamera();
                    
                    [session startRunning];
                    
                    if(!bThis->m_orientationLocked) {
                        if(bThis->m_useInterfaceOrientation) {
                            [[NSNotificationCenter defaultCenter] addObserver:((id)bThis->m_callbackSession) selector:@selector(orientationChanged:) name:UIApplicationDidChangeStatusBarOrientationNotification object:nil];
                        } else {
                            [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
                            [[NSNotificationCenter defaultCenter] addObserver:((id)bThis->m_callbackSession) selector:@selector(orientationChanged:) name:UIDeviceOrientationDidChangeNotification object:nil];
                        }
                    }
                    [output release];
                }
                if (callbackBlock) {
                    callbackBlock();
                }
            }
        };
        @autoreleasepool {
            if(SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"7.0")) {
                AVAuthorizationStatus auth = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
                
                if(auth == AVAuthorizationStatusAuthorized || !SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"8.0")) {
                    permissions(true);
                }
                else if(auth == AVAuthorizationStatusNotDetermined) {
                    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:permissions];
                }
            } else {
                permissions(true);
            }
            
        }
    }

    void
    CameraSource::getPreviewLayer(void** outAVCaptureVideoPreviewLayer)
    {
        if(!m_previewLayer) {
            @autoreleasepool {
                AVCaptureSession* session = (AVCaptureSession*)m_captureSession;
                AVCaptureVideoPreviewLayer* previewLayer;
                previewLayer = [[AVCaptureVideoPreviewLayer layerWithSession:session] retain];
                previewLayer.videoGravity = AVLayerVideoGravityResizeAspectFill;
                m_previewLayer = previewLayer;
            }
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
    CameraSource::orientationLocked()
    {
        return m_orientationLocked;
    }
    void
    CameraSource::setOrientationLocked(bool orientationLocked)
    {
        m_orientationLocked = orientationLocked;
    }
    bool
    CameraSource::setTorch(bool torchOn)
    {
        bool ret = false;
        if(!m_captureSession) return ret;
        
        AVCaptureSession* session = (AVCaptureSession*)m_captureSession;
        
        [session beginConfiguration];
        
        if (session.inputs.count > 0) {
            AVCaptureDeviceInput* currentCameraInput = [session.inputs objectAtIndex:0];
            
            if(currentCameraInput.device.torchAvailable) {
                NSError* err = nil;
                if([currentCameraInput.device lockForConfiguration:&err]) {
                    [currentCameraInput.device setTorchMode:( torchOn ? AVCaptureTorchModeOn : AVCaptureTorchModeOff ) ];
                    [currentCameraInput.device unlockForConfiguration];
                    ret = (currentCameraInput.device.torchMode == AVCaptureTorchModeOn);
                } else {
                    NSLog(@"Error while locking device for torch: %@", err);
                    ret = false;
                }
            } else {
                NSLog(@"Torch not available in current camera input");
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
        
        NSError* error;
        AVCaptureSession* session = (AVCaptureSession*)m_captureSession;
        if(session) {
            [session beginConfiguration];
            [(AVCaptureDevice*)m_captureDevice lockForConfiguration: &error];
            
            if (session.inputs.count > 0) {
                AVCaptureInput* currentCameraInput = [session.inputs objectAtIndex:0];
                
                [session removeInput:currentCameraInput];
                [(AVCaptureDevice*)m_captureDevice unlockForConfiguration];
                
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
                [newCamera lockForConfiguration:&error];
                [session addInput:newVideoInput];
                
                m_captureDevice = newCamera;
                [newCamera unlockForConfiguration];
                [session commitConfiguration];
                
                [newVideoInput release];
            }
            
            reorientCamera();
        }
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
        
        //bool reorient = false;
        
        AVCaptureSession* session = (AVCaptureSession*)m_captureSession;
        // [session beginConfiguration];
        
        for (AVCaptureVideoDataOutput* output in session.outputs) {
            for (AVCaptureConnection * av in output.connections) {
                
                switch (orientation) {
                        // UIInterfaceOrientationPortraitUpsideDown, UIDeviceOrientationPortraitUpsideDown
                    case UIInterfaceOrientationPortraitUpsideDown:
                        if(av.videoOrientation != AVCaptureVideoOrientationPortraitUpsideDown) {
                            av.videoOrientation = AVCaptureVideoOrientationPortraitUpsideDown;
                        //    reorient = true;
                        }
                        break;
                        // UIInterfaceOrientationLandscapeRight, UIDeviceOrientationLandscapeLeft
                    case UIInterfaceOrientationLandscapeRight:
                        if(av.videoOrientation != AVCaptureVideoOrientationLandscapeRight) {
                            av.videoOrientation = AVCaptureVideoOrientationLandscapeRight;
                        //    reorient = true;
                        }
                        break;
                        // UIInterfaceOrientationLandscapeLeft, UIDeviceOrientationLandscapeRight
                    case UIInterfaceOrientationLandscapeLeft:
                        if(av.videoOrientation != AVCaptureVideoOrientationLandscapeLeft) {
                            av.videoOrientation = AVCaptureVideoOrientationLandscapeLeft;
                         //   reorient = true;
                        }
                        break;
                        // UIInterfaceOrientationPortrait, UIDeviceOrientationPortrait
                    case UIInterfaceOrientationPortrait:
                        if(av.videoOrientation != AVCaptureVideoOrientationPortrait) {
                            av.videoOrientation = AVCaptureVideoOrientationPortrait;
                        //    reorient = true;
                        }
                        break;
                    default:
                        break;
                }
            }
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
        
        //auto mixer = std::static_pointer_cast<IVideoMixer>(output);
        
    }
    void
    CameraSource::bufferCaptured(CVPixelBufferRef pixelBufferRef)
    {
        auto output = m_output.lock();
        if(output) {
            
            VideoBufferMetadata md(1.f / float(m_fps));
            
            md.setData(1, m_matrix, false, shared_from_this());
            
            auto pixelBuffer = std::make_shared<Apple::PixelBuffer>(pixelBufferRef, true);
            
            pixelBuffer->setState(kVCPixelBufferStateEnqueued);
            output->pushBuffer((uint8_t*)&pixelBuffer, sizeof(pixelBuffer), md);
            
        }
    }
    
    bool
    CameraSource::setContinuousAutofocus(bool wantsContinuous)
    {
        AVCaptureDevice* device = (AVCaptureDevice*)m_captureDevice;
        AVCaptureFocusMode newMode = wantsContinuous ?  AVCaptureFocusModeContinuousAutoFocus : AVCaptureFocusModeAutoFocus;
        bool ret = [device isFocusModeSupported:newMode];

        if(ret) {
            NSError *err = nil;
            if ([device lockForConfiguration:&err]) {
                device.focusMode = newMode;
                [device unlockForConfiguration];
            } else {
                NSLog(@"Error while locking device for autofocus: %@", err);
                ret = false;
            }
        } else {
            NSLog(@"Focus mode not supported: %@", wantsContinuous ? @"AVCaptureFocusModeContinuousAutoFocus" : @"AVCaptureFocusModeAutoFocus");
        }

        return ret;
    }

    bool
    CameraSource::setContinuousExposure(bool wantsContinuous) {
        AVCaptureDevice *device = (AVCaptureDevice *) m_captureDevice;
        AVCaptureExposureMode newMode = wantsContinuous ? AVCaptureExposureModeContinuousAutoExposure : AVCaptureExposureModeAutoExpose;
        bool ret = [device isExposureModeSupported:newMode];

        if(ret) {
            NSError *err = nil;
            if ([device lockForConfiguration:&err]) {
                device.exposureMode = newMode;
                [device unlockForConfiguration];
            } else {
                NSLog(@"Error while locking device for exposure: %@", err);
                ret = false;
            }
        } else {
            NSLog(@"Exposure mode not supported: %@", wantsContinuous ? @"AVCaptureExposureModeContinuousAutoExposure" : @"AVCaptureExposureModeAutoExpose");
        }

        return ret;
    }
    
    bool
    CameraSource::setFocusPointOfInterest(float x, float y)
    {
        AVCaptureDevice* device = (AVCaptureDevice*)m_captureDevice;
        bool ret = device.focusPointOfInterestSupported;
        
        if(ret) {
            NSError* err = nil;
            if([device lockForConfiguration:&err]) {
                [device setFocusPointOfInterest:CGPointMake(x, y)];
                if (device.focusMode == AVCaptureFocusModeLocked) {
                    [device setFocusMode:AVCaptureFocusModeAutoFocus];
                }
                device.focusMode = device.focusMode;
                [device unlockForConfiguration];
            } else {
                NSLog(@"Error while locking device for focus POI: %@", err);
                ret = false;
            }
        } else {
            NSLog(@"Focus POI not supported");
        }
        
        return ret;
    }
    
    bool
    CameraSource::setExposurePointOfInterest(float x, float y)
    {
        AVCaptureDevice* device = (AVCaptureDevice*)m_captureDevice;
        bool ret = device.exposurePointOfInterestSupported;
        
        if(ret) {
            NSError* err = nil;
            if([device lockForConfiguration:&err]) {
                [device setExposurePointOfInterest:CGPointMake(x, y)];
                device.exposureMode = device.exposureMode;
                [device unlockForConfiguration];
            } else {
                NSLog(@"Error while locking device for exposure POI: %@", err);
                ret = false;
            }
        } else {
            NSLog(@"Exposure POI not supported");
        }
        
        return ret;
    }
    
}
}
