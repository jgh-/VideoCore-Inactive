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
#include <videocore/system/JobQueue.hpp>
#include <videocore/system/PreBuffer.hpp>
#include <videocore/stream/IStreamSession.hpp>

namespace videocore {
    
    typedef enum {
        kAsyncStreamStateNone           = 0,
        kAsyncStreamStateConnecting     = 1,
        kAsyncStreamStateConnected      = 2,
        kAsyncStreamStateDisconnecting  = 4,
        kAsyncStreamStateDisconnected   = 5,
        kAsyncStreamStateError          = 6,
    } AsyncStreamState_T;
    
    typedef std::vector<uint8_t> AsyncStreamBuffer;
    typedef std::shared_ptr<AsyncStreamBuffer> AsyncStreamBufferSP;
    
#pragma mark -
#pragma mark AnsyncStreamSession
    
    typedef std::function<void(StreamStatus_T status)> SSConnectionStatus_T;
    typedef std::function<void(AsyncStreamBuffer& abuff)> SSAnsyncReadCallBack_T;
    typedef std::function<void()> SSAnsyncWriteCallBack_T;

    
    class AsyncStreamReader;
    class AsyncStreamWriter;
    
    class AsyncStreamSession {
    public:
        AsyncStreamSession(IStreamSession *stream);
        ~AsyncStreamSession();
        void connect(const std::string &host, int port, SSConnectionStatus_T statuscb);
        void disconnect();
        void write(uint8_t *buffer, size_t length, SSAnsyncWriteCallBack_T writecb=nullptr);
        void readLength(size_t length, SSAnsyncReadCallBack_T readcb);
        void readLength(size_t length, AsyncStreamBufferSP orgbuf, size_t offset, SSAnsyncReadCallBack_T readcb);
        
    private:
        void setState(AsyncStreamState_T state);
        std::shared_ptr<AsyncStreamReader> getCurrentReader();
        void doReadData();
        bool innnerReadData();
        void finishCurrentReader();
        
        std::shared_ptr<AsyncStreamWriter> getCurrentWriter();

        void doWriteData();
        void innerWriteData();
        void finishCurrentWriter();
        
    private:
        std::unique_ptr<IStreamSession> m_stream;
        JobQueue m_eventTriggerJob;
        JobQueue m_socketJob;
        std::queue<std::shared_ptr<AsyncStreamReader>> m_readerQueue;
        std::queue<std::shared_ptr<AsyncStreamWriter>> m_writerQueue;
        SSConnectionStatus_T m_connectionStatusCB;
        AsyncStreamState_T m_state;
        PreallocBuffer m_inputBuffer;
        bool m_doReadingData;
    };
}
#endif /* AnsyncStreamSession_hpp */

