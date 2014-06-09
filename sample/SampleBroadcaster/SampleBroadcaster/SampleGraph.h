
/*
 
 Video Core
 Copyright (C) 2014 James G. Hurley
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 USA
 
 */

#include <videocore/transforms/IOutputSession.hpp>
#include <videocore/transforms/ITransform.hpp>
#include <videocore/sources/ISource.hpp>
#include <videocore/mixers/IMixer.hpp>
#include <videocore/transforms/Split.h>

#include "PixelBufferOutput.h"
#include "SampleImageTransform.h"

#include <vector>

#ifndef __sample__SampleGraph_h
#define __sample__SampleGraph_h


//
//  This file is intended as a sample of what a complete graph would look like.
//  It is not a standalone file that can be compiled and run.
//

namespace videocore { namespace sample {
 
    enum SessionState
    {
        kSessionStateNone,
        kSessionStateStarted,
        kSessionStateEnded,
        kSessionStateError
        
    } ;
    
    using SessionStateCallback = std::function<void(SessionState state)>;
    
    class SampleGraph
    {
    public:
        SampleGraph(SessionStateCallback callback) : m_callback(callback) {};
        ~SampleGraph() {};

        
        void setPBCallback(PixelBufferCallback callback) ;
        
        // Starting a new session will end the current session.
        void startRtmpSession(std::string uri, int frame_w, int frame_h, int bitrate, int fps);
        
        void spin(bool spin);
        
    private:
        void setupGraph(int frame_w, int frame_h, int fps, int bitrate);
        void addTransform(std::vector< std::shared_ptr<videocore::ITransform> > & chain, std::shared_ptr<videocore::ITransform> transform);
        
    private:
        std::shared_ptr<PixelBufferOutput> m_pbOutput;
        std::shared_ptr<videocore::ISource> m_micSource;
        std::shared_ptr<videocore::ISource> m_cameraSource;
        std::shared_ptr<videocore::Split> m_videoSplit;
        
        std::vector< std::shared_ptr<videocore::ITransform> > m_audioTransformChain;
        std::vector< std::shared_ptr<videocore::ITransform> > m_videoTransformChain;
        
        std::shared_ptr<videocore::IOutputSession> m_outputSession;
        
        std::weak_ptr<SampleImageTransform> m_spinTransform;
        
        SessionStateCallback m_callback;
        
        SessionState m_state;
        
    };
    
}
}

#endif