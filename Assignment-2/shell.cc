#include "shell.h"
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <climits>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <map>
#include <vector>

#include "process.h"
#include "parser.h"

using namespace std;

map<pid_t, int> proc2job;  // pid -> Job index in Job Table
vector<Job*> jobTable;

int numJobs = 0;
static pid_t fgpid = 0;  // current foreground process id
static void reap(int sig) {
    while (1) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (pid <= 0) break;
        int jidx = proc2job[pid];
        Job& job = *jobTable[jidx];

        if (WIFSTOPPED(status)) {
            cout << "[" << job.pgid << "] stopped" << endl;
            job.status = STOPPED;
        } else if (WIFSIGNALED(status) || WIFEXITED(status)) {
            job.status = DONE;
        } else if (WIFCONTINUED(status)) {
            cout << "[" << job.pgid << "] continued" << endl;
            job.status = RUNNING;
            job.num_active = (int)job.processes.size();
        }
        if (job.pgid == fgpid && !WIFCONTINUED(status)) {
            job.num_active--;
            if (job.num_active == 0) {
                fgpid = 0;
            }
        }
    }
}

void prompt(string& inp) {
    char buff[PATH_MAX];
    getcwd(buff, PATH_MAX);
    std::string wcd(buff);
    std::string wd = wcd.substr(wcd.find_last_of("/") + 1);
    cout << GREEN << wd << RESET << "$ ";
    getline(cin, inp);  // todo: write wrapper using non-canonical mode

    if (cin.bad()) {
        cin.clear();
        numJobs = 0;
        cout << endl;
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

static void handleSIGINT(int sig) {
    std::cin.setstate(std::ios::badbit);
}

vector<pid_t> fg_jobs;
void multiwatch_handler(int sig) {
    for (int i = 0; i < fg_jobs.size(); i++) {
        kill(-fg_jobs[i], SIGINT);
    }
}

pid_t run_command(Job& job) {
    int fpgid = 0;  // fg process group id
    int pipefd[2];
    int prevfd[2];
    toggleSIGCHLDBlock(SIG_BLOCK);

    int num_cmds = job.processes.size();
    auto& processes = job.processes;

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
                processes[i].open_fds();
            // set fg process group id = pid(child1)
            if (i == 0)
                setpgrp();
            else {
                setpgid(0, fpgid);
                // set input pipe file descriptor
                dup2(prevfd[0], processes[i].fd_in);
                // close unused pipe file descriptors
                close(prevfd[0]);
                close(prevfd[1]);
            }
            if (i < num_cmds && num_cmds > 1) {
                dup2(pipefd[1], processes[i].fd_out);
                close(pipefd[1]);
                close(pipefd[0]);
            }
            vector<char*> args = processes[i].get_argv();
            execvp(args[0], args.data());
            perror("execvp");
            exit(1);
        } else {
            if (i == 0) {
                fpgid = cpid;
                setpgid(cpid, fpgid);
                tcsetpgrp(STDIN_FILENO, fpgid);
                job.pgid = fpgid;
                job.status = RUNNING;
            } else {
                setpgid(cpid, fpgid);
            }
            if (i > 0) {
                close(prevfd[0]);
                close(prevfd[1]);
            }
            prevfd[0] = pipefd[0];
            prevfd[1] = pipefd[1];
            processes[i].pid = cpid;
            job.num_active++;
            proc2job[cpid] = jobTable.size() - 1;
        }
    }

    if (job.processes.back().bg == false) {  // todo: associate bg with a job and not a process
        waitFg(fpgid);
    } else
        toggleSIGCHLDBlock(SIG_UNBLOCK);
    tcsetpgrp(STDIN_FILENO, getpid());
    return fpgid;
}

int main() {
    signal(SIGCHLD, reap);
    struct sigaction sig_act;
    sig_act.sa_handler = handleSIGINT;  // sets cin to badbit
    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = 0;

    sigaction(SIGTSTP, &sig_act, NULL);
    sigaction(SIGINT, &sig_act, NULL);
    signal(SIGTTOU, SIG_IGN);

    while (!cin.eof()) {
        string inp;
        prompt(inp);
        if (inp.empty())
            continue;
        Parser parser;
        vector<Job*> joblist;
        int numJobs = 0;

        parser.parse(inp, joblist, numJobs);
        jobTable.insert(jobTable.end(), joblist.begin(), joblist.end());
        

        if (parser.is_builtin == true) {
            string builtin_cmd = parser.builtin_cmd;
            if (builtin_cmd == "exit") {
                break;
            } else if (builtin_cmd == "jobs") {
                for (auto it = jobTable.begin(); it != jobTable.end(); it++) {
                    cout << *(*it) << endl;
                }
            } else if (builtin_cmd == "fg") {
                pid_t gpid = atoi(parser.builtin_argv[0].c_str());
                bool flag = 0;
                for (auto it = jobTable.rbegin(); it != jobTable.rend(); it++) {
                    if ((*it)->pgid == gpid) {
                        if ((*it)->status == STOPPED) {
                            flag = 1;
                        }
                        break;
                    }
                }
                if (!flag) {
                    cout << "No such job" << endl;
                    continue;
                }
                tcsetpgrp(STDIN_FILENO, gpid);
                toggleSIGCHLDBlock(SIG_BLOCK);
                kill(-gpid, SIGCONT);
                waitFg(gpid);
                tcsetpgrp(STDIN_FILENO, getpid());
                continue;
            } else if (builtin_cmd == "bg") {
                pid_t gpid = atoi(parser.builtin_argv[0].c_str());
                bool flag = 0;
                for (auto it = jobTable.rbegin(); it != jobTable.rend(); it++) {
                    if ((*it)->pgid == gpid) {
                        if ((*it)->status == STOPPED) {
                            flag = 1;
                        }
                        break;
                    }
                }
                if (!flag) {
                    cout << "No such job" << endl;
                    continue;
                }
                kill(-gpid, SIGCONT);
                continue;
            } else if (builtin_cmd == "multiwatch") {
                //
            }
        } else {
            run_command(*joblist[0]);  // non-builtin command
        }
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