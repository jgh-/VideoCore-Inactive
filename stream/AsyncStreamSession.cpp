//
//  AnsyncStreamSession.cpp
//  Pods
//
//  Created by jinchu darwin on 15/9/29.
//
//

#include <videocore/stream/AsyncStreamSession.hpp>

#ifndef DLOG_LEVEL_DEF
#define DLOG_LEVEL_DEF DLOG_LEVEL_VERBOSE
#endif
#include <videocore/system/Logger.hpp>

#include <cstdlib>
#include <cassert>

namespace videocore {    
#pragma mark -
#pragma mark AsyncStreamReader
    /**
     *  Reader object
     *  Reader object represent a read request for specail length or other (TODO)
     *  Caller can provide buffer, read object maybe adjust the buffer size for read
     *  If not provite, reader object will create the buffer, and manager it.
     */
    class AsyncStreamReader {
    public:
        /**
         *  Ctor read specail length
         *  Read is asynchronous, when the the data read enough, it is trigger the event callback
         */
        AsyncStreamReader(size_t length, AsyncStreamBufferSP buf, size_t offset, SSAnsyncReadCallBack_T eventcb){
            m_bytesDone = 0;
            
            if (buf) {
                m_buffer = buf;
                m_startOffset = offset;
                m_bufferOwner = true;
            }
            else {
                m_buffer = std::make_shared<AsyncStreamBuffer>(length);
                m_startOffset = 0;
                m_bufferOwner = false;
            }
            
            m_readLength = length;
            m_eventCallback = eventcb;
        }
        
        AsyncStreamReader() {
            DLogVerbose("AsyncReadConsumer::~AsyncReadConsumer\n");
        }
        
        /**
         *  Read data from pre-allocated buffer
         *
         *  @param abuff When the buffer was read, it will change it's status.
         */
        void readFromBuffer(PreallocBuffer &abuff) {
            size_t availableBytes = abuff.availableBytes();
            size_t bytesToCopy = readLengthIfAvailable(availableBytes);
            resizeForRead(bytesToCopy);
            memcpy(&(*m_buffer)[m_startOffset+m_bytesDone], abuff.readBuffer(), bytesToCopy);
            abuff.didRead(bytesToCopy);
            
            m_bytesDone += bytesToCopy;
        }

        /**
         *  Read from stream
         *
         *  @return Result of stream read method.
         */
        ssize_t readFromStream(IStreamSession *stream) {
            size_t bytesToRead = readLengthRemains();
            resizeForRead(bytesToRead);
            ssize_t result = stream->read(&(*m_buffer)[m_startOffset+m_bytesDone], bytesToRead);
            if (result > 0) {
                m_bytesDone += result;
            }
            return result;
        }
        
        bool done() {
            return m_readLength == m_bytesDone;
        }
        
        void triggerEvent() {
            if (m_eventCallback) {
                // TODO: check the diference owned buffer or not?
                m_eventCallback(*m_buffer);
            }
        }
        
    private:
        size_t readLengthIfAvailable(size_t bytesAvailable) {
            return std::min(bytesAvailable, readLengthRemains());
        }
        
        size_t readLengthRemains() {
            return m_readLength - m_bytesDone;
        }
        
        void resizeForRead(size_t bytesToRead) {
            size_t buffSize = m_buffer->size();
            size_t buffUsed = m_startOffset + m_bytesDone;
            
            size_t buffSpace = buffSize - buffUsed;
            
            if (bytesToRead > buffSpace)
            {
                size_t buffInc = bytesToRead - buffSpace;
                m_buffer->resize(buffInc);
            }
        }
        
    private:
        AsyncStreamBufferSP m_buffer;
        size_t m_startOffset;
        size_t m_bytesDone;
        size_t m_readLength;
        bool m_bufferOwner;
        SSAnsyncReadCallBack_T m_eventCallback;
    };
#pragma mark -
#pragma mark AsyncStreamWriter
    class AsyncStreamWriter {
    public:
        AsyncStreamWriter(AsyncStreamBufferSP buf, SSAnsyncWriteCallBack_T writecb = nullptr) {
            m_bytesDone = 0;
            m_buffer = buf;
            m_eventCallback = writecb;
        }
        AsyncStreamWriter() {
            DLog("AnsyncStreamWriter::~AnsyncStreamWriter\n");
        }
        
        ssize_t writeToStream(IStreamSession *stream) {
            size_t bytesToWrite = m_buffer->size() - m_bytesDone;
            assert(bytesToWrite > 0);
            ssize_t result = stream->write(&(*m_buffer)[m_bytesDone], bytesToWrite);
            if (result > 0) {
                m_bytesDone += result;
            }
            return result;
        }
        bool done() {
            return m_bytesDone == m_buffer->size();
        }
        void triggerEvent() {
            if (m_eventCallback) {
                m_eventCallback();
            }
        }
        
    private:
        AsyncStreamBufferSP m_buffer;
        size_t m_bytesDone;
        SSAnsyncWriteCallBack_T m_eventCallback;
    };
    
#pragma mark -
#pragma mark AnsyncStreamSession
    AsyncStreamSession::AsyncStreamSession(IStreamSession *stream)
    : m_state(kAsyncStreamStateNone)
    , m_inputBuffer(4*1024)
    , m_socketJob("AnsyncStreamSession Socket")
    , m_eventTriggerJob("AnsyncStreamSession Event Trigger")
    , m_doReadingData(false)
    {
        m_stream.reset(stream);
    }
    AsyncStreamSession::~AsyncStreamSession(){
        m_socketJob.mark_exiting();
        m_socketJob.enqueue_sync([=]{});
        DLogVerbose("AsyncStreamSession::~AsyncStreamSession\n");
    }
    
    void AsyncStreamSession::connect(const std::string& host, int port, SSConnectionStatus_T statuscb) {
        m_connectionStatusCB = statuscb;
        m_state = kAsyncStreamStateConnecting;
        m_stream->connect(host, port, [=](IStreamSession& session, StreamStatus_T status){
            m_socketJob.enqueue([=]{
                // make sure set connected status only one time
                if (status & kStreamStatusConnected && m_state < kAsyncStreamStateConnected) {
                    DLogDebug("AnsyncStreamSession connected\n");
                    setState(kAsyncStreamStateConnected);
                    doWriteData();
                    doReadData();
                }
                if (status & kStreamStatusWriteBufferHasSpace) {
                    DLogDebug("AnsyncStreamSession Write ready\n");
                    doWriteData();
                }
                if (status & kStreamStatusReadBufferHasBytes) {
                    DLogDebug("AnsyncStreamSession Read ready\n");
                    doReadData();
                }
                if(status & kStreamStatusErrorEncountered) {
                    DLogDebug("AnsyncStreamSession Error!\n");
                    setState(kAsyncStreamStateError);
                }
                if (status & kStreamStatusEndStream) {
                    DLogDebug("AnsyncStreamSession end.\n");
                    setState(kAsyncStreamStateDisconnected);
                }
            });
        });
    }
    
    void AsyncStreamSession::disconnect(){
        DLogDebug("Disconnecting\n");
        setState(kAsyncStreamStateDisconnecting);
        m_stream->disconnect();
    }
    
    void AsyncStreamSession::write(uint8_t *buffer, size_t length, SSAnsyncWriteCallBack_T writecb){
        // make a reference for the lamdba function
        auto bufref = std::make_shared<AsyncStreamBuffer>(buffer, buffer+length);
        m_socketJob.enqueue([=]{
            auto writer = std::make_shared<AsyncStreamWriter>(bufref, writecb);
            m_writerQueue.push(writer);
            DLogVerbose("Queue write request length:%zd\n", length);
            doWriteData();
        });
    }
    
    void AsyncStreamSession::readLength(size_t length, SSAnsyncReadCallBack_T readcb){
        this->readLength(length, nullptr, 0, readcb);
    }
    
    void AsyncStreamSession::readLength(size_t length, AsyncStreamBufferSP orgbuf, size_t offset, SSAnsyncReadCallBack_T readcb){
        assert(!m_socketJob.thisThreadInQueue());
        
        m_socketJob.enqueue([=]{
            DLogVerbose("Queue read request length:%zd\n", length);
            auto reader = std::make_shared<AsyncStreamReader>(length, orgbuf, offset, readcb);
            m_readerQueue.push(reader);
            doReadData();
        });

    }

#pragma mark -
#pragma mark - Private
    void AsyncStreamSession::setState(AsyncStreamState_T state) {
        assert(m_socketJob.thisThreadInQueue());

        m_state = state;
        m_eventTriggerJob.enqueue([=]{
            m_connectionStatusCB(m_state);
        });
    }
    
#pragma mark -
#pragma mark - Reading
    std::shared_ptr<AsyncStreamReader> AsyncStreamSession::getCurrentReader() {
        assert(m_socketJob.thisThreadInQueue());

        if (m_readerQueue.size() > 0) {
            return m_readerQueue.front();
        }
        return nullptr;
    }
    
    void AsyncStreamSession::doReadData(){
        assert(m_socketJob.thisThreadInQueue());
        
        // Just in case logical error
        if (m_doReadingData) {
            DLogError("Already reading\n");
            return ;
        }
        m_doReadingData = true;
        
        DLogVerbose("Do read data\n");
        while (innnerReadData()) {
            ;
        }
        m_doReadingData = false;
    }
    
    bool AsyncStreamSession::innnerReadData(){
        assert(m_socketJob.thisThreadInQueue());

        auto currentReader = getCurrentReader();
        if (currentReader) {
            // 从已读buffer中读取
            if (m_inputBuffer.availableBytes() > 0) {
                DLogVerbose("Read from prebuffer\n");
                currentReader->readFromBuffer(m_inputBuffer);
            }else {
                DLogVerbose("No data in prebuffer\n");
            }
            if (currentReader->done()) {
                finishCurrentReader();
                return true;
            }
            if (m_stream->status() & kStreamStatusReadBufferHasBytes) {
                // 从SOCKET读取
                DLogVerbose("Reading from stream\n");
                ssize_t result = currentReader->readFromStream(m_stream.get());
                if (result > 0) {
                    DLogVerbose("Read %ld bytes from stream\n", result);
                }
                else {
                    DLogVerbose("ERROR! Read from stream error:%ld\n", result);
                }
            }
            else {
                DLogVerbose("No data in current stream\n");
            }
            if (currentReader->done()) {
                finishCurrentReader();
                return true;
            }
        }
        else {
            DLogVerbose("No reader currently\n");
            if (m_stream->status() & kStreamStatusReadBufferHasBytes) {
                // read the SOCKET into pre buffer ?
                size_t availibleSize = m_inputBuffer.availableSpace();
                DLogVerbose("Try reading from stream\n");
                ssize_t result = m_stream->read(m_inputBuffer.writeBuffer(), availibleSize);
                if (result > 0) {
                    m_inputBuffer.didWrite(result);
                    DLogVerbose("Read %ld bytes into prebuffer\n", result);
                }
                else {
                    DLogVerbose("ERROR! read from stream error:%ld\n", result);
                }
            }
            else {
                DLogVerbose("No data in current stream\n");
            }
        }
        return false;
    }
    
    void AsyncStreamSession::finishCurrentReader(){
        assert(m_socketJob.thisThreadInQueue());

        auto currentReader = getCurrentReader();
        assert(currentReader);
        
        if (currentReader) {
            DLogVerbose("Reader done\n");
            m_readerQueue.pop();
            m_eventTriggerJob.enqueue([=]{
                currentReader->triggerEvent();
            });
        }
    }

    
#pragma mark -
#pragma mark - Writing
    std::shared_ptr<AsyncStreamWriter> AsyncStreamSession::getCurrentWriter() {
        assert(m_socketJob.thisThreadInQueue());
        if (m_writerQueue.size() > 0) {
            return m_writerQueue.front();
        }
        return nullptr;
    }
    void AsyncStreamSession::doWriteData(){
        assert(m_socketJob.thisThreadInQueue());
        
        DLogVerbose("Do write data\n");
        while (m_stream->status() & kStreamStatusWriteBufferHasSpace && m_writerQueue.size() > 0) {
            innerWriteData();
        }
    }
    void AsyncStreamSession::innerWriteData(){
        assert(m_socketJob.thisThreadInQueue());
        
        if (m_stream->status() & kStreamStatusWriteBufferHasSpace) {
            auto writer = getCurrentWriter();
            if (writer) {
                DLogVerbose("Writing data to stream\n");
                ssize_t result = writer->writeToStream(m_stream.get());
                if (result <= 0) {
                    DLogError("ERROR! Write to stream error:%ld\n", result);
                }
                else {
                    DLogVerbose("Wrote %ld bytes to stream\n", result);
                }
                if (writer->done()) {
                    finishCurrentWriter();
                }
            }            
        }
        else {
            DLogVerbose("Stream can't write now\n");
        }
    }
    
    void AsyncStreamSession::finishCurrentWriter() {
        assert(m_socketJob.thisThreadInQueue());
        
        auto currentWriter = getCurrentWriter();
        assert(currentWriter);
        
        if (currentWriter) {
            m_writerQueue.pop();
            DLogVerbose("Writer done\n");
            m_eventTriggerJob.enqueue([=]{
                currentWriter->triggerEvent();
            });
        }
    }
}