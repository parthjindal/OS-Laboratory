#include "sighandlers.h"

#include <csignal>
#include <cstdio>

pid_t FG_PID = 0;
void toggleSIGCHLDBlock(int how) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(how, &mask, NULL);
}
void waitForeground(pid_t cpid) {
    FG_PID = cpid;
    sigset_t empty;
    sigemptyset(&empty);
    while (FG_PID == cpid) {
        sigsuspend(&empty);
    }
    toggleSIGCHLDBlock(SIG_UNBLOCK);
}