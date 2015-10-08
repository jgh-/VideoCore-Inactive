//
//  JCWebSocket.m
//  showhls
//
//  Created by DawenRie on 15/9/8.
//  Copyright (c) 2015年 DawenRie. All rights reserved.
//

#import <Endian.h>
#import <CommonCrypto/CommonDigest.h>
#import <Security/SecRandom.h>

#import "JCChatClient.h"

#import "JCChatFrame.h"
#import "GCDAsyncSocket.h"
#import "DDLog.h"
#import "DDTTYLogger.h"

#include <videocore/stream/AnsyncStreamSession.hpp>
#include <videocore/stream/Apple/StreamSession.h>


#define USE_CocoaAsyncSocket (YES)


NSString *const JCCWebSocketErrorDomain = @"JCCWebSocketErrorDomain";
NSString *const JCCHTTPResponseErrorKey = @"SRHTTPResponseErrorKey";

static NSString *const WebSocketMagicSecKey = @"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";  // rfc6455

NS_ENUM(long, JCSocketTag) {
    JCWriteShakeTag = 1,
    JCReadShakeTag,
    JCReadHeader,       // 帧头的第一部分
    JCReadExtraHeader,  // 帧头的额外长度
    JCReadFrameData,    // 读取真的内容
    JCWriteFrameTag,
};

@interface JCChatClient() <GCDAsyncSocketDelegate> {
    NSURL *_URL;
#ifdef USE_CocoaAsyncSocket
    GCDAsyncSocket *_socket;
#else
    videocore::AnsyncStreamSession *m_anyncStream;
#endif
    
    dispatch_queue_t _delegateQueue;
    JCChatFrame *_chatFrame;

    NSString *_secKey;
    
    NSString *_chatVID;
    NSString *_chatUID;
    
    NSUInteger _chatMessageIndex;
}
@end

@implementation JCChatClient
static __strong NSData *CRLFCRLF;   // HTTP消息分隔符
+ (void)initialize;
{
    CRLFCRLF = [[NSData alloc] initWithBytes:"\r\n\r\n" length:4];
}

#pragma mark -
#pragma mark - 初始化
- (id)initWithURLString:(NSString *)urlstring andVID:(NSString *)vid andUID:(NSString *)uid{
    self = [super init];
    if (self) {
        NSString *finalUrlstr = [NSString stringWithFormat:@"%@%@vid=%@&uid=%@",
                                 urlstring,
                                 [urlstring rangeOfString:@"?"].length == 0 ? @"?" : @"&",
                                 vid,
                                 uid];

        _URL = [NSURL URLWithString:[finalUrlstr stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];
        _chatVID = vid;
        _chatUID = uid;
        [self commonInitialize];
    }
    
    return self;
}

- (void)commonInitialize {
    _chatMessageIndex = 1;
    [DDLog addLogger:[DDTTYLogger sharedInstance] withLogLevel:LOG_LEVEL_WARN];
    _delegateQueue = dispatch_queue_create("cn.jclive.chatclient.delegatequeue", DISPATCH_QUEUE_SERIAL);
#ifdef USE_CocoaAsyncSocket
    _socket = [[GCDAsyncSocket alloc] initWithDelegate:self delegateQueue:_delegateQueue];
#else
    m_anyncStream = new videocore::AnsyncStreamSession(new videocore::Apple::StreamSession);
#endif
}

- (void)dealloc {
    NSLog(@"dealloc %@", NSStringFromClass([self class]));
    [self close];
}

#pragma mark -
#pragma mark - WebSocket协议交互
- (void)sendShakeHeader{
    NSMutableString *reqString = [NSMutableString string];
    [reqString appendFormat:@"GET %@?%@ HTTP/1.1\r\n", _URL.path, _URL.query];
    [reqString appendFormat:@"Host: %@", _URL.host];
    if (_URL.port) {
        [reqString appendFormat:@":%@", _URL.port];
    }
    [reqString appendString:@"\r\n"];
    
    [reqString appendString:@"Upgrade: websocket\r\n"];
    [reqString appendString:@"Connection: Upgrade\r\n"];

    NSMutableData *keyBytes = [[NSMutableData alloc] initWithLength:16];
    SecRandomCopyBytes(kSecRandomDefault, keyBytes.length, (uint8_t *)keyBytes.mutableBytes);
    _secKey = [keyBytes base64EncodedStringWithOptions:0];
    NSAssert([_secKey length] == 24, @"Something wrong about Base64");

    [reqString appendFormat:@"Sec-WebSocket-Key: %@\r\n", _secKey];
    [reqString appendString:@"Sec-WebSocket-Version: 13\r\n"];
    
    NSString *scheme = [_URL.scheme lowercaseString];
    
    if ([scheme isEqualToString:@"wss"]) {
        scheme = @"https";
    } else if ([scheme isEqualToString:@"ws"]) {
        scheme = @"http";
    }
    
    if (_URL.port) {
        [reqString appendFormat:@"Origin: %@://%@:%@/\r\n", scheme, _URL.host, _URL.port];
    } else {
        [reqString appendFormat:@"Origin: %@://%@/\r\n", scheme, _URL.host];
    }
    // 消息结束
    [reqString appendString:@"\r\n"];
    
    JCCLog(@"send:\n%@", reqString);
    NSData *reqData = [reqString dataUsingEncoding:NSUTF8StringEncoding];
#ifdef USE_CocoaAsyncSocket
    [_socket writeData:reqData withTimeout:-1 tag:JCWriteShakeTag];
#else
    m_anyncStream->write((uint8_t*)reqData.bytes, reqData.length);
#endif
    
    [self readShakeResponse];
}

- (void)readShakeResponse {
    // 读取握手数据
#ifdef USE_CocoaAsyncSocket
    [_socket readDataToData:CRLFCRLF withTimeout:-1 tag:JCReadShakeTag];
    [_socket readDataToData:CRLFCRLF completionHandler:^(NSData *data) {
        NSLog(@"Shake response length:%d", (int)data.length);
        [self checkShakeResponse:data];
    }];
#else
    // test only
    m_anyncStream->readLength(129, [=](videocore::AsyncStreamBuffer &buf){
        NSLog(@"Shake response length:%ld", buf.size());
        NSData *data = [NSData dataWithBytes:&buf[0] length:buf.size()];
        [self checkShakeResponse:data];
    });
#endif
}

static NSString *SHA1StringOfString(NSString *str) {
    uint8_t md[CC_SHA1_DIGEST_LENGTH];
    
    CC_SHA1(str.UTF8String, (CC_LONG)str.length, md);
    
    NSData *data = [NSData dataWithBytes:md length:CC_SHA1_DIGEST_LENGTH];
    return [data base64EncodedStringWithOptions:0];
}

// 校验握手信息
- (void)checkShakeResponse:(NSData *)data {
    // 不想自己解析HTTP头了，用一下CF的库吧~
    CFHTTPMessageRef HTTPHeaders  = CFHTTPMessageCreateEmpty(NULL, NO);
    CFHTTPMessageAppendBytes(HTTPHeaders, (const UInt8 *)data.bytes, data.length);
    CFIndex responseCode = CFHTTPMessageGetResponseStatusCode(HTTPHeaders);
    if (responseCode >= 400) {
        NSString *prompt = [NSString stringWithFormat:@"Request failed with response code %ld", responseCode];
        JCCLog(@"%@", prompt);
        NSDictionary *info = @{NSLocalizedDescriptionKey:prompt, JCCHTTPResponseErrorKey:@(responseCode)};
        [self failWithError:[NSError errorWithDomain:JCCWebSocketErrorDomain
                                                code:2132
                                            userInfo:info]];
        return;
    }
    NSString *acceptHeader = CFBridgingRelease(CFHTTPMessageCopyHeaderFieldValue(HTTPHeaders, CFSTR("Sec-WebSocket-Accept")));
    
    CFRelease(HTTPHeaders);
    HTTPHeaders = nil;
    
    if (acceptHeader != nil) {
        NSString *concattedString = [_secKey stringByAppendingString:WebSocketMagicSecKey];
        NSString *expectedAccept = SHA1StringOfString(concattedString);
        if ([acceptHeader isEqualToString:expectedAccept]) {
            JCCLog(@"Shake OK");
            [self performDelegateInMain:^{
                if ([self.delegate respondsToSelector:@selector(chatClientDidOpen:)]) {
                    [self.delegate chatClientDidOpen:self];
                };
            }];
            [self readNewFrame];
            return ;
        }
    }
    // here failed
    NSDictionary *info = @{NSLocalizedDescriptionKey:@"Invalid Sec-WebSocket-Accept response"};
    [self failWithError:[NSError errorWithDomain:JCCWebSocketErrorDomain code:2133 userInfo:info]];
}

// 重置开始新的一帧
- (void)readNewFrame {
    _chatFrame = [[JCChatFrame alloc] init];
    [self readContinueFrame];
}

// 开始分片的后续帧（或者第一帧）
- (void)readContinueFrame{
    // 先读取两个字节的头
    JCCLog(@"Read header length");
#ifdef USE_CocoaAsyncSocket
    [_socket readDataToLength:2 completionHandler:^(NSData *data) {
        [self checkFrameHeader:data];
    }];
#else
    m_anyncStream->readLength(2, [=](videocore::AsyncStreamBuffer &buf){
        NSData *data = [NSData dataWithBytes:&buf[0] length:buf.size()];
        JCCLog(@"Read %zd data", data.length);
        [self checkFrameHeader:data];
    });
#endif
}

- (void)checkFrameHeader:(NSData *)data {
    NSError *error = nil;
    [_chatFrame parseHeader:data error:&error];
    if (!error) {
        size_t extraLength = [_chatFrame extraHeaderLength];
        if (extraLength == 0) {
            // 已经收取一个完整的帧头
            [self handleFrameHeader];
        }
        else {
            JCCLog(@"Read extra header length(%zd)", extraLength);
            // 还要额外的数据组成帧头
#ifdef USE_CocoaAsyncSocket
            [_socket readDataToLength:extraLength completionHandler:^(NSData *data) {
                [self checkFrameExtraHeader:data];
            }];
#else
            m_anyncStream->readLength(extraLength, [=](videocore::AsyncStreamBuffer &buf){
                NSData *data = [NSData dataWithBytes:&buf[0] length:buf.size()];
                JCCLog(@"Read %zd data", data.length);
                [self checkFrameExtraHeader:data];
            });
#endif
        }
    }
    else {
        [self closeWithCode:JCCStatusCodeProtocolError reason:[error localizedDescription]];
    }
}

- (void)checkFrameExtraHeader:(NSData *)data {
    NSError *error = nil;
    [_chatFrame parseExtraHeader:data error:&error];
    if (!error) {
        [self handleFrameHeader];
    }
    else {
        [self closeWithCode:JCCStatusCodeProtocolError reason:[error localizedDescription]];

    }
}

// 处理完整的一帧头
- (void)handleFrameHeader {
    BOOL isControlFrame = [_chatFrame isControllFrame];
    
    if (isControlFrame && ![_chatFrame isFrameFinished]) {
        [self closeWithCode:JCCStatusCodeProtocolError reason:@"Fragmented control frames not allowed"];
        return;
    }
    
    if (isControlFrame && [_chatFrame payloadLength] >= 126) {
        [self closeWithCode:JCCStatusCodeProtocolError reason:@"Control frames cannot have payloads larger than 126 bytes"];
        return;
    }
    
    if (!isControlFrame) {
        [_chatFrame addFrameCount];
    }
    
    // 这一帧没有额外的数据需要读取了
    if ([_chatFrame payloadLength] == 0) {
        // 相当于读取一个0长度内容，直接出发检查数据的逻辑
        [self checkFrameData:nil];
    }
    else {
        JCCLog(@"Read payload length=%zd", [_chatFrame payloadLength]);
#ifdef USE_CocoaAsyncSocket
        [_socket readDataToLength:[_chatFrame payloadLength] completionHandler:^(NSData *data) {
            [self checkFrameData:data];
        }];
#else
        m_anyncStream->readLength([_chatFrame payloadLength], [=](videocore::AsyncStreamBuffer &buf){
            NSData *data = [NSData dataWithBytes:&buf[0] length:buf.size()];
            JCCLog(@"Read %zd payload", data.length);
            [self checkFrameData:data];
        });
#endif
    }
}

- (void)checkFrameData:(NSData *)data {
    if (data) {
        [_chatFrame appendFrameData:data];
    }
    
    if ([_chatFrame isControllFrame]) {
        [self handleFrameData];
    }
    else {
        if ([_chatFrame isFrameFinished]) {
            [self handleFrameData];
        }
        else {
            [self readContinueFrame];
        }
    }
}

// 处理完整的一帧数据
- (void)handleFrameData {
    JCChatFrame *currentFrame = _chatFrame;
    // 把异步读取的逻辑走起来
    [self readNewFrame];
    if ([currentFrame isTextFrame]) {
        NSData *frameData = [currentFrame totalData];
        [self handleMessage:frameData];
    }
}

- (void)handleMessage:(NSData *)message {
    NSString *msg = [[NSString alloc] initWithData:message encoding:NSUTF8StringEncoding];
    JCCLog("Handle message:%@", msg);
    [self performDelegateInMain:^(){
        if ([self.delegate respondsToSelector:@selector(chatClient:didReceiveMessage:)]) {
            [self.delegate chatClient:self didReceiveMessage:message];
        }
    }];

}

- (void)failWithError:(NSError *)error;
{
    JCCLog(@"Failing with error %@", error.localizedDescription);
    [self performDelegateInMain:^(){
        if ([self.delegate respondsToSelector:@selector(chatClient:didFailWithError:)]) {
            [self.delegate chatClient:self didFailWithError:error];
        }
    }];
}

- (void)performDelegateInMain:(dispatch_block_t)block;
{
    dispatch_async(dispatch_get_main_queue(), block);
}

#pragma mark -
#pragma mark - 基本API
- (BOOL)open{
    NSError *error = nil;
    uint16_t port = _URL.port.unsignedShortValue;
    if (port == 0) {
        port = 80;
    }
    NSString *host = _URL.host;
#ifdef USE_CocoaAsyncSocket
    [_socket connectToHost:host onPort:port error:&error];
    if (error) {
        JCCLog(@"Connect to URL(%@) error:%@", _URL, error);
        return NO;
    }
#else 
    // test only
    std::string strhost("192.168.50.19");
    m_anyncStream->connect(strhost, port, [=](videocore::StreamStatus_t status) {
        if (status == videocore::kAsyncStreamStateConnected) {
            [self sendShakeHeader];
        }
    });
#endif
    
    return YES;
}

- (void)close{
#ifdef USE_CocoaAsyncSocket
    [_socket disconnect];
#endif
}

- (void)closeWithCode:(int)code reason:(NSString *)reason{
#ifdef USE_CocoaAsyncSocket
    NSData *frameData = [JCChatFrame makeFrame:[reason dataUsingEncoding:NSUTF8StringEncoding] withOpCode:JCCOpCodeConnectionClose];
    [_socket writeData:frameData withTimeout:-1 tag:JCWriteFrameTag];
    [_socket disconnectAfterWriting];
#endif
}

- (void)sendMessage:(NSString *)message ofChatter:(NSString *)chatter{
    NSDictionary *msgDict = @{@"vid":_chatVID,
                              @"uid":_chatUID,
                              @"name":chatter,
                              @"msg":message};
    NSError *error = nil;
    NSData *msgData = [NSJSONSerialization dataWithJSONObject:msgDict options:0 error:&error];
    if (error) {
        JCCLog(@"Can't make JSON message");
    }
    NSData *socketData = [JCChatFrame makeTextFrame:msgData];
    JCCLog(@"Writing data with tag(%ld):%@", JCWriteFrameTag + _chatMessageIndex, msgDict);
#ifdef USE_CocoaAsyncSocket
    [_socket writeData:socketData withTimeout:-1 tag:(JCWriteFrameTag + _chatMessageIndex++)];
#else
    m_anyncStream->write((uint8_t *)socketData.bytes, socketData.length);
#endif
}

#pragma mark -
#pragma mark - GCDAsyncSocket 代理处理
/**
 * Called when a socket connects and is ready for reading and writing.
 * The host parameter will be an IP address, not a DNS name.
 **/
- (void)socket:(GCDAsyncSocket *)sock didConnectToHost:(NSString *)host port:(uint16_t)port{
    [self sendShakeHeader];
}

/**
 * Called when a socket has completed reading the requested data into memory.
 * Not called if there is an error.
 **/
- (void)socket:(GCDAsyncSocket *)sock didReadData:(NSData *)data withTag:(long)tag{
    NSLog(@"Missed handler for tag:%ld", tag);
}

- (void)socket:(GCDAsyncSocket *)sock didWriteDataWithTag:(long)tag{
    JCCLog(@"didWriteDataWithTag(%ld)", tag);
}

- (void)socketDidDisconnect:(GCDAsyncSocket *)sock withError:(NSError *)err{
    JCCLog(@"socketDidDisconnect:%@", err);
}

@end
