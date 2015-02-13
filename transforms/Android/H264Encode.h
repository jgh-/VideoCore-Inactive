#include <videocore/transforms/IEncoder.hpp>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MediaWriter.h>
#include <media/stagefright/OMXCodec.h>

namespace videocore { namespace Android {
    struct ImageSource;

	class H264Encode : public IEncoder {
	public:
		H264Encode();
		~H264Encode();

	public:
        /*! ITransform */
        void setOutput(std::shared_ptr<IOutput> output) { m_output = output; };
        void pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata);

    public:
        /*! IEncoder */
        void setBitrate(int bitrate) ;
        const int bitrate() const { return m_bitrate; };
        void requestKeyframe();

    private:
        std::unique_ptr<ImageSource> m_imageSource;
        std::weak_ptr<IOutput> m_output;

        int m_bitrate;

	};
}}