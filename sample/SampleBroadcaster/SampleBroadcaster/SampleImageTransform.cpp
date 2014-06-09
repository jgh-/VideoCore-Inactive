//
//  SampleImageTransform.cpp
//  SampleBroadcaster
//
//  Created by James Hurley on 6/8/14.
//  Copyright (c) 2014 videocore. All rights reserved.
//

#include "SampleImageTransform.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <videocore/mixers/IVideoMixer.hpp>

namespace videocore { namespace sample {
 
    SampleImageTransform::SampleImageTransform()
    :  m_spinning(0)
    {
        
    }
    SampleImageTransform::~SampleImageTransform()
    {
        
    }
    void
    SampleImageTransform::setOutput(std::shared_ptr<IOutput> output)
    {
        m_output = output;
    }
    void
    SampleImageTransform::pushBuffer(const uint8_t *const data, size_t size, videocore::IMetadata &metadata)
    {
        if(m_spinning) {
            const int period = 66 * 30 ; // 2 seconds
            auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_epoch).count();
            float time = float(millis % period) / float(period);
            
            videocore::VideoBufferMetadata& md = dynamic_cast<videocore::VideoBufferMetadata&>(metadata);
            glm::mat4 & mat = md.getData<videocore::kVideoMetadataMatrix>();
            glm::vec3 rotation(0.f,0.f,1.f);
            float angle = time * 360.f;
            mat = glm::rotate(mat,  angle, rotation);
            
        }
        auto output = m_output.lock();
        if(output) {
            output->pushBuffer(data, size, metadata);
        }
    }
    void
    SampleImageTransform::startSpinning()
    {
        if(!m_spinning) {
            setEpoch(std::chrono::steady_clock::now());
            m_spinning = true;
        }
    }
    void
    SampleImageTransform::stopSpinning()
    {
        m_spinning = false;
    }
}
}