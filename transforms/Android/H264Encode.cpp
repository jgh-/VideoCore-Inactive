#include <videocore/transforms/Android/H264Encode.h>


namespace videocore { namespace Android {

	struct ImageSource : public android::MediaSource {
		android::status_t 
		start(android::MetaData* parms = nullptr) {

		}
		android::sp<android::MetaData> 
		getFormat() {

		}
		android::status_t 
		read(android::MediaBuffer **buffer, const ReadOptions *options = NULL) {

		}
	};

	struct MemWriter : public android::MediaWriter {
		
	};
	H264Encode::H264Encode() {

	}

	H264Encode::~H264Encode() {

	}
	void 
	H264Encode::pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata) {

	}
	void 
	H264Encode::setBitrate(int bitrate) {

	}
	void
	H264Encode::requestKeyframe() {
		
	}
}}