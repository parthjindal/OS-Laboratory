#ifndef _PARSER_H
#define _PARSER_H
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "process.h"

class Parser {
   public:
    bool is_builtin;
    std::string builtin_cmd, cmd;
    std::vector<std::string> builtin_argv;
    Parser() {
        is_builtin = false;
        builtin_cmd = "";
        cmd = "";
    }
    ~Parser() {}
    void trim(std::string& s, char delim = ' ');
    std::vector<std::string> tokenize(const std::string& _cmd, char delim);
    void parse_process(const std::string& s, Process& p);
    void parse_job(const std::string& s, Job& job);
    void parse(const std::string& inp, std::vector<Job*>& joblist, int& numJobs);
};

#endif  // _PARSER_H