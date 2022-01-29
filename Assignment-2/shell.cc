#include "shell.h"

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <climits>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <map>
#include <vector>

using namespace std;

int argc = 0;
vector<char*> argv;
pid_t cpid;
pid_t gpid;
vector<int> bg_pids;  // background pids

void sig_handler(int sig) {
    if (cpid == gpid) {
        std::cin.setstate(std::ios::badbit);
        return;
    }
    if (cpid == getpid()) {  // child process
        signal(sig, SIG_DFL);
        kill(cpid, sig);
    } else {  // parent process
        if (sig == SIGTSTP)
            bg_pids.push_back(cpid);
        kill(cpid, sig);
    }
}

void tokenize(const string& cmd, vector<char*>& argv) {
    stringstream ss(cmd);
    string t;
    while (getline(ss, t, ' ')) {
        char* cstr = new char[t.length() + 1];
        strcpy(cstr, t.c_str());
        argv.push_back(cstr);
    }
    argc = argv.size();
    argv.push_back(NULL);
}

int prompt(string& cmd) {
    char buff[PATH_MAX];
    getcwd(buff, PATH_MAX);
    std::string wcd(buff);
    std::string wd = wcd.substr(wcd.find_last_of("/") + 1);
    cout << GREEN << wd << RESET << "$ ";
    getline(cin, cmd);
    if (cin.bad()) {
        cin.clear();
        cout << endl;
    }
}

int main() {
    string cmd;
    gpid = cpid = getpid();  // get subgroup id

    signal(SIGTSTP, sig_handler);  // ctrl-z
    signal(SIGINT, sig_handler);   // ctrl-c

    while (cin.eof()) {
        while (!argv.empty()) {
            char* cstr = argv.back();
            if (cstr != NULL)
                delete[] cstr;
            argv.pop_back();
        }
        prompt(cmd);
        tokenize(cmd, argv);
        if (argc == 0) {
            continue;
        }
        if (strcmp(argv[0], "exit") == 0) {
            break;
        }
        if (strcmp(argv[0], "fg") == 0) {  // Bring a process to foreground
            if (bg_pids.size() == 0) {     // No background process
                cout << "fg: No background jobs" << endl;
                continue;
            }
            if (argc == 1) {
                cpid = bg_pids[0];
                kill(bg_pids.back(), SIGCONT);
                waitpid(bg_pids.back(), NULL, 0);
                cpid = gpid;
                bg_pids.pop_back();
                continue;
            } else {  // indexing based on 1. ex) fg 1 -> bg.back()
                int idx = atoi(argv[1]);
                if (idx < 1 || idx > int(bg_pids.size())) {
                    cout << "fg: No such job" << endl;
                    continue;
                }
                cpid = bg_pids[0];
                kill(bg_pids[int(bg_pids.size()) - idx], SIGCONT);
                waitpid(bg_pids[int(bg_pids.size()) - idx], NULL, 0);
                cpid = gpid;
                bg_pids.erase(bg_pids.begin() + int(bg_pids.size()) - idx);
                continue;
            }
        } else if (strcmp(argv[0], "bg") == 0) {  // Put the process in background
            if (bg_pids.size() == 0) {
                cout << "bg: No current jobs" << endl;
                continue;
            }
            if (argc == 1) {
                if (bg_pids.size() == 0) {
                    cout << "bg: No background jobs" << endl;
                    continue;
                }
                kill(bg_pids.back(), SIGCONT);  // don't wait
                bg_pids.pop_back();
                continue;
            } else {
                int idx = atoi(argv[1]);
                if (idx < 1 || idx > int(bg_pids.size())) {
                    cout << "bg: No such job" << endl;
                    continue;
                }
                kill(bg_pids[int(bg_pids.size()) - idx], SIGCONT);  // don't wait
                cout << "[" << idx << "] "
                     << bg_pids[int(bg_pids.size()) - idx]
                     << " continued" << endl;
                bg_pids.erase(bg_pids.begin() + int(bg_pids.size()) - idx);
                continue;
            }
        }
        bool bg = strcmp(argv[argc - 1], "&") == 0;
        if (bg) {
            argv.pop_back();
            argc--;
        }
        int pid = fork();
        if (pid < 0) {
            cout << "fork error" << endl;
            continue;
        }
        if (pid == 0) {
            execvp(argv[0], &argv[0]);
        } else {
            int status;
            if (!bg) {
                cpid = pid;
                waitpid(pid, &status, 0);
                cpid = gpid;
            }
            cout << "Child pid: " << pid << endl;
        }
    }
}
