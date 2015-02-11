#include <OMXAL/OpenMAXAL.h>
#include <OMXAL/OpenMAXAL_Android.h>
#include <videocore/transforms/IEncoder.hpp>

namespace videocore { namespace Android {
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

	};
}}