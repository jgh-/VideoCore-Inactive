//
//  JCWebSocket.h
//  showhls
//
//  Created by DawenRie on 15/9/8.
//  Copyright (c) 2015年 DawenRie. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol JCChatClientDelegate;

@interface JCChatClient : NSObject
@property (nonatomic, assign) id <JCChatClientDelegate> delegate;

- (id)initWithURLString:(NSString *)urlstring andVID:(NSString *)vid andUID:(NSString *)uid;

- (BOOL)open;
- (void)close;
- (void)closeWithCode:(int)code reason:(NSString *)reason;

- (void)sendMessage:(NSString *)message ofChatter:(NSString *)chatter;
@end

#pragma mark -
#pragma mark - JCWebSocketDelegate
@protocol JCChatClientDelegate <NSObject>
- (void)chatClient:(JCChatClient *)chatClient didReceiveMessage:(NSData *)data;
@optional
- (void)chatClientDidOpen:(JCChatClient *)chatClient;
- (void)chatClient:(JCChatClient *)chatClient didFailWithError:(NSError *)error;
- (void)chatClient:(JCChatClient *)chatClient didCloseWithCode:(NSInteger)code;
@end