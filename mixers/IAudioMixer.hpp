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
#ifndef videocore_IAudioMixer_hpp
#define videocore_IAudioMixer_hpp

#include <videocore/system/Buffer.hpp>
#include <videocore/mixers/IMixer.hpp>
#include <videocore/transforms/IMetadata.hpp>

namespace videocore {


    /*! Enum values for the AudioBufferMetadata tuple */
    enum {
        kAudioMetadataFrequencyInHz,    /*!< Specifies the sampling rate of the buffer */
        kAudioMetadataBitsPerChannel,   /*!< Specifies the number of bits per channel */
        kAudioMetadataChannelCount,     /*!< Specifies the number of channels */
        kAudioMetadataFlags,            /*!< Specifies the audio flags */
        kAudioMetadataBytesPerFrame,    /*!< Specifies the number of bytes per frame */
        kAudioMetadataNumberFrames,     /*!< Number of sample frames in the buffer. */
        kAudioMetadataUsesOSStruct,     /*!< Indicates that the audio is not raw but instead uses a platform-specific struct */
        kAudioMetadataLoops,            /*!< Indicates whether or not the buffer should loop. Currently ignored. */
        kAudioMetadataSource            /*!< A smart pointer to the source. */
    };

    /*!
     *  Specifies the properties of the incoming audio buffer.
     */
    typedef MetaData<'soun', int, int, int, int, int, int, bool, bool, std::weak_ptr<ISource> > AudioBufferMetadata;

    class ISource;

    /*! IAudioMixer interface.  Defines the required interface methods for Audio mixers. */
    class IAudioMixer : public IMixer
    {
    public:

        /*! Virtual destructor */
        virtual ~IAudioMixer() {};

        /*!
         *  Set the output gain of the specified source.
         *
         *  \param source  A smart pointer to the source to be modified
         *  \param gain    A value between 0 and 1 representing the desired gain.
         */
        virtual void setSourceGain(std::weak_ptr<ISource> source,
                                   float gain) = 0;

        /*!
         *  Set the channel count.
         *
         *  \param channelCount  The number of audio channels.
         */
        virtual void setChannelCount(int channelCount) = 0;

        /*!
         *  Set the channel count.
         *
         *  \param frequencyInHz  The audio sample frequency in Hz.
         */
        virtual void setFrequencyInHz(float frequencyInHz) = 0;

        /*!
         *  Set the amount of time to buffer before emitting mixed samples.
         *
         *  \param duration The duration, in seconds, to buffer.
         */
        virtual void setMinimumBufferDuration(const double duration) = 0;
    };
}

#endif
