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
#pragma mark PreallocBuffer

    /**
     *  Pre-allocated buffer. Design for get the raw pointer for read and write
     *  Must alloc enough space before writing
     *  Must check how many byte in buffer before reading
     */
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
        DLogVerbose("PreallocBuffer::~PreallocBuffer\n");
    }
    
    /**
     *  Make sure buffer has enough size for write
     *
     *  @param capBytes ensure size
     */
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
    
    /**
     *  Check how much bytes can be read
     *
     *  @return read size
     */
    size_t PreallocBuffer::availableBytes(){
        return m_writePointer - m_readPointer;
    }
    
    /**
     *  Raw read pointer
     *
     */
    uint8_t *PreallocBuffer::readBuffer(){
        return m_readPointer;
    }
    

    /**
     *  Get raw pointer and availble size for read
     *
     *  @param bufferPtr         raw read pointer
     *  @param availableBytesPtr how many bytes can read
     */
    void PreallocBuffer::getReadBuffer(uint8_t **bufferPtr, size_t *availableBytesPtr){
        if (bufferPtr) *bufferPtr = m_readPointer;
        if (availableBytesPtr) *availableBytesPtr = availableBytes();
    }
    
    /**
     *  Confirm read
     *
     */
    void PreallocBuffer::didRead(size_t bytesRead){
        m_readPointer += bytesRead;
        
        assert(m_readPointer <= m_writePointer);
        
        if (m_readPointer == m_writePointer)
        {
            // 缓冲区都被读走了，可以重置了
            reset();
        }
    }
    
    /**
     *  How many space for write
     *
     */
    size_t PreallocBuffer::availableSpace(){
        return m_preBufferSize - (m_writePointer - m_preBuffer);
    }
    
    /**
     *  Raw write pointer
     *
     */
    uint8_t *PreallocBuffer::writeBuffer(){
        return m_writePointer;
    }
    
    /**
     *  Get raw pointer and availble size for write
     *
     *  @param bufferPtr         raw write pointer
     *  @param availableSpacePtr how many bytes can write
     */
    void PreallocBuffer::getWriteBuffer(uint8_t **bufferPtr, size_t *availableSpacePtr){
        if (bufferPtr) *bufferPtr = m_writePointer;
        if (availableSpacePtr) *availableSpacePtr = availableSpace();
    }
    
    /**
     *  Confirm written
     *
     */
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
         *  Read is asynchronous, when the
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
            if (m_eventCallback) {
                m_eventCallback(*m_buffer);
            }
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
    }
    
    void AsyncStreamSession::connect(const std::string& host, int port, SSConnectionStatus_T statuscb) {
        m_connectionStatusCB = statuscb;
        m_state = kAsyncStreamStateConnecting;
        m_stream->connect(host, port, [=](IStreamSession& session, StreamStatus_T status){
            // 检查自己的Connected状态，避免重复
            m_socketJob.enqueue([=]{
                if (status & kStreamStatusConnected && m_state < kAsyncStreamStateConnected) {
                    DLog("AnsyncStreamSession connected\n");
                    setState(kAsyncStreamStateConnected);
                    doWriteData();
                    doReadData();
                }
                if (status & kStreamStatusWriteBufferHasSpace) {
                    DLog("AnsyncStreamSession Write ready\n");
                    doWriteData();
                }
                if (status & kStreamStatusReadBufferHasBytes) {
                    DLog("AnsyncStreamSession Read ready\n");
                    doReadData();
                }
                if(status & kStreamStatusErrorEncountered) {
                    DLog("AnsyncStreamSession Error!\n");
                    setState(kAsyncStreamStateError);
                }
                if (status & kStreamStatusEndStream) {
                    DLog("AnsyncStreamSession end.\n")
                    setState(kAsyncStreamStateDisconnected);
                }
            });
        });
    }
    
    void AsyncStreamSession::disconnect(){
        setState(kAsyncStreamStateDisconnecting);
        m_stream->disconnect();
    }
    
    void AsyncStreamSession::write(uint8_t *buffer, size_t length, SSAnsyncWriteCallBack_T writecb){
        auto bufsp = std::make_shared<AsyncStreamBuffer>(buffer, buffer+length);
        // 因为Job是异步处理过程，需要自己小心处理引用计数
        m_socketJob.enqueue([=]{
            auto writer = std::make_shared<AsyncStreamWriter>(bufsp, writecb);
            m_writerQueue.push(writer);
            DLog("Queue write request:%ld\n", length);
            doWriteData();
        });
    }
    
    void AsyncStreamSession::readLength(size_t length, SSAnsyncReadCallBack_T readcb){
        readMoreLength(length, nullptr, 0, readcb);
    }
    
    void AsyncStreamSession::readMoreLength(size_t length, AsyncStreamBufferSP orgbuf, size_t offset, SSAnsyncReadCallBack_T readcb){
        assert(!m_socketJob.thisThreadInQueue());
        
        m_socketJob.enqueue([=]{
            DLog("Want to read length:%zd\n", length);
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
        
        if (m_doReadingData) {
            DLog("DEBUG, Already reading\n");
            return ;
        }
        // 防止某些情况的递归重入
        // FIXME: 梳理调用逻辑，避免递归调用。一个可能的地方是
        m_doReadingData = true;
        DLog("Do read data\n");
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
    
    void AsyncStreamSession::finishCurrentReader(){
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
    std::shared_ptr<AsyncStreamWriter> AsyncStreamSession::getCurrentWriter() {
        assert(m_socketJob.thisThreadInQueue());
        if (m_writerQueue.size() > 0) {
            return m_writerQueue.front();
        }
        return nullptr;
    }
    void AsyncStreamSession::doWriteData(){
        assert(m_socketJob.thisThreadInQueue());
        DLog("Do write data\n");
        while (m_stream->status() & kStreamStatusWriteBufferHasSpace && m_writerQueue.size() > 0) {
            innerWriteData();
        }
    }
    void AsyncStreamSession::innerWriteData(){
        assert(m_socketJob.thisThreadInQueue());
        
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
                    finishCurrentWriter();
                }
            }            
        }
    }
    
    void AsyncStreamSession::finishCurrentWriter() {
        assert(m_socketJob.thisThreadInQueue());
        
        auto currentWriter = getCurrentWriter();
        assert(currentWriter);
        if (currentWriter) {
            m_writerQueue.pop();
            DLog("Writer done\n");
            m_eventTriggerJob.enqueue([=]{
                currentWriter->triggerEvent();
            });
        }
    }
}