#ifndef _PROCESS_H
#define _PROCESS_H

#include <iostream>
#include <vector>

typedef void (*builtin_func_t)(void);

enum JobStatus {
    READY,
    RUNNING,
    STOPPED,
    DONE
};

class Process {
   public:
    pid_t pid;                      // process id
    std::string cmd;                // command string
    std::vector<std::string> argv;  // argv vector (null-terminated)
    int argc;                       // argc
    bool bg;                        // background
    int fd_in, fd_out;              // file descriptors
    std::string infile;
    std::string outfile;  // input/output file names

    // Constructor
    Process(const std::string& _cmd);
    // Destructor
    ~Process();
    friend std::ostream& operator<<(std::ostream& os, const Process& p);
    void open_fds();
    std::vector<char*> get_argv();
};

struct Job {
    std::string _cmd;
    pid_t pgid;
    std::vector<Process> processes;
    JobStatus status;
    int num_active;
    Job(pid_t _pgid = -1) : pgid(_pgid) {
        status = READY;
        num_active = 0;
        _cmd = "";
    }
};

std::ostream& operator<<(std::ostream& os, const Job& j);
std::string enum2string(JobStatus status);

#endif  // _PROCESS_H