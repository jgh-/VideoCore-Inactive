#include <videocore/transforms/Android/H264Encode.h>
#include <videocore/system/pixelBuffer/Android/PixelBuffer.h>

#include <jni.h>

namespace videocore { namespace Android {
	
	H264Encode::H264Encode(JavaVM* vm, int frame_w, int frame_h, int fps, int bitrate)
	: m_vm(vm), m_frameW(frame_w), m_frameH(frame_h), m_fps(fps), m_bitrate(bitrate)
	{
		m_queue.enqueue([=]{
			m_vm->AttachCurrentThread(&m_env, nullptr);
			jclass tmp;
			// MediaCodec
			m_codec.klass =  m_env->FindClass("android/media/MediaCodec");
			m_codec.createEncoderByType = m_env->GetStaticMethodID(m_codec.klass, "createEncoderByType", "(Ljava/lang/String;)Landroid/media/MediaCodec;");
			m_codec.configure = m_env->GetMethodID(m_codec.klass, "configure", "(Landroid/media/MediaFormat;Landroid/view/Surface;Landroid/media/MediaCrypto;I)V");
			m_codec.start = m_env->GetMethodID(m_codec.klass, "start", "()V");
			m_codec.stop = m_env->GetMethodID(m_codec.klass, "stop", "()V");

			// MediaFormat
			m_format.klass = m_env->FindClass("android/media/MediaFormat");
			m_format.setInteger = m_env->GetMethodID(m_format.klass, "setInteger", "(Ljava/lang/String;I)V"); // void setInteger(String,Int)
			m_format.createVideoFormat = m_env->GetStaticMethodID(m_format.klass, "createVideoFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;");

			DLog("Creating encoder..\n");
			jstring mime = m_env->NewStringUTF("video/avc");
			m_codec.obj = m_env->CallStaticObjectMethod(m_codec.klass, m_codec.createEncoderByType, mime);

			m_format.obj = m_env->CallStaticObjectMethod(m_format.klass, m_format.createVideoFormat, mime, frame_w, frame_h);

			jstring key_bitrate = m_env->NewStringUTF("bitrate");
			jstring key_fps = m_env->NewStringUTF("frame-rate");
			jstring key_kfi = m_env->NewStringUTF("i-frame-interval");
			jstring key_color_fmt = m_env->NewStringUTF("color-format");
			m_env->CallVoidMethod(m_format.obj, m_format.setInteger, key_bitrate, bitrate);
			m_env->CallVoidMethod(m_format.obj, m_format.setInteger, key_fps, fps);
			m_env->CallVoidMethod(m_format.obj, m_format.setInteger, key_kfi, 2);		 // 2 seconds
			m_env->CallVoidMethod(m_format.obj, m_format.setInteger, key_color_fmt, 21); // COLOR_FormatYUV420SemiPlanar
			DLog("Attempting to configure encoder...\n");
			m_env->CallVoidMethod(m_codec.obj, m_codec.configure, m_format.obj, 0, 0, 1);
			m_env->CallVoidMethod(m_codec.obj, m_codec.start);
			DLog("Done: %p\n", m_codec.obj);
		});
	}

	H264Encode::~H264Encode() {
		m_queue.enqueue_sync([&]{
			DLog("calling DetachCurrentThread\n");
			m_env->CallVoidMethod(m_codec.obj, m_codec.stop);
			m_vm->DetachCurrentThread();
		});

	}
	void 
	H264Encode::pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata) {
		//std::shared_ptr<PixelBuffer> img = *(std::shared_ptr<PixelBuffer>*) data;
		//m_imageSource->setBuffer(img);
	}
	void 
	H264Encode::setBitrate(int bitrate) {

	}
	void
	H264Encode::requestKeyframe() {
		
	}
}}