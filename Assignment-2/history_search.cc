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

void initialise_history() {
    int history_file = open(HISTORY_FILE, O_RDONLY | O_CREAT, 0644);
    FILE *fp = fopen(history_file, "r");
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
    printf("%s\n", cmd);
    printf("cmd_len: %d\n", strlen(cmd));
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
    cout << "History Size: " << history_size << endl;
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
            results.clear();
            results.push_back(*it);
            return results;
        }
    }
    return results;
}

void cleanup_history() {
    print_history();
    FILE *fp = fopen(HISTORY_FILE, "w");
    for (int i = 0; i < history_size; i++) {
        cout << history[i] << endl;
        // fprintf(stderr, "%s\n", history[i]);
        // fprintf(fp, "%s\n", history[i]);
    }
    cout << "History Cleaned" << endl;
    fclose(fp);
}
