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

#include <videocore/mixers/Apple/AudioMixer.h>

static const UInt32 s_samplingRateConverterComplexity = kAudioConverterSampleRateConverterComplexity_Normal;
static const UInt32 s_samplingRateConverterQuality = kAudioConverterQuality_High;

namespace videocore { namespace Apple {
    
    struct UserData {
        uint8_t* data;
        uint8_t* p;
        int size;
        int packetSize;
        UInt32 numberPackets;
        AudioStreamPacketDescription * pd ;
        
    } ;
    
    AudioMixer::AudioMixer(int outChannelCount,
                           int outFrequencyInHz,
                           int outBitsPerChannel,
                           double frameDuration)
    : GenericAudioMixer(outChannelCount, outFrequencyInHz, outBitsPerChannel, frameDuration)
    {
    }
    AudioMixer::~AudioMixer()
    {
        
    }
    std::shared_ptr<Buffer>
    AudioMixer::resample(const uint8_t* const buffer,
                         size_t size,
                         AudioBufferMetadata& metadata)
    {
        const auto inFrequncyInHz = metadata.getData<kAudioMetadataFrequencyInHz>();
        const auto inBitsPerChannel = metadata.getData<kAudioMetadataBitsPerChannel>();
        const auto inChannelCount = metadata.getData<kAudioMetadataChannelCount>();
        //auto inLoops = metadata.getData<kAudioMetadataLoops>();
        
        if(m_outFrequencyInHz == inFrequncyInHz && m_outBitsPerChannel == inBitsPerChannel && m_outChannelCount == inChannelCount)
        {
            // No resampling necessary
            return std::make_shared<Buffer>();
        }
        
        AudioStreamBasicDescription in = {0};
        AudioStreamBasicDescription out = {0};
        
        in.mFormatID = kAudioFormatLinearPCM;
        in.mFormatFlags =  kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
        in.mChannelsPerFrame = inChannelCount;
        in.mSampleRate = inFrequncyInHz;
        in.mBitsPerChannel = inBitsPerChannel;
        in.mBytesPerFrame = (in.mBitsPerChannel * in.mChannelsPerFrame) / 8;
        in.mFramesPerPacket = 1;
        in.mBytesPerPacket = in.mBytesPerFrame * in.mFramesPerPacket;
        
        out.mFormatID = kAudioFormatLinearPCM;
        out.mFormatFlags =  kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
        out.mChannelsPerFrame = m_outChannelCount;
        out.mSampleRate = m_outFrequencyInHz;
        out.mBitsPerChannel = m_outBitsPerChannel;
        out.mBytesPerFrame = (out.mBitsPerChannel * out.mChannelsPerFrame) / 8;
        out.mFramesPerPacket = 1;
        out.mBytesPerPacket = out.mBytesPerFrame * out.mFramesPerPacket;
        
        const double inBufferTime = double(size) / (double(in.mBytesPerPacket) * double(in.mSampleRate));
        const double outBufferSampleCount = inBufferTime * double(m_outFrequencyInHz);
        const size_t outBufferSize = out.mBytesPerPacket * outBufferSampleCount;
        const auto outBuffer = std::make_shared<Buffer>(outBufferSize);
        
        AudioConverterRef audioConverter;
        AudioConverterNew(&in, &out, &audioConverter);
        
        AudioConverterSetProperty(audioConverter,
                                  kAudioConverterSampleRateConverterComplexity,
                                  sizeof(s_samplingRateConverterComplexity),
                                  &s_samplingRateConverterComplexity);
        
        AudioConverterSetProperty(audioConverter,
                                  kAudioConverterSampleRateConverterQuality,
                                  sizeof(s_samplingRateConverterQuality),
                                  &s_samplingRateConverterQuality);
        
        
        std::unique_ptr<UserData> ud(new UserData());
        ud->size = static_cast<int>(size);
        ud->data = const_cast<uint8_t*>(buffer);
        ud->p = ud->data;
        ud->packetSize = in.mBytesPerPacket;
        ud->numberPackets = ud->size / ud->packetSize;
        
        AudioBufferList outBufferList;
        outBufferList.mNumberBuffers = 1;
        outBufferList.mBuffers[0].mDataByteSize = static_cast<UInt32>(outBufferSize);
        outBufferList.mBuffers[0].mNumberChannels = m_outChannelCount;
        outBufferList.mBuffers[0].mData = (*outBuffer)();
        
        UInt32 sampleCount = outBufferSampleCount;
        AudioConverterFillComplexBuffer(audioConverter, /* AudioConverterRef inAudioConverter */
                                        AudioMixer::ioProc, /* AudioConverterComplexInputDataProc inInputDataProc */
                                        ud.get(), /* void *inInputDataProcUserData */
                                        &sampleCount, /* UInt32 *ioOutputDataPacketSize */
                                        &outBufferList, /* AudioBufferList *outOutputData */
                                        NULL /* AudioStreamPacketDescription *outPacketDescription */
                                        );
        
        AudioConverterDispose(audioConverter);
        outBuffer->setSize(outBufferList.mBuffers[0].mDataByteSize);
        return outBuffer;
    }
    //http://stackoverflow.com/questions/6610958/os-x-ios-sample-rate-conversion-for-a-buffer-using-audioconverterfillcomplex
    OSStatus
    AudioMixer::ioProc(AudioConverterRef audioConverter,
                       UInt32 *ioNumDataPackets,
                       AudioBufferList* ioData,
                       AudioStreamPacketDescription** ioPacketDesc,
                       void* inUserData )
    {
        UserData* ud = static_cast<UserData*>(inUserData);
        
        UInt32 maxPackets = std::min(ud->numberPackets, *ioNumDataPackets);
        ud->numberPackets -= maxPackets;
        *ioNumDataPackets = maxPackets;
        OSStatus err = noErr;
        if(maxPackets) {
            ioData->mBuffers[0].mData = ud->p;
            ioData->mBuffers[0].mDataByteSize = static_cast<UInt32>(ud->size - (ud->p-ud->data));
            ioData->mBuffers[0].mNumberChannels = 2;
            ud->p += maxPackets * ud->packetSize;
        } else {
            err = -50;
        }
        return err;
    }
}
}
