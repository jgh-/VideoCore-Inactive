//
//  AnsyncStreamSession.hpp
//  Pods
//
//  Created by jinchu darwin on 15/9/29.
//
//

#ifndef AnsyncStreamSession_hpp
#define AnsyncStreamSession_hpp

#include <cstddef>
#include <functional>
#include <vector>
#include <queue>
#include <string>

#include <videocore/system/util.h>
#include <videocore/stream/IStreamSession.hpp>

namespace videocore {
    
    typedef enum {
        kAsyncStreamStateNone           = 0,
        kAsyncStreamStateConnecting     = 1,
        kAsyncStreamStateConnected      = 2,
        kAsyncStreamStateDisconnecting  = 4,
        kAsyncStreamStateDisconnected   = 5,
        kAsyncStreamStateError          = 6,
    } AnsyncStreamState_T;

#pragma mark -
#pragma mark AnsyncBuffer
    class AnsyncBuffer {
    public:
        AnsyncBuffer(size_t capBytes);
        ~AnsyncBuffer();
        
        void ensureCapacityForWrite(size_t capBytes);
        
        size_t availableBytes();    // for read
        uint8_t *readBuffer();
        
        void getReadBuffer(uint8_t **bufferPtr, size_t *availableBytesPtr);
        
        void didRead(size_t bytesRead);
        
        size_t availableSpace();   // for write
        uint8_t *writeBuffer();
        
        void getWriteBuffer(uint8_t **bufferPtr, size_t *availableSpacePtr);
        
        void didWrite(size_t bytesWritten);
        
        void reset();
        
    private:
        uint8_t *preBuffer;
        size_t preBufferSize;
        
        uint8_t *readPointer;
        uint8_t *writePointer;
    };
    
    typedef std::vector<uint8_t> AsyncReadBuffer;
    
#pragma mark -
#pragma mark AnsyncStreamSession
    
    typedef std::function<void(StreamStatus_t status)> SSConnectionStatus_T;
    typedef std::function<void(AsyncReadBuffer& abuff)> AnsyncReadCallBack_T;
    
    class AnsyncReadConsumer;
    
    class AnsyncStreamSession {
    public:
        AnsyncStreamSession(IStreamSession *stream);
        ~AnsyncStreamSession();
        void connect(const std::string &host, int port, SSConnectionStatus_T statuscb);
        void disconnect();
        void write(uint8_t *buffer, size_t length);
        void readLength(size_t length, AnsyncReadCallBack_T readcb);
        
    private:
        void setState(AnsyncStreamState_T state);
        void maybeDequeueReader();
        void pumpReader();
        bool innerPumpReader();
        void pumpWriter();
        
    private:
        std::unique_ptr<IStreamSession> m_stream;
        std::queue<std::shared_ptr<AnsyncReadConsumer>> m_readerQueue;
        std::shared_ptr<AnsyncReadConsumer> m_currentReader;
        SSConnectionStatus_T m_connectionStatusCB;
        AnsyncStreamState_T m_state;
        AnsyncBuffer m_outputBuffer;
        AnsyncBuffer m_inputBuffer;
        bool m_pumpingReader;
    };
}
#endif /* AnsyncStreamSession_hpp */

