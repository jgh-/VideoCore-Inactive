#include <videocore/transforms/IEncoder.hpp>
#include <videocore/system/JobQueue.hpp>
#include <videocore/transforms/Android/VCMediaCodec.h>
#include <memory>
namespace videocore { namespace Android {

   /* */
	class H264Encode : public IEncoder {
	public:
		H264Encode( JavaVM* vm, int frame_w, int frame_h, int fps, int bitrate );
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

        std::unique_ptr<VCMediaCodec> m_mediaCodec;
        
        JobQueue m_queue;
        std::weak_ptr<IOutput> m_output;

        int m_bitrate;
        int m_frameW;
        int m_frameH;
        int m_fps;

	};
}}