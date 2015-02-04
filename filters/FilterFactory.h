

#ifndef __videocore__FilterFactory__
#define __videocore__FilterFactory__
#include <videocore/filters/IFilter.hpp>
#include <map>
#include <memory>
#include <functional>
namespace videocore {
    
    using InstantiateFilter = std::function<IFilter*()> ;
    
    class FilterFactory {
    public:
        FilterFactory();
        ~FilterFactory() {};
        
        IFilter* filter(std::string name);
        
    public:
        static void _register(std::string name, InstantiateFilter instantiation );
        
    private:
        std::map<std::string, std::unique_ptr<IFilter>> m_filters;
        
    private:
        static std::map<std::string, InstantiateFilter>* s_registration;
    };
    
}

#endif
