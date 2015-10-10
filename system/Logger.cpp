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

    // 简单的实现，直接输出日志就好
    void Logger::log(bool synchronous, int level, int flag, int ctx, const char *file, const char *function, int line, const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        
        vfprintf(stdout, format, args);
        va_end(args);
    }
    
    void Logger::dumpBuffer(const char *desc, uint8_t *buf, size_t size, const char *sep, size_t breaklen) {
        char hexBuf[size * 4 + 1];
        int j = 0;
        for (size_t i=0; i<size; i++) {
            sprintf(&hexBuf[j], "%02x%s", buf[i], sep ? sep : "");
            j += 3;
            if (i % breaklen == breaklen-1) {
                sprintf(&hexBuf[j], "\n");
                j++;
            }
        }
        hexBuf[j] = '\0';
        DLog("%s:\n%s\n", desc, hexBuf);
    }
}
