/*
 
 Video Core
 Copyright (C) 2014 James G. Hurley
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 USA
 
 */
#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <iostream>
#include <videocore/stream/Apple/StreamSession.h>


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
        
        StreamSession::StreamSession() : m_status(0)
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
        StreamSession::connect(std::string host, int port, StreamSessionCallback_t callback)
        {
            m_callback = callback;
            if(m_status > 0) {
                disconnect();
            }
            @autoreleasepool {
                
                CFReadStreamRef readStream;
                CFWriteStreamRef writeStream;

                CFStreamCreatePairWithSocketToHost(kCFAllocatorDefault, (CFStringRef)[NSString stringWithUTF8String:host.c_str()], port, &readStream, &writeStream);
            
                m_inputStream = (NSInputStream*)readStream;
                m_outputStream = (NSOutputStream*)writeStream;
            
                dispatch_async(dispatch_get_global_queue(0, 0), ^{
                    this->startNetwork();
                });
            }

        }
        
        void
        StreamSession::disconnect()
        {
            [NSIS(m_inputStream) close];
            [NSOS(m_outputStream) close];
            [NSIS(m_inputStream) release];
            [NSOS(m_outputStream) release];
        }
        
        size_t
        StreamSession::write(uint8_t *buffer, size_t size)
        {
            NSInteger ret = 0;
            ret = [NSOS(m_outputStream) write:buffer maxLength:size];
          
            if(ret >= 0 &&ret < size && (m_status & kStreamStatusWriteBufferHasSpace)) {
                m_status ^= kStreamStatusWriteBufferHasSpace;
            }

            return ret;
        }
        
        size_t
        StreamSession::read(uint8_t *buffer, size_t size)
        {
            size_t ret = 0;
            
            ret = [NSIS(m_inputStream) read:buffer maxLength:size];
            
            if((ret < size) && (m_status & kStreamStatusReadBufferHasBytes)) {
                m_status ^= kStreamStatusReadBufferHasBytes;
            }
            return ret;
        }
        
        void
        StreamSession::setStatus(StreamStatus_t status, bool clear)
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
                
                if(NSIS(m_inputStream).streamStatus > 0 &&
                   NSOS(m_outputStream).streamStatus > 0 &&
                   NSIS(m_inputStream).streamStatus < 5 &&
                   NSOS(m_outputStream).streamStatus < 5) {
                    // Connected.
                    setStatus(kStreamStatusConnected, true);
                } else return;
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
                setStatus(kStreamStatusErrorEncountered);
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
            
            [NSRL(m_runLoop) run];
        }
        
    }
}
