#include "autocomplete.h"

#include <dirent.h>
#include <fcntl.h>

#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

deque<char*> history;
int history_size = HISTORY_SIZE;

void initialise_history() {
    int history_file = open(".terminal_history.txt", O_RDONLY | O_CREAT, 0644);
    FILE* fp = fdopen(history_file, "r");
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

void update_history(char* cmd) {
    history.push_back(strdup(cmd));
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

void search_history() {
    if (history_size == 0) {
        cout << "No history found" << endl;
        return;
    }
    cout << "Enter Search Term: ";
    char* search_term = (char*)malloc(sizeof(char) * 1000);
    scanf("%[^\n]", search_term);
    getchar();
    int flag = 0;
    for (auto it = rbegin(history); it != rend(history) && (!flag); it++) {
        if (strstr(*it, search_term)) {
            cout << *it << endl;
            flag = 1;
        }
    }
    if (!flag) {
        cout << "No match for search term in history" << endl;
    }
}

void clean_history() {
    int history_file = open(".terminal_history.txt", O_WRONLY);
    FILE* fp = fdopen(history_file, "w");
    for (int i = 0; i < history.size(); i++) {
        fprintf(fp, "%s\n", history[i]);
    }
    fclose(fp);
}

vector<char*> autocomplete(char* input) {
    vector<char*> ret = {};
    vector<char*> tokens;
    char* token = strtok(input, "/");
    while (token != NULL) {
        tokens.push_back(token);
        token = strtok(NULL, "/");
    }

    char* dir_path = (char*)malloc(sizeof(char) * 200);
    char* file_name = (char*)malloc(sizeof(char) * 200);
    strcpy(dir_path, "./");
    strcpy(file_name, tokens[tokens.size() - 1]);
    int name_len = strlen(file_name);

    if (tokens.size() == 0) {
        return ret;
    } else if (tokens.size() > 1) {
        for (int i = 0; i < tokens.size() - 1; i++) {
            strcat(dir_path, tokens[i]);
            strcat(dir_path, "/");
        }
    }

    DIR* dir = opendir(dir_path);
    if (dir == NULL) {
        return ret;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char* name = (char*)malloc(sizeof(char) * 200);
        strcpy(name, entry->d_name);
        if (entry->d_type == DT_DIR) {
            name[strlen(name)] = '/';
            name[strlen(name)] = '\0';
        }
        int flag = 1;
        for (int i = 0; i < name_len; i++) {
            if (name[i] != file_name[i]) {
                flag = 0;
            }
        }
        if (flag == 1)
            ret.push_back(name);
    }
    cout << dir_path << endl;
    closedir(dir);
    for (int i = 0; i < ret.size(); i++) {
        cout << ret[i] << endl;
    }
    return ret;
}