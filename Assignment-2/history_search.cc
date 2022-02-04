#include <fcntl.h>

#include <algorithm>
#include <cstring>
#include <deque>
#include <iostream>
#include <iterator>
#include <vector>

using namespace std;

#include "history_search.h"

deque<char *> history;
int history_size;
string history_fname;

void initialise_history() {
    history_fname = string(getenv("HOME")) + string("/") + HISTORY_FILE;
    FILE *fp = fopen(history_fname.c_str(), "a+");
    char buff[1000];
    while (fgets(buff, 1000, fp)) {
        buff[strcspn(buff, "\n")] = 0;
        history.push_back(strdup(buff));
    }
    history_size = history.size();
    while (history.size() > HISTORY_SIZE) {
        free(history.front());
        history.pop_front();
    }
    fclose(fp);
}

void update_history(const char *cmd) {
    char *cpy = strdup(cmd);
    history.push_back(cpy);
    history_size++;
    while (history_size > HISTORY_SIZE) {
        free(history.front());
        history.pop_front();
        history_size--;
    }
}

void print_history() {
    int read = HISTORY_PRINT;
    cout << "History: " << endl;
    if (history_size < HISTORY_PRINT)
        read = history_size;
    for (int i = 1; i <= read; i++) {
        cout << history[history_size - i] << endl;
    }
}

vector<char *> search_history(const char *search_term) {
    vector<char *> results;
    if (history_size == 0) {
        return results;
    }
    int flag = 0;
    int exact = 0;
    vector<int> indices;
    for (auto it = history.rbegin(); it != history.rend(); it++) {
        if (strstr(*it, search_term)) {  // use kmp later
            results.push_back(*it);
        }
    }
    return results;
}

void cleanup_history() {
    FILE *fp = fopen(history_fname.c_str(), "w");
    for (int i = 0; i < history_size; i++) {
        fprintf(fp, "%s\n", history[i]);
    }
    fclose(fp);
}
