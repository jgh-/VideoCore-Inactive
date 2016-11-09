//
//  PreBuffer.hpp
//  Pods
//
//  Created by jinchu darwin on 15/10/10.
//
//

#ifndef PreBuffer_hpp
#define PreBuffer_hpp

#include <string>

namespace videocore {
    class PreallocBuffer {
    public:
        PreallocBuffer(size_t capBytes);
        ~PreallocBuffer();
        
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
        
        void dumpInfo();
        
    private:
        uint8_t *m_preBuffer;
        size_t m_preBufferSize;
        
        uint8_t *m_readPointer;
        uint8_t *m_writePointer;
    };
}
#endif /* PreBuffer_hpp */

