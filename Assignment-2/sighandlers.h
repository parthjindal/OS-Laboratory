#ifndef _SIG_HANDLER_H
#define _SIG_HANDLER_H

#include <fcntl.h>
#include <unistd.h>
#include <csignal>

// pid_t FG_PID;
void toggleSIGCHLDBlock(int how);
void waitForeground(pid_t cpid);

#endif // _SIG_HANDLER_H