/*
 * fs.h - In-memory hierarchical file system for lopeShell
 *
 * Memory separation from pager (Project 4):
 *   File data lives on the general heap via malloc()/free().
 *   The pager uses its own static global arrays (frames[], processes[]).
 *   The two regions never overlap.
 */

#ifndef FS_H
#define FS_H

#include <stddef.h>
#include <sys/types.h>
#include <time.h>

#define FS_NAME_MAX    256
#define FS_PATH_MAX   4096
#define FS_MAX_FILES  1024
#define FS_INIT_CAP      8

typedef enum { FS_FILE, FS_DIR } NodeType;

typedef struct FSNode {
    char        name[FS_NAME_MAX];
    NodeType    type;
    struct FSNode *parent;

    /* Directory fields */
    struct FSNode **children;
    int         child_count;
    int         child_capacity;

    /* File fields */
    char       *data;
    size_t      size;

    /* Metadata */
    time_t      created;
    time_t      modified;
    mode_t      permissions;
} FSNode;

typedef struct {
    FSNode *entries[FS_MAX_FILES];
    int     count;
} FileDirectory;

/* ---- globals (defined in fs.c) ---- */
extern FSNode       *fs_root;
extern FSNode       *fs_cwd;
extern FileDirectory file_dir;

/* ---- lifecycle ---- */
void     fs_init(void);
void     fs_cleanup(void);

/* ---- path resolution ---- */
FSNode  *resolve_path(const char *path);
char    *get_full_path(const FSNode *node);

/* ---- file directory (descriptor table) ---- */
void     filedir_add(FSNode *file);
void     filedir_remove(FSNode *file);

/* ---- node helpers ---- */
FSNode  *create_node(const char *name, NodeType type, FSNode *parent);
void     free_node(FSNode *node);
void     detach_child(FSNode *parent, FSNode *child);
void     attach_child(FSNode *parent, FSNode *child);
FSNode  *find_child(FSNode *dir, const char *name);
FSNode  *deep_copy_node(FSNode *src, FSNode *new_parent);

/* ---- built-in command handlers (argc/argv style) ---- */
void     cmd_cd(int argc, char *argv[]);
void     cmd_pwd(int argc, char *argv[]);
void     cmd_ls(int argc, char *argv[]);
void     cmd_mkdir(int argc, char *argv[]);
void     cmd_rmdir(int argc, char *argv[]);
void     cmd_touch(int argc, char *argv[]);
void     cmd_rm(int argc, char *argv[]);
void     cmd_rename(int argc, char *argv[]);
void     cmd_edit(int argc, char *argv[]);
void     cmd_mv(int argc, char *argv[]);
void     cmd_cp(int argc, char *argv[]);
void     cmd_find(int argc, char *argv[]);
void     cmd_tree(int argc, char *argv[]);
void     cmd_stat(int argc, char *argv[]);
void     cmd_dirinfo(int argc, char *argv[]);

/*
 * dispatch_fs_builtin: returns 1 if argv[0] matched a file-system
 * built-in and was handled, 0 otherwise.
 */
int      dispatch_fs_builtin(int argc, char *argv[]);

#endif /* FS_H */
