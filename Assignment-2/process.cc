#include "process.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <iostream>

using namespace std;

string enum2string(JobStatus status) {
    if (status == JobStatus::READY) {
        return "ready";
    } else if (status == JobStatus::RUNNING) {
        return "running";
    } else if (status == JobStatus::STOPPED) {
        return "stopped";
    } else if (status == JobStatus::DONE) {
        return "done";
    } else
        return "unknown";
}

Process::Process(const std::string& _cmd) : pid(-1),
                                            cmd(_cmd),
                                            argc(0),
                                            bg(false),
                                            fd_in(STDIN_FILENO),
                                            fd_out(STDOUT_FILENO),
                                            infile(""),
                                            outfile("") {}
Process::~Process() {
    if (fd_in != 0) close(fd_in);
    if (fd_out != 1) close(fd_out);
}

std::ostream& operator<<(std::ostream& os, const Process& p) {
    os << p.pid << " " << p.cmd << (p.infile == "" ? "" : " < " + p.infile)
       << (p.outfile == "" ? "" : " > " + p.outfile)
       << (p.bg ? " &" : "");
    return os;
}

vector<char*> Process::get_argv() {
    vector<char*> _argv;
    for (auto& arg : argv) {
        _argv.push_back((char*)&arg[0]);
    }
    _argv.push_back(nullptr);
    return _argv;
}

void Process::open_fds() {
    if (infile != "") {
        fd_in = open(infile.c_str(), O_RDONLY);
        dup2(fd_in, STDIN_FILENO);
    }
    if (outfile != "") {
        fd_out = open(outfile.c_str(), O_WRONLY | O_CREAT, 0644);
        dup2(fd_out, STDOUT_FILENO);
    }
}

ostream& operator<<(ostream& os, const Job& j) {
    os << j.pgid << ": " << enum2string(j.status) << "\n";
    for (int i = 0; i < (int)j.processes.size(); i++) {
        os << "----> " << j.processes[i];
        if (i != (int)j.processes.size() - 1) os << " |\n";
    }
    return os;
}
