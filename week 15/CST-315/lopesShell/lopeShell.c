// lopeShell.c - Mini command line interpreter (no raw mode)
//
// Build: gcc -Wall -Wextra -O2 -o lopeShell lopeShell.c fs.c
// Run interactive: ./lopeShell
// Run batch:       ./lopeShell batch.txt
//
// Key combos (no raw mode):
// - Ctrl-C  (SIGINT)  : End execution (terminate running children, keep shell alive)
// - Ctrl-\  (SIGQUIT) : Exit the shell
//
// Built-ins:
// - end              : end execution
// - quit / exit      : exit shell
// - File system:     cd, pwd, ls, mkdir, rmdir, touch, rm, rename, edit,
//                    mv, cp, find, tree, stat, dirinfo
//
// External commands:
// - fork() + execv() with PATH searching if no '/' is provided
// - Semicolon-separated commands run concurrently; parent waits with waitpid()

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "fs.h"

#define PROMPT "$lopeShell > "
#define MAX_LINE 4096
#define MAX_CMDS 256
#define MAX_ARGS 256

// Track running children for "end execution"
static pid_t g_pids[MAX_CMDS];
static size_t g_pid_count = 0;

// Exit requested via Ctrl-\ (SIGQUIT) or typed command
static volatile sig_atomic_t g_exit_requested = 0;

static void clear_pid_list(void) { g_pid_count = 0; }
static void add_pid(pid_t p) { if (g_pid_count < MAX_CMDS) g_pids[g_pid_count++] = p; }

static char *ltrim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static void rtrim_inplace(char *s) {
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[n - 1] = '\0';
        n--;
    }
}

static bool is_blank(const char *s) {
    for (; *s; s++) if (!isspace((unsigned char)*s)) return false;
    return true;
}

static void end_execution(void) {
    // Terminate running children
    for (size_t i = 0; i < g_pid_count; i++) {
        if (g_pids[i] > 0) kill(g_pids[i], SIGTERM);
    }
    // Reap them
    for (size_t i = 0; i < g_pid_count; i++) {
        if (g_pids[i] > 0) (void)waitpid(g_pids[i], NULL, 0);
    }
    clear_pid_list();
}

static void on_sigint(int signo) {
    (void)signo;
    // Ctrl-C: end execution
    end_execution();
}

static void on_sigquit(int signo) {
    (void)signo;
    // Ctrl-\: exit shell
    g_exit_requested = 1;
    // Also end running children so we don't leave zombies
    end_execution();
}

// Tokenize a command segment into argv[] (in-place). Returns argc.
static int tokenize(char *cmd, char *argv[], int max_args) {
    int argc = 0;
    char *p = ltrim(cmd);
    rtrim_inplace(p);
    if (*p == '\0') return 0;

    char *save = NULL;
    for (char *tok = strtok_r(p, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save)) {
        if (argc + 1 >= max_args) break;
        argv[argc++] = tok;
    }
    argv[argc] = NULL;
    return argc;
}

static bool has_slash(const char *s) { return s && strchr(s, '/') != NULL; }

// Resolve executable for execv():
// - If argv0 has '/', use it directly (check X_OK).
// - Else search PATH env (split by ':').
// Returns malloc'd path or NULL.
static char *resolve_execv_path(const char *argv0) {
    if (!argv0 || *argv0 == '\0') return NULL;

    if (has_slash(argv0)) {
        if (access(argv0, X_OK) == 0) return strdup(argv0);
        return NULL;
    }

    const char *pathenv = getenv("PATH");
    if (!pathenv || *pathenv == '\0') return NULL;

    char *paths = strdup(pathenv);
    if (!paths) return NULL;

    char *save = NULL;
    for (char *dir = strtok_r(paths, ":", &save); dir; dir = strtok_r(NULL, ":", &save)) {
        if (*dir == '\0') continue;

        size_t need = strlen(dir) + 1 + strlen(argv0) + 1;
        char *full = (char *)malloc(need);
        if (!full) { free(paths); return NULL; }

        snprintf(full, need, "%s/%s", dir, argv0);
        if (access(full, X_OK) == 0) {
            free(paths);
            return full; // caller frees
        }
        free(full);
    }

    free(paths);
    return NULL;
}

// Execute one full line (may include ';') concurrently.
static void execute_line(char *line) {
    char *work = ltrim(line);
    rtrim_inplace(work);
    if (*work == '\0') return;

    // Split by ';'
    char *cmds[MAX_CMDS];
    int cmd_count = 0;

    char *save = NULL;
    for (char *seg = strtok_r(work, ";", &save); seg; seg = strtok_r(NULL, ";", &save)) {
        if (cmd_count >= MAX_CMDS) break;
        cmds[cmd_count++] = seg;
    }

    clear_pid_list();

    // Launch all commands first (concurrent)
    for (int i = 0; i < cmd_count; i++) {
        char *argv[MAX_ARGS];
        int argc = tokenize(cmds[i], argv, MAX_ARGS);
        if (argc == 0) continue;

        // Built-ins (handled by parent)
        if (strcmp(argv[0], "quit") == 0 || strcmp(argv[0], "exit") == 0) {
            g_exit_requested = 1;
            end_execution();
            return;
        }
        if (strcmp(argv[0], "end") == 0) {
            end_execution();
            continue;
        }

        if (dispatch_fs_builtin(argc, argv))
            continue;

        char *path = resolve_execv_path(argv[0]);
        if (!path) {
            fprintf(stderr, "Command not found: %s\n", argv[0]);
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            free(path);
            continue;
        }

        if (pid == 0) {
            execv(path, argv);
            fprintf(stderr, "execv failed (%s): %s\n", path, strerror(errno));
            _exit(127);
        }

        free(path);
        add_pid(pid);
    }

    // Wait for all launched children
    for (size_t i = 0; i < g_pid_count; i++) {
        while (1) {
            pid_t w = waitpid(g_pids[i], NULL, 0);
            if (w >= 0) break;
            if (errno == EINTR) {
                // If Ctrl-\ requested exit, break out
                if (g_exit_requested) return;
                // Otherwise continue waiting (Ctrl-C handler already ended children)
                continue;
            }
            break;
        }
    }
    clear_pid_list();
}

static void run_batch(FILE *fp) {
    char line[MAX_LINE];

    while (fgets(line, sizeof(line), fp)) {
        // Echo each line (requirement)
        fputs(line, stdout);
        if (!strchr(line, '\n')) fputc('\n', stdout);

        if (is_blank(line)) continue;
        execute_line(line);

        if (g_exit_requested) break;
    }
}

int main(int argc, char *argv[]) {
    fs_init();

    struct sigaction sa_int = {0};
    sa_int.sa_handler = on_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    struct sigaction sa_quit = {0};
    sa_quit.sa_handler = on_sigquit;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;
    sigaction(SIGQUIT, &sa_quit, NULL);

    // Batch mode
    if (argc == 2) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) {
            perror("fopen");
            return 1;
        }
        run_batch(fp);
        fclose(fp);
        fs_cleanup();
        return 0;
    }

    // Interactive mode
    if (argc == 1) {
        char line[MAX_LINE];

        while (!g_exit_requested) {
            printf("%s", PROMPT);
            fflush(stdout);

            if (!fgets(line, sizeof(line), stdin)) break; // EOF

            if (is_blank(line)) continue;
            execute_line(line);
        }
        fs_cleanup();
        return 0;
    }

    fprintf(stderr, "Usage: %s [batch_file]\n", argv[0]);
    return 1;
}