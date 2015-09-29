//
//  AnsyncStreamSession.cpp
//  Pods
//
//  Created by jinchu darwin on 15/9/29.
//
//

#include <cstdlib>
#include <cassert>

#include "AnsyncStreamSession.hpp"


namespace videocore {
#pragma mark -
#pragma mark AnsyncBuffer

    // 简单的缓冲区
    // 写入之间应该分配足够的空间，然后拿到裸指针写入
    // 读之前应该获得可读的长度，然后获得裸指针写入
    AnsyncBuffer::AnsyncBuffer(size_t capBytes){
        preBufferSize = capBytes;
        preBuffer = (uint8_t *)malloc(preBufferSize);
        
        readPointer = preBuffer;
        writePointer = preBuffer;

    }
    
    AnsyncBuffer::~AnsyncBuffer(){
        if (preBuffer) {
            free(preBuffer);
        }
    }
    
    void AnsyncBuffer::ensureCapacityForWrite(size_t capBytes){
        size_t availableSpace = this->availableSpace();
        if (capBytes > availableSpace) {
            size_t additionalBytes = capBytes - availableSpace;
            size_t newPreBufferSize = preBufferSize + additionalBytes;
            uint8_t *newPreBuffer = (uint8_t *)realloc(preBuffer, newPreBufferSize);
            
            size_t readPointerOffset = readPointer - preBuffer;
            size_t writePointerOffset = writePointer - preBuffer;
            
            preBuffer = newPreBuffer;
            preBufferSize = newPreBufferSize;
            readPointer = preBuffer + readPointerOffset;
            writePointer = preBuffer + writePointerOffset;
        }
    }
    
    size_t AnsyncBuffer::availableBytes(){
        return writePointer - readPointer;
    }
    
    uint8_t *AnsyncBuffer::readBuffer(){
        return readPointer;
    }
    
    void AnsyncBuffer::getReadBuffer(uint8_t **bufferPtr, size_t *availableBytesPtr){
        if (bufferPtr) *bufferPtr = readPointer;
        if (availableBytesPtr) *availableBytesPtr = availableBytes();
    }
    
    void AnsyncBuffer::didRead(size_t bytesRead){
        readPointer += bytesRead;
        
        assert(readPointer <= writePointer);
        
        if (readPointer == writePointer)
        {
            // The prebuffer has been drained. Reset pointers.
            readPointer  = preBuffer;
            writePointer = preBuffer;
        }
    }
    
    size_t AnsyncBuffer::availableSpace(){
        return preBufferSize - (writePointer - preBuffer);
    }
    
    uint8_t *AnsyncBuffer::writeBuffer(){
        return writePointer;        
    }
    
    void AnsyncBuffer::getWriteBuffer(uint8_t **bufferPtr, size_t *availableSpacePtr){
        if (bufferPtr) *bufferPtr = writePointer;
        if (availableSpacePtr) *availableSpacePtr = availableSpace();
    }
    
    void AnsyncBuffer::didWrite(size_t bytesWritten){
        writePointer += bytesWritten;
        assert(writePointer <= (preBuffer + preBufferSize));
    }
    
    void AnsyncBuffer::reset(){
        readPointer  = preBuffer;
        writePointer = preBuffer;
    }
    
#pragma mark -
#pragma mark AnsyncStreamSession
    class AnsyncReadConsumer {
    public:
        AnsyncReadConsumer(AsyncReadBuffer& orgbuf, size_t length, AnsyncReadCallBack_T eventcb){
            bytesDone = 0;
            
            // TODO: make sure this move!
            buffer = std::move(orgbuf);
            
            startOffset = buffer.size();
            bufferOwner = false;
            readLength = length;
            eventCallback = eventcb;
        }
        AnsyncReadConsumer(size_t length, AnsyncReadCallBack_T eventcb){
            bytesDone = 0;
            
            startOffset = 0;
            bufferOwner = true;
            readLength = length;
            eventCallback = eventcb;
        }
        
        // 从缓冲区复制数据过来
        void readFromBuffer(AnsyncBuffer abuff) {
            size_t availableBytes = abuff.availableBytes();
            size_t bytesToCopy = readLengthIfAvailable(availableBytes);
            resizeForRead(bytesToCopy);
            memcpy(&buffer[startOffset+bytesDone], abuff.readBuffer(), bytesToCopy);
            abuff.didRead(bytesToCopy);
            
            bytesDone += bytesToCopy;
        }
        
        // 直接从流读取数据
        size_t readFromStream(IStreamSession *stream) {
            size_t bytesToRead = readLengthRemains();
            resizeForRead(bytesToRead);
            ssize_t result = stream->read(&buffer[startOffset+bytesDone], bytesToRead);
            if (result > 0) {
                bytesToRead += result;
            }
            return result;
        }
        bool done() {
            return readLength == bytesDone;
        }
        void triggerEvent() {
            eventCallback(buffer);
        }
        
    private:
        // 如果可以从SOCKET读取bytesAvailable，需要多少自己就能满足次消费
        size_t readLengthIfAvailable(size_t bytesAvailable) {
            return std::min(bytesAvailable, readLengthRemains());
        }
        
        // 要需要读取多少才合适
        size_t readLengthRemains() {
            return readLength - bytesDone;
        }
        
        // 调整合适的缓冲区大小以便可以直接写入
        void resizeForRead(size_t bytesToRead) {
            size_t buffSize = buffer.size();
            size_t buffUsed = startOffset + bytesDone;
            
            size_t buffSpace = buffSize - buffUsed;
            
            if (bytesToRead > buffSpace)
            {
                size_t buffInc = bytesToRead - buffSpace;
                DLog("DEBUG: resize for read");
                buffer.resize(buffInc);
            }
        }
        
    private:
        AsyncReadBuffer buffer;
        size_t startOffset;
        size_t bytesDone;
        size_t readLength;
        bool bufferOwner;
        AnsyncReadCallBack_T eventCallback;
    };
#pragma mark -
#pragma mark AnsyncStreamSession
    AnsyncStreamSession::AnsyncStreamSession(IStreamSession *stream)
    : m_state(kAsyncStreamStateNone)
    , m_outputBuffer(10*1024*1024)
    , m_inputBuffer(4*1024)
    , m_pumpingReader(false)
    {
        m_stream.reset(stream);
    }
    AnsyncStreamSession::~AnsyncStreamSession(){
        
    }
    
    void AnsyncStreamSession::connect(const std::string& host, int port, SSConnectionStatus_T statuscb) {
        m_connectionStatusCB = statuscb;
        m_state = kAsyncStreamStateConnecting;
        m_stream->connect(host, port, [=](IStreamSession& session, StreamStatus_t status){
            if (status & kStreamStatusConnected && m_state < kAsyncStreamStateConnected) {
                DLog("Ansync stream connected\n");
                setState(kAsyncStreamStateConnected);
                pumpReader();
            }
            if (status & kStreamStatusWriteBufferHasSpace) {
                DLog("Write ready\n");
            }
            if (status & kStreamStatusReadBufferHasBytes) {
                pumpReader();
                DLog("read ready\n");
            }
            if(status & kStreamStatusErrorEncountered) {
                setState(kAsyncStreamStateError);
            }
            if (status & kStreamStatusEndStream) {
                setState(kAsyncStreamStateDisconnected);
            }
        });
    }
    
    void AnsyncStreamSession::disconnect(){
        setState(kAsyncStreamStateDisconnecting);
        m_stream->disconnect();
    }
    void AnsyncStreamSession::write(uint8_t *buffer, size_t length){
        // FIXME: put into job queue
        m_stream->write(buffer, length);
    }
    
    void AnsyncStreamSession::readLength(size_t length, AnsyncReadCallBack_T readcb){
        std::shared_ptr<AnsyncReadConsumer> reader(new AnsyncReadConsumer(length, readcb));
        m_readerQueue.push(reader);
        maybeDequeueReader();
    }

#pragma mark -
#pragma mark - Private
    void AnsyncStreamSession::setState(AnsyncStreamState_T state) {
        m_state = state;
        m_connectionStatusCB(m_state);
    }
    
    void AnsyncStreamSession::maybeDequeueReader(){
        if (!m_currentReader && m_readerQueue.size() > 0) {
            DLog("Dequeue reader\n");
            m_currentReader = m_readerQueue.front();
            m_readerQueue.pop();
        }
    }
    
    void AnsyncStreamSession::pumpReader(){
        maybeDequeueReader();
        if (!m_pumpingReader) {
            m_pumpingReader = true;
            DLog("Pump reader\n");
            while (innerPumpReader()) {
                ;
            }
        }
        m_pumpingReader = false;
    }
    
    bool AnsyncStreamSession::innerPumpReader(){
        if (m_currentReader) {
            DLog("Read from stream");
            m_currentReader->readFromStream(m_stream.get());
            if (m_currentReader->done()) {
                DLog("Reader done");
                m_currentReader->triggerEvent();
                maybeDequeueReader();
                return true;
            }
        }
        return false;
    }
    
    void pumpWriter(){
        
    }
    
}