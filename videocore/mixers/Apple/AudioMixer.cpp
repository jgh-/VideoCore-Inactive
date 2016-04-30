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
static const UInt32 s_samplingRateConverterQuality = kAudioConverterQuality_Medium;

namespace videocore { namespace Apple {
    
    struct UserData {
        uint8_t* data;
        uint8_t* p;
        int size;
        int packetSize;
        UInt32 numberPackets;
        int numChannels;
        AudioStreamPacketDescription * pd ;
        bool isInterleaved;
        bool usesOSStruct;
        
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
        for ( auto & it : m_converters ) {
            AudioConverterDispose(it.second.converter);
        }
    }
    std::shared_ptr<Buffer>
    AudioMixer::resample(const uint8_t* const buffer,
                         size_t size,
                         AudioBufferMetadata& metadata)
    {
        const auto inFrequncyInHz = metadata.getData<kAudioMetadataFrequencyInHz>();
        const auto inBitsPerChannel = metadata.getData<kAudioMetadataBitsPerChannel>();
        const auto inChannelCount = metadata.getData<kAudioMetadataChannelCount>();
        const auto inFlags = metadata.getData<kAudioMetadataFlags>();
        const auto inBytesPerFrame = metadata.getData<kAudioMetadataBytesPerFrame>();
        const auto inNumberFrames = metadata.getData<kAudioMetadataNumberFrames>();
        const auto inUsesOSStruct = metadata.getData<kAudioMetadataUsesOSStruct>();
        
        if(m_outFrequencyInHz == inFrequncyInHz &&
           m_outBitsPerChannel == inBitsPerChannel &&
           m_outChannelCount == inChannelCount
           && !(inFlags & kAudioFormatFlagIsNonInterleaved)
           && !(inFlags & kAudioFormatFlagIsFloat))
        {
            // No resampling necessary
            return std::make_shared<Buffer>();
        }
        
        uint64_t hash = uint64_t(inBytesPerFrame&0xFF) << 56 | uint64_t(inFlags&0xFF) << 48 | uint64_t(inChannelCount&0xFF) << 40
                        | uint64_t(inBitsPerChannel&0xFF) << 32 | inFrequncyInHz;
        
        auto it = m_converters.find(hash) ;
        ConverterInst converter = {0};
        
        if(it == m_converters.end()) {
            AudioStreamBasicDescription in = {0};
            AudioStreamBasicDescription out = {0};
            
            in.mFormatID = kAudioFormatLinearPCM;
            in.mFormatFlags =  inFlags;
            in.mChannelsPerFrame = inChannelCount;
            in.mSampleRate = inFrequncyInHz;
            in.mBitsPerChannel = inBitsPerChannel;
            in.mBytesPerFrame = inBytesPerFrame;
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
            
            converter.asbdIn = in;
            converter.asbdOut = out;
            
            OSStatus ret = AudioConverterNew(&in, &out, &converter.converter);
            
            AudioConverterSetProperty(converter.converter,
                                      kAudioConverterSampleRateConverterComplexity,
                                      sizeof(s_samplingRateConverterComplexity),
                                      &s_samplingRateConverterComplexity);
            
            AudioConverterSetProperty(converter.converter,
                                      kAudioConverterSampleRateConverterQuality,
                                      sizeof(s_samplingRateConverterQuality),
                                      &s_samplingRateConverterQuality);
        
            auto prime = kConverterPrimeMethod_None;
            
            AudioConverterSetProperty(converter.converter,
                                      kAudioConverterPrimeMethod,
                                      sizeof(prime),
                                      &prime);
            
            m_converters[hash] = converter;
            
            if(ret != noErr) {
                DLog("ret = %d (%x)", (int)ret, (unsigned)ret);
            }
            
        } else {
            converter = it->second;
        }
        
        auto & in = converter.asbdIn;
        auto & out = converter.asbdOut;

        const double inSampleCount = inNumberFrames;
        const double ratio = static_cast<double>(inFrequncyInHz) / static_cast<double>(m_outFrequencyInHz);
        
        const double outBufferSampleCount = std::round(double(inSampleCount) / ratio);
        
        const size_t outBufferSize = out.mBytesPerPacket * outBufferSampleCount;
        const auto outBuffer = std::make_shared<Buffer>(outBufferSize);
        
        
        std::unique_ptr<UserData> ud(new UserData());
        ud->size = static_cast<int>(size);
        ud->data = const_cast<uint8_t*>(buffer);
        ud->p = inUsesOSStruct ? 0 : ud->data;
        ud->packetSize = in.mBytesPerPacket;
        ud->numberPackets = inSampleCount;
        ud->numChannels = inChannelCount;
        ud->isInterleaved = !(inFlags & kAudioFormatFlagIsNonInterleaved);
        ud->usesOSStruct = inUsesOSStruct;
        
        AudioBufferList outBufferList;
        outBufferList.mNumberBuffers = 1;
        outBufferList.mBuffers[0].mDataByteSize = static_cast<UInt32>(outBufferSize);
        outBufferList.mBuffers[0].mNumberChannels = m_outChannelCount;
        outBufferList.mBuffers[0].mData = (*outBuffer)();
        
        UInt32 sampleCount = outBufferSampleCount;
        OSStatus ret = AudioConverterFillComplexBuffer(converter.converter, /* AudioConverterRef inAudioConverter */
                                        AudioMixer::ioProc, /* AudioConverterComplexInputDataProc inInputDataProc */
                                        ud.get(), /* void *inInputDataProcUserData */
                                        &sampleCount, /* UInt32 *ioOutputDataPacketSize */
                                        &outBufferList, /* AudioBufferList *outOutputData */
                                        NULL /* AudioStreamPacketDescription *outPacketDescription */
                                        );
        if(ret != noErr) {
            DLog("ret = %d (%x)", (int)ret, (unsigned)ret);
        }
      
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
        OSStatus err = noErr;
        UserData* ud = static_cast<UserData*>(inUserData);
        
        int numPackets = std::min(*ioNumDataPackets, ud->numberPackets);
        
        *ioNumDataPackets = numPackets;
        if(!ud->usesOSStruct) {
            ioData->mBuffers[0].mData = ud->p;
            ioData->mBuffers[0].mDataByteSize = numPackets * ud->packetSize;
            ioData->mBuffers[0].mNumberChannels = ud->numChannels;
            ud->p += numPackets * ud->packetSize;
        } else {
            AudioBufferList* ab = (AudioBufferList*) ud->data;
            ioData->mNumberBuffers = ab->mNumberBuffers;
            const long p = (long)ud->p;
            for ( int i = 0 ; i < ab->mNumberBuffers ; ++i ) {
                uint8_t* data = (uint8_t*)ab->mBuffers[i].mData;
                ioData->mBuffers[i].mData = (void*)(data + p);
                ioData->mBuffers[i].mDataByteSize = numPackets * ud->packetSize;
                ioData->mBuffers[i].mNumberChannels = ab->mBuffers[i].mNumberChannels;
            }
            ud->p += numPackets * ud->packetSize;
        }
        
        return err;
    }
}
}
