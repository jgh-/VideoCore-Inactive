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
#pragma mark PreallocBuffer

    // 简单的缓冲区
    // 写入之前应该分配足够的空间，然后拿到裸指针写入
    // 读之前应该获得可读的长度，然后获得裸指针读取
    // 这个类设计成返回裸指针是为了方便各种不同形式的写入，比如需要拿到一个裸指针给SOCKET写入,etc...
    // 因此在写入（读取）完毕之后必须记得调用响应的确认函数
    PreallocBuffer::PreallocBuffer(size_t capBytes){
        m_preBufferSize = capBytes;
        m_preBuffer = (uint8_t *)malloc(m_preBufferSize);
        
        m_readPointer = m_preBuffer;
        m_writePointer = m_preBuffer;

    }
    
    PreallocBuffer::~PreallocBuffer(){
        if (m_preBuffer) {
            free(m_preBuffer);
        }
        DLog("PreallocBuffer::~PreallocBuffer\n");
    }
    
    void PreallocBuffer::ensureCapacityForWrite(size_t capBytes){
        size_t availableSpace = this->availableSpace();
        if (capBytes > availableSpace) {
            size_t additionalBytes = capBytes - availableSpace;
            size_t newPreBufferSize = m_preBufferSize + additionalBytes;
            uint8_t *newPreBuffer = (uint8_t *)realloc(m_preBuffer, newPreBufferSize);
            
            size_t readPointerOffset = m_readPointer - m_preBuffer;
            size_t writePointerOffset = m_writePointer - m_preBuffer;
            
            m_preBuffer = newPreBuffer;
            m_preBufferSize = newPreBufferSize;
            m_readPointer = m_preBuffer + readPointerOffset;
            m_writePointer = m_preBuffer + writePointerOffset;
        }
    }
    
    // 检查有多少字节可以读取
    size_t PreallocBuffer::availableBytes(){
        return m_writePointer - m_readPointer;
    }
    
    uint8_t *PreallocBuffer::readBuffer(){
        return m_readPointer;
    }
    
    // 获取读取指针以及可读取字节数
    void PreallocBuffer::getReadBuffer(uint8_t **bufferPtr, size_t *availableBytesPtr){
        if (bufferPtr) *bufferPtr = m_readPointer;
        if (availableBytesPtr) *availableBytesPtr = availableBytes();
    }
    
    // 确认读取
    void PreallocBuffer::didRead(size_t bytesRead){
        m_readPointer += bytesRead;
        
        assert(m_readPointer <= m_writePointer);
        
        if (m_readPointer == m_writePointer)
        {
            // 缓冲区都被读走了，可以重置了
            reset();
        }
    }
    
    // 检查有多少可写空间
    size_t PreallocBuffer::availableSpace(){
        return m_preBufferSize - (m_writePointer - m_preBuffer);
    }
    
    uint8_t *PreallocBuffer::writeBuffer(){
        return m_writePointer;
    }
    
    // 获取写入指针以及可以写入的字节数
    void PreallocBuffer::getWriteBuffer(uint8_t **bufferPtr, size_t *availableSpacePtr){
        if (bufferPtr) *bufferPtr = m_writePointer;
        if (availableSpacePtr) *availableSpacePtr = availableSpace();
    }
    
    // 确认写入
    void PreallocBuffer::didWrite(size_t bytesWritten){
        m_writePointer += bytesWritten;
        assert(m_writePointer <= (m_preBuffer + m_preBufferSize));
    }
    
    void PreallocBuffer::reset(){
        m_readPointer  = m_preBuffer;
        m_writePointer = m_preBuffer;
    }
    
#pragma mark -
#pragma mark AnsyncStreamReader
    // 抽象表达一个读取请求（称为一次读取消费）
    // 可以指定只有获得了多少数据之后才有时间返回
    class AnsyncStreamReader {
    public:
        // 使用者自己拥有buffer
        // 对于一些分包的协议，如果已经知道了包长度，那么使用者可以预先分配buffer的大小，然后按照分包协议多次读取一定长度的分包数据合并成一个独立的buffer
        // 这样可以避免多次的重新分配buffer空间
        AnsyncStreamReader(size_t length, AsyncStreamBufferSP buf, size_t offset, SSAnsyncReadCallBack_T eventcb){
            m_bytesDone = 0;
            
            m_buffer = buf;
            
            m_startOffset = offset;
            m_bufferOwner = false;
            m_readLength = length;
            m_eventCallback = eventcb;
        }
        AnsyncStreamReader(size_t length, SSAnsyncReadCallBack_T eventcb){
            m_bytesDone = 0;
            
            m_buffer = std::make_shared<AsyncStreamBuffer>(length);
            m_startOffset = 0;
            m_bufferOwner = true;
            m_readLength = length;
            m_eventCallback = eventcb;
        }
        
        ~AnsyncStreamReader() {
            DLog("AnsyncReadConsumer::~AnsyncReadConsumer\n");
        }
        
        // 从缓冲区复制数据过来，此方法会改变参数的读取状态
        void readFromBuffer(PreallocBuffer &abuff) {
            size_t availableBytes = abuff.availableBytes();
            size_t bytesToCopy = readLengthIfAvailable(availableBytes);
            resizeForRead(bytesToCopy);
            memcpy(&(*m_buffer)[m_startOffset+m_bytesDone], abuff.readBuffer(), bytesToCopy);
            abuff.didRead(bytesToCopy);
            
            m_bytesDone += bytesToCopy;
        }
        
        // 直接从流读取数据
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
            m_eventCallback(*m_buffer);
        }
        
    private:
        // 如果可以从SOCKET读取bytesAvailable，需要多少自己就能满足次消费
        size_t readLengthIfAvailable(size_t bytesAvailable) {
            return std::min(bytesAvailable, readLengthRemains());
        }
        
        // 要需要读取多少才合适
        size_t readLengthRemains() {
            return m_readLength - m_bytesDone;
        }
        
        // 调整合适的缓冲区大小以便可以直接写入
        void resizeForRead(size_t bytesToRead) {
            size_t buffSize = m_buffer->size();
            size_t buffUsed = m_startOffset + m_bytesDone;
            
            size_t buffSpace = buffSize - buffUsed;
            
            if (bytesToRead > buffSpace)
            {
                size_t buffInc = bytesToRead - buffSpace;
                DLog("DEBUG: resize for read");
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
#pragma mark AnsyncStreamWriter
    class AnsyncStreamWriter {
    public:
        AnsyncStreamWriter(AsyncStreamBufferSP buf) {
            m_bytesDone = 0;
            m_buffer = buf;
        }
        ~AnsyncStreamWriter() {
            DLog("AnsyncStreamWriter::~AnsyncStreamWriter\n");
        }
        ssize_t writeToStream(IStreamSession *stream) {
            size_t bytesToWrite = m_buffer->size() - m_bytesDone;
            ssize_t result = stream->write(&(*m_buffer)[m_bytesDone], bytesToWrite);
            if (result > 0) {
                m_bytesDone += result;
            }
            return result;
        }
        bool done() {
            return m_bytesDone == m_buffer->size();
        }
    private:
        AsyncStreamBufferSP m_buffer;
        size_t m_bytesDone;
    };
    
#pragma mark -
#pragma mark AnsyncStreamSession
    AnsyncStreamSession::AnsyncStreamSession(IStreamSession *stream)
    : m_state(kAsyncStreamStateNone)
    , m_inputBuffer(4*1024)
    , m_socketJob("AnsyncStreamSession Socket")
    , m_eventTriggerJob("AnsyncStreamSession Event Trigger")
    , m_doReadingData(false)
    {
        m_stream.reset(stream);
    }
    AnsyncStreamSession::~AnsyncStreamSession(){
        m_socketJob.mark_exiting();
        m_socketJob.enqueue_sync([=]{});
    }
    
    void AnsyncStreamSession::connect(const std::string& host, int port, SSConnectionStatus_T statuscb) {
        m_connectionStatusCB = statuscb;
        m_state = kAsyncStreamStateConnecting;
        m_stream->connect(host, port, [=](IStreamSession& session, StreamStatus_t status){
            // 检查自己的Connected状态，避免重复
            m_socketJob.enqueue([=]{
                if (status & kStreamStatusConnected && m_state < kAsyncStreamStateConnected) {
                    DLog("Ansync stream connected\n");
                    setState(kAsyncStreamStateConnected);
                    doWriteData();
                    doReadData();
                }
                if (status & kStreamStatusWriteBufferHasSpace) {
                    DLog("Write ready\n");
                    doWriteData();
                }
                if (status & kStreamStatusReadBufferHasBytes) {
                    DLog("Read ready\n");
                    doReadData();
                }
                if(status & kStreamStatusErrorEncountered) {
                    setState(kAsyncStreamStateError);
                }
                if (status & kStreamStatusEndStream) {
                    setState(kAsyncStreamStateDisconnected);
                }
            });
        });
    }
    
    void AnsyncStreamSession::disconnect(){
        setState(kAsyncStreamStateDisconnecting);
        m_stream->disconnect();
    }
    
    void AnsyncStreamSession::write(uint8_t *buffer, size_t length){
        m_socketJob.enqueue([=]{
            auto writer = std::make_shared<AnsyncStreamWriter>(std::make_shared<AsyncStreamBuffer>(buffer, buffer+length));
            m_writerQueue.push(writer);
            DLog("Queue write request:%ld\n", length);
            doWriteData();
        });
    }
    
    void AnsyncStreamSession::readLength(size_t length, SSAnsyncReadCallBack_T readcb){
        // 不能在相同的队列里发起请求，否则死锁呢。
        assert(!m_socketJob.thisThreadInQueue());
        
        m_socketJob.enqueue([=]{
            DLog("Want to read length:%zd\n", length);
            auto reader = std::make_shared<AnsyncStreamReader>(length, readcb);
            m_readerQueue.push(reader);
            doReadData();
        });
    }

#pragma mark -
#pragma mark - Private
    void AnsyncStreamSession::setState(AnsyncStreamState_T state) {
        assert(m_socketJob.thisThreadInQueue());

        m_state = state;
        m_eventTriggerJob.enqueue([=]{
            m_connectionStatusCB(m_state);
        });
    }
    
#pragma mark -
#pragma mark - Reading
    std::shared_ptr<AnsyncStreamReader> AnsyncStreamSession::getCurrentReader() {
        assert(m_socketJob.thisThreadInQueue());

        if (m_readerQueue.size() > 0) {
            return m_readerQueue.front();
        }
        return nullptr;
    }
    
    void AnsyncStreamSession::doReadData(){
        assert(m_socketJob.thisThreadInQueue());
        
        if (m_doReadingData) {
            DLog("DEBUG, Already reading\n");
            return ;
        }
        // 防止某些情况的递归重入
        // FIXME: 梳理调用逻辑，避免递归调用。一个可能的地方是
        m_doReadingData = true;
        DLog("Do read data\n");
//        auto currentReader = getCurrentReader();
//        while(currentReader) {
//            if (currentReader) {
//                innnerReadData();
//            }
//            currentReader = getCurrentReader();
//        }
        while (innnerReadData()) {
            ;
        }
        m_doReadingData = false;
    }
    
    bool AnsyncStreamSession::innnerReadData(){
        assert(m_socketJob.thisThreadInQueue());

        auto currentReader = getCurrentReader();
        if (currentReader) {
            // 从已读buffer中读取
            if (m_inputBuffer.availableBytes() > 0) {
                DLog("Read from prebuffer\n");
                currentReader->readFromBuffer(m_inputBuffer);
            }else {
                DLog("No data in prebuffer\n");
            }
            if (currentReader->done()) {
                finishCurrentReader();
                return true;
            }
            if (m_stream->status() & kStreamStatusReadBufferHasBytes) {
                // 从SOCKET读取
                DLog("Reading from stream\n");
                ssize_t result = currentReader->readFromStream(m_stream.get());
                if (result > 0) {
                    DLog("Read %ld bytes from stream\n", result);
                }
                else {
                    DLog("ERROR! Read from stream error:%ld\n", result);
                }
            }
            else {
                DLog("No data in current stream\n");
            }
            if (currentReader->done()) {
                finishCurrentReader();
                return true;
            }
        }
        else {
            DLog("No reader currently\n");
            if (m_stream->status() & kStreamStatusReadBufferHasBytes) {
                // read the SOCKET into pre buffer ?
                size_t availibleSize = m_inputBuffer.availableSpace();
                DLog("Try reading from stream\n");
                ssize_t result = m_stream->read(m_inputBuffer.writeBuffer(), availibleSize);
                if (result > 0) {
                    m_inputBuffer.didWrite(result);
                    DLog("Read %ld bytes into prebuffer\n", result);
                }
                else {
                    DLog("ERROR! read from stream error:%ld\n", result);
                }
            }
            else {
                DLog("No data in current stream\n");
            }
        }
        return false;
    }
    
    void AnsyncStreamSession::finishCurrentReader(){
        assert(m_socketJob.thisThreadInQueue());

        auto currentReader = getCurrentReader();
        assert(currentReader);
        
        if (currentReader) {
            DLog("Reader done\n");
            m_readerQueue.pop();
            // trigger event in other queue?
            m_eventTriggerJob.enqueue([=]{
                currentReader->triggerEvent();
            });
        }
    }

    
#pragma mark -
#pragma mark - Writing
    std::shared_ptr<AnsyncStreamWriter> AnsyncStreamSession::getCurrentWriter() {
        assert(m_socketJob.thisThreadInQueue());
        if (m_writerQueue.size() > 0) {
            return m_writerQueue.front();
        }
        return nullptr;
    }
    
    void AnsyncStreamSession::doWriteData(){
        assert(m_socketJob.thisThreadInQueue());
        DLog("Do write data\n");
        if (m_stream->status() & kStreamStatusWriteBufferHasSpace) {
            auto writer = getCurrentWriter();
            if (writer) {
                DLog("Writing data to stream\n");
                ssize_t result = writer->writeToStream(m_stream.get());
                if (result <= 0) {
                    DLog("ERROR! Write to stream error:%ld\n", result);
                }
                else {
                    DLog("Wrote %ld bytes to stream\n", result);
                }
                if (writer->done()) {
                    DLog("Writer done\n");
                    m_writerQueue.pop();
                }
            }
            
        }
    }
}