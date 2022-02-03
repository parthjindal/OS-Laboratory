#include "shell.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <dirent.h>
#include <algorithm>
#include <climits>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

using namespace std;


deque <char *> history;
int history_size;

void initialise_history() {
    int history_file = open(".terminal_history.txt", O_RDONLY | O_CREAT, 0644);
    FILE* fp = fdopen(history_file, "r");
    char buff[1000];
    while (fgets(buff, 1000, fp)) {
        buff[strcspn(buff, "\n")] = 0;
        history.push_back(strdup(buff));
    }
    history_size = history.size();
    while(history.size() > HISTORY_SIZE) {
        free(history.front());
        history.pop_front();
    }
    fclose(fp);
}

void update_history(char *cmd) {
    history.push_back(strdup(cmd));
    history_size++;
    while(history_size  > HISTORY_SIZE) {
        free(history.front());
        history.pop_front();
        history_size--;
    }
}

void print_history() {
    int read = HISTORY_PRINT;
    cout << "History Size: " << history_size << endl;
    cout << "History: " << endl;
    if(history_size < HISTORY_PRINT)
        read = history_size;
    for(int i = 1; i <= read; i++) {
        cout << history[history_size - i] << endl;
    }
}

void search_history() {
    if(history_size == 0) {
        cout << "No history found" << endl;
        return;
    }
    cout << "Enter Search Term: ";
    char * search_term = (char *)malloc(sizeof(char) * 1000);
    scanf("%[^\n]", search_term);
    getchar();
    int flag = 0;
    int exact = 0;
    vector <int> indices;
    for(auto it = rbegin(history); it != rend(history); it++) {
        if(strcmp(search_term, *it) == 0) {
            exact = 1;
            flag = 1;
            cout << "Exact Match Found: " << *it << endl;
            break;
        }
        if(strstr(*it, search_term)) {
            flag = 1;
            indices.push_back(history_size - distance(rbegin(history), it) - 1);
        }
    }

    if(exact == 0 && flag == 1) {
        cout << "Search Results: " << endl;
        int i = 1;
        for(auto &it: indices) {
            cout << i << ". " << history[it] << endl;
            i++;
        }
    }else if(!flag) {
        cout << "No match for search term in history" << endl;
    }
}

void clean_history(){
    int history_file = open(".terminal_history.txt", O_WRONLY);
    FILE* fp = fdopen(history_file, "w");  
    for(int i = 0; i < history.size(); i++){
        fprintf(fp, "%s\n", history[i]);
    }
    fclose(fp);
}

int main() {
    initialise_history();
    char *cmd = (char *)malloc(sizeof(char) * 1000);
    while (1) {
        cout << "> ";
        scanf("%[^\n]", cmd);
        getchar();
        if(strcmp(cmd, "exit") == 0) {
            clean_history();
            return 0;
        }
        if(strcmp(cmd, "history") == 0) {
            print_history();
            continue;
        }
        if(strcmp(cmd, "search") == 0) {
            search_history();
            continue;
        }
        update_history(cmd);
    }
    
}