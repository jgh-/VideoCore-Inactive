//
//  PixelBufferOutput.h
//  SampleBroadcaster
//
//  Created by James Hurley on 5/6/14.
//  Copyright (c) 2014 videocore. All rights reserved.
//

#ifndef __SampleBroadcaster__PixelBufferOutput__
#define __SampleBroadcaster__PixelBufferOutput__

#include <iostream>
#include <videocore/transforms/IOutput.hpp>

namespace videocore { namespace sample {
    
    using PixelBufferCallback = std::function<void(const uint8_t* const data, size_t size)> ;
    
    class PixelBufferOutput : public IOutput
    {
    public:
        PixelBufferOutput(PixelBufferCallback callback) ;
        
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata) ;
        
    private:
        
        PixelBufferCallback m_callback;
    };
}
}
#endif /* defined(__SampleBroadcaster__PixelBufferOutput__) */
