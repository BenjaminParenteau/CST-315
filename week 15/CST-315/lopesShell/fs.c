/*
 * fs.c - In-memory hierarchical file system implementation for lopeShell
 */

#define _POSIX_C_SOURCE 200809L

#include "fs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  Globals
 * ================================================================ */

FSNode       *fs_root = NULL;
FSNode       *fs_cwd  = NULL;
FileDirectory file_dir;

/* ================================================================
 *  File directory (flat descriptor table)
 * ================================================================ */

void filedir_add(FSNode *file) {
    if (!file || file->type != FS_FILE) return;
    if (file_dir.count >= FS_MAX_FILES) {
        fprintf(stderr, "fs: file directory full\n");
        return;
    }
    file_dir.entries[file_dir.count++] = file;
}

void filedir_remove(FSNode *file) {
    for (int i = 0; i < file_dir.count; i++) {
        if (file_dir.entries[i] == file) {
            file_dir.entries[i] = file_dir.entries[--file_dir.count];
            return;
        }
    }
}

/* ================================================================
 *  Node helpers
 * ================================================================ */

FSNode *create_node(const char *name, NodeType type, FSNode *parent) {
    FSNode *n = calloc(1, sizeof(FSNode));
    if (!n) { perror("calloc"); return NULL; }

    strncpy(n->name, name, FS_NAME_MAX - 1);
    n->type        = type;
    n->parent      = parent;
    n->created     = time(NULL);
    n->modified    = n->created;
    n->permissions = (type == FS_DIR) ? 0755 : 0644;

    if (type == FS_DIR) {
        n->child_capacity = FS_INIT_CAP;
        n->children = calloc(n->child_capacity, sizeof(FSNode *));
    }

    if (parent) attach_child(parent, n);
    return n;
}

void free_node(FSNode *node) {
    if (!node) return;

    if (node->type == FS_DIR) {
        for (int i = 0; i < node->child_count; i++)
            free_node(node->children[i]);
        free(node->children);
    } else {
        filedir_remove(node);
        free(node->data);
    }
    free(node);
}

void attach_child(FSNode *parent, FSNode *child) {
    if (!parent || parent->type != FS_DIR || !child) return;

    if (parent->child_count >= parent->child_capacity) {
        int newcap = parent->child_capacity * 2;
        FSNode **tmp = realloc(parent->children, newcap * sizeof(FSNode *));
        if (!tmp) { perror("realloc"); return; }
        parent->children      = tmp;
        parent->child_capacity = newcap;
    }
    parent->children[parent->child_count++] = child;
    child->parent = parent;
    parent->modified = time(NULL);
}

void detach_child(FSNode *parent, FSNode *child) {
    if (!parent || !child) return;
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            parent->children[i] = parent->children[--parent->child_count];
            child->parent = NULL;
            parent->modified = time(NULL);
            return;
        }
    }
}

FSNode *find_child(FSNode *dir, const char *name) {
    if (!dir || dir->type != FS_DIR) return NULL;
    for (int i = 0; i < dir->child_count; i++) {
        if (strcmp(dir->children[i]->name, name) == 0)
            return dir->children[i];
    }
    return NULL;
}

FSNode *deep_copy_node(FSNode *src, FSNode *new_parent) {
    if (!src) return NULL;

    FSNode *copy = create_node(src->name, src->type, new_parent);
    if (!copy) return NULL;

    copy->permissions = src->permissions;
    copy->created     = src->created;
    copy->modified    = time(NULL);

    if (src->type == FS_FILE) {
        if (src->size > 0 && src->data) {
            copy->data = malloc(src->size);
            if (copy->data) {
                memcpy(copy->data, src->data, src->size);
                copy->size = src->size;
            }
        }
        filedir_add(copy);
    } else {
        for (int i = 0; i < src->child_count; i++)
            deep_copy_node(src->children[i], copy);
    }
    return copy;
}

/* ================================================================
 *  Path resolution
 * ================================================================ */

char *get_full_path(const FSNode *node) {
    static char buf[FS_PATH_MAX];

    if (!node) { buf[0] = '\0'; return buf; }
    if (node == fs_root) { strcpy(buf, "/"); return buf; }

    /* Walk up to root, collect names */
    const FSNode *stack[256];
    int depth = 0;
    for (const FSNode *n = node; n && n != fs_root && depth < 256; n = n->parent)
        stack[depth++] = n;

    buf[0] = '\0';
    for (int i = depth - 1; i >= 0; i--) {
        strcat(buf, "/");
        strcat(buf, stack[i]->name);
    }
    if (buf[0] == '\0') strcpy(buf, "/");
    return buf;
}

FSNode *resolve_path(const char *path) {
    if (!path || *path == '\0') return fs_cwd;

    FSNode *cur = (*path == '/') ? fs_root : fs_cwd;

    char tmp[FS_PATH_MAX];
    strncpy(tmp, path, FS_PATH_MAX - 1);
    tmp[FS_PATH_MAX - 1] = '\0';

    char *save = NULL;
    for (char *tok = strtok_r(tmp, "/", &save); tok; tok = strtok_r(NULL, "/", &save)) {
        if (strcmp(tok, ".") == 0) continue;

        if (strcmp(tok, "..") == 0) {
            if (cur->parent) cur = cur->parent;
            continue;
        }

        if (cur->type != FS_DIR) return NULL;
        FSNode *child = find_child(cur, tok);
        if (!child) return NULL;
        cur = child;
    }
    return cur;
}

/*
 * Resolve all but the last component (the parent), and write the
 * last component into 'leaf' buffer. Returns the parent node or NULL.
 */
static FSNode *resolve_parent_and_leaf(const char *path, char *leaf, size_t leafsz) {
    if (!path || *path == '\0') return NULL;

    char tmp[FS_PATH_MAX];
    strncpy(tmp, path, FS_PATH_MAX - 1);
    tmp[FS_PATH_MAX - 1] = '\0';

    /* Find the last '/' */
    char *slash = strrchr(tmp, '/');
    if (slash) {
        strncpy(leaf, slash + 1, leafsz - 1);
        leaf[leafsz - 1] = '\0';
        if (slash == tmp)
            return fs_root;           /* path like "/foo" */
        *slash = '\0';
        return resolve_path(tmp);
    }

    /* No slash -> parent is cwd, leaf is the whole string */
    strncpy(leaf, tmp, leafsz - 1);
    leaf[leafsz - 1] = '\0';
    return fs_cwd;
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

void fs_init(void) {
    memset(&file_dir, 0, sizeof(file_dir));
    fs_root = create_node("/", FS_DIR, NULL);
    fs_cwd  = fs_root;
    srand((unsigned)time(NULL));
}

void fs_cleanup(void) {
    free_node(fs_root);
    fs_root = NULL;
    fs_cwd  = NULL;
    memset(&file_dir, 0, sizeof(file_dir));
}

/* ================================================================
 *  Navigation commands: cd, pwd, ls
 * ================================================================ */

void cmd_cd(int argc, char *argv[]) {
    const char *target = (argc >= 2) ? argv[1] : "/";
    FSNode *node = resolve_path(target);

    if (!node)          { fprintf(stderr, "cd: no such directory: %s\n", target); return; }
    if (node->type != FS_DIR) { fprintf(stderr, "cd: not a directory: %s\n", target); return; }
    fs_cwd = node;
}

void cmd_pwd(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("%s\n", get_full_path(fs_cwd));
}

void cmd_ls(int argc, char *argv[]) {
    FSNode *dir = fs_cwd;
    if (argc >= 2) {
        dir = resolve_path(argv[1]);
        if (!dir) { fprintf(stderr, "ls: no such directory: %s\n", argv[1]); return; }
        if (dir->type != FS_DIR) { fprintf(stderr, "ls: not a directory: %s\n", argv[1]); return; }
    }
    for (int i = 0; i < dir->child_count; i++) {
        FSNode *c = dir->children[i];
        if (c->type == FS_DIR)
            printf("  [DIR]  %s\n", c->name);
        else
            printf("  [FILE] %s  (%zu bytes)\n", c->name, c->size);
    }
    if (dir->child_count == 0)
        printf("  (empty)\n");
}

/* ================================================================
 *  Directory commands: mkdir, rmdir
 * ================================================================ */

void cmd_mkdir(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: mkdir <path>\n"); return; }

    char leaf[FS_NAME_MAX];
    FSNode *parent = resolve_parent_and_leaf(argv[1], leaf, sizeof(leaf));
    if (!parent || parent->type != FS_DIR) {
        fprintf(stderr, "mkdir: cannot create directory '%s': parent not found\n", argv[1]);
        return;
    }
    if (find_child(parent, leaf)) {
        fprintf(stderr, "mkdir: '%s' already exists\n", leaf);
        return;
    }
    create_node(leaf, FS_DIR, parent);
    printf("Directory created: %s\n", argv[1]);
}

static void rmdir_recursive(FSNode *node) {
    if (!node) return;
    if (node->parent) detach_child(node->parent, node);
    free_node(node);
}

void cmd_rmdir(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: rmdir [-r] <path>\n"); return; }

    int force = 0;
    const char *target;
    if (argc >= 3 && strcmp(argv[1], "-r") == 0) {
        force = 1;
        target = argv[2];
    } else {
        target = argv[1];
    }

    FSNode *node = resolve_path(target);
    if (!node)                { fprintf(stderr, "rmdir: no such directory: %s\n", target); return; }
    if (node->type != FS_DIR) { fprintf(stderr, "rmdir: not a directory: %s\n", target); return; }
    if (node == fs_root)      { fprintf(stderr, "rmdir: cannot remove root directory\n"); return; }

    if (node->child_count > 0 && !force) {
        fprintf(stderr, "rmdir: '%s' is not empty (use -r to force)\n", target);
        return;
    }

    if (fs_cwd == node) fs_cwd = node->parent ? node->parent : fs_root;

    printf("Directory removed: %s\n", target);
    rmdir_recursive(node);
}

/* ================================================================
 *  File commands: touch, rm, rename, edit
 * ================================================================ */

void cmd_touch(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: touch [-s <bytes>] <name>\n"); return; }

    int has_size = 0;
    size_t gen_size = 0;
    const char *name;

    if (argc >= 4 && strcmp(argv[1], "-s") == 0) {
        has_size = 1;
        gen_size = (size_t)atol(argv[2]);
        name = argv[3];
    } else {
        name = argv[1];
    }

    char leaf[FS_NAME_MAX];
    FSNode *parent = resolve_parent_and_leaf(name, leaf, sizeof(leaf));
    if (!parent || parent->type != FS_DIR) {
        fprintf(stderr, "touch: cannot create file '%s': parent not found\n", name);
        return;
    }
    if (find_child(parent, leaf)) {
        fprintf(stderr, "touch: '%s' already exists\n", leaf);
        return;
    }

    FSNode *f = create_node(leaf, FS_FILE, parent);
    if (!f) return;

    if (has_size && gen_size > 0) {
        f->data = malloc(gen_size);
        if (f->data) {
            for (size_t i = 0; i < gen_size; i++)
                f->data[i] = (char)(33 + rand() % 94); /* printable ASCII */
            f->size = gen_size;
        }
    }

    filedir_add(f);
    if (has_size)
        printf("File created: %s (%zu bytes of random data)\n", name, gen_size);
    else
        printf("File created: %s (empty)\n", name);
}

void cmd_rm(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: rm <file>\n"); return; }

    FSNode *node = resolve_path(argv[1]);
    if (!node)                 { fprintf(stderr, "rm: no such file: %s\n", argv[1]); return; }
    if (node->type != FS_FILE) { fprintf(stderr, "rm: not a file (use rmdir for directories): %s\n", argv[1]); return; }

    printf("File removed: %s\n", argv[1]);
    if (node->parent) detach_child(node->parent, node);
    free_node(node);
}

void cmd_rename(int argc, char *argv[]) {
    if (argc < 3) { fprintf(stderr, "Usage: rename <old> <new_name>\n"); return; }

    FSNode *node = resolve_path(argv[1]);
    if (!node) { fprintf(stderr, "rename: no such file or directory: %s\n", argv[1]); return; }

    if (node->parent && find_child(node->parent, argv[2])) {
        fprintf(stderr, "rename: '%s' already exists in the same directory\n", argv[2]);
        return;
    }

    printf("Renamed '%s' -> '%s'\n", node->name, argv[2]);
    strncpy(node->name, argv[2], FS_NAME_MAX - 1);
    node->name[FS_NAME_MAX - 1] = '\0';
    node->modified = time(NULL);
}

void cmd_edit(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: edit <file>\n"); return; }

    FSNode *node = resolve_path(argv[1]);
    if (!node)                 { fprintf(stderr, "edit: no such file: %s\n", argv[1]); return; }
    if (node->type != FS_FILE) { fprintf(stderr, "edit: not a file: %s\n", argv[1]); return; }

    printf("--- Editing '%s' ---\n", node->name);
    if (node->size > 0 && node->data)
        printf("Current content (%zu bytes):\n%.*s\n", node->size, (int)node->size, node->data);
    else
        printf("(file is empty)\n");

    printf("Enter new content (type END on a line by itself to finish):\n");

    char   *buf  = NULL;
    size_t  len  = 0;
    size_t  cap  = 0;
    char    line[1024];

    while (fgets(line, sizeof(line), stdin)) {
        /* Strip trailing newline for comparison */
        size_t ln = strlen(line);
        if (ln > 0 && line[ln - 1] == '\n') line[--ln] = '\0';

        if (strcmp(line, "END") == 0) break;

        /* Re-add the newline for storage */
        line[ln] = '\n';
        line[ln + 1] = '\0';
        ln++;

        if (len + ln >= cap) {
            cap = (cap == 0) ? 256 : cap * 2;
            if (cap < len + ln + 1) cap = len + ln + 1;
            char *tmp = realloc(buf, cap);
            if (!tmp) { perror("realloc"); free(buf); return; }
            buf = tmp;
        }
        memcpy(buf + len, line, ln);
        len += ln;
    }

    free(node->data);
    node->data     = buf;
    node->size     = len;
    node->modified = time(NULL);
    printf("File updated: %s (%zu bytes)\n", node->name, len);
}

/* ================================================================
 *  File operations: mv, cp
 * ================================================================ */

void cmd_mv(int argc, char *argv[]) {
    if (argc < 3) { fprintf(stderr, "Usage: mv <file> <dest_dir>\n"); return; }

    FSNode *src = resolve_path(argv[1]);
    if (!src)                 { fprintf(stderr, "mv: no such file: %s\n", argv[1]); return; }
    if (src->type != FS_FILE) { fprintf(stderr, "mv: not a file: %s\n", argv[1]); return; }

    FSNode *dst = resolve_path(argv[2]);
    if (!dst)                { fprintf(stderr, "mv: no such directory: %s\n", argv[2]); return; }
    if (dst->type != FS_DIR) { fprintf(stderr, "mv: destination is not a directory: %s\n", argv[2]); return; }

    if (find_child(dst, src->name)) {
        fprintf(stderr, "mv: '%s' already exists in destination\n", src->name);
        return;
    }

    detach_child(src->parent, src);
    attach_child(dst, src);
    src->modified = time(NULL);
    printf("Moved '%s' -> %s/\n", src->name, get_full_path(dst));
}

void cmd_cp(int argc, char *argv[]) {
    int recursive = 0;
    const char *src_path, *dst_path;

    if (argc >= 4 && strcmp(argv[1], "-r") == 0) {
        recursive = 1;
        src_path = argv[2];
        dst_path = argv[3];
    } else if (argc >= 3) {
        src_path = argv[1];
        dst_path = argv[2];
    } else {
        fprintf(stderr, "Usage: cp [-r] <src> <dest>\n");
        return;
    }

    FSNode *src = resolve_path(src_path);
    if (!src) { fprintf(stderr, "cp: no such file or directory: %s\n", src_path); return; }

    if (src->type == FS_DIR && !recursive) {
        fprintf(stderr, "cp: '%s' is a directory (use -r to copy recursively)\n", src_path);
        return;
    }

    FSNode *dst = resolve_path(dst_path);

    if (dst && dst->type == FS_DIR) {
        /* Destination is an existing directory: copy src into it */
        if (find_child(dst, src->name)) {
            fprintf(stderr, "cp: '%s' already exists in destination\n", src->name);
            return;
        }
        deep_copy_node(src, dst);
        if (src->type == FS_DIR)
            printf("Directory copied: %s -> %s/\n", src_path, get_full_path(dst));
        else
            printf("File copied: %s -> %s/\n", src_path, get_full_path(dst));
    } else if (!dst) {
        /* Destination doesn't exist: create copy with new name in parent dir */
        char leaf[FS_NAME_MAX];
        FSNode *parent = resolve_parent_and_leaf(dst_path, leaf, sizeof(leaf));
        if (!parent || parent->type != FS_DIR) {
            fprintf(stderr, "cp: cannot copy to '%s': parent not found\n", dst_path);
            return;
        }
        if (find_child(parent, leaf)) {
            fprintf(stderr, "cp: '%s' already exists\n", leaf);
            return;
        }
        FSNode *copy = deep_copy_node(src, parent);
        if (copy) {
            strncpy(copy->name, leaf, FS_NAME_MAX - 1);
            copy->name[FS_NAME_MAX - 1] = '\0';
        }
        if (src->type == FS_DIR)
            printf("Directory copied: %s -> %s\n", src_path, dst_path);
        else
            printf("File copied: %s -> %s\n", src_path, dst_path);
    } else {
        fprintf(stderr, "cp: destination '%s' already exists and is not a directory\n", dst_path);
    }
}

/* ================================================================
 *  Search and display: find, tree
 * ================================================================ */

static void find_recursive(FSNode *node, const char *name, const char *prefix) {
    if (!node) return;
    char path[FS_PATH_MAX];

    if (strcmp(node->name, name) == 0) {
        snprintf(path, sizeof(path), "%s%s%s",
                 prefix, (prefix[0] && prefix[strlen(prefix)-1] != '/') ? "/" : "",
                 node->name);
        printf("  Found: %s  [%s]\n", path, node->type == FS_DIR ? "DIR" : "FILE");
    }

    if (node->type == FS_DIR) {
        char newprefix[FS_PATH_MAX];
        if (node == fs_root)
            snprintf(newprefix, sizeof(newprefix), "/");
        else
            snprintf(newprefix, sizeof(newprefix), "%s%s%s",
                     prefix, (prefix[0] && prefix[strlen(prefix)-1] != '/') ? "/" : "",
                     node->name);

        for (int i = 0; i < node->child_count; i++)
            find_recursive(node->children[i], name, newprefix);
    }
}

void cmd_find(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: find <name> [start_dir]\n"); return; }

    const char *name = argv[1];
    FSNode *start = fs_cwd;
    if (argc >= 3) {
        start = resolve_path(argv[2]);
        if (!start || start->type != FS_DIR) {
            fprintf(stderr, "find: invalid start directory: %s\n", argv[2]);
            return;
        }
    }

    printf("Searching for '%s' from %s:\n", name, get_full_path(start));

    char prefix[FS_PATH_MAX];
    if (start == fs_root)
        strcpy(prefix, "");
    else
        strncpy(prefix, get_full_path(start), FS_PATH_MAX - 1);

    /* Check start node itself first, then recurse into its children */
    if (start->type == FS_DIR) {
        if (strcmp(start->name, name) == 0)
            printf("  Found: %s  [DIR]\n", get_full_path(start));
        for (int i = 0; i < start->child_count; i++)
            find_recursive(start->children[i], name, prefix);
    }

    printf("Search complete.\n");
}

static void tree_recursive(FSNode *node, const char *indent, int is_last) {
    if (!node) return;

    printf("%s%s %s", indent, is_last ? "└──" : "├──",  node->name);
    if (node->type == FS_FILE)
        printf("  (%zu bytes)", node->size);
    printf("\n");

    if (node->type == FS_DIR) {
        char new_indent[FS_PATH_MAX];
        snprintf(new_indent, sizeof(new_indent), "%s%s", indent, is_last ? "    " : "│   ");

        for (int i = 0; i < node->child_count; i++)
            tree_recursive(node->children[i], new_indent, i == node->child_count - 1);
    }
}

void cmd_tree(int argc, char *argv[]) {
    FSNode *start = fs_cwd;
    if (argc >= 2) {
        start = resolve_path(argv[1]);
        if (!start) { fprintf(stderr, "tree: no such directory: %s\n", argv[1]); return; }
        if (start->type != FS_DIR) { fprintf(stderr, "tree: not a directory: %s\n", argv[1]); return; }
    }

    printf("%s\n", get_full_path(start));
    for (int i = 0; i < start->child_count; i++)
        tree_recursive(start->children[i], "", i == start->child_count - 1);
}

/* ================================================================
 *  Info commands: stat, dirinfo
 * ================================================================ */

static size_t dir_total_size(FSNode *dir) {
    size_t total = 0;
    if (!dir) return 0;
    for (int i = 0; i < dir->child_count; i++) {
        if (dir->children[i]->type == FS_FILE)
            total += dir->children[i]->size;
        else
            total += dir_total_size(dir->children[i]);
    }
    return total;
}

void cmd_stat(int argc, char *argv[]) {
    int detailed = 0;
    const char *target;

    if (argc >= 3 && strcmp(argv[1], "-l") == 0) {
        detailed = 1;
        target = argv[2];
    } else if (argc >= 2) {
        target = argv[1];
    } else {
        fprintf(stderr, "Usage: stat [-l] <file>\n");
        return;
    }

    FSNode *node = resolve_path(target);
    if (!node)                 { fprintf(stderr, "stat: no such file: %s\n", target); return; }
    if (node->type != FS_FILE) { fprintf(stderr, "stat: not a file (use dirinfo for directories): %s\n", target); return; }

    printf("  Name: %s\n", node->name);
    printf("  Type: FILE\n");
    printf("  Size: %zu bytes\n", node->size);

    if (detailed) {
        printf("  Path: %s\n", get_full_path(node));
        printf("  Perm: %04o\n", node->permissions);

        char tbuf[64];
        struct tm *tm;
        tm = localtime(&node->created);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);
        printf("  Created:  %s\n", tbuf);

        tm = localtime(&node->modified);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);
        printf("  Modified: %s\n", tbuf);
    }
}

void cmd_dirinfo(int argc, char *argv[]) {
    int detailed = 0;
    const char *target;

    if (argc >= 3 && strcmp(argv[1], "-l") == 0) {
        detailed = 1;
        target = argv[2];
    } else if (argc >= 2) {
        target = argv[1];
    } else {
        fprintf(stderr, "Usage: dirinfo [-l] <directory>\n");
        return;
    }

    FSNode *node = resolve_path(target);
    if (!node)                { fprintf(stderr, "dirinfo: no such directory: %s\n", target); return; }
    if (node->type != FS_DIR) { fprintf(stderr, "dirinfo: not a directory: %s\n", target); return; }

    printf("  Name:     %s\n", node->name);
    printf("  Type:     DIR\n");
    printf("  Children: %d\n", node->child_count);

    if (detailed) {
        printf("  Path:       %s\n", get_full_path(node));
        printf("  Total size: %zu bytes\n", dir_total_size(node));
        printf("  Perm:       %04o\n", node->permissions);

        char tbuf[64];
        struct tm *tm;
        tm = localtime(&node->created);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);
        printf("  Created:    %s\n", tbuf);

        tm = localtime(&node->modified);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);
        printf("  Modified:   %s\n", tbuf);
    }
}

/* ================================================================
 *  Dispatch table
 * ================================================================ */

typedef struct {
    const char *name;
    void (*handler)(int, char **);
} BuiltinEntry;

static const BuiltinEntry fs_builtins[] = {
    { "cd",      cmd_cd      },
    { "pwd",     cmd_pwd     },
    { "ls",      cmd_ls      },
    { "mkdir",   cmd_mkdir   },
    { "rmdir",   cmd_rmdir   },
    { "touch",   cmd_touch   },
    { "rm",      cmd_rm      },
    { "rename",  cmd_rename  },
    { "edit",    cmd_edit    },
    { "mv",      cmd_mv      },
    { "cp",      cmd_cp      },
    { "find",    cmd_find    },
    { "tree",    cmd_tree    },
    { "stat",    cmd_stat    },
    { "dirinfo", cmd_dirinfo },
    { NULL, NULL }
};

int dispatch_fs_builtin(int argc, char *argv[]) {
    if (argc == 0 || !argv[0]) return 0;
    for (const BuiltinEntry *b = fs_builtins; b->name; b++) {
        if (strcmp(argv[0], b->name) == 0) {
            b->handler(argc, argv);
            return 1;
        }
    }
    return 0;
}
