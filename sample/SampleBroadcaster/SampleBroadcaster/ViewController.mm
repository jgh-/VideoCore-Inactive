//
//  ViewController.m
//  SampleBroadcaster
//
//  Created by James Hurley on 5/6/14.
//  Copyright (c) 2014 videocore. All rights reserved.
//

#import "ViewController.h"
#import "VCSimpleSession.h"


@interface ViewController ()
@property (nonatomic, retain) VCSimpleSession* session;
@end

@implementation ViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
    
    _session = [[VCSimpleSession alloc] initWithVideoSize:CGSizeMake(1280, 720) frameRate:30 bitrate:1000000];
    
    [self.previewView addSubview:_session.previewView];
    _session.previewView.frame = self.previewView.bounds;
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)dealloc {
    [_btnConnect release];
    [_previewView release];
    [_session release];
    
    [super dealloc];
}

- (IBAction)btnConnectTouch:(id)sender {
    
    switch(_session.rtmpSessionState) {
        case VCSessionStateEnded:
        case VCSessionStateError:
        case VCSessionStateNone:
            [_session startRtmpSessionWithURL:@"rtmp://192.168.1.151/live" andStreamKey:@"myStream"];
            break;
        default:
            [_session endRtmpSession];
            break;
    }
}

- (void) connectionStatusChange:(VCSessionState) state
{
    switch(state) {
        case VCSessionStateStarting:
            [self.btnConnect setTitle:@"Connecting" forState:UIControlStateNormal];
            break;
        case VCSessionStateStarted:
            [self.btnConnect setTitle:@"Disconnect" forState:UIControlStateNormal];
            break;
        default:
            [self.btnConnect setTitle:@"Connect" forState:UIControlStateNormal];
            break;
    }
}

@end
