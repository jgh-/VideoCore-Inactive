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
#include <videocore/transforms/iOS/AACEncode.h>
#include <sstream>

namespace videocore { namespace iOS {

    struct UserData {
        uint8_t* data;
        int size;
        int packetSize;
        AudioStreamPacketDescription * pd ;

    } ;

    static const int kSamplesPerFrame = 1024;

    static char *FormatError(char *str, OSStatus error)
    {
        // see if it appears to be a 4-char-code
        *(UInt32 *)(str + 1) = CFSwapInt32HostToBig(error);
        if (isprint(str[1]) && isprint(str[2]) && isprint(str[3]) && isprint(str[4])) {
            str[0] = str[5] = '\'';
            str[6] = '\0';
        } else
            // no, format it as an integer
            sprintf(str, "%d", (int)error);
        return str;
    }

    AACEncode::AACEncode(int frequencyInHz, int channelCount)
    : m_sentConfig(false)
    {

        OSStatus result = 0;
        char err[5];

        AudioStreamBasicDescription in = {0}, out = {0};


        // passing anything except 48000, 44100, and 22050 for mSampleRate results in "!dat"
        // OSStatus when querying for kAudioConverterPropertyMaximumOutputPacketSize property
        // below
        in.mSampleRate = frequencyInHz;
        // passing anything except 2 for mChannelsPerFrame results in "!dat" OSStatus when
        // querying for kAudioConverterPropertyMaximumOutputPacketSize property below
        in.mChannelsPerFrame = channelCount;
        in.mBitsPerChannel = 16;
        in.mFormatFlags =  kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
        in.mFormatID = kAudioFormatLinearPCM;
        in.mFramesPerPacket = 1;
        in.mBytesPerFrame = in.mBitsPerChannel * in.mChannelsPerFrame / 8;
        in.mBytesPerPacket = in.mFramesPerPacket*in.mBytesPerFrame;

        out.mFormatID = kAudioFormatMPEG4AAC;
        out.mFormatFlags = 0;
        out.mFramesPerPacket = kSamplesPerFrame;
        out.mSampleRate = frequencyInHz;
        out.mChannelsPerFrame = channelCount;

        bool canResume = true;
        UInt32 outputBitrate = 128000; // 128 kbps
        UInt32 propSize = sizeof(outputBitrate);
        UInt32 outputPacketSize = 0;

        AudioClassDescription requestedCodecs[1] = {
            {
                kAudioEncoderComponentType,
                kAudioFormatMPEG4AAC,
                kAppleSoftwareAudioCodecManufacturer
            }
        };

        result = AudioConverterNewSpecific(&in, &out, 1, requestedCodecs, &m_audioConverter);
        if (result) FormatError(err, result);

        result = AudioConverterSetProperty(m_audioConverter, kAudioConverterEncodeBitRate, propSize, &outputBitrate);
        if (result) FormatError(err, result);

        result = AudioConverterSetProperty(m_audioConverter, kAudioConverterPropertyCanResumeFromInterruption, sizeof(canResume), &canResume);
        if (result) FormatError(err, result);

        result = AudioConverterGetProperty(m_audioConverter, kAudioConverterPropertyMaximumOutputPacketSize, &propSize, &outputPacketSize);
        if (result) FormatError(err, result);


        m_outputPacketMaxSize = outputPacketSize;

        m_bytesPerSample = 2 * channelCount;

        int sampleRateIndex;
        switch(frequencyInHz) {
            case 48000:
                sampleRateIndex = 3;
                break;
            case 44100:
                sampleRateIndex = 4;
                break;
            case 22050:
                sampleRateIndex = 7;
                break;
            case 11025:
                sampleRateIndex = 10;
                break;
            case 8000:
                sampleRateIndex = 11;
                break;
            default:
                sampleRateIndex = 15;
        }
        makeAsc(sampleRateIndex, channelCount);

    }
    AACEncode::~AACEncode() {
        AudioConverterDispose(m_audioConverter);
    }
    void
    AACEncode::makeAsc(char sampleRateIndex, char channelCount)
    {
        // http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio#Audio_Specific_Config
        m_asc[0] = 0x10 | ((sampleRateIndex>>1) & 0x3);
        m_asc[1] = ((sampleRateIndex & 0x1)<<7) | ((channelCount & 0xF) << 3);
    }
    OSStatus
    AACEncode::ioProc(AudioConverterRef audioConverter, UInt32 *ioNumDataPackets, AudioBufferList* ioData, AudioStreamPacketDescription** ioPacketDesc, void* inUserData )
    {
        UserData* ud = static_cast<UserData*>(inUserData);

        UInt32 maxPackets = ud->size / ud->packetSize;

        *ioNumDataPackets = std::min(maxPackets, *ioNumDataPackets);

        ioData->mBuffers[0].mData = ud->data;
        ioData->mBuffers[0].mDataByteSize = ud->size;
        ioData->mBuffers[0].mNumberChannels = 1;

        return noErr;
    }
    void
    AACEncode::pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata)
    {
        const size_t sampleCount = size / m_bytesPerSample;
        const size_t aac_packet_count = sampleCount / kSamplesPerFrame;
        const size_t required_bytes = aac_packet_count * m_outputPacketMaxSize;

        if(m_outputBuffer.total() < (required_bytes)) {
            m_outputBuffer.resize(required_bytes);
        }
        uint8_t* p = m_outputBuffer();
        uint8_t* p_out = (uint8_t*)data;

        for ( size_t i = 0 ; i < aac_packet_count ; ++i ) {
            UInt32 num_packets = 1;

            AudioBufferList l;
            l.mNumberBuffers=1;
            l.mBuffers[0].mDataByteSize = m_outputPacketMaxSize * num_packets;
            l.mBuffers[0].mData = p;

            std::unique_ptr<UserData> ud(new UserData());
            ud->size = static_cast<int>(kSamplesPerFrame * m_bytesPerSample);
            ud->data = const_cast<uint8_t*>(p_out);
            ud->packetSize = static_cast<int>(m_bytesPerSample);

            AudioStreamPacketDescription output_packet_desc[num_packets];

            AudioConverterFillComplexBuffer(m_audioConverter, AACEncode::ioProc, ud.get(), &num_packets, &l, output_packet_desc);

            p += output_packet_desc[0].mDataByteSize;
            p_out += kSamplesPerFrame * m_bytesPerSample;
        }
        const size_t totalBytes = p - m_outputBuffer();


        auto output = m_output.lock();
        if(output && totalBytes) {
            if(!m_sentConfig) {
                output->pushBuffer((const uint8_t*)m_asc, sizeof(m_asc), metadata);
                m_sentConfig = true;
            }

            output->pushBuffer(m_outputBuffer(), totalBytes, metadata);
        }
    }
}
}
