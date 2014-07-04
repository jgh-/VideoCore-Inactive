//
//  ViewController.h
//  SampleBroadcaster
//
//  Created by James Hurley on 5/6/14.
//  Copyright (c) 2014 videocore. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>


@interface ViewController : UIViewController


@property (retain, nonatomic) IBOutlet UIView *previewView;
@property (retain, nonatomic) IBOutlet UIButton *btnConnect;

- (IBAction)btnConnectTouch:(id)sender;

@end
