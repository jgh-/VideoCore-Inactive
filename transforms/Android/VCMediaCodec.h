#ifndef __videocore_android_VCMediaCodec_h
#define __videocore_android_VCMediaCodec_h

#include <jni.h>
#include <stdint.h>
#include <string>

namespace videocore { namespace Android {
	struct MediaCodec_jni_ {
        jclass klass;
        jmethodID configure;
        jmethodID createEncoderByType;
        jmethodID dequeueInputBuffer;
        jmethodID queueInputBuffer;
        jmethodID dequeueOutputBuffer;
        jmethodID getInputBuffers;
        jmethodID getOutputBuffers;
        jmethodID start;
        jmethodID stop;
        jmethodID release;
    };
    struct MediaFormat_jni_ {
        jclass klass;
        jmethodID setInteger;
        jmethodID setString;
        jmethodID createVideoFormat;
        jmethodID createAudioFormat;
    };
    struct ByteBuffer_jni_ {
        jclass klass;
    };

    /*
     *	After instantiating this class you must always call it on the same thread if Android version < 5.0
     */
	class VCMediaCodec
	{
	public:

		VCMediaCodec(JavaVM* vm, std::string mimetype, int width, int height, int bitrate);
		VCMediaCodec(JavaVM* vm, std::string mimetype, int samplerate, int channelcount);
		~VCMediaCodec();

		// Sets int32 for the Format
		void setInt32(std::string key, int32_t value);
		// Set a string for the Format
		void setString(std::string key, std::string value);

		void start();
		void stop();

		// Set the configuration
		void configure();

	private:
		static void staticInit();
		void init();
	private:

		MediaCodec_jni_  m_mcj;
		MediaFormat_jni_ m_mfj;
		ByteBuffer_jni_  m_bbj;

		JavaVM* m_vm;
		JNIEnv* m_env;

		void*   m_pCodec;
		void*   m_pFmt;

		jobject m_oCodec;
		jobject m_oFmt;

	};

}}


#endif
