//
//  JCChatConsts.h
//  showhls
//
//  Created by DawenRie on 15/9/9.
//  Copyright (c) 2015年 DawenRie. All rights reserved.
//

#ifndef showhls_JCChatConsts_h
#define showhls_JCChatConsts_h

extern NSString *const JCCWebSocketErrorDomain;
extern NSString *const JCCHTTPResponseErrorKey;

enum JCCStatusCode {
    JCCStatusCodeNormal = 1000,
    JCCStatusCodeGoingAway = 1001,
    JCCStatusCodeProtocolError = 1002,
    JCCStatusCodeUnhandledType = 1003,
    // 1004 reserved.
    JCCStatusNoStatusReceived = 1005,
    // 1004-1006 reserved.
    JCCStatusCodeInvalidUTF8 = 1007,
    JCCStatusCodePolicyViolated = 1008,
    JCCStatusCodeMessageTooBig = 1009,
};

enum JCWebSocketOpCode{
    JCCOpCodeTextFrame = 0x1,           // 文本模式
    JCCOpCodeBinaryFrame = 0x2,         // 二进制模式
    // 3-7 reserved.
    JCCOpCodeConnectionClose = 0x8,     // 关闭连接
    JCCOpCodePing = 0x9,                // ping
    JCCOpCodePong = 0xA,                // pong
    // B-F reserved.
};

#define JCCLog(msg, args...) {\
NSLog(@"[JCC] " msg, ##args); \
}

#endif
