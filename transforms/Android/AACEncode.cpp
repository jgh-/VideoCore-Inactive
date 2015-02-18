#include <videocore/transforms/Android/AACEncode.h>


namespace videocore { namespace Android {

	AACEncode::AACEncode( JavaVM* vm, int frequencyInHz, int channelCount, int averageBitrate )
	: m_channelCount(channelCount), m_freq(frequencyInHz), m_bitrate(averageBitrate)
	{
		m_queue.enqueue([=]{
			DLog("Creating AAC encoder...\n");
			m_mediaCodec.reset(new VCMediaCodec(vm, "audio/mp4a-latm", frequencyInHz, channelCount));

			m_mediaCodec->setInt32("bitrate", averageBitrate);
			m_mediaCodec->setInt32("aac-profile", 2); // AACObjectLC

			DLog("Configuring AAC..\n");
			m_mediaCodec->configure();
			m_mediaCodec->start();
			DLog("Done.\n");
		});
	}

	AACEncode::~AACEncode() {
		m_queue.enqueue_sync([&]{

			m_mediaCodec.reset();

		});

	}
	void 
	AACEncode::pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata) {

		m_queue.enqueue([=]{

		});
	}
	void 
	AACEncode::setBitrate(int bitrate) {

	}

}}