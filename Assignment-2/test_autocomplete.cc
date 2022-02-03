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

char* longestCommonPrefix(vector<char*>& S) {
    char* empty = (char*)malloc(sizeof(char));
    empty[0] = '\0';
    if (S.size() == 0) return empty;
    string prefix = S[0];

    for (int i = 1; i < S.size(); ++i) {
        string s = S[i];
        if (s.size() == 0 || prefix == "") return empty;
        prefix = prefix.substr(0, min(prefix.size(), s.size()) );
        
        for (int k = 0; k < s.size() && k < prefix.size(); ++k) {
            if (s[k] != prefix[k]) {
                prefix = prefix.substr(0, k);
                break;
            }
        }
    }
    return strdup(prefix.c_str());
}

vector<char*> autocomplete(char* input){
    vector<char*> ret = {};
    vector<char*> tokens;
    char* token = strtok(input, "/");
    while(token != NULL){
        tokens.push_back(token);
        token = strtok(NULL, "/");
    }

    char* dir_path = (char *)malloc(sizeof(char) * 200);
    char* file_name = (char *)malloc(sizeof(char) * 200);
    strcpy(dir_path, "./");
    strcpy(file_name, tokens[tokens.size() - 1]);
    int name_len = strlen(file_name);

    if(tokens.size() == 0){
        return ret;
    } else if(tokens.size() > 1) {
        for(int i = 0; i < tokens.size() - 1; i++){
            strcat(dir_path, tokens[i]);
            strcat(dir_path, "/");
        }
    }

    DIR* dir = opendir(dir_path);
    if(dir == NULL){
        return ret;
    }
    struct dirent* entry;
    while((entry = readdir(dir)) != NULL){
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
            continue;
        }
        char* name = (char *)malloc(sizeof(char) * 200);
        strcpy(name, entry->d_name);
        if (entry->d_type == DT_DIR){
            name[strlen(name)] = '/';
            name[strlen(name)] = '\0';
        }
        int flag = 1;
        for(int i = 0; i < name_len; i++){
            if(name[i] != file_name[i]){
                flag = 0;
            }
        }        
        if(flag == 1)
            ret.push_back(name);
    }
    // cout << dir_path << endl;
    closedir(dir);
    char *prefix = longestCommonPrefix(ret);
    // cout << prefix << endl;
    if(strlen(prefix) != strlen(file_name) && ret.size() > 1){
        vector <char*> ret_prefix;
        ret_prefix.push_back(prefix);
        return ret_prefix;
    }
    // for(int i = 0; i < ret.size(); i++){
        // cout << ret[i] << endl;
    // }
    return ret;
}

int main() {
    char str1 []= "Tes";
    char str2 []= "Test/Test2/abe";
    char str3 []= "Test/Test2/a";
    char str4 []= "Test/Tes";

    vector <char*> ret;
    ret = autocomplete(str1);
    cout << ret.size() << endl;
    for(int i = 0; i < ret.size(); i++){
        cout << ret[i] << endl;
    }
    ret = autocomplete(str2);
    cout << ret.size() << endl;
    for(int i = 0; i < ret.size(); i++){
        cout << ret[i] << endl;
    }
    ret = autocomplete(str3);
    cout << ret.size() << endl;
    for(int i = 0; i < ret.size(); i++){
        cout << ret[i] << endl;
    }
    ret = autocomplete(str4);
    cout << ret.size() << endl;
    for(int i = 0; i < ret.size(); i++){
        cout << ret[i] << endl;
    }
    return 0;
}