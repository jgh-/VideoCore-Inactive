//
//  VCPreviewView.h
//  VideoCoreDevelopment
//
//  Created by James Hurley on 7/3/14.
//  Copyright (c) 2014 VideoCore. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface VCPreviewView : UIView

- (void) drawFrame: (CVPixelBufferRef) pixelBuffer;

@end
