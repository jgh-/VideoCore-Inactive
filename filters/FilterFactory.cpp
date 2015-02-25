#include <videocore/filters/FilterFactory.h>
#include <videocore/filters/Basic/BasicVideoFilterBGRA.h>
#include <videocore/filters/Basic/BasicVideoFilterBGRAinYUVAout.h>
#include <videocore/filters/Basic/BasicVideoFilterYUVAinNV12out.h>
#include <videocore/filters/Basic/BasicVideoFilterYUVAinYUVPlanarOut.h>
#include <videocore/filters/Basic/GrayscaleVideoFilter.h>

namespace videocore {
    std::map<std::string, InstantiateFilter>* FilterFactory::s_registration = nullptr ;
    
    FilterFactory::FilterFactory() {
        {
            filters::BasicVideoFilterBGRA b;
            filters::BasicVideoFilterBGRAinYUVAout y;
            filters::BasicVideoFilterYUVAinNV12out n;
            filters::BasicVideoFilterYUVAinYUVPlanarOut p;
            filters::GrayscaleVideoFilter g;
        }
    }
    IFilter*
    FilterFactory::filter(std::string name) {
        IFilter* filter = nullptr;
        
        auto it = m_filters.find(name) ;
        if( it != m_filters.end() ) {
            filter = it->second.get();
        } else if (FilterFactory::s_registration != nullptr) {
            auto iit = FilterFactory::s_registration->find(name);
            
            if(iit != FilterFactory::s_registration->end()) {
                m_filters[name].reset(iit->second());
                filter = m_filters[name].get();
            }
        }
        
        return filter;
    }
    
    void
    FilterFactory::_register(std::string name, InstantiateFilter instantiation)
    {
        if(!s_registration) {
            s_registration = new std::map<std::string, InstantiateFilter>();
        }
        s_registration->emplace(std::make_pair(name, instantiation));
    }
}