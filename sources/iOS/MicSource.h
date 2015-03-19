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
#ifndef __videocore__MicSource__
#define __videocore__MicSource__

#include <iostream>
#import <CoreAudio/CoreAudioTypes.h>
#import <AudioToolbox/AudioToolbox.h>
#include <videocore/sources/ISource.hpp>
#include <videocore/transforms/IOutput.hpp>

#import <Foundation/Foundation.h>

@class InterruptionHandler;

namespace videocore { namespace iOS {

    /*!
     *  Capture audio from the device's microphone.
     *
     */

    class MicSource : public ISource, public std::enable_shared_from_this<MicSource>
    {
    public:

        /*!
         *  Constructor.
         *
         *  \param audioSampleRate the sample rate in Hz to capture audio at.  Best results if this matches the mixer's sampling rate.
         *  \param excludeAudioUnit An optional lambda method that is called when the source generates its Audio Unit.
         *                          The parameter of this method will be a reference to its Audio Unit.  This is useful for
         *                          applications that may be capturing Audio Unit data and do not wish to capture this source.
         *
         */
        MicSource(double sampleRate = 44100.,
                  int channelCount = 2,
                  std::function<void(AudioUnit&)> excludeAudioUnit = nullptr);

        /*! Destructor */
        ~MicSource();


    public:

        /*! ISource::setOutput */
        void setOutput(std::shared_ptr<IOutput> output);

        /*! Used by the Audio Unit as a callback method */
        void inputCallback(uint8_t* data, size_t data_size, int inNumberFrames);

        void interruptionBegan();
        void interruptionEnded();
        
        /*!
         *  \return a reference to the source's Audio Unit
         */
        const AudioUnit& audioUnit() const { return m_audioUnit; };

    private:

        InterruptionHandler*   m_interruptionHandler;
        
        AudioComponentInstance m_audioUnit;
        AudioComponent         m_component;

        double m_sampleRate;
        int m_channelCount;

        std::weak_ptr<IOutput> m_output;

    };

}
}
@interface InterruptionHandler : NSObject
{
    @public
    videocore::iOS::MicSource* _source;
}
- (void) handleInterruption: (NSNotification*) notification;
@end

#endif /* defined(__videocore__MicSource__) */
