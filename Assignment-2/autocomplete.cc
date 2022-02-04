#include "autocomplete.h"

#include <dirent.h>

#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "shell.h"

using namespace std;

char* longestCommonPrefix(vector<char*>& S) {
    char* empty = (char*)malloc(sizeof(char));
    empty[0] = '\0';
    if (S.size() == 0) return empty;
    string prefix = S[0];

    for (int i = 1; i < S.size(); ++i) {
        string s = S[i];
        if (s.size() == 0 || prefix == "") return empty;
        prefix = prefix.substr(0, min(prefix.size(), s.size()));

        for (int k = 0; k < s.size() && k < prefix.size(); ++k) {
            if (s[k] != prefix[k]) {
                prefix = prefix.substr(0, k);
                break;
            }
        }
    }
    return strdup(prefix.c_str());
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
    dir_path[0] = '\0';
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
    if (strlen(dir_path) == 0) {
        strcpy(dir_path, ".");
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
    closedir(dir);
    char* prefix = longestCommonPrefix(ret);
    if (strlen(prefix) != strlen(file_name) && ret.size() > 1) {
        ret.clear();
        ret.push_back(prefix);
    }
    for (int i = 0; i < ret.size(); i++) {
        if (strcmp(dir_path, ".") != 0) {
            ret[i] = strcat(dir_path, ret[i]);
        }
    }
    return ret;
}