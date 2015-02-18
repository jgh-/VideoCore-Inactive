#include <videocore/transforms/Android/VCMediaCodec.h>
#include <videocore/system/util.h>
#include <dlfcn.h>

namespace videocore { namespace Android {


    static bool s_initialized = false;
    static void* s_hMediaNdk = nullptr;

    typedef void (*amf_setString_t)(void*, const char* name, const char* value);
    typedef void (*amf_setInt32_t)(void*, const char* name, int32_t value);
    typedef void* (*amf_new_t)();
    typedef int (*amf_delete_t)(void*);

    typedef void* (*amc_createEncoderByType_t)(const char* name);
    typedef int (*amc_delete_t)(void*);
    typedef int (*amc_configure_t)(void*, const void* format, const void* surface, const void* crypto, uint32_t flags);
    typedef int (*amc_start_t)(void*);
    typedef int (*amc_stop_t)(void*);

    static amf_setString_t AMF_setString = nullptr;
    static amf_setInt32_t AMF_setInt32 = nullptr;
    static amf_new_t AMF_new = nullptr;
    static amf_delete_t AMF_delete = nullptr;

    static amc_createEncoderByType_t AMC_createEncoderByType = nullptr;
    static amc_delete_t AMC_delete = nullptr;
    static amc_configure_t AMC_configure = nullptr;
    static amc_start_t AMC_start = nullptr;
    static amc_stop_t AMC_stop = nullptr;

    void
    VCMediaCodec::staticInit() {
    	if(!s_initialized) {
    		s_initialized = true;

    		s_hMediaNdk = dlopen("libmediandk.so", RTLD_LAZY); 
    		if(s_hMediaNdk) {
    			// We are on >= 5.0 and can use the direct C methods
    			DLog("Able to use direct NDK Media Framework\n");
    			AMF_setString 	= (amf_setString_t)dlsym(s_hMediaNdk, "AMediaFormat_setString");
    			AMF_setInt32 	= (amf_setInt32_t)dlsym(s_hMediaNdk, "AMediaFormat_setInt32");
    			AMF_new 		= (amf_new_t)dlsym(s_hMediaNdk, "AMediaFormat_new");
    			AMF_delete 		= (amf_delete_t)dlsym(s_hMediaNdk, "AMediaFormat_delete");

    			AMC_createEncoderByType 	= (amc_createEncoderByType_t)dlsym(s_hMediaNdk, "AMediaCodec_createEncoderByType");
    			AMC_delete 					= (amc_delete_t)dlsym(s_hMediaNdk, "AMediaCodec_delete");
    			AMC_configure 				= (amc_configure_t)dlsym(s_hMediaNdk, "AMediaCodec_configure");
    			AMC_start 					= (amc_start_t)dlsym(s_hMediaNdk, "AMediaCodec_start");
    			AMC_stop 					= (amc_stop_t)dlsym(s_hMediaNdk, "AMediaCodec_stop");
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

			// MediaFormat
			m_mfj.klass = m_env->FindClass("android/media/MediaFormat");
			m_mfj.setInteger = m_env->GetMethodID(m_mfj.klass, "setInteger", "(Ljava/lang/String;I)V"); // void setInteger(String,Int)
			m_mfj.setString = m_env->GetMethodID(m_mfj.klass, "setString", "(Ljava/lang/String;Ljava/lang/String;)V");
			m_mfj.createVideoFormat = m_env->GetStaticMethodID(m_mfj.klass, "createVideoFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;");
			m_mfj.createAudioFormat = m_env->GetStaticMethodID(m_mfj.klass, "createAudioFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;");

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
}}