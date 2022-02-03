#ifndef _SHELL_H
#define _SHELL_H

#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define RESET "\033[0m"

#define HISTORY_FILE ".terminal_history.txt"
#define HISTORY_SIZE 10000
#define HISTORY_PRINT 1000

static FILE* _logFp = NULL;
static void initLogger(const char* logFile) {
    _logFp = logFile ? fopen(logFile, "w") : stdout;
}

static void log_print(FILE* fp, const char* fmt, ...) {
    if (_logFp != NULL)
        fp = _logFp;
    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    fflush(fp);
    va_end(args);
}

// https://github.com/kuroidoruido/ColorLog/blob/master/colorlog.h
#define _COLOR_RED "1;31"
#define _COLOR_BLUE "1;34"
#define _COLOR_GREEN "0;32"

#define __LOG_COLOR(FD, CLR, CTX, TXT, args...) log_print(FD, "\033[%sm[%s] \033[0m" TXT, CLR, CTX, ##args)
#define INFO_LOG(TXT, args...) __LOG_COLOR(stdout, _COLOR_GREEN, "info", TXT, ##args)
#define DEBUG_LOG(TXT, args...) __LOG_COLOR(stderr, _COLOR_BLUE, "debug", TXT, ##args)
#define ERROR_LOG(TXT, args...) __LOG_COLOR(stderr, _COLOR_RED, "error", TXT, ##args)

// class Parser {

//     int _bg;
//     std::regex re;

//    public:
//     Parser(){
//         re = R"([\s]|"
//     }
//     parse(const std::string& cmd,
//           vector<string>& tokens) {
//         const std
//     }
// }

#endif  // _SHELL_H