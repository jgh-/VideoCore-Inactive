/*

 Video Core
 Copyright (c) 2014 James G. Hurley

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.

 */

#ifndef __videocore__AACPacketizer__
#define __videocore__AACPacketizer__

#include <iostream>
#include <videocore/transforms/ITransform.hpp>
#include <Vector>

namespace videocore { namespace rtmp {

    class AACPacketizer : public ITransform
    {
    public:
        AACPacketizer(float sampleRate=44100, int channelCount=2, int ctsOffset=0);

        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);
        void setOutput(std::shared_ptr<IOutput> output);
        void setEpoch(const std::chrono::steady_clock::time_point epoch) { m_epoch = epoch; };

    private:


        std::chrono::steady_clock::time_point m_epoch;
        std::weak_ptr<IOutput> m_output;
        std::vector<uint8_t> m_outbuffer;

        double m_audioTs;
        char m_asc[2];
        bool m_sentAudioConfig;
        float m_sampleRate;
        int m_ctsOffset;
        int m_channelCount;
    };

}
}
#endif /* defined(__videocore__AACPacketizer__) */
