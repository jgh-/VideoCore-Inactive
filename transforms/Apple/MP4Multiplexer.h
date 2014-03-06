//
//  MP4Multiplexer.h
//  videocore
//
//  Created by James Hurley on 1/31/14.
//  Copyright (c) 2014 Raster Multimedia. All rights reserved.
//

#ifndef __videocore__MP4Multiplexer__
#define __videocore__MP4Multiplexer__

#include <iostream>
#include <vector>
#include <videocore/transforms/IOutputSession.hpp>

namespace videocore { namespace apple {
    
    enum {
        kMP4SessionFilename=0,
        kMP4SessionFPS,
        kMP4SessionWidth,
        kMP4SessionHeight
    };
    typedef MetaData<'mp4 ', std::string, int, int, int> MP4SessionParameters_t;
    
    class MP4Multiplexer : public IOutputSession
    {
        
    public:
        MP4Multiplexer();
        ~MP4Multiplexer();
        
        void setSessionParameters(IMetadata & parameters) ;
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
        void setEpoch(const std::chrono::steady_clock::time_point epoch) { m_epoch = epoch; };
        
    private:
        void pushVideoBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
        void pushAudioBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
        void createAVCC();
        
    private:
        void*       m_assetWriter;
        void*       m_videoInput;
        void*       m_audioInput;
    
        void*       m_videoFormat;
        
        std::vector<uint8_t> m_sps;
        std::vector<uint8_t> m_pps;
        
        std::chrono::steady_clock::time_point m_epoch;
            
        std::string m_filename;
        int m_fps;
        int m_width;
        int m_height;
        int m_framecount;
    };
    
}
}
#endif /* defined(__videocore__MP4Multiplexer__) */
