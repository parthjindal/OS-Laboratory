#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include <cstring>
#include <string>

char* autocomplete(char* input) {
    char* suff = new char[3];
    suff[0] = 'a';
    suff[1] = 'b';
    suff[2] = '\0';
    return suff;
}

char* getinput() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    int i = 0;
    char c = getchar();
    char* buff = new char[1000];
    std::string s;
    int prev = 0;
    while (c != '\n') {
        if (c == '\t') {
            char* inp = new char[((i - prev + 1) * sizeof(char))];
            strcpy(inp, buff + prev);
            char* res = autocomplete(inp);
            int j = 0;
            while (res[j] != '\0') {
                buff[i] = res[j];
                printf("%c", buff[i]);
                i++;
                j++;
            }
        } else if (c == 127) {  // backspace
            if (i > 0) {
                i--;
                printf("\b \b");
            } else {
                continue;
            }
        } else {
            buff[i] = c;
            i++;
            printf("%c", c);
        }
        c = getchar();
    }
    printf("\n");
    buff[i] = '\0';
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return buff;
}

int main() {
    char* input = getinput();
    printf("%s", input);
    return 0;
}