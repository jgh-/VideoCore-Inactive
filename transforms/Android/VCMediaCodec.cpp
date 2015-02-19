#include <videocore/transforms/Android/VCMediaCodec.h>
#include <videocore/system/util.h>
#include <dlfcn.h>

struct __AMediaCodecBufferInfo {
    int32_t offset;
    int32_t size;
    int64_t presentationTimeUs;
    uint32_t flags;
};
typedef void __AMediaCodec;
typedef void __AMediaFormat;

namespace videocore { namespace Android {


    static bool s_initialized = false;
    static void* s_hMediaNdk = nullptr;

    typedef void (*amf_setString_t)(__AMediaFormat*, const char* name, const char* value);
    typedef void (*amf_setInt32_t)(__AMediaFormat*, const char* name, int32_t value);
    typedef void* (*amf_new_t)();
    typedef int (*amf_delete_t)(__AMediaFormat*);

    typedef __AMediaCodec* (*amc_createEncoderByType_t)(const char* name);
    typedef int (*amc_delete_t)(__AMediaCodec*);
    typedef int (*amc_configure_t)(__AMediaCodec*, const void* format, const void* surface, const void* crypto, uint32_t flags);
    typedef int (*amc_start_t)(__AMediaCodec*);
    typedef int (*amc_stop_t)(__AMediaCodec*);
	typedef uint8_t* (*amc_getInputBuffer_t)(__AMediaCodec*, size_t idx, size_t* out_size);
	typedef uint8_t* (*amc_getOutputBuffer_t)(__AMediaCodec*, size_t idx, size_t* out_size);
	typedef ssize_t (*amc_dequeueInputBuffer_t)(__AMediaCodec*, int64_t timeoutUs);
	typedef ssize_t (*amc_dequeueOutputBuffer_t)(__AMediaCodec*, __AMediaCodecBufferInfo* bufferInfo, int64_t timeoutUs);
	typedef void (*amc_queueInputBuffer_t)(__AMediaCodec*, size_t idx, off_t offset, size_t size, uint64_t time, uint32_t flags);
	typedef void (*amc_releaseOutputBuffer_t)(__AMediaCodec*, size_t idx, bool render);

    static amf_setString_t AMF_setString 	= nullptr;
    static amf_setInt32_t AMF_setInt32 		= nullptr;
    static amf_new_t AMF_new 				= nullptr;
    static amf_delete_t AMF_delete 			= nullptr;

    static amc_createEncoderByType_t AMC_createEncoderByType 	= nullptr;
    static amc_delete_t AMC_delete 						     	= nullptr;
    static amc_configure_t AMC_configure 						= nullptr;
    static amc_start_t AMC_start 								= nullptr;
    static amc_stop_t AMC_stop 									= nullptr;
    static amc_getInputBuffer_t AMC_getInputBuffer 				= nullptr;
    static amc_getOutputBuffer_t AMC_getOutputBuffer 			= nullptr;
    static amc_dequeueOutputBuffer_t AMC_dequeueOutputBuffer 	= nullptr;
    static amc_dequeueInputBuffer_t AMC_dequeueInputBuffer 		= nullptr;
    static amc_queueInputBuffer_t AMC_queueInputBuffer 			= nullptr;
    static amc_releaseOutputBuffer_t AMC_releaseOutputBuffer 	= nullptr;

    void
    VCMediaCodec::staticInit() {
    	if(!s_initialized) {
    		s_initialized = true;

    		s_hMediaNdk = dlopen("libmediandk.so", RTLD_LAZY); 
    		if(s_hMediaNdk) {
    			// We are on >= 5.0 and can use the direct C methods
    			DLog("Able to use direct NDK Media Framework\n");
    			AMF_setString 	= (amf_setString_t)dlsym(s_hMediaNdk, "AMediaFormat_setString");
    			AMF_setInt32 	= (amf_setInt32_t) dlsym(s_hMediaNdk, "AMediaFormat_setInt32");
    			AMF_new 		= (amf_new_t)      dlsym(s_hMediaNdk, "AMediaFormat_new");
    			AMF_delete 		= (amf_delete_t)   dlsym(s_hMediaNdk, "AMediaFormat_delete");

    			AMC_createEncoderByType 	= (amc_createEncoderByType_t)dlsym(s_hMediaNdk, "AMediaCodec_createEncoderByType");
    			AMC_delete 					= (amc_delete_t)             dlsym(s_hMediaNdk, "AMediaCodec_delete");
    			AMC_configure 				= (amc_configure_t)          dlsym(s_hMediaNdk, "AMediaCodec_configure");
    			AMC_start 					= (amc_start_t)              dlsym(s_hMediaNdk, "AMediaCodec_start");
    			AMC_stop 					= (amc_stop_t)               dlsym(s_hMediaNdk, "AMediaCodec_stop");
    			AMC_getInputBuffer 			= (amc_getInputBuffer_t)     dlsym(s_hMediaNdk, "AMediaCodec_getInputBuffer");
    			AMC_getOutputBuffer 	 	= (amc_getOutputBuffer_t)	 dlsym(s_hMediaNdk, "AMediaCodec_getOutputBuffer");
    			AMC_dequeueOutputBuffer     = (amc_dequeueOutputBuffer_t)dlsym(s_hMediaNdk, "AMediaCodec_dequeueOutputBuffer");
    			AMC_dequeueInputBuffer 		= (amc_dequeueInputBuffer_t) dlsym(s_hMediaNdk, "AMediaCodec_dequeueInputBuffer");
    			AMC_queueInputBuffer 		= (amc_queueInputBuffer_t)   dlsym(s_hMediaNdk, "AMediaCodec_queueInputBuffer");
    			AMC_releaseOutputBuffer 	= (amc_releaseOutputBuffer_t)dlsym(s_hMediaNdk, "AMediaCodec_releaseOutputBuffer");
    		} 
    	}
    }

    VCMediaCodec::VCMediaCodec(JavaVM* vm, std::string mime, int width, int height, int bitrate) 
    : m_vm(vm), m_pCodec(nullptr), m_pFmt(nullptr)
    {
    	VCMediaCodec::staticInit();
    	init();
    	if(s_hMediaNdk) {
    		m_pCodec = AMC_createEncoderByType(mime.c_str());
    		m_pFmt = AMF_new();
    		setString("mime", mime);    // The Java createVideoFormat takes mime, width, height
    		setInt32("width", width);
    		setInt32("height", height);

    	} else {
    		jstring jmime = m_env->NewStringUTF(mime.c_str());
    		m_oFmt = m_env->CallStaticObjectMethod(m_mfj.klass, m_mfj.createVideoFormat, jmime, width, height);
    		m_oCodec = m_env->CallStaticObjectMethod(m_mcj.klass, m_mcj.createEncoderByType, jmime);
    	}

    	setInt32("bitrate", bitrate);
    }
    VCMediaCodec::VCMediaCodec(JavaVM* vm, std::string mime, int samplerate, int channelcount)
    : m_vm(vm), m_pCodec(nullptr), m_pFmt(nullptr)
    {
    	VCMediaCodec::staticInit();
    	init();
    	if(s_hMediaNdk) {
    		m_pCodec = AMC_createEncoderByType(mime.c_str());
    		m_pFmt = AMF_new();
    		setString("mime", mime);    // The Java createVideoFormat takes mime, width, height
    		setInt32("channel-count", channelcount);
    		setInt32("sample-rate", samplerate);

    	} else {
    		jstring jmime = m_env->NewStringUTF(mime.c_str());
    		m_oFmt = m_env->CallStaticObjectMethod(m_mfj.klass, m_mfj.createAudioFormat, jmime, samplerate, channelcount);
    		m_oCodec = m_env->CallStaticObjectMethod(m_mcj.klass, m_mcj.createEncoderByType, jmime);
    	}
    }
    VCMediaCodec::~VCMediaCodec()
    {
    	DLog("VCMediaCodec::~VCMediaCodec\n");
    	stop();
    	if(!s_hMediaNdk) {
    		m_env->CallVoidMethod(m_oCodec, m_mcj.release);
    		m_vm->DetachCurrentThread();
    	} else {
    		AMC_delete(m_pCodec);
    		AMF_delete(m_pFmt);
    	}
    }
    void
    VCMediaCodec::init()
    {
    	if(!s_hMediaNdk) {
    		// We are on < 5.0 so we must use JNI functionality
    		DLog("No NDK media framework, using JNI\n");
    		m_vm->AttachCurrentThread(&m_env, nullptr);

			// MediaCodec
			m_mcj.klass =  m_env->FindClass("android/media/MediaCodec");
			m_mcj.createEncoderByType = m_env->GetStaticMethodID(m_mcj.klass, "createEncoderByType", "(Ljava/lang/String;)Landroid/media/MediaCodec;");
			m_mcj.configure = m_env->GetMethodID(m_mcj.klass, "configure", "(Landroid/media/MediaFormat;Landroid/view/Surface;Landroid/media/MediaCrypto;I)V");
			m_mcj.start = m_env->GetMethodID(m_mcj.klass, "start", "()V");
			m_mcj.stop = m_env->GetMethodID(m_mcj.klass, "stop", "()V");
			m_mcj.release = m_env->GetMethodID(m_mcj.klass, "release", "()V");
			m_mcj.getInputBuffer = m_env->GetMethodID(m_mcj.klass, "getInputBuffer", "(I)Ljava/nio/ByteBuffer;");
			m_mcj.getOutputBuffer = m_env->GetMethodID(m_mcj.klass, "getOutputBuffer", "(I)Ljava/nio/ByteBuffer;");
			m_mcj.dequeueInputBuffer = m_env->GetMethodID(m_mcj.klass, "dequeueInputBuffer", "(J)I");
			m_mcj.dequeueOutputBuffer = m_env->GetMethodID(m_mcj.klass, "dequeueOutputBuffer", "(Landroid/media/MediaCodec$BufferInfo;J)I");
			m_mcj.queueInputBuffer = m_env->GetMethodID(m_mcj.klass, "queueInputBuffer", "(IIIJI)V");
			m_mcj.releaseOutputBuffer = m_env->GetMethodID(m_mcj.klass, "releaseOutputBuffer", "(IZ)V");

			// MediaFormat
			m_mfj.klass = m_env->FindClass("android/media/MediaFormat");
			m_mfj.setInteger = m_env->GetMethodID(m_mfj.klass, "setInteger", "(Ljava/lang/String;I)V"); // void setInteger(String,Int)
			m_mfj.setString = m_env->GetMethodID(m_mfj.klass, "setString", "(Ljava/lang/String;Ljava/lang/String;)V");
			m_mfj.createVideoFormat = m_env->GetStaticMethodID(m_mfj.klass, "createVideoFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;");
			m_mfj.createAudioFormat = m_env->GetStaticMethodID(m_mfj.klass, "createAudioFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;");

			// BufferInfo
			m_bij.klass = m_env->FindClass("android/media/MediaCodec$BufferInfo");
			m_bij.init = m_env->GetMethodID(m_bij.klass, "<init>", "()V");
			m_bij.flags = m_env->GetFieldID(m_bij.klass, "flags", "I");
			m_bij.offset = m_env->GetFieldID(m_bij.klass, "offset", "I");
			m_bij.presentationTimeUs = m_env->GetFieldID(m_bij.klass, "presentationTimeUs", "J");
			m_bij.size = m_env->GetFieldID(m_bij.klass, "size", "I");

    	}
    }
    void
    VCMediaCodec::setString(std::string key, std::string value)
    {
    	if(s_hMediaNdk) {
    		AMF_setString(m_pFmt, key.c_str(), value.c_str());
    	} else {
    		jstring jkey = m_env->NewStringUTF(key.c_str());
    		jstring jvalue = m_env->NewStringUTF(value.c_str());
    		m_env->CallVoidMethod(m_oFmt, m_mfj.setString, jkey, jvalue);
    	}
    }
    void
    VCMediaCodec::setInt32(std::string key, int32_t value)
    {
    	if(s_hMediaNdk) {
    		AMF_setInt32(m_pFmt, key.c_str(), value);
    	} else {
    		jstring jkey = m_env->NewStringUTF(key.c_str());
    		m_env->CallVoidMethod(m_oFmt, m_mfj.setInteger, jkey, value);
    	}
    }
    void
    VCMediaCodec::start()
    {
    	if(s_hMediaNdk) {
    		AMC_start(m_pCodec);
    	} else {
    		m_env->CallVoidMethod(m_oCodec, m_mcj.start);
    	}
    }
    void
    VCMediaCodec::stop()
    {
    	if(s_hMediaNdk) {
    		AMC_stop(m_pCodec);
    	} else {
    		m_env->CallVoidMethod(m_oCodec, m_mcj.stop);
    	}
    }
    void
    VCMediaCodec::configure()
    {
    	if(s_hMediaNdk) {
    		AMC_configure(m_pCodec, m_pFmt, nullptr, nullptr, 1); // configure for encoding
    	} else {
    		m_env->CallVoidMethod(m_oCodec, m_mcj.configure, m_oFmt, 0, 0, 1);
    	}
    }
    uint8_t*
    VCMediaCodec::dequeueInputBuffer(size_t* outSize) 
    {
    	uint8_t* ptr = nullptr;
    	size_t size = 0;
    	ssize_t idx = -1;

    	if(s_hMediaNdk) {
    		idx = AMC_dequeueInputBuffer(m_pCodec, 0);
    		if ( idx >= 0 ) {
    			ptr = AMC_getInputBuffer(m_pCodec, idx, &size);
    			m_mapBufferIdx[ptr] = idx;
    		}
    	} else {
    		idx = m_env->CallIntMethod(m_oCodec, m_mcj.dequeueInputBuffer, 0);
    		if(idx >= 0) {
    			jobject buf = m_env->CallObjectMethod(m_oCodec, m_mcj.getInputBuffer, idx);
    			ptr = (uint8_t*)m_env->GetDirectBufferAddress(buf);
    			size = m_env->GetDirectBufferCapacity(buf);
    			m_mapBufferIdx[ptr] = idx;
    		}
    	}
    	if(outSize) {
    		*outSize = size;
    	}
    	return ptr;
    }
    void
    VCMediaCodec::enqueueInputBuffer(uint8_t* buffer, size_t size)
    {
    	if(!buffer) return ;

    	if(s_hMediaNdk) {
    		AMC_queueInputBuffer(m_pCodec, m_mapBufferIdx[buffer], 0, size, 0, 0);
    	} else {
    		m_env->CallVoidMethod(m_oCodec, m_mcj.queueInputBuffer, m_mapBufferIdx[buffer], 0, size, 0, 0);
    	}
    }
    uint8_t*
    VCMediaCodec::dequeueOutputBuffer(size_t* outSize)
    {
    	uint8_t* ptr = nullptr;
    	size_t size;
    	ssize_t idx = -1;

    	if(s_hMediaNdk) {
    		__AMediaCodecBufferInfo bufferInfo{};
    		idx = AMC_dequeueOutputBuffer(m_pCodec, &bufferInfo, 0);
    		if(idx >= 0) {
    			ptr = AMC_getOutputBuffer(m_pCodec, idx, &size);
    			m_mapBufferIdx[ptr] = idx;
    		}
    	} else {
    		jobject bufferInfo = m_env->NewObject(m_bij.klass, m_bij.init);

    		idx = m_env->CallIntMethod(m_oCodec, m_mcj.dequeueOutputBuffer, bufferInfo, 0);
    		if(idx >= 0) {
    			jobject buf = m_env->CallObjectMethod(m_oCodec, m_mcj.getOutputBuffer, idx);
    			ptr = (uint8_t*)m_env->GetDirectBufferAddress(buf);
    			size = m_env->GetDirectBufferCapacity(buf);
    			m_mapBufferIdx[ptr] = idx;
    		}
    	}
    	if(outSize) {
    		*outSize = size;
    	}
    	return ptr;
    }
    void 
    VCMediaCodec::releaseOutputBuffer(uint8_t* buffer)
    {
    	if(!buffer) return;

    	if(s_hMediaNdk) {
    		AMC_releaseOutputBuffer(m_pCodec, m_mapBufferIdx[buffer], false);
    	} else {
    		m_env->CallVoidMethod(m_oCodec, m_mcj.releaseOutputBuffer, m_mapBufferIdx[buffer], false);
    	}
    }

}}