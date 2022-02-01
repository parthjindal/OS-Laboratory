#include "parser.h"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

vector<string> Parser::tokenize(const string& _cmd, char delim) {
    vector<string> argv;
    stringstream ss(_cmd);
    string token;
    while (getline(ss, token, delim)) {
        if (token.size() != 0) argv.push_back(token);
    }
    return argv;
}

void Parser::trim(string& s, char delim) {
    if (s.size() == 0) return;
    size_t start = s.find_first_not_of(delim);
    size_t end = s.find_last_not_of(delim);
    s = s.substr(start, end - start + 1);
}

void Parser::parse_process(const string& s, Process& p) {
    vector<string> _argv = tokenize(s, ' ');
    for (int i = 0; i < int(_argv.size()); i++) {
        if (_argv[i] == "&") {
            p.bg = true;
        } else if (_argv[i] == "<") {
            p.infile = _argv[i + 1];
            i++;
        } else if (_argv[i] == ">") {
            p.outfile = _argv[i + 1];
            i++;
        } else if (_argv[i].size() != 0) {
            p.argv.push_back(_argv[i]);
        }
    }
    p.argc = p.argv.size();
    p.cmd = "";
    for (int i = 0; i < p.argc; i++) {
        p.cmd += p.argv[i];
        if (i != p.argc - 1)
            p.cmd += " ";
    }
}

void Parser::parse_job(const string& s, Job& job) {
    vector<string> _procc_cmds = tokenize(s, '|');
    for (auto it = _procc_cmds.begin(); it != _procc_cmds.end(); it++) {
        string cpy = *it;
        trim(cpy, ' ');
        Process p(cpy);
        parse_process(cpy, p);
        job.processes.push_back(p);
    }
}

void Parser::parse(const string& inp, vector<Job*>& joblist, int& numJobs) {
    stringstream ss(inp);
    string token;
    getline(ss, token, ' ');
    while (token == " ") {
        getline(ss, token, ' ');
    }
    if (token == " ")
        return;
    trim(token);
    if (token == "exit") {
        is_builtin = true;
        builtin_cmd = "exit";
        numJobs = 0;
    } else if (token == "jobs") {
        is_builtin = true;
        builtin_cmd = "jobs";
        numJobs = 0;

    } else if (token == "fg") {
        is_builtin = true;
        builtin_cmd = "fg";
        getline(ss, token, ' ');
        trim(token);
        builtin_argv.push_back(token);
        numJobs = 0;

    } else if (token == "bg") {
        is_builtin = true;
        builtin_cmd = "bg";
        getline(ss, token, ' ');
        trim(token);
        builtin_argv.push_back(token);
        numJobs = 0;

    } else if (token == "multiwatch") {
                is_builtin = true;
        builtin_cmd = "multiwatch";

        getline(ss, token, '[');
        getline(ss, token, ']');
        // cout << token << endl;
        vector<string> _cmds = tokenize(token, ',');
        for (auto it = _cmds.begin(); it != _cmds.end(); it++) {
            string cpy = *it;
            trim(cpy, ' ');
            trim(cpy, '"');
            Job* job = new Job();
            parse_job(cpy, *job);
            joblist.push_back(job);
            numJobs++;
        }
        getline(ss, token, '>');
        getline(ss, token, '>');
        trim(token);
        if (token.size() != 0) {
            builtin_argv.push_back(token);  // outfile
        }
    } else {
        Job* job = new Job();
        parse_job(inp, *job);
        joblist.push_back(job);
        numJobs++;
    }
}

// int main() {
//     string cmd;
//     vector<Job*> joblist;
//     int numJobs = 0;
//     getline(cin, cmd);
//     Parser p(cmd);
//     p.parse(cmd, joblist, numJobs);
//     if (p.is_builtin) {
//         cout << "builtin: " << p.builtin_cmd << endl;
//         for (auto it = p.builtin_argv.begin(); it != p.builtin_argv.end(); it++) {
//             cout << *it << endl;
//         }
//     }
//     cout << "numJobs: " << numJobs << endl;
//     for (int i = 0; i < numJobs; i++) {
//         cout << *joblist[i] << endl;
//     }
//     return 0;
// }