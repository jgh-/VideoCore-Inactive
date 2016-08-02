//
//  PreBuffer.cpp
//  Pods
//
//  Created by jinchu darwin on 15/10/10.
//
//

#include "PreBuffer.hpp"

#include <videocore/system/util.h>


#include <cstdlib>
#include <cassert>

namespace videocore {
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
    
    void PreallocBuffer::dumpInfo(){
        DLog("PreallocBuffer begin:%p, writer:%p(%zd), reader:%p(%zd)\n", m_preBuffer, m_writePointer, availableSpace(), m_readPointer, availableBytes());
    }

}
