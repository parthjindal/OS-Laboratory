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
#include <set>
#include <stdexcept>
#include <vector>

using namespace std;

#define RUNNING 0
#define STOPPED 1
#define DONE 2

string getStatus(int status) {
    if (status == RUNNING)
        return "running";
    else if (status == STOPPED)
        return "stopped";
    else
        return "done";
}

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
    string cmd;           // command string
    vector<string> argv;  // arguments
    int argc;
    bool bg;
    int fd_in, fd_out;
    string fd_in_name, fd_out_name;

    Command(const string& _cmd) : cmd(_cmd), bg(false), fd_in(0), fd_out(1) {
        fd_in_name = fd_out_name = "";
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

    vector<char*> vc_str() {
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
            dup2(fd_in, STDIN_FILENO);
        }
        if (fd_out_name != "") {
            fd_out = open(fd_out_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0700);
            dup2(fd_out, STDOUT_FILENO);
        }
    }
};
vector<Command*> cmds;

struct Process {
    int pid;
    string cmd;
};

struct Job {
    pid_t pgid;
    vector<Process> processes;
    int _cnt;
    int status;
    Job(pid_t _pgid) : pgid(_pgid) {
        _cnt = 0;
        status = RUNNING;
    }
};

map<int, int> proc2job;
vector<Job> jobs;

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

static pid_t fgpid = 0;
static void reap(int sig) {
    while (1) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (pid <= 0) break;
        int jidx = proc2job[pid];
        if (WIFSTOPPED(status)) {
            cout << "[" << jobs[jidx].pgid << "] stopped" << endl;
            jobs[jidx].status = STOPPED;
        } else if (WIFSIGNALED(status) || WIFEXITED(status)) {
            jobs[jidx].status = DONE;
        } else if (WIFCONTINUED(status)) {
            cout << "[" << jobs[jidx].pgid << "] continued" << endl;
            jobs[jidx].status = RUNNING;
            jobs[jidx]._cnt = (int)jobs[jidx].processes.size();
        }
        if (jobs[jidx].pgid == fgpid && !WIFCONTINUED(status)) {
            jobs[jidx]._cnt--;
            if (jobs[jidx]._cnt == 0) {
                fgpid = 0;
            }
        }
    }
}

static void toggleSIGCHLDBlock(int how) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(how, &mask, NULL);
}

static void waitFg(pid_t pid) {
    fgpid = pid;
    sigset_t empty;
    sigemptyset(&empty);
    while (fgpid == pid) {
        sigsuspend(&empty);
    }
    toggleSIGCHLDBlock(SIG_UNBLOCK);
}

int main() {
    std::string inp;

    signal(SIGCHLD, reap);

    struct sigaction sig_act;
    sig_act.sa_handler = SIG_IGN;
    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = 0;

    sigaction(SIGTSTP, &sig_act, NULL);
    sigaction(SIGINT, &sig_act, NULL);
    signal(SIGTTOU, SIG_IGN);

    while (!cin.eof()) {
        prompt(inp);
        if (num_cmds == 0)
            continue;
        Command* cmd_begin = cmds[0];
        Command* cmd_end = cmds[num_cmds - 1];

        if (cmd_begin->cmd == "exit") {
            break;
        }
        if (cmd_begin->cmd == "jobs") {
            for (auto it = jobs.begin(); it != jobs.end(); it++) {
                cout << it->pgid << ": " << getStatus(it->status) << "\n";
                for (auto pit = it->processes.begin(); pit != it->processes.end(); pit++) {
                    cout << "----> " << pit->pid << " " << pit->cmd;
                    if (pit != it->processes.end() - 1)
                        cout << " |\n";
                }
                cout << endl;
            }
            continue;
        }
        if (cmd_begin->argv[0] == "bg") {
            pid_t gpid = atoi(cmd_begin->argv[1].c_str());  // gpid is the pgid of the job
            bool f = 0;
            for (auto it = jobs.rbegin(); it != jobs.rend(); it++) {
                if (it->pgid == gpid && it->status == STOPPED) {
                    f = 1;
                    break;
                }
            }
            if (!f) {
                cout << "No such job" << endl;
                continue;
            }
            kill(-gpid, SIGCONT);
            continue;
        }
        if (cmd_begin->argv[0] == "fg") {
            pid_t gpid = atoi(cmd_begin->argv[1].c_str());  // check if gpid is valid or not
            bool f = 0;
            for (auto it = jobs.rbegin(); it != jobs.rend(); it++) {
                if (it->pgid == gpid && it->status == STOPPED) {
                    f = 1;
                    break;
                }
            }
            if (!f) {
                cout << "No such job" << endl;
                continue;
            }
            toggleSIGCHLDBlock(SIG_BLOCK);
            tcsetpgrp(STDIN_FILENO, gpid);
            kill(-gpid, SIGCONT);
            waitFg(gpid);
            tcsetpgrp(STDIN_FILENO, getpid());
            continue;
        }

        int fpgid = 0;  // fg process group id
        int pipefd[2];
        int prevfd[2];
        toggleSIGCHLDBlock(SIG_BLOCK);

        if (cmd_begin->argv[0] == "mulitwatch") {
            // workflow: fork for cmd1, cmd2, cmd3, cmd4, etc
        }

        for (int i = 0; i < num_cmds; i++) {
            if (i < num_cmds - 1)
                pipe(pipefd);
            pid_t cpid = fork();
            if (cpid < 0) {  // error in forking
                perror("fork");
                exit(1);
            }
            if (cpid == 0) {
                toggleSIGCHLDBlock(SIG_UNBLOCK);
                // reinstall signal-handlers
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                // open redirection files for end pipe commands
                if (i == 0 || i + 1 == num_cmds)
                    cmds[i]->open_fds();
                // set fg process group id = pid(child1)
                if (i == 0)
                    setpgrp();
                else {
                    setpgid(0, fpgid);
                    // set input pipe file descriptor
                    dup2(prevfd[0], cmds[i]->fd_in);
                    // close unused pipe file descriptors
                    close(prevfd[0]);
                    close(prevfd[1]);
                }
                if (i < num_cmds) {  // set output pipe file descriptor
                    dup2(pipefd[1], cmds[i]->fd_out);
                    close(pipefd[1]);
                    close(pipefd[0]);
                }
                vector<char*> args = cmds[i]->vc_str();
                execvp(args[0], args.data());
                perror("execvp");
                exit(1);
            } else {
                if (i == 0) {
                    fpgid = cpid;
                    setpgid(cpid, fpgid);
                    tcsetpgrp(STDIN_FILENO, fpgid);
                } else {
                    setpgid(cpid, fpgid);
                }
                if (i > 0) {
                    close(prevfd[0]);
                    close(prevfd[1]);
                }
                prevfd[0] = pipefd[0];
                prevfd[1] = pipefd[1];
                if (i == 0) {
                    jobs.push_back(Job(fpgid));
                }
                jobs.back().processes.push_back({cpid, cmds[i]->cmd});
                proc2job[cpid] = jobs.size() - 1;
            }
        }
        jobs.back()._cnt = num_cmds;
        if (cmds.back()->bg == false) {
            waitFg(fpgid);
            // toggle to send SIGCONT instantly
            /* if (jobs.back().status == STOPPED) {
                kill(-fpgid, SIGCONT);
            } */
        } else
            toggleSIGCHLDBlock(SIG_UNBLOCK);
        tcsetpgrp(STDIN_FILENO, getpid());
    }
}

/**
 * @brief TODO: 1. add cd builtin.
 *              2. add autocomplete
 *              3. multiwatch
 *              4. history search
 *              5. cleanup on exit
 *              6. using termios for ctrl r, tab completion
 */