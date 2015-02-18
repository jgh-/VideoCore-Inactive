#ifndef __videocore_Android_AACEncode
#define __videocore_Android_AACEncode

#include <videocore/transforms/IEncoder.hpp>
#include <videocore/system/JobQueue.hpp>
#include <videocore/transforms/Android/VCMediaCodec.h>
#include <memory>
namespace videocore { namespace Android {

   /* */
	class AACEncode : public IEncoder {
	public:
		AACEncode( JavaVM* vm, int frequencyInHz, int channelCount, int averageBitrate );
		~AACEncode();

	public:
        /*! ITransform */
        void setOutput(std::shared_ptr<IOutput> output) { m_output = output; };
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);

    public:
        /*! IEncoder */
        void setBitrate(int bitrate) ;
        const int bitrate() const { return m_bitrate; };

    private:

        std::unique_ptr<VCMediaCodec> m_mediaCodec;
        
        JobQueue m_queue;
        std::weak_ptr<IOutput> m_output;

        int m_bitrate;
        int m_freq;
        int m_channelCount;

	};
}}

#endif