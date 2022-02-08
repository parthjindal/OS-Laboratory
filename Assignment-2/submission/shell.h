#ifndef _SHELL_H
#define _SHELL_H

#include <string>
#include <vector>

#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define RESET "\033[0m"

// https://github.com/kuroidoruido/ColorLog/blob/master/colorlog.h
#define _COLOR_RED "1;31"
#define _COLOR_BLUE "1;34"
#define _COLOR_GREEN "0;32"

#define __LOG_COLOR(FD, CLR, CTX, TXT, args...) fprintf(FD, "\033[%sm[%s] \033[0m" TXT, CLR, CTX, ##args)
#define INFO_LOG(TXT, args...) __LOG_COLOR(stdout, _COLOR_GREEN, "info", TXT, ##args)
#define DEBUG_LOG(TXT, args...) __LOG_COLOR(stderr, _COLOR_BLUE, "debug", TXT, ##args)
#define ERROR_LOG(TXT, args...) __LOG_COLOR(stderr, _COLOR_RED, "error", TXT, ##args)

extern std::string CDIR;
extern int prompt_len;
extern std::string prompt_str;
extern bool CONTINUE;

std::string getinput();

#define PROMPT                                             \
    do {                                                   \
        std::cout << GREEN << CDIR << RESET << prompt_str; \
        prompt_len = CDIR.size() + prompt_str.size();      \
    } while (0)

#endif  // _SHELL_H