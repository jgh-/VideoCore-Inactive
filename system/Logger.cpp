//
//  Loggeer.cpp
//  Pods
//
//  Created by jinchu darwin on 15/10/8.
//
//

#include <videocore/system/Logger.hpp>

#include <cstdarg>

namespace videocore {

    void Logger::log(bool synchronous, int level, int flag, int ctx, const char *file, const char *function, int line, const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        
        vfprintf(stdout, format, args);
        va_end(args);
    }
    
    void dumpBuffer(const char *desc, uint8_t *buf, size_t size) {
        char hexBuf[size * 4 + 1];
        int j = 0;
        for (size_t i=0; i<size; i++) {
            sprintf(&hexBuf[j], "%02x ", buf[i]);
            j += 3;
            if (i % 16 == 15) {
                sprintf(&hexBuf[j], "\n");
                j++;
            }
        }
        hexBuf[j] = '\0';
        DLog("%s:\n%s\n", desc, hexBuf);
    }
}
