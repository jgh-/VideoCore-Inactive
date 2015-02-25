

#ifndef __mobcrush__BasicVideoFilterYUVAinNV12out__
#define __mobcrush__BasicVideoFilterYUVAinNV12out__

#include <videocore/filters/IVideoFilter.hpp>

namespace videocore {
    namespace filters {
        class BasicVideoFilterYUVAinNV12out : public IVideoFilter {
            
        public:
            BasicVideoFilterYUVAinNV12out();
            ~BasicVideoFilterYUVAinNV12out();
            
        public:
            virtual void initialize();
            virtual bool initialized() const { return m_initialized; };
            virtual std::string const name() { return "com.videocore.filters.yuva2nv12"; };
            virtual void bind();
            virtual void unbind();
            
        public:
            
            const char * const vertexKernel() const ;
            const char * const pixelKernel() const ;
            
        private:
            static bool registerFilter();
            static bool s_registered;
        private:
            
            unsigned int m_vao;
            unsigned int m_uMatrix;
            unsigned int m_uScreenSize;
            
            bool m_initialized;
            bool m_bound;
            
        };
    }
}
#endif /* defined(__mobcrush__BasicVideoFilterYUVAinNV12out__) */
