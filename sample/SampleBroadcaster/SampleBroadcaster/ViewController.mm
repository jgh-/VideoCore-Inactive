//
//  ViewController.m
//  SampleBroadcaster
//
//  Created by James Hurley on 5/6/14.
//  Copyright (c) 2014 videocore. All rights reserved.
//

#import "ViewController.h"

@interface ViewController () <GLKViewDelegate>
{
    GLuint _renderBuffer;
}
@property (nonatomic, retain) EAGLContext* glContext;
@property (nonatomic, retain) CIContext*   ciContext;
@property (nonatomic) CIImage* ciImage;
@end

@implementation ViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
    self.glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    [EAGLContext setCurrentContext:self.glContext];
    self.glkView.context = self.glContext;
    self.glkView.drawableDepthFormat = GLKViewDrawableDepthFormat24;
    glGenRenderbuffers(1, &_renderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _renderBuffer);
    self.ciContext = [CIContext contextWithEAGLContext:self.glContext];
    self.glkView.delegate = self;
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)dealloc {
    [_btnConnect release];
    [_glkView release];
    [super dealloc];
}
- (void) glkView:(GLKView *)view drawInRect:(CGRect)rect
{
    float aspect = self.ciImage.extent.size.width / self.ciImage.extent.size.height;
    float aspect_w = aspect > 1.f ? 1.f : aspect;
    float aspect_h = aspect <= 1.f ? 1.f : aspect;
    
    rect.size.height *= [UIScreen mainScreen].scale;
    rect.size.width *= [UIScreen mainScreen].scale;
    
    CGRect nRect = rect;
    nRect.size.width /= aspect_w;
    nRect.size.height /= aspect_h;
    
    nRect.origin.x -= (nRect.size.width - rect.size.width) / 2;
    nRect.origin.y -= (nRect.size.height - rect.size.height) / 2;
    
    [self.ciContext drawImage:self.ciImage inRect:nRect fromRect:self.ciImage.extent];
    
    self.ciImage = nil;
}
- (IBAction)btnConnectTouch:(id)sender {
    
    if([self.btnConnect.titleLabel.text isEqualToString:@"Connect"]) {
        [self.btnConnect setTitle:@"Connecting..." forState:UIControlStateNormal];
        NSString* rtmpUrl = @"rtmp://192.168.2.1/live/myStream";
        
        _sampleGraph.reset(new videocore::sample::SampleGraph([self](videocore::sample::SessionState state){
            [self connectionStatusChange:state];
        }));
    
        
        _sampleGraph->setPBCallback([=](const uint8_t* const data, size_t size) {
            [self gotPixelBuffer: data withSize: size];
        });
        
        float scr_w = 480;
        float scr_h = 320;
        
        _sampleGraph->startRtmpSession([rtmpUrl UTF8String], scr_w, scr_h, 500000 /* video bitrate */, 15 /* video fps */);
    }
    else if ( [self.btnConnect.titleLabel.text isEqualToString:@"Connected"]) {
        // disconnect
        _sampleGraph.reset();
        [self.btnConnect setTitle:@"Connect" forState:UIControlStateNormal];

    }
    
}

- (void) connectionStatusChange:(videocore::sample::SessionState) state
{
    NSLog(@"Connection status: %d", state);
    if(state == videocore::sample::kSessionStateStarted) {
        NSLog(@"Connected");
        [self.btnConnect setTitle:@"Connected" forState:UIControlStateNormal];
        [self.btnConnect.titleLabel sizeToFit];

        
    } else if(state == videocore::sample::kSessionStateError || state == videocore::sample::kSessionStateEnded) {
        NSLog(@"Disconnected");
        [self.btnConnect setTitle:@"Connect" forState:UIControlStateNormal];
        _sampleGraph.reset();
    }
}
- (void) gotPixelBuffer: (const uint8_t* const) data withSize: (size_t) size {
    
    @autoreleasepool {
        
       
        CVPixelBufferRef pb = (CVPixelBufferRef) data;
        CVPixelBufferLockBaseAddress(pb, 1);
        CIImage *ciImage = [CIImage imageWithCVPixelBuffer:pb];
        
        self.ciImage = ciImage;
        [self.glkView display];
        
        CVPixelBufferUnlockBaseAddress(pb, 0);

    }
}
@end
