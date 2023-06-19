#pragma once
#include "crt.h"

begin_c

typedef struct folders_s *folders_t;

typedef struct {
    folders_t (*open)(void);
    int (*enumerate)(folders_t folders, const char* folder);
    // name of the last enumerated folder
    const char* (*foldername)(folders_t folders);
    // number of enumerated files and sub folders inside folder
    int (*count)(folders_t folders);
    // name() of the [i]-th enumerated entry (folder or file) (not pathname!)
    const char* (*name)(folders_t folders, int i);
    bool (*is_folder)(folders_t folders, int i);
    bool (*is_symlink)(folders_t folders, int i);
    int64_t (*size)(folders_t folders, int i);
    // functions created/updated/accessed() return time in absolute nanoseconds
    // since start of OS epoch or 0 if failed or not available
    uint64_t (*created)(folders_t folders, int i);
    uint64_t (*updated)(folders_t folders, int i);
    uint64_t (*accessed)(folders_t folders, int i);
    void (*close)(folders_t folders);
} folders_if;

extern folders_if folders;

typedef struct {
    int (*write_fully)(const char* filename, const void* data, int64_t bytes);
    bool (*exists)(const char* pathname); // does not guarantee any access writes
    bool (*is_folder)(const char* pathname);
    int (*mkdirs)(const char* pathname); // tries to deep create all folders in pathname
    int (*rmdirs)(const char* pathname); // tries to remove folder and its subtree
    int (*create_temp_folder)(char* folder, int count);
    int (*remove)(const char* pathname); // delete file or empty folder
} files_if;

extern files_if files;

end_c
