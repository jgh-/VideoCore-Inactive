//
//  Loggeer.hpp
//  Pods
//
//  Created by jinchu darwin on 15/10/8.
//
//

#ifndef Loggeer_hpp
#define Loggeer_hpp

#include <videocore/system/util.h>
#include <string>

namespace videocore {
    class Logger {
    public:
        static void log(bool synchronous,
                        int level,
                        int flag,
                        int ctx,
                        const char *file,
                        const char *function,
                        int line,
                        const char* frmt,
                        ...)  __attribute__ ((format (printf, 8, 9)));
        static void dumpBuffer(const char *desc, uint8_t *buf, size_t size, const char *sep=" ", size_t breaklen = 16);
    };

    // Define DLOG_LEVEL_DEF to control the output log message
#define DLOG_MACRO(isAsynchronous, lvl, flg, ctx, fnct, frmt, ...) \
videocore::Logger::log(isAsynchronous, lvl, flg, ctx, __FILE__, fnct, __LINE__, frmt, ##__VA_ARGS__);
    
#define  SYNC_DLOG_MACRO(lvl, flg, ctx, frmt, ...) \
DLOG_MACRO(false, lvl, flg, ctx, frmt, ##__VA_ARGS__)
    
#define ASYNC_DLOG_MACRO(lvl, flg, ctx, frmt, ...) \
DLOG_MACRO(true , lvl, flg, ctx, frmt, ##__VA_ARGS__)
}


#define DLOG_MAYBE(async, lvl, flg, ctx, frmt, ...) \
do { if(lvl & flg) DLOG_MACRO(async, lvl, flg, ctx, __FUNCTION__, frmt, ##__VA_ARGS__); } while(0)

#define  SYNC_DLOG_MAYBE(lvl, flg, ctx, frmt, ...) \
DLOG_MAYBE( NO, lvl, flg, ctx, frmt, ##__VA_ARGS__)

#define ASYNC_LOG_MAYBE(lvl, flg, ctx, frmt, ...) \
DLOG_MAYBE(YES, lvl, flg, ctx, frmt, ##__VA_ARGS__)

// FLAG value
#define DLOG_FLAG_ERROR    (1 << 0)
#define DLOG_FLAG_WARN     (1 << 1)
#define DLOG_FLAG_INFO     (1 << 2)
#define DLOG_FLAG_DEBUG    (1 << 3)
#define DLOG_FLAG_VERBOSE  (1 << 4)

// LAVEL value
#define DLOG_LEVEL_OFF     0
#define DLOG_LEVEL_ERROR   (DLOG_FLAG_ERROR)
#define DLOG_LEVEL_WARN    (DLOG_FLAG_ERROR | DLOG_FLAG_WARN)
#define DLOG_LEVEL_INFO    (DLOG_FLAG_ERROR | DLOG_FLAG_WARN | DLOG_FLAG_INFO)
#define DLOG_LEVEL_DEBUG   (DLOG_FLAG_ERROR | DLOG_FLAG_WARN | DLOG_FLAG_INFO | DLOG_FLAG_DEBUG)
#define DLOG_LEVEL_VERBOSE (DLOG_FLAG_ERROR | DLOG_FLAG_WARN | DLOG_FLAG_INFO | DLOG_FLAG_DEBUG | DLOG_FLAG_VERBOSE)

// Can log value
#define DLOG_ERROR    (DLOG_LEVEL_DEF & DLOG_FLAG_ERROR)
#define DLOG_WARN     (DLOG_LEVEL_DEF & DLOG_FLAG_WARN)
#define DLOG_INFO     (DLOG_LEVEL_DEF & DLOG_FLAG_INFO)
#define DLOG_DEBUG    (DLOG_LEVEL_DEF & DLOG_FLAG_DEBUG)
#define DLOG_VERBOSE  (DLOG_LEVEL_DEF & DLOG_FLAG_VERBOSE)

#define DLOG_ASYNC_ENABLED 1

// Can async log value
#define DLOG_ASYNC_ERROR    (0 && DLOG_ASYNC_ENABLED)
#define DLOG_ASYNC_WARN     (1 && DLOG_ASYNC_ENABLED)
#define DLOG_ASYNC_INFO     (1 && DLOG_ASYNC_ENABLED)
#define DLOG_ASYNC_DEBUG    (1 && DLOG_ASYNC_ENABLED)
#define DLOG_ASYNC_VERBOSE  (1 && DLOG_ASYNC_ENABLED)

// Log macros
#define DLogError(frmt, ...)   DLOG_MAYBE(DLOG_ASYNC_ERROR,   DLOG_LEVEL_DEF, DLOG_FLAG_ERROR,   0, frmt, ##__VA_ARGS__)
#define DLogWarn(frmt, ...)    DLOG_MAYBE(DLOG_ASYNC_WARN,    DLOG_LEVEL_DEF, DLOG_FLAG_WARN,    0, frmt, ##__VA_ARGS__)
#define DLogInfo(frmt, ...)    DLOG_MAYBE(DLOG_ASYNC_INFO,    DLOG_LEVEL_DEF, DLOG_FLAG_INFO,    0, frmt, ##__VA_ARGS__)
#define DLogDebug(frmt, ...)   DLOG_MAYBE(DLOG_ASYNC_DEBUG,   DLOG_LEVEL_DEF, DLOG_FLAG_DEBUG,   0, frmt, ##__VA_ARGS__)
#define DLogVerbose(frmt, ...) DLOG_MAYBE(DLOG_ASYNC_VERBOSE, DLOG_LEVEL_DEF, DLOG_FLAG_VERBOSE, 0, frmt, ##__VA_ARGS__)

#endif /* Loggeer_hpp */
