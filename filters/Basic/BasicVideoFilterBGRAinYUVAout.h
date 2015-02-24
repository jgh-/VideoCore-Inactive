
#ifndef __mobcrush__BasicVideoFilterBGRAinYUVAout__
#define __mobcrush__BasicVideoFilterBGRAinYUVAout__

#include <videocore/filters/IVideoFilter.hpp>

namespace videocore {
    namespace filters {
        class BasicVideoFilterBGRAinYUVAout : public IVideoFilter {
            
        public:
            BasicVideoFilterBGRAinYUVAout();
            ~BasicVideoFilterBGRAinYUVAout();
            
        public:
            virtual void initialize();
            virtual bool initialized() const { return m_initialized; };
            virtual std::string const name() { return "com.videocore.filters.bgra2yuva"; };
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
            bool m_initialized;
            bool m_bound;
            
        };
    }
}

#endif /* defined(__mobcrush__BasicVideoFilterBGRAinYUVAout__) */
