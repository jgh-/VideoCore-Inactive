//
//  SampleImageTransform.h
//  SampleBroadcaster
//
//  Created by James Hurley on 6/8/14.
//  Copyright (c) 2014 videocore. All rights reserved.
//

#ifndef __SampleBroadcaster__SampleImageTransform__
#define __SampleBroadcaster__SampleImageTransform__

#include <iostream>
#include <videocore/transforms/ITransform.hpp>

namespace videocore { namespace sample {

    class SampleImageTransform : public ITransform
    {
    public:
        SampleImageTransform();
        ~SampleImageTransform();
        
        void setEpoch(const std::chrono::steady_clock::time_point epoch) { m_epoch = epoch; };
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
        void setOutput(std::shared_ptr<IOutput> output);
        
    public:
        
        void startSpinning();
        void stopSpinning();
        
    private:
        std::chrono::steady_clock::time_point m_epoch;
        std::weak_ptr<IOutput> m_output;
        
        bool m_spinning;
        
    };
    
}
}
#endif /* defined(__SampleBroadcaster__SampleImageTransform__) */
