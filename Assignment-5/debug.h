#define DEBUG_LEVEL 4

#define _INFO_L 1
#define _DEBUG_L 2
#define _ERROR_L 3

#define _COLOR_RED "1;31"
#define _COLOR_BLUE "1;34"
#define _COLOR_GREEN "0;32"

#define __LOG_COLOR(FD, CLR, CTX, TXT, args...) fprintf(FD, "\033[%sm[%s]: \033[0m" TXT, CLR, CTX, ##args)
#define INFO(TXT, args...)                                                                  \
    do {                                                                                    \
        if (DEBUG_LEVEL >= _INFO_L) __LOG_COLOR(stdout, _COLOR_GREEN, "info", TXT, ##args); \
    } while (0)
#define DEBUG(TXT, args...)                                                                  \
    do {                                                                                     \
        if (DEBUG_LEVEL >= _DEBUG_L) __LOG_COLOR(stdout, _COLOR_BLUE, "debug", TXT, ##args); \
    } while (0)
#define ERROR(TXT, args...)                                                                 \
    do {                                                                                    \
        if (DEBUG_LEVEL >= _ERROR_L) __LOG_COLOR(stderr, _COLOR_RED, "error", TXT, ##args); \
    } while (0)

#define LOG(HEADER, COLOR, args...)                                             \
    do {                                                                        \
        if (DEBUG_LEVEL >= _INFO_L) __LOG_COLOR(stdout, COLOR, HEADER, ##args); \
    } while (0)

#define PTHREAD_MUTEX_LOCK(mutex_p)                                              \
    do {                                                                         \
        int ret = pthread_mutex_lock(mutex_p);                                   \
        if (ret != 0) {                                                          \
            ERROR("%d: pthread_mutex_lock failed: %s", __LINE__, strerror(ret)); \
            exit(1);                                                             \
        }                                                                        \
    } while (0)

#define PTHREAD_MUTEX_UNLOCK(mutex_p)                                              \
    do {                                                                           \
        int ret = pthread_mutex_unlock(mutex_p);                                   \
        if (ret != 0) {                                                            \
            ERROR("%d: pthread_mutex_unlock failed: %s", __LINE__, strerror(ret)); \
            exit(1);                                                               \
        }                                                                          \
    } while (0)
