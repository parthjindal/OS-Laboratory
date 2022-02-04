#include "multiwatch.h"

#include <sys/epoll.h>
#include <sys/inotify.h>

#include <ctime>
#include <map>
#include <vector>

#include "shell.h"
#include "sighandlers.h"
using namespace std;

#define MAX_EVENTS 32

extern vector<Job*> jobTable;
extern map<pid_t, int> proc2job;
extern pid_t FG_PID;
map<pid_t, int> pid2wd;
pid_t inofd;

static pid_t run_command(int idx) {
    int fpgid = 0;
    int pipefd[2];
    int prevfd[2];
    toggleSIGCHLDBlock(SIG_BLOCK);
    string outfile = "";

    Job& job = *jobTable[idx];
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
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            if (i == 0)
                fpgid = getpid();
            if (i == num_cmds - 1) {
                processes[i].outfile = ".tmp" + to_string(fpgid) + ".txt";
            }
            processes[i].open_fds();
            if (i == 0) {
                setpgrp();
                // tcsetpgrp(STDIN_FILENO, getpid());
            } else {
                setpgid(0, fpgid);
                dup2(prevfd[0], processes[i].fd_in);
                close(prevfd[0]);
                close(prevfd[1]);
            }
            if (i < num_cmds - 1) {
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
            proc2job[cpid] = idx;
        }
    }
    toggleSIGCHLDBlock(SIG_UNBLOCK);
    tcsetpgrp(STDIN_FILENO, getpid());
    return fpgid;
}

void handler_multiwatch(int sig) {
    // send sigchld to all pids in pid2wd
    for (auto it : pid2wd) {
        kill(-it.first, SIGINT);
    }
    close(inofd);
}

void builtin_multiwatch(std::vector<Job*>& joblist, string outfile) {
    vector<int> watch_fds;
    map<int, int> wd2job;
    vector<int> pids;
    vector<int> readfds;

    inofd = inotify_init();  // inofd is global
    if (inofd < 0) {
        perror("inotify_init");
        exit(1);
    }
    for (int i = 0; i < (int)joblist.size(); i++) {
        jobTable.push_back(joblist[i]);
        pid_t pid = run_command(jobTable.size() - 1);
        string logfile = ".tmp" + to_string(pid) + ".txt";
        int fd = open(logfile.c_str(), O_RDONLY);
        if (fd < 0) {
            int fd1 = open(logfile.c_str(), O_CREAT, 0777);
            if (fd1 == -1) {
                perror("open fd1");
                exit(1);
            }
            close(fd1);
            fd = open(logfile.c_str(), O_RDONLY | O_NONBLOCK);
        }
        int wd = inotify_add_watch(inofd, logfile.c_str(), IN_MODIFY);
        if (wd < 0) {
            perror("inotify_add_watch");
            exit(1);
        }
        wd2job[wd] = i;
        readfds.push_back(fd);
        pid2wd[pid] = wd;
    }
    FILE* outfp = (outfile == "") ? stdout : fopen(outfile.c_str(), "w");
    size_t len = sizeof(struct inotify_event);

    struct inotify_event events[MAX_EVENTS * len];
    char buff[1024];

    int count = (int)readfds.size();
    while (count) {
        int n = read(inofd, events, sizeof(events));
        if (n < 0) {
            if (errno == EBADF)  // inofd is closed (sigint handler)
                break;
            perror("read");
            return;
        }
        int i = 0;
        while (i < n) {
            struct inotify_event* event = (struct inotify_event*)&events[i];
            if (event->mask & IN_MODIFY) {
                int wd = event->wd;
                int rfd = readfds[wd2job[wd]];
                int l = 0;
                bool f = false;
                while ((l = read(rfd, buff, sizeof(buff))) > 0 || errno == EINTR) {
                    if (!f) {
                        fprintf(outfp, "%s, %ld\n", joblist[wd2job[wd]]->_cmd.c_str(), time(0));
                        fprintf(outfp, "----------------------------------------------------\n");
                        f = true;
                    }
                    if (l > 0)
                        fwrite(buff, 1, l, outfp);
                }
                if (f)
                    fprintf(outfp, "----------------------------------------------------\n\n");
            }
            if (event->mask & IN_IGNORED) {
                int wd = event->wd;
                int readfd = readfds[wd2job[wd]];
                count--;
            }
            i += len + event->len;
        }
    }
    if (outfp != stdout)
        fclose(outfp);
    for (int fd : readfds)  // close all fds
        close(fd);
    close(inofd);  // close inofd if not closed already
    for (auto it : pid2wd) {
        const char* fname = (".tmp" + to_string(it.first) + ".txt").c_str();
        remove(fname);
    }
}