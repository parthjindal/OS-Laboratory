#include <deque>
#include <vector>

#define HISTORY_FILE ".terminal_history.txt"
#define HISTORY_SIZE 10000
#define HISTORY_PRINT 1000

std::deque<char*> history;
int history_size;

void initialise_history();
void update_history(char* cmd);
void print_history();
void search_history();
void clean_history();
std::vector<char*> autocomplete(char* input);