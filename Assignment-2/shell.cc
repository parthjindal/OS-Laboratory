#include "shell.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <climits>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <list>
#include <map>
#include <stdexcept>
#include <vector>

using namespace std;

vector<string> tokenize(const string& _cmd, char delim) {
    vector<string> argv;
    stringstream ss(_cmd);
    string token;
    while (getline(ss, token, delim)) {
        argv.push_back(token);
    }
    return argv;
}

class Command {
   public:
    string cmd;  // command after substitution
    vector<string> argv;
    int argc;
    bool bg;
    int fd_in, fd_out, fd_err;  // file descriptors
    string fd_in_name, fd_out_name, fd_err_name;

    Command(const string& _cmd)
        : cmd(_cmd), bg(false), fd_in(0), fd_out(1), fd_err(2) {
    }
    ~Command() {
        // clean up file descriptors
        if (fd_in != 0) close(fd_in);
        if (fd_out != 1) close(fd_out);
    }
    void parse() {
        vector<string> _argv = tokenize(cmd, ' ');
        for (int i = 0; i < int(_argv.size()); i++) {
            if (_argv[i] == "&") {
                bg = true;
            } else if (_argv[i] == "<") {
                fd_in_name = _argv[i + 1];
                i++;
            } else if (_argv[i] == ">") {
                fd_out_name = _argv[i + 1];
                i++;
            } else if (_argv[i].size() != 0) {
                argv.push_back(_argv[i]);
            }
        }
        argc = argv.size();
        cmd = "";
        for (int i = 0; i < argc; i++) {
            cmd += argv[i];
            if (i != argc - 1)
                cmd += " ";
        }
    }

    friend ostream& operator<<(ostream& os, const Command& _cmd) {
        os << _cmd.cmd << " < " << _cmd.fd_in << " > " << _cmd.fd_out << (_cmd.bg ? " &" : "");
        return os;
    }

    vector<char*> c_str() {
        vector<char*> _args;
        for (int i = 0; i < argc; i++) {
            char* tmp = new char[argv[i].length() + 1];
            strcpy(tmp, argv[i].c_str());
            _args.push_back(tmp);
        }
        _args.push_back(nullptr);
        return _args;
    }

    void open_fds() {
        if (fd_in_name != "") {
            fd_in = open(fd_in_name.c_str(), O_RDONLY);
            if (fd_in == -1) {
                perror("open");
                exit(1);
            }
            dup2(fd_in, STDIN_FILENO);
        }
        if (fd_out_name != "") {
            fd_out = open(fd_out_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0700);
            if (fd_out == -1) {
                perror("open");
                exit(1);
            }
            dup2(fd_out, STDOUT_FILENO);
        }
    }

    void set_fd_in(int fd) {
        if (fd_in != 0) close(fd_in);
        fd_in = fd;
    }

    void set_fd_out(int fd) {
        if (fd_out != 1) close(fd_out);
        fd_out = fd;
    }
};
typedef pair<int, int> job_t;  // <pid,status>

map<int, job_t> bg_jobs;
vector<Command*> cmds;
int num_cmds = 0;

void prompt(string& inp) {
    for (int i = 0; i < num_cmds; i++) {
        delete cmds[i];
    }
    cmds.clear();
    num_cmds = 0;

    char buff[PATH_MAX];
    getcwd(buff, PATH_MAX);
    std::string wcd(buff);
    std::string wd = wcd.substr(wcd.find_last_of("/") + 1);
    cout << GREEN << wd << RESET << "$ ";
    getline(cin, inp);

    if (cin.bad()) {
        cin.clear();
        num_cmds = 0;
        cout << endl;
        return;
    }
    vector<string> _cmds = tokenize(inp, '|');
    for (auto it = _cmds.begin(); it != _cmds.end(); it++) {
        Command* cmd = new Command(*it);
        cmd->parse();
        cmds.push_back(cmd);
    }
    num_cmds = cmds.size();
}

int main() {
    string inp;

    struct sigaction sig_act;
    sig_act.sa_handler = SIG_IGN;
    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = 0;

    sigaction(SIGTSTP, &sig_act, NULL);
    sigaction(SIGINT, &sig_act, NULL);
    signal(SIGTTOU, SIG_IGN);

    // add a handler for

    while (!cin.eof()) {
        prompt(inp);
        if (num_cmds == 0)
            continue;

        Command* cmd_begin = cmds[0];
        Command* cmd_end = cmds[num_cmds - 1];

        if (cmd_begin->cmd == "exit") {
            break;
        }
        // if (cmd_begin->cmd == "jobs") {
        //     for (int i = 0; i < int(bg_pids.size()); i++) {
        //         cout << "[" << i << "] " << bg_pids[i] <<
        //     }
        //     continue;
        // }

        int fpgid = 0;
        int in = 0;
        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            Command* cmd = cmd_begin;
            cmd->open_fds();  // io redirection
            setpgrp();
            tcsetpgrp(STDIN_FILENO, getpgrp());

            auto args = cmd->c_str();
            execvp(args[0], &args[0]);
            perror("execvp");
            exit(1);
        } else {
            setpgid(pid, pid);
            tcsetpgrp(STDIN_FILENO, getpgrp());
            if (!cmd_begin->bg) {
                waitpid(pid, NULL, WUNTRACED);
            }
        }
        tcsetpgrp(STDIN_FILENO, getpid());

        // for (int i = 0; i < num_cmds - 1; i++) {
        //     pipe(pipefd);
        //     pid_t pid = fork();
        //     if (pid == 0) {  // child
        //         Command* curr = cmds[i];
        //         curr->open_fds();
        //         dup2(curr->fd_out, pipefd[1]);
        //         dup2(curr->fd_in, in);
        //         if (i == 0)
        //             setpgrp();
        //         else
        //             setpgid(getpid(), fpgid);

        //         signal(SIGINT, SIG_DFL);
        //         signal(SIGTSTP, SIG_DFL);

        //         auto args = cmds[i]->c_str();
        //         execvp(args[0], &args[0]);
        //         perror("execvp");
        //         exit(1);  // exec error
        //     } else {
        //         if (i == 0) {
        //             fpgid = pid;
        //             tcsetpgrp(STDIN_FILENO, pid);
        //         }
        //         fg_pids.push_back(pid);
        //         setpgid(pid, fpgid);
        //         in = pipefd[0];
        //     }
        // }
        // pid_t pid = fork();
        // if (pid == 0) {
        //     dup2(cmd_end->fd_in, in);
        //     dup2(cmd_end->fd_out, STDOUT_FILENO);
        //     setpgid(getpid(), fpgid);
        //     signal(SIGINT, SIG_DFL);
        //     signal(SIGTSTP, SIG_DFL);

        //     auto args = cmd_end->c_str();
        //     execvp(args[0], &args[0]);
        //     perror("execvp");
        //     exit(1);  // exec error
        // } else {
        //     fg_pids.push_back(pid);
        //     setpgid(pid, fpgid);
        // }

        // waitpid(-fpgid, NULL, WUNTRACED);
        tcsetpgrp(STDIN_FILENO, getpid());
        // int pipe[2];
        // for (Command* cmd : cmds) {
        // }
        // if (strcmp(argv[0], "fg") == 0) {  // Bring a process to foreground
        //     if (bg_pids.size() == 0) {     // No background process
        //         cout << "fg: No background jobs" << endl;
        //         continue;
        //     }
        //     if (argc == 1) {
        //         cpid = bg_pids[0];
        //         kill(bg_pids.back(), SIGCONT);
        //         waitpid(bg_pids.back(), NULL, 0);
        //         cpid = gpid;
        //         bg_pids.pop_back();
        //         continue;
        //     } else {  // indexing based on 1. ex) fg 1 -> bg.back()
        //         int idx = atoi(argv[1]);
        //         if (idx < 1 || idx > int(bg_pids.size())) {
        //             cout << "fg: No such job" << endl;
        //             continue;
        //         }
        //         cpid = bg_pids[0];
        //         kill(bg_pids[int(bg_pids.size()) - idx], SIGCONT);
        //         waitpid(bg_pids[int(bg_pids.size()) - idx], NULL, 0);
        //         cpid = gpid;
        //         bg_pids.erase(bg_pids.begin() + int(bg_pids.size()) - idx);
        //         continue;
        //     }
        // } else if (strcmp(argv[0], "bg") == 0) {  // Put the process in background
        //     if (bg_pids.size() == 0) {
        //         cout << "bg: No current jobs" << endl;
        //         continue;
        //     }
        //     if (argc == 1) {
        //         if (bg_pids.size() == 0) {
        //             cout << "bg: No background jobs" << endl;
        //             continue;
        //         }
        //         kill(bg_pids.back(), SIGCONT);  // don't wait
        //         bg_pids.pop_back();
        //         continue;
        //     } else {
        //         int idx = atoi(argv[1]);
        //         if (idx < 1 || idx > int(bg_pids.size())) {
        //             cout << "bg: No such job" << endl;
        //             continue;
        //         }
        //         kill(bg_pids[int(bg_pids.size()) - idx], SIGCONT);  // don't wait
        //         cout << "[" << idx << "] "
        //              << bg_pids[int(bg_pids.size()) - idx]
        //              << " continued" << endl;
        //         bg_pids.erase(bg_pids.begin() + int(bg_pids.size()) - idx);
        //         continue;
        //     }
        // }
        // bool bg = strcmp(argv[argc - 1], "&") == 0;
        // if (bg) {
        //     argv.pop_back();
        //     argc--;
        // }
        // Command* curr = cmds[0];
        // int pid = fork();
        // if (pid < 0) {
        //     cout << "fork error" << endl;
        //     exit(EXIT_FAILURE);
        // }
        // if (pid == 0) {  // child-process
        //     setpgrp();
        //     auto args = curr->c_str();
        //     dup2(curr->fd_in, STDIN_FILENO);
        //     dup2(curr->fd_out, STDOUT_FILENO);
        //     execvp(args[0], &args[0]);
        //     perror("execvp");
        //     exit(1);  // exec error
        // } else {
        //     setpgid(pid, pid);
        //     tcsetpgrp(0, pid);
        //     int status;
        //     if (curr->bg) {
        //         bg_pids.push_back(pid);
        //         cout << "[" << bg_pids.size() << "] " << pid << " started" << endl;
        //     } else {
        //         waitpid(pid, NULL, 0);
        //     }

        //     int status;
        //     if (!curr->bg) {
        //         cpid = pid;
        //         pid_t t;
        //         while ((t = waitpid(pid, &status, WUNTRACED)) == -1) {
        //             if (errno != EINTR)
        //         }

        //         cout << status << endl;
        //         // if (errno == EINTR){
        //         //     cout << "Interrupted" << endl;
        //         // }
        //         if (WIFSTOPPED(status) || (errno == EINTR && !WIFEXITED(status)) {
        //             bg_pids.push_back(pid);
        //             cout << "[" << bg_pids.size() << "] " << pid << " stopped" << endl;
        //             kill(pid, SIGCONT);
        //         }
        //         cpid = gpid;
        //     }
    }
}
