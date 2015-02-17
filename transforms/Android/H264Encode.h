#include <videocore/transforms/IEncoder.hpp>
#include <videocore/system/JobQueue.hpp>
#include <jni.h>

namespace videocore { namespace Android {

    struct MediaCodec {
        jclass klass;
        jobject obj;
        jmethodID configure;
        jmethodID createEncoderByType;
        jmethodID dequeueInputBuffer;
        jmethodID queueInputBuffer;
        jmethodID dequeueOutputBuffer;
        jmethodID start;
        jmethodID stop;
    };
    struct MediaFormat {
        jclass klass;
        jobject obj;
        jmethodID setInteger;
        jmethodID createVideoFormat;
    };
    struct ByteBuffer {
        jclass klass;
        jobject obj;
    };
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
        MediaCodec m_codec;
        MediaFormat m_format;

        JobQueue m_queue;
        std::weak_ptr<IOutput> m_output;
        JavaVM* m_vm;
        JNIEnv* m_env;

        int m_bitrate;
        int m_frameW;
        int m_frameH;
        int m_fps;

	};
}}