#include <videocore/system/pixelBuffer/Android/GraphicBuffer.h>
#include <dlfcn.h>

namespace videocore { namespace Android {
    typedef android::status_t (*pfnGraphicsBufferLock)(void*, uint32_t usage, void** addr);
    typedef android::status_t (*pfnGraphicsBufferUnlock)(void*);
    typedef android_native_buffer_t* (*pfnGraphicsBufferGetNativeBuffer)(void*);
    typedef void (*pfnGraphicsBufferCtor)(void*, uint32_t w, uint32_t h, android::PixelFormat f, uint32_t u);
    typedef void (*pfnGraphicsBufferDtor)(void*);


    pfnGraphicsBufferLock fGraphicsBufferLock;
    pfnGraphicsBufferUnlock fGraphicsBufferUnlock;
    pfnGraphicsBufferGetNativeBuffer fGraphicsBufferGetNativeBuffer;
    pfnGraphicsBufferCtor fGraphicsBufferCtor;
    pfnGraphicsBufferDtor fGraphicsBufferDtor;

    bool GraphicBuffer::s_isSetup = false;

    void
    GraphicBuffer::setup()
    {
        if(!s_isSetup)
        {
            s_isSetup = true;
            void* handle = dlopen("libui.so", RTLD_LAZY);

            fGraphicsBufferLock = (pfnGraphicsBufferLock)dlsym(handle, "_ZN7android13GraphicBuffer4lockEjPPv");
            fGraphicsBufferUnlock = (pfnGraphicsBufferUnlock)dlsym(handle, "_ZN7android13GraphicBuffer6unlockEv");
            fGraphicsBufferGetNativeBuffer = (pfnGraphicsBufferGetNativeBuffer)dlsym(handle, "_ZNK7android13GraphicBuffer15getNativeBufferEv");
            fGraphicsBufferCtor = (pfnGraphicsBufferCtor)dlsym(handle, "_ZN7android13GraphicBufferC1Ejjij");
            fGraphicsBufferDtor = (pfnGraphicsBufferDtor)dlsym(handle, "_ZN7android13GraphicBufferD1Ev");

        }
    }

    GraphicBuffer::GraphicBuffer(uint32_t width, uint32_t height, android::PixelFormat format, uint32_t usage)
    : m_width(width), m_height(height), m_format(format)
    {
    	GraphicBuffer::setup();

    	m_buffer = malloc(sizeof(android::GraphicBuffer));
    	fGraphicsBufferCtor(m_buffer, width, height, format, usage);

    }
    GraphicBuffer::~GraphicBuffer() 
    {
    	fGraphicsBufferDtor(m_buffer);
    	free(m_buffer);
    	m_buffer = nullptr;
    }
    
    android::status_t
    GraphicBuffer::lock(uint32_t usage, void** addr)
    {
    	return fGraphicsBufferLock(m_buffer, usage, addr);
    }

    android::status_t
    GraphicBuffer::unlock()
    {
    	return fGraphicsBufferUnlock(m_buffer);
    }

    android_native_buffer_t*
    GraphicBuffer::getNativeBuffer() const
    {
    	return fGraphicsBufferGetNativeBuffer(m_buffer);
    }

}}