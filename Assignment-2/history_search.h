
#ifndef _HISTORY_SEARCH_H
#define _HISTORY_SEARCH_H

#include <deque>
#include <vector>
#include <string>

#define HISTORY_FILE ".terminal_history.txt"
#define HISTORY_SIZE 10000
#define HISTORY_PRINT 1000

extern std::deque<char *> history;
extern int history_size;
extern std::string history_fname;

void initialise_history();
void update_history(const char *cmd);
void print_history();
std::vector<char *> search_history(const char *search_term);
void cleanup_history();

#endif  // _HISTORY_SEARCH_H