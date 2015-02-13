#include <videocore/transforms/Android/H264Encode.h>
#include <videocore/system/pixelBuffer/Android/PixelBuffer.h>


/*namespace android {
	RefBase::~RefBase()
	{
	    if (mRefs->mWeak == 0) {
	        delete mRefs;
	    }
	}
	MediaSource::~MediaSource() {};
}*/
namespace videocore { namespace Android {

	struct ImageSource : public  android::MediaSource {
		ImageSource() : android::MediaSource() {

		}
		virtual ~ImageSource() {

		}
		android::status_t 
		start(android::MetaData* parms = nullptr) {
			return 0;
		}
		android::status_t 
		stop() {
			return 0;
		}
		android::sp<android::MetaData> 
		getFormat() {

		}
		android::status_t 
		read(android::MediaBuffer **buffer, const ReadOptions *options = NULL) {

		}

		void setBuffer(std::shared_ptr<PixelBuffer> buffer) { m_buffer = buffer; }

	private:
		std::shared_ptr<PixelBuffer> m_buffer;
	};

	struct MemWriter : public android::MediaWriter {

	};
	H264Encode::H264Encode() {
		m_imageSource.reset(new ImageSource());
	}

	H264Encode::~H264Encode() {

	}
	void 
	H264Encode::pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata) {
		std::shared_ptr<PixelBuffer> img = *(std::shared_ptr<PixelBuffer>*) data;
		m_imageSource->setBuffer(img);
	}
	void 
	H264Encode::setBitrate(int bitrate) {

	}
	void
	H264Encode::requestKeyframe() {
		
	}
}}