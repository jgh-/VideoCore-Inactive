#ifndef __videocore_android_VCMediaCodec_h
#define __videocore_android_VCMediaCodec_h

#include <jni.h>
#include <stdint.h>
#include <string>
#include <unordered_map>

namespace videocore { namespace Android {
	struct MediaCodec_jni_ {
        jclass klass;
        jmethodID configure;
        jmethodID createEncoderByType;
        jmethodID dequeueInputBuffer;
        jmethodID queueInputBuffer;
        jmethodID dequeueOutputBuffer;
        jmethodID getInputBuffer;
        jmethodID getOutputBuffer;
        jmethodID releaseOutputBuffer;
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
    struct BufferInfo_jni_ {
    	jclass klass;
    	jmethodID init;
    	jfieldID flags;
    	jfieldID offset;
    	jfieldID presentationTimeUs;
    	jfieldID size;
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

		uint8_t* dequeueInputBuffer(size_t* outSize);
		uint8_t* dequeueOutputBuffer(size_t* outSize);

		void enqueueInputBuffer(uint8_t* buffer, size_t size);
		void releaseOutputBuffer(uint8_t* buffer);

	private:
		static void staticInit();
		void init();
	private:

		MediaCodec_jni_  m_mcj;
		MediaFormat_jni_ m_mfj;
		BufferInfo_jni_ m_bij;

		std::unordered_map<uint8_t*, ssize_t> m_mapBufferIdx;

		JavaVM* m_vm;
		JNIEnv* m_env;

		void*   m_pCodec;
		void*   m_pFmt;

		jobject m_oCodec;
		jobject m_oFmt;

	};

}}


#endif
