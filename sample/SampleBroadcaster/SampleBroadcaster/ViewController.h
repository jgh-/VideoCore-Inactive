//
//  ViewController.h
//  SampleBroadcaster
//
//  Created by James Hurley on 5/6/14.
//  Copyright (c) 2014 videocore. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>

#import "SampleGraph.h"

@interface ViewController : UIViewController
{
    std::unique_ptr<videocore::sample::SampleGraph> _sampleGraph;
}

@property (retain, nonatomic) IBOutlet UIButton *btnSpin;
@property (retain, nonatomic) IBOutlet GLKView *glkView;
@property (retain, nonatomic) IBOutlet UIButton *btnConnect;
- (IBAction)btnConnectTouch:(id)sender;
- (IBAction)btnSpinTouch:(id)sender;
@end
