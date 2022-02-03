#ifndef _MULTIWATCH_H
#define _MULTIWATCH_H
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <vector>

#include "process.h"

void handler_multiwatch(int sig);
void builtin_multiwatch(std::vector<Job*>& joblist, std::string outfile);
#endif  // _MULTIWATCH_H