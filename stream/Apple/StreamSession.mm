/*
 
 Video Core
 Copyright (c) 2014 James G. Hurley
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 
 */
#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <iostream>
#include <videocore/stream/Apple/StreamSession.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#define SCB(x) ((NSStreamCallback*)(x))
#define NSIS(x) ((NSInputStream*)(x))
#define NSOS(x) ((NSOutputStream*)(x))
#define NSRL(x) ((NSRunLoop*)(x))

@interface NSStreamCallback : NSObject<NSStreamDelegate>
@property (nonatomic, assign) videocore::Apple::StreamSession* session;
@end

@implementation NSStreamCallback

- (void) stream:(NSStream *)aStream handleEvent:(NSStreamEvent)eventCode
{
    self.session->nsStreamCallback(aStream,static_cast<unsigned>( eventCode ));
}

@end
namespace videocore {
    namespace Apple {
        
        StreamSession::StreamSession()
        : m_status(0)
        , m_runLoop(nullptr)
        , m_outputStream(nullptr)
        , m_inputStream(nullptr)
        {
            m_streamCallback = [[NSStreamCallback alloc] init];
            SCB(m_streamCallback).session = this;
        }
        
        StreamSession::~StreamSession()
        {
            disconnect();
            [SCB(m_streamCallback) release];
        }
        
        void
        StreamSession::connect(const std::string& host, int port, StreamSessionCallback_T callback)
        {
            m_callback = callback;
            if(m_status > 0) {
                disconnect();
            }
            @autoreleasepool {
                
                CFReadStreamRef readStream;
                CFWriteStreamRef writeStream;

                CFStreamCreatePairWithSocketToHost(kCFAllocatorDefault,
                                                   (CFStringRef)[NSString stringWithUTF8String:host.c_str()],
                                                   port,
                                                   &readStream,
                                                   &writeStream);
            
                m_inputStream = (NSInputStream*)readStream;
                m_outputStream = (NSOutputStream*)writeStream;
            

                dispatch_queue_t queue = dispatch_queue_create("com.videocore.network", 0);
                
                if(m_inputStream && m_outputStream) {
                    dispatch_async(queue, ^{
                        this->startNetwork();
                    });
                }
                else {
                    nsStreamCallback(nullptr, NSStreamEventErrorOccurred);
                }
                dispatch_release(queue);
            }

        }
        
        void
        StreamSession::disconnect()
        {
            if(m_outputStream) {
                //if(m_runLoop) {
                //    [NSOS(m_outputStream) removeFromRunLoop:NSRL(m_runLoop) forMode:NSDefaultRunLoopMode];
                //}
                [NSOS(m_outputStream) close];
                [NSOS(m_outputStream) release];
                m_outputStream = nullptr;
            }
            if(m_inputStream) {
                //if(m_runLoop) {
                //    [NSOS(m_inputStream) removeFromRunLoop:NSRL(m_runLoop) forMode:NSDefaultRunLoopMode];
                //}
                [NSIS(m_inputStream) close];
                [NSIS(m_inputStream) release];
                m_inputStream = nullptr;
            }

            if(m_runLoop) {
                CFRunLoopStop([NSRL(m_runLoop) getCFRunLoop]);
                [(id)m_runLoop release];
                m_runLoop = nullptr;
            }
        }

        ssize_t
        StreamSession::write(uint8_t *buffer, size_t size)
        {
            NSInteger ret = 0;
          
            if( NSOS(m_outputStream).hasSpaceAvailable ) {
                ret = [NSOS(m_outputStream) write:buffer maxLength:size];
            }
            if(ret >= 0 && ret < size && (m_status & kStreamStatusWriteBufferHasSpace)) {
                // Remove the Has Space Available flag
                m_status ^= kStreamStatusWriteBufferHasSpace;
            }
            else if (ret < 0) {
                NSLog(@"ERROR! [%ld] buffer: %p [ 0x%02x ], size: %zu", (long)NSOS(m_outputStream).streamError.code, buffer, buffer[0], size);
            }

            return ret;
        }
        
        ssize_t
        StreamSession::read(uint8_t *buffer, size_t size)
        {
            NSInteger ret = 0;
            
            ret = [NSIS(m_inputStream) read:buffer maxLength:size];
            
            if((ret < size) && (m_status & kStreamStatusReadBufferHasBytes)) {
                m_status ^= kStreamStatusReadBufferHasBytes;
            }
            else if (NSIS(m_inputStream).hasBytesAvailable != YES) {
                NSLog(@"No more data in stream, clear read status");
                m_status ^= kStreamStatusReadBufferHasBytes;
            }
            return ret;
        }

        void
        StreamSession::setStatus(StreamStatus_T status, bool clear)
        {
            if(clear) {
                m_status = status;
            } else {
                m_status |= status;
            }
            m_callback(*this, status);
        }
        void
        StreamSession::nsStreamCallback(void* stream, unsigned event)
        {
            if(event & NSStreamEventOpenCompleted) {
                // Only set connected event when input and output stream both connected
                if(NSIS(m_inputStream).streamStatus >= NSStreamStatusOpen &&
                   NSOS(m_outputStream).streamStatus >= NSStreamStatusOpen &&
                   NSIS(m_inputStream).streamStatus < NSStreamStatusAtEnd &&
                   NSOS(m_outputStream).streamStatus < NSStreamStatusAtEnd)
                {
                    setStatus(kStreamStatusConnected, true);
                }
                else return;
            }
            if(event & NSStreamEventHasBytesAvailable) {
                setStatus(kStreamStatusReadBufferHasBytes);
            }
            if(event & NSStreamEventHasSpaceAvailable) {
                setStatus(kStreamStatusWriteBufferHasSpace);
            }
            if(event & NSStreamEventEndEncountered) {
                setStatus(kStreamStatusEndStream, true);
            }
            if(event & NSStreamEventErrorOccurred) {
                setStatus(kStreamStatusErrorEncountered, true);
                if (NSIS(m_inputStream).streamError) {
                    NSLog(@"Input stream error:%@", NSIS(m_inputStream).streamError);
                }
                if (NSOS(m_outputStream).streamError) {
                    NSLog(@"Output stream error:%@", NSIS(m_outputStream).streamError);
                }
            }
        }
        
        void
        StreamSession::startNetwork()
        {
            m_runLoop = [NSRunLoop currentRunLoop];
            [NSIS(m_inputStream) setDelegate:SCB(m_streamCallback)];
            [NSIS(m_inputStream) scheduleInRunLoop:NSRL(m_runLoop) forMode:NSDefaultRunLoopMode];
            [NSOS(m_outputStream) setDelegate:SCB(m_streamCallback)];
            [NSOS(m_outputStream) scheduleInRunLoop:NSRL(m_runLoop) forMode:NSDefaultRunLoopMode];
            [NSOS(m_outputStream) open];
            [NSIS(m_inputStream) open];

            [(id)m_runLoop retain];
            [NSRL(m_runLoop) run];
        }
        
    }
}
