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
    string init_cmd;  // original command
    string cmd;       // command after substitution
    vector<string> argv;
    int argc;
    bool bg;
    int fd_in, fd_out, fd_err;  // file descriptors

    Command(const string& _cmd)
        : cmd(_cmd), bg(false), fd_in(0), fd_out(1), fd_err(2) {
        init_cmd = cmd;
    }
    ~Command() {
        if (fd_in != 0) close(fd_in);
        if (fd_out != 1) close(fd_out);
        if (fd_err != 2) close(fd_err);
    }
    void parse() noexcept(false) {
        vector<string> _argv = tokenize(cmd, ' ');
        for (int i = 0; i < int(_argv.size()); i++) {
            if (_argv[i] == "&") {
                if (i != argc - 1)
                    throw invalid_argument("& must be the last argument");
                bg = true;
            } else if (_argv[i] == "<") {
                if (i == argc - 1)
                    throw invalid_argument("< must be followed by a file name");
                if (fd_out != 1)
                    throw invalid_argument("< must be the first redirection");
                else {
                    fd_in = open(_argv[i + 1].c_str(), O_RDONLY);
                    if (fd_in == -1)
                        throw runtime_error("cannot open input file");
                }
                i++;
            } else if (_argv[i] == ">") {
                if (i == argc - 1)
                    throw invalid_argument("> must be followed by a file name");
                fd_out = open(_argv[i + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0700);
                // fd_err = dup(fd_out); // TODO: should errstream be redirected to outstream?
                if (fd_out == -1)
                    throw runtime_error("cannot open output file");
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
    void set_fd_in(int fd) {
        if (fd_in != 0) close(fd_in);
        fd_in = fd;
    }
    void set_fd_out(int fd) {
        if (fd_out != 1) close(fd_out);
        fd_out = fd;
    }
};

pid_t cpid;
pid_t gpid;
vector<int> bg_pids;  // background pids
vector<int> fg_pids;  // foreground pids

vector<Command*> cmds;
int cmd_idx = 0;
int num_cmds = 0;

void prompt(string& inp) {
    for (int i = 0; i < num_cmds; i++) {
        delete cmds[i];
    }
    cmds.clear();
    cmd_idx = 0;
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

void spawn_process(Command* cmd, int fdin, int fdout,
                   int pgid) {
    setpgid(0, pgid);
    cpid = 
}

int main() {
    string inp;
    gpid = cpid = getpid();  // get subgroup id

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

        for (Command* cmd : cmds) {
            cout << *cmd << endl;
        }

        int fgpid = 0;
        int pipefd[2];
        int in = cmd_begin->fd_in;

        for (int i = 0; i < num_cmds - 1; i++) {
            pipe(pipefd);
            pid_t pid = fork();
            if (pid == 0) {
                Command* curr = cmds[i];
                dup2(curr->fd_out, pipefd[1]);
                dup2(curr->fd_in, in);
                if (i == 0)
                    setpgrp();
                else
                    setpgid(getpid(), fgpid);

                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);

                auto args = cmds[i]->c_str();
                execvp(args[0], &args[0]);
                perror("execvp");
                exit(1);  // exec error
            } else {
                if (i == 0) {
                    fgpid = pid;
                    tcsetpgrp(STDIN_FILENO, pid);
                }
                fg_pids.push_back(pid);
                setpgid(pid, fgpid);
                in = pipefd[0];
            }
        }

        bool flag = 1;
        for (int i = 0; i < num_cmds - 1; i++) {
            if (cmds[i]->fd_out != 1) {
                cout << "Error: " << cmds[i]->cmd << ": redirection error" << endl;
                flag = 0;
                break;
            }
            if (cmds[i + 1]->fd_in != 0) {
                cout << "Error: " << cmds[i + 1]->cmd << ": redirection error" << endl;
                flag = 0;
                break;
            }
            p[0] = cmds[i + 1]->fd_in;
            p[1] = cmds[i]->fd_out;
            if (pipe(p) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }
        if (!flag) {
            continue;
        }
        int fgpid = getpid();
        for (int i = 0; i < num_cmds; i++) {
            cout << "cmd: " << cmds[i]->cmd << endl;
            pid_t pid = fork();
            if (pid == 0) {
                if (i == 0) {
                    setpgrp();
                } else
                    setpgid(getpid(), fgpid);

                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);

                auto args = cmds[i]->c_str();
                dup2(cmds[i]->fd_in, STDIN_FILENO);
                dup2(cmds[i]->fd_out, STDOUT_FILENO);
                execvp(args[0], &args[0]);

                perror("execvp");
                exit(1);  // exec error
            } else {
                if (i == 0) {
                    fgpid = pid;
                    tcsetpgrp(STDIN_FILENO, pid);
                }
                fg_pids.push_back(pid);
                setpgid(pid, fgpid);
            }
        }
        waitpid(-fgpid, NULL, WUNTRACED);
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
