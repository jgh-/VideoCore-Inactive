//
//  JCChatFrame.h
//  showhls
//
//  Created by DawenRie on 15/9/9.
//  Copyright (c) 2015年 DawenRie. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "JCChatConsts.h"

@interface JCChatFrame : NSObject

- (void)parseHeader:(NSData *)data error:(NSError **)error;
- (void)parseExtraHeader:(NSData *)data error:(NSError **)error;

- (size_t)extraHeaderLength;

- (BOOL)isTextFrame;
- (BOOL)isControllFrame;
- (BOOL)isFrameFinished;
- (uint64_t)payloadLength;
- (void)addFrameCount;
- (void)appendFrameData:(NSData *)data;
- (NSData *)totalData;

+ (NSData *)makeTextFrame:(NSData *)message;
+ (NSData *)makeFrame:(NSData *)message withOpCode:(int)opcode;
@end
