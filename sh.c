#define _GNU_SOURCE  // Для использования getline()

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

extern char **environ;

// Структура для представления команды
typedef struct {
    char *name;   // Имя команды
    char **args;  // Аргументы команды (NULL-терминированный массив)
} Command;

/**
 * Чтение строки ввода от пользователя.
 * Возвращает указатель на строку без символа новой строки.
 */
static char *read_line(void) {
    char *line = NULL;
    size_t len = 0;

    if (getline(&line, &len, stdin) <= 0) {
        free(line);
        return NULL;
    }

    // Удаление символа новой строки, если он присутствует
    line[strcspn(line, "\n")] = '\0';
    return line;
}

/**
 * Разделение строки по заданному разделителю.
 * Возвращает NULL-терминированный массив строк.
 */
static char **split(const char *s, char delim) {
    size_t count = 1;
    for (const char *p = s; *p; p++) {
        if (*p == delim) count++;
    }

    char **parts = malloc((count + 1) * sizeof(char*));
    if (!parts) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    char *copy = strdup(s);
    if (!copy) {
        perror("strdup");
        exit(EXIT_FAILURE);
    }

    size_t i = 0;
    char *tok = strtok(copy, (char[]){delim, '\0'});
    while (tok) {
        parts[i++] = tok;
        tok = strtok(NULL, (char[]){delim, '\0'});
    }
    parts[i] = NULL;
    return parts;
}

/**
 * Разбор строки команды в структуру Command.
 */
static Command parse_cmd(char *line) {
    char **tokens = split(line, ' ');
    return (Command){ .name = tokens[0], .args = tokens };
}

/**
 * Выполнение простой команды с возможной перенаправлением ввода/вывода.
 */
static void exec_simple(Command cmd) {
    int in_fd = -1, out_fd = -1;
    char **argv = cmd.args;

    // Обработка перенаправления ввода/вывода
    for (int i = 0; argv[i]; i++) {
        if (strcmp(argv[i], "<") == 0 && argv[i+1]) {
            in_fd = open(argv[i+1], O_RDONLY);
            if (in_fd < 0) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            argv[i] = NULL; // Удаление символа перенаправления из аргументов
        } else if (strcmp(argv[i], ">") == 0 && argv[i+1]) {
            out_fd = open(argv[i+1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (out_fd < 0) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            argv[i] = NULL;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
    } else if (pid == 0) {
        // Дочерний процесс

        // Перенаправление ввода, если указано
        if (in_fd >= 0) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }

        // Перенаправление вывода, если указано
        if (out_fd >= 0) {
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }

        execvp(cmd.name, cmd.args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        // Родительский процесс
        waitpid(pid, NULL, 0);
        if (in_fd >= 0) close(in_fd);
        if (out_fd >= 0) close(out_fd);
    }
}

/**
 * Выполнение команды с использованием пайпа между двумя командами.
 */
static void exec_pipe(char *left, char *right) {
    int fd[2];
    if (pipe(fd) < 0) {
        perror("pipe");
        return;
    }

    pid_t p1 = fork();
    if (p1 == 0) {
        // Первый дочерний процесс: выполняет левую команду
        dup2(fd[1], STDOUT_FILENO); // Перенаправление stdout в пайп
        close(fd[0]); close(fd[1]);
        Command c = parse_cmd(left);
        exec_simple(c);
        _exit(EXIT_FAILURE);
    }

    pid_t p2 = fork();
    if (p2 == 0) {
        // Второй дочерний процесс: выполняет правую команду
        dup2(fd[0], STDIN_FILENO); // Перенаправление stdin из пайпа
        close(fd[1]); close(fd[0]);
        Command c = parse_cmd(right);
        exec_simple(c);
        _exit(EXIT_FAILURE);
    }

    // Родительский процесс закрывает пайп и ожидает завершения дочерних процессов
    close(fd[0]); close(fd[1]);
    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);
}

/**
 * Главная функция: цикл чтения и выполнения команд.
 */
int main(void) {
    while (1) {
        printf("esd> ");
        fflush(stdout);
        char *line = read_line();
        if (!line) break;

        // Обработка встроенных команд: exit, cd
        if (strcmp(line, "exit") == 0) {
            free(line);
            break;
        }

        if (strncmp(line, "cd", 2) == 0 &&
            (line[2] == ' ' || line[2] == '\0')) {
            char *path = line[2] ? line + 3 : getenv("HOME");
            if (chdir(path) < 0) perror("chdir");
            free(line);
            continue;
        }

        // Обработка пайпов
        char *pipe_pos = strchr(line, '|');
        if (pipe_pos) {
            *pipe_pos = '\0';
            char *left = line;
            char *right = pipe_pos + 1;
            exec_pipe(left, right);
        } else {
            // Выполнение простой или перенаправленной команды
            Command cmd = parse_cmd(line);
            exec_simple(cmd);
        }

        free(line);
    }
    return 0;
}
