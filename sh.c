#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

typedef struct {
    char *filename;
    char **argv; 
} cmd;

char *read_input(void) {
    size_t lineLen = 0;
    char *input = NULL;
    if (getline(&input, &lineLen, stdin) == -1) {
        return NULL;
    }
    size_t res = strchr(input, '\n') - input;
    input[res] = 0x00;
    return input;
}

cmd parse_input(char *input) {
    char *split_string[255];
    char *token = strtok(input, " ");
    size_t count = 0;

    while (token != NULL) {
        split_string[count++] = token;
        token = strtok(NULL, " ");
    }
    cmd result;
    if (count > 0) {
        result.filename = split_string[0];
        result.argv = split_string + 1;
    } else {
        result.filename = NULL;
        result.argv = NULL;
    }
    return result;
}

int main() {
    for (;;){
        cmd command = parse_input(read_input());
        execve(command.filename, command.argv, NULL);
    }
    return 0;
}
