#include <videocore/transforms/Android/H264Encode.h>
#include <videocore/system/pixelBuffer/Android/PixelBuffer.h>

namespace videocore { namespace Android {

	H264Encode::H264Encode(JavaVM* vm, int frame_w, int frame_h, int fps, int bitrate)
	: m_frameW(frame_w), m_frameH(frame_h), m_fps(fps), m_bitrate(bitrate)
	{
		m_queue.enqueue([=]{

			m_mediaCodec.reset(new VCMediaCodec(vm, "video/avc", frame_w, frame_h, bitrate));

			m_mediaCodec->setInt32("frame-rate", fps);
			m_mediaCodec->setInt32("i-frame-interval", 2);
			m_mediaCodec->setInt32("color-format", 21);
			

			m_mediaCodec->configure();
			m_mediaCodec->start();

		});
	}

	H264Encode::~H264Encode() {
		m_queue.enqueue_sync([&]{

			m_mediaCodec.reset();

		});

	}
	void 
	H264Encode::pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata) {
		std::shared_ptr<PixelBuffer> img = *(std::shared_ptr<PixelBuffer>*) data;

		m_queue.enqueue([=]{

		});
	}
	void 
	H264Encode::setBitrate(int bitrate) {

	}
	void
	H264Encode::requestKeyframe() {
		
	}
}}