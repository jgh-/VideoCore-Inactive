//
//  PixelBufferOutput.cpp
//  SampleBroadcaster
//
//  Created by James Hurley on 5/6/14.
//  Copyright (c) 2014 videocore. All rights reserved.
//

#include "PixelBufferOutput.h"

namespace videocore { namespace sample {
 
    PixelBufferOutput::PixelBufferOutput(PixelBufferCallback callback)
    : m_callback(callback)
    {
        
    }
    
    void
    PixelBufferOutput::pushBuffer(const uint8_t *const data, size_t size, videocore::IMetadata &metadata)
    {
        if(m_callback) {
            m_callback(data, size);
        }
    }
}
}