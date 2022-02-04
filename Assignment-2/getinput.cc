#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include <climits>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "autocomplete.h"
#include "history_search.h"
#include "shell.h"

using namespace std;

int x = 0;
int y = 0;

void erase(int num, const char* pr = "") {
    for (int i = 0; i < num; i++) {
        printf("%s\b \b", pr);
    }
}

void moveleft(int num) {
    for (int i = 0; i < num; i++) {
        printf("\x1b[D");
        x--;
    }
}
void moveright(int num) {
    for (int i = 0; i < num; i++) {
        printf("\x1b[C");
        x++;
    }
}

void moveup(int num) {
    for (int i = 0; i < num; i++) {
        printf("\x1b[A");
        y--;
    }
}

void movedown(int num) {
    for (int i = 0; i < num; i++) {
        printf("\x1b[B");
        y++;
    }
}

string CDIR;
int prompt_len;
string prompt_str = "$ ";
bool CONTINUE = 1;

int getint() {
    int c;
    int num = 0;
    char n[10];
    while ((c = getchar()) != '\n') {
        if (c == 127) {
            if (num > 0) {
                num--;
                erase(1);
            }
        } else if (c >= '0' && c <= '9') {
            printf("%c", c);
            n[num++] = c;
        }
    }
    if (num == 0)
        return 0;
    else
        return atoi(n);
}

void move2(int xx, int yy) {
    if (xx > x) {
        moveright(xx - x);
    } else if (xx < x) {
        moveleft(x - xx);
    }
    if (yy > y) {
        movedown(yy - y);
    } else if (yy < y) {
        moveup(y - yy);
    }
}

FILE* fp;

string getinput() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    string buff;
    char c;
    int num = 0;
    while ((c = getchar()) != '\n') {
        if (c == 127) {  // backspace
            if (num > 0) {
                erase(1);
                buff.pop_back();
                num--;
            } else {
                printf("\a");
            }
        } else if (c == '\t') {
            if (num == 0)
                continue;
            int id = buff.find_last_of(' ');
            id = id == string::npos ? 0 : id + 1;
            string suff = buff.substr(id);
            if (suff.length() == 0)
                continue;
            char* suff_c = strdup(suff.c_str());
            auto opts = autocomplete(suff_c);
            if (opts.size() == 1) {
                int len = strlen(opts[0]);
                for (int i = num - id; opts[0][i]; i++) {
                    buff.push_back(opts[0][i]);
                    printf("%c", opts[0][i]);
                    num++;
                }
            } else if (opts.size() > 1) {
                printf("\nEnter choice \n");
                for (int i = 0; i < opts.size(); i++) {
                    printf("%d. %s\n", i + 1, opts[i]);
                }
                printf("Choice: ");
                int choice = getint();
                printf("\n");
                PROMPT;
                printf("%s", buff.c_str());
                if (choice == 0 || choice > opts.size())
                    continue;
                for (int i = num - id; opts[choice - 1][i]; i++) {
                    buff.push_back(opts[choice - 1][i]);
                    printf("%c", opts[choice - 1][i]);
                    num++;
                }
            }
        } else if (c == 18) {
            x = 0;
            y = 0;
            string search_prompt = "Enter search term: ";
            printf("\n%s", search_prompt.c_str());
            y += 1;
            x += search_prompt.length();
            char search_term[100];
            int c = 0;
            int iter = 0;
            int res_len = 0;
            int last_len = 0;
            string res;
            while ((c = getchar()) != '\n') {
                if (c == 127) {
                    if (iter) {
                        erase(1);
                        x--;
                        iter--;
                    }
                } else if (c >= 32 && c <= 126) {
                    search_term[iter++] = c;
                    search_term[iter] = '\0';
                    x++;
                    printf("%c", c);
                } else {
                    continue;
                }
                vector<char*> results = search_history(search_term);
                if (results.size() == 0) {
                    results.push_back(strdup("No results found"));
                    res = "";
                } else
                    res = results[0];
                move2(last_len + prompt_len, y - 1);
                erase(last_len);
                x = prompt_len;
                printf("%s", results[0]);
                last_len = strlen(results[0]);
                x += last_len;
                move2(search_prompt.size() + iter, y + 1);
            }

            printf("\n");
            PROMPT;
            buff.clear();
            num = 0;
            if (res == "")
                res.insert(res.end(), search_term, search_term + iter);

            buff.insert(buff.end(), res.begin(), res.end());
            num = buff.size();
            printf("%s", buff.c_str());
        } else if (c == 4) {
            buff.clear();
            CONTINUE = 0;
            break;
        } else if (c == -1) {
            buff.clear();
            break;
        } else if (c >= 32 && c <= 126) {
            buff.push_back(c);
            num++;
            printf("%c", c);
        }
    }
    printf("\n");
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return buff;
}

// int main() {
//     for (int i = 0; i < 4; i++) {
//         string s;
//         getline(cin, s);
//         update_history(strdup(s.c_str()));
//     }
//     string input = getinput();
//     cout << input << endl;
//     return 0;
// }