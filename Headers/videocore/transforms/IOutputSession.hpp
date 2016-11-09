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

#ifndef videocore_IOutputSession_hpp
#define videocore_IOutputSession_hpp

#include <videocore/transforms/IOutput.hpp>

namespace videocore {

    /*!
     *  Called when the graph should make an adjustment to outgoing data rates
     *
     *  \param rateVector   A unit vector representing the direction for change. (-1 for reduce bitrate, 0 for no change, 1 for increase bitrate)
     *  \param estimatedAvailableBandwidth  An estimate of the available bandwidth [Bytes per second]. This may not be accurate.
     *
     */
    using BandwidthCallback = std::function<void(float rateVector, float estimatedAvailableBandwidth, int immediateThroughput)>;
    
    class IOutputSession : public IOutput
    {
    public:
        
        virtual void setSessionParameters(IMetadata & parameters) = 0 ;
        virtual void setBandwidthCallback(BandwidthCallback callback) = 0;
        
        virtual ~IOutputSession() {};
        
    };
}


#endif
