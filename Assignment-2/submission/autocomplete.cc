#include "autocomplete.h"

#include <dirent.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <bits/stdc++.h>
#include "shell.h"

using namespace std;

string longestCommonPrefix(vector<string>& S) {
    string res;
    if (S.size() == 0) return res;
    string prefix = S[0];

    for (int i = 1; i < S.size(); ++i) {
        string s = S[i];
        if (s.size() == 0 || prefix == "") return res;
        prefix = prefix.substr(0, min(prefix.size(), s.size()));

        for (int k = 0; k < s.size() && k < prefix.size(); ++k) {
            if (s[k] != prefix[k]) {
                prefix = prefix.substr(0, k);
                break;
            }
        }
    }
    return prefix;
}

vector<string> autocomplete(string input) {
    vector<string> ret = {};
    vector<string> tokens;
    stringstream ss(input);
    string token;
    while (getline(ss, token, '/')) {
        tokens.push_back(token);
    }
    if (input.back() == '/' || tokens.size() == 0) { // if input is a directory
        tokens.push_back("");
    }
    string dir_path;
    string file_name;
    file_name = tokens[tokens.size() - 1];
    int name_len = file_name.size();

    if (tokens.size() == 0) {
        return ret;
    } else if (tokens.size() > 1) {
        for (int i = 0; i < tokens.size() - 1; i++) {
            dir_path += tokens[i] + "/";
        }
    }
    if (dir_path == "") {
        dir_path = ".";
    }
    DIR* dir = opendir(dir_path.c_str());
    if (dir == NULL) {
        return ret;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        string name = entry->d_name;
        if (entry->d_type == DT_DIR) {
            name += "/";
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
    string prefix = longestCommonPrefix(ret);
    if ((prefix.size() > file_name.size()) && ret.size() > 1) {
        ret.clear();
        ret.push_back(prefix);
    }
    for (int i = 0; i < ret.size(); i++) {
        if (dir_path != ".") {
            ret[i] = dir_path + ret[i];
        }
    }
    return ret;
}