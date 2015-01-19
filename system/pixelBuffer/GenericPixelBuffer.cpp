#include <videocore/system/pixelBuffer/GenericPixelBuffer.h>

namespace videocore {

	GenericPixelBuffer::GenericPixelBuffer(int width, int height, PixelBufferFormatType pixelFormat)
	{
		size_t bufferSize = 0;
		switch(pixelFormat) {
			case kVCPixelBufferFormat32BGRA:
			case kVCPixelBufferFormat32RGBA:
				bufferSize = width*height*4;
				break;
			case kVCPixelBufferFormatL565:
				bufferSize = width*height*2;
				break;
			case kCVPixelBufferFormat420v:
				bufferSize = width*height*1.5;
				break;

		}
		
		m_buffer = std::make_shared<std::vector<uint8_t>>(bufferSize);
	}

}