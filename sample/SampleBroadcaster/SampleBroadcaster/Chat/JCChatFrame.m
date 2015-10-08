//
//  JCChatFrame.m
//  showhls
//
//  Created by DawenRie on 15/9/9.
//  Copyright (c) 2015年 DawenRie. All rights reserved.
//

#import <Endian.h>

#import "JCChatFrame.h"


/* From RFC:
 0                   1                   2                   3
  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 +-+-+-+-+-------+-+-------------+-------------------------------+
 |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 | |1|2|3|       |K|             |                               |
 +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 |     Extended payload length continued, if payload len == 127  |
 + - - - - - - - - - - - - - - - +-------------------------------+
 |                               |Masking-key, if MASK set to 1  |
 +-------------------------------+-------------------------------+
 | Masking-key (continued)       |          Payload Data         |
 +-------------------------------- - - - - - - - - - - - - - - - +
 :                     Payload Data continued ...                :
 + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 |                     Payload Data continued ...                |
 +---------------------------------------------------------------+
 */

static const uint8_t JCCFinMask          = 0x80;    // B1000 0000
static const uint8_t JCCOpCodeMask       = 0x0F;    // B0000 1111
static const uint8_t JCCRsvMask          = 0x70;    // B0111 0000
static const uint8_t JCCMaskMask         = 0x80;    // B1000 0000 第二个字节的首位
static const uint8_t JCCPayloadLenMask   = 0x7F;    // B0111 1111 第二个字节剩下的后7个位

@implementation JCChatFrame {
    BOOL _fin;
    uint8_t _opCode;
    BOOL _masked;
    uint64_t _payloadLength;
    
    uint8_t _maskKey[4];
    size_t _currentMaskKeyOffset;
    
    size_t _frameCount;
    NSMutableData *_totalData;
    BOOL _payloadUnmasked;
}

static BOOL isControlOpCode(uint8_t opCode) {
    return opCode == JCCOpCodePing || opCode == JCCOpCodePong || opCode == JCCOpCodeConnectionClose;
}

- (BOOL)isTextFrame {
    return _opCode == JCCOpCodeTextFrame;
}

- (BOOL)isControllFrame {
    return isControlOpCode(_opCode);
}

- (BOOL)isFrameFinished {
    return _fin;
}

- (uint64_t)payloadLength {
    return _payloadLength;
}

- (void)addFrameCount {
    _frameCount++;
}

- (void)appendFrameData:(NSData *)data{
    if (!_totalData) {
        _totalData = [NSMutableData dataWithCapacity:_payloadLength];
    }
    [_totalData appendData:data];
}

- (NSData *)maskData:(NSData *)data offset:(size_t)offset{
    NSMutableData *mdata = nil;
    if ([data isKindOfClass:[NSMutableData class]]) {
        mdata = (NSMutableData *)data;
    }
    else {
        mdata = [data mutableCopy];
    }
    uint8_t *mptr = (uint8_t *)[mdata mutableBytes];
    // 直接修改了内部数据
    for (size_t i=offset, j=0; i<mdata.length; i++, j++) {
        mptr[i] = mptr[i] ^ (_maskKey[j % 4]);
    }
    return mdata;
}

- (NSData *)totalData {
    if (_masked && !_payloadUnmasked) {
        JCCLog(@"Unmasking data");
        // unmask same as mask
        _payloadUnmasked = YES;
        return [self maskData:_totalData offset:0];
    }
    return _totalData;
}

void makeProtocolError(NSError **error, NSString *errMsg) {
    if (error) {
        NSDictionary *info = @{NSLocalizedDescriptionKey:errMsg};
        *error = [NSError errorWithDomain:JCCWebSocketErrorDomain code:2134 userInfo:info];
    }
}
- (void)parseHeader:(NSData *)data error:(NSError **)error{
    NSAssert([data length] >= 2, @"第一部分的头长度应该只有2个字节");
    uint8_t *headerBuffer = (uint8_t *)[data bytes];
    if (headerBuffer[0] & JCCRsvMask) {
        makeProtocolError(error, @"Server used RSV bits");
        return ;
    }

    uint8_t receivedOpcode = (JCCOpCodeMask & headerBuffer[0]);
    BOOL isControlFrame = isControlOpCode(receivedOpcode);
    
    if (!isControlFrame && receivedOpcode != 0 && _frameCount > 0) {
        makeProtocolError(error, @"all data frames after the initial data frame must have opcode 0");
        return ;
    }
    
    if (receivedOpcode == 0 && _frameCount == 0) {
        makeProtocolError(error, @"Not a continued message, but opcode=0");
        return ;
    }
    
    // 如果是分帧的话，不更改当前的操作码
    if (receivedOpcode != 0) {
        _opCode = receivedOpcode;
    }
    _fin = !!(JCCFinMask & headerBuffer[0]);
    _masked = !!(JCCMaskMask & headerBuffer[1]);
    _payloadLength = JCCPayloadLenMask & headerBuffer[1];
    
    *error = nil;
}

- (void)parseExtraHeader:(NSData *)data error:(NSError **)error{
    size_t extraLength = [self extraHeaderLength];
    if (extraLength != [data length]) {
        makeProtocolError(error, @"extra length invalid");
        return ;
    }
    const void *headerBuffer = (uint8_t *)[data bytes];
    if (extraLength == 2) {
        _payloadLength = (uint16_t)EndianU16_NtoB(*(uint16_t *)headerBuffer);
    }
    else {
        uint64_t length = *(uint64_t *)headerBuffer;
        _payloadLength = EndianU64_NtoB(length);
    }
}

- (size_t)extraHeaderLength {
    size_t extraHeaderLength = _masked ? 4 : 0;
    if (_payloadLength == 126) {
        extraHeaderLength += 2;
    }
    else if (_payloadLength == 127) {
        extraHeaderLength += 8;
    }
    return extraHeaderLength;
}

+ (NSData *)makeTextFrame:(NSData *)message {
    return [self makeFrame:message withOpCode:JCCOpCodeTextFrame];
}

+ (NSData *)makeFrame:(NSData *)message withOpCode:(int)opcode{
    NSUInteger payloadLength = message.length;
    // 给data分配足够的头空间（32），最后设置一个正确的值
    NSMutableData *frameData = [[NSMutableData alloc] initWithLength:(payloadLength + 32)];
    uint8_t *frameBuffer = (uint8_t *)[frameData mutableBytes];
    frameBuffer[0] = JCCFinMask | opcode;   // 不分帧
    frameBuffer[1] |= JCCMaskMask;
    size_t filledFrameOffset = 2;
    if (payloadLength < 126) {
        frameBuffer[1] |= (uint8_t)payloadLength;
    }
    else if (payloadLength <= UINT16_MAX) {
        frameBuffer[1] |= 126;
        *((uint16_t *)(frameBuffer+filledFrameOffset)) = EndianU16_BtoN((uint16_t)payloadLength);
        filledFrameOffset += 2;
    }
    else  {
        frameBuffer[1] += 127;
        *((uint64_t *)(frameBuffer+filledFrameOffset)) = EndianU64_BtoN((uint64_t)payloadLength);
        filledFrameOffset += 8;
    }
    uint8_t *sendMaskKey = frameBuffer + filledFrameOffset;
    SecRandomCopyBytes(kSecRandomDefault, 4, (uint8_t *)sendMaskKey);
    filledFrameOffset += 4;
    const uint8_t *unmaskedMessage = [message bytes];
    for (NSUInteger i=0; i<payloadLength; i++) {
        frameBuffer[filledFrameOffset] = unmaskedMessage[i] ^ sendMaskKey[i % 4];
        filledFrameOffset += 1;
    }
    NSAssert(filledFrameOffset<=[frameData length], @"逻辑错误导致数据溢出了");
    
    frameData.length = filledFrameOffset;
    return frameData;
}
@end
