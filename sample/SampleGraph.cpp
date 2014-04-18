
#include "SampleGraph.h"

#ifdef __APPLE__
#   include <videocore/mixers/Apple/AudioMixer.h>
#   ifdef TARGET_OS_IPHONE
#       include <videocore/sources/iOS/CameraSource.h>
#       include <videocore/sources/iOS/MicSource.h>
#       include <videocore/mixers/iOS/GLESVideoMixer.h>
#       include <videocore/transforms/iOS/AACEncode.h>
#       include <videocore/transforms/iOS/H264Encode.h>

#   else /* OS X */

#   endif
#else
#   include <videocore/mixers/GenericAudioMixer.h>

namespace videocore { namespace sample {

    void
    SampleGraph::startRtmpSession(std::string uri, int frame_w, int frame_h, int bitrate, int fps)
    {

        m_outputSession.reset( new videocore::RTMPSession ( uri, [=](videocore::RTMPSession& session, ClientState_t state) {
            
            switch(state) {
                    
                case kClientStateSessionStarted:
                {
                    std::cout << "RTMP Started\n";
                    this->m_state = kSessionStateStarted;
                    
                    this->setupGraph(frame_w, frame_h, fps, bitrate);
                    
                    this->m_callback(kSessionStateStarted) ;
                }
                    break;
                case kClientStateError:
                    std::cout << "RTMP Error\n";
                    m_state = kSessionStateError;
                    this->m_callback(kSessionStateError);
                    break;
                case kClientStateNotConnected:
                    std::cout << "RTMP Ended\n";
                    m_state = kSessionStateEnded;
                    this->m_callback(kSessionStateEnded);
                    break;
                default:
                    break;
                    
            }
            
        }) );
        videocore::RTMPSessionParameters_t sp ( 0. );
        
        sp.setData(frame_w, frame_h, 1. / static_cast<double>(fps), bitrate);
        
        m_outputSession->setSessionParameters(sp);
        
    }
    void
    SampleGraph::setupGraph( int frame_w, int frame_h, int fps, int bitrate)
    {
        
        m_audioTransformChain.clear();
        m_videoTransformChain.clear();
        
        
        {
            // Add audio mixer
            const double aacPacketTime = 1024. / 44100.0;
            
#ifdef __APPLE__
            addTransform(m_audioTransformChain, std::make_shared<videocore::Apple::AudioMixer>(2,44100,16,aacPacketTime));
#else
            addTransform(m_audioTransformChain, std::make_shared<videocore::GenericAudioMixer>(2,44100,16,aacPacketTime));
#endif
        }
#ifdef __APPLE__
#ifdef TARGET_OS_IPHONE
        
        
        {
            // Add video mixer
            auto mixer = std::make_shared<videocore::iOS::GLESVideoMixer>(frame_w, frame_h, 1. / static_cast<double>(fps));
            addTransform(m_videoTransformChain, mixer);

            
        }
        {
            // Add encoders
            addTransform(m_audioTransformChain, std::make_shared<videocore::iOS::AACEncode>(44100, 2));
            addTransform(m_videoTransformChain, std::make_shared<videocore::iOS::H264Encode>(frame_w, frame_h, fps, bitrate));
            
        }
        {
            // [Optional] add splits
            // Splits would be used to add different graph branches at various
            // stages.  For example, if you wish to record an MP4 file while
            // streaming to RTMP.
            
            auto videoSplit = std::make_shared<videocore::Split>();
            auto audioSplit = std::make_shared<videocore::Split>();
            m_videoSplit = videoSplit;
            m_audioSplit = audioSplit;
            
            addTransform(m_audioTransformChain, audioSplit);
            addTransform(m_videoTransformChain, videoSplit);
            
        }
#else
#endif // TARGET_OS_IPHONE
#endif // __APPLE__
        
        addTransform(m_audioTransformChain, std::make_shared<videocore::rtmp::AACPacketizer>());
        addTransform(m_videoTransformChain, std::make_shared<videocore::rtmp::H264Packetizer>());
        
        
        m_videoTransformChain.back()->setOutput(this->m_outputSession);
        m_audioTransformChain.back()->setOutput(this->m_outputSession);
        
        const auto epoch = std::chrono::steady_clock::now();
        for(auto it = m_videoTransformChain.begin() ; it != m_videoTransformChain.end() ; ++it) {
            (*it)->setEpoch(epoch);
        }
        for(auto it = m_audioTransformChain.begin() ; it != m_audioTransformChain.end() ; ++it) {
            (*it)->setEpoch(epoch);
        }
        
        
        // Create sources
        {
            
            // Add camera source
            m_cameraSource = std::make_shared<videocore::iOS::CameraSource>(0,0,frame_w,frame_h, float(frame_w) / float(frame_h));
            m_cameraSource->setOutput(m_videoTransformChain.front());
        }
        {
            // Add mic source
            m_micSource = std::make_shared<videocore::iOS::MicSource>();
            m_micSource->setOutput(m_audioTransformChain.front());
            
        }
    }

    void
    SampleGraph::addTransform(std::vector<std::shared_ptr<videocore::ITransform> > &chain, std::shared_ptr<videocore::ITransform> transform)
    {
        if( chain.size() > 0 ) {
            chain.back()->setOutput(transform);
        }
        chain.push_back(transform);
        
    }


}
};