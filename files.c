#include "files.h"

#include <Windows.h>
#include "Shlwapi.h"
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>

begin_c

static int files_write_fully(const char* filename, const void* data, int64_t bytes) {
    int r = 0;
    int fd = open(filename, O_CREAT | O_WRONLY | O_BINARY, _S_IREAD | _S_IWRITE);
    if (fd < 0) { r = errno; }
    if (r == 0) {
        int64_t transferred = 0;
        assert(0 <= bytes && bytes <= UINT32_MAX);
        transferred = write(fd, data, (uint32_t)bytes);
        assert(transferred == bytes);
        r = close(fd);
    }
    return r;
}

static int files_remove_file_or_folder(const char* pathname) {
    if (files.is_folder(pathname)) {
        return RemoveDirectoryA(pathname) == 0 ? GetLastError() : 0;
    } else {
        return DeleteFileA(pathname) == 0 ? GetLastError() : 0;
    }
}

static int files_create_temp_folder(char* folder, int count) {
    assert(folder != null && count > 0);
    char temp_path[1024] = { 0 };
    // If GetTempPathA() succeeds, the return value is the length,
    // in chars, of the string copied to lpBuffer, not including
    // the terminating null character.
    int r = GetTempPathA(countof(temp_path), temp_path) == 0 ? GetLastError() : 0;
    if (r != 0) { traceln("GetTempPathA() failed %s", crt.error(r)); }
    if (count < (int)strlen(temp_path) + 8) {
        r = ERROR_BUFFER_OVERFLOW;
    }
    if (r == 0) {
        assert(count > (int)strlen(temp_path) + 8);
        // If GetTempFileNameA() succeeds, the return value is the length,
        // in chars, of the string copied to lpBuffer, not including the
        // terminating null character.
        if (count > (int)strlen(temp_path) + 8) {
            char prefix[4] = {0};
            r = GetTempFileNameA(temp_path, prefix, 0, folder) == 0 ?
                GetLastError() : 0;
            if (r != 0) { traceln("GetTempFileNameA() failed %s", crt.error(r)); }
        } else {
            r = ERROR_BUFFER_OVERFLOW;
        }
    }
    if (r == 0) {
        r = DeleteFileA(folder) == 0 ? GetLastError() : 0;
        if (r != 0) { traceln("DeleteFileA(%s) failed %s", folder, crt.error(r)); }
    }
    if (r == 0) {
        r = files.mkdirs(folder);
        if (r != 0) { traceln("mkdirs(%s) failed %s", folder, crt.error(r)); }
    }
    return r;
}

static int files_create_folder(const char* dir) {
    int r = 0;
    const int n = (int)strlen(dir) + 1;
    char* s = (char*)alloca(n);
    memset(s, 0, n);
    const char* next = strchr(dir, '\\');
    if (next == null) { next = strchr(dir, '/'); }
    while (next != null) {
        if (next > dir && *(next - 1) != ':') {
            memcpy(s, dir, next - dir);
            r = CreateDirectoryA(s, null) ? 0 : GetLastError();
            if (r != 0 && r != ERROR_ALREADY_EXISTS) { break; }
        }
        const char* prev = ++next;
        next = strchr(prev, '\\');
        if (next == null) { next = strchr(prev, '/'); }
    }
    if (r == 0 || r == ERROR_ALREADY_EXISTS) {
        r = CreateDirectoryA(dir, null) ? 0 : GetLastError();
    }
    return r == ERROR_ALREADY_EXISTS ? 0 : r;
}

static int files_remove_folder(const char* folder) {
    folders_t dirs = folders.open();
    int r = dirs == null ? -1 : folders.enumerate(dirs, folder);
    if (r == 0) {
        const int n = folders.count(dirs);
        for (int i = 0; i < n; i++) { // recurse into sub folders and remove them first
            // do NOT follow symlinks - it could be disastorous
            if (!folders.is_symlink(dirs, i) && folders.is_folder(dirs, i)) {
                const char* name = folders.name(dirs, i);
                int pathname_length = (int)(strlen(folder) + strlen(name) + 3);
                char* pathname = (char*)malloc(pathname_length);
                if (pathname == null) { r = -1; break; }
                crt.sformat(pathname, pathname_length, "%s/%s", folder, name);
                r = files.rmdirs(pathname);
                free(pathname);
                if (r != 0) { break; }
            }
        }
        for (int i = 0; i < n; i++) {
            if (!folders.is_folder(dirs, i)) { // symlinks are removed as normal files
                const char* name = folders.name(dirs, i);
                int pathname_length = (int)(strlen(folder) + strlen(name) + 3);
                char* pathname = (char*)malloc(pathname_length);
                if (pathname == null) { r = -1; break; }
                crt.sformat(pathname, pathname_length, "%s/%s", folder, name);
                r = remove(pathname) == -1 ? errno : 0;
                if (r != 0) {
                    traceln("remove(%s) failed %s", pathname, crt.error(r));
                }
                free(pathname);
                if (r != 0) { break; }
            }
        }
    }
    if (dirs != null) { folders.close(dirs); }
    if (r == 0) { r = RemoveDirectoryA(folder) == -1 ? crt.err() : 0; }
    return r;
}

static bool files_exists(const char* path) { return PathFileExistsA(path); }

static bool files_is_folder(const char* path) { return PathIsDirectoryA(path); }

files_if files = {
    .write_fully = files_write_fully,
    .exists = files_exists,
    .is_folder = files_is_folder,
    .mkdirs = files_create_folder,
    .rmdirs = files_remove_folder,
    .create_temp_folder = files_create_temp_folder,
    .remove = files_remove_file_or_folder
};

// folders enumarator

typedef struct folders_data_s {
    WIN32_FIND_DATAA ffd;
} folders_data_t;

typedef struct folders_s {
    int n;
    int allocated;
    int fd;
    char* folder;
    folders_data_t* data;
} folders_t_;

static folders_t folders_open() {
    folders_t_* d = (folders_t_*)malloc(sizeof(folders_t_));
    if (d != null) { memset(d, 0, sizeof(*d)); }
    return d;
}

void folders_close(folders_t dirs) {
    folders_t_* d = (folders_t_*)dirs;
    if (d != null) {
        free(d->data);  d->data = null;
        free(d->folder); d->folder = null;
    }
    free(d);
}

const char* folders_foldername(folders_t dirs) {
    folders_t_* d = (folders_t_*)dirs;
    return d->folder;
}

int folders_count(folders_t dirs) {
    folders_t_* d = (folders_t_*)dirs;
    return d->n;
}

#define return_time_field(field) \
    folders_t_* d = (folders_t_*)dirs; \
    assert(0 <= i && i < d->n, "assertion %d out of range [0..%d[", i, d->n); \
    return 0 <= i && i < d->n ? \
        (((uint64_t)d->data[i].ffd.field.dwHighDateTime) << 32 | \
                    d->data[i].ffd.field.dwLowDateTime) * 100 : 0

#define return_bool_field(field, bit) \
    folders_t_* d = (folders_t_*)dirs; \
    assert(0 <= i && i < d->n, "assertion %d out of range [0..%d[", i, d->n); \
    return 0 <= i && i < d->n ? (d->data[i].ffd.field & bit) != 0 : false

#define return_int64_file_size() \
    folders_t_* d = (folders_t_*)dirs; \
    assert(0 <= i && i < d->n, "assertion %d out of range [0..%d[", i, d->n); \
    return 0 <= i && i < d->n ? \
        (int64_t)(((uint64_t)d->data[i].ffd.nFileSizeHigh) << 32 | \
        d->data[i].ffd.nFileSizeLow) : -1

const char* folders_filename(folders_t dirs, int i) {
    folders_t_* d = (folders_t_*)dirs;
    assert(0 <= i && i < d->n, "assertion %d out of range [0..%d[", i, d->n);
    return 0 <= i && i < d->n ? d->data[i].ffd.cFileName : null;
}

bool folders_is_folder(folders_t dirs, int i) {
    return_bool_field(dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY);
}

bool folders_is_symlink(folders_t dirs, int i) {
    return_bool_field(dwFileAttributes, FILE_ATTRIBUTE_REPARSE_POINT);
}

int64_t folders_file_size(folders_t dirs, int i) {
    return_int64_file_size();
}

// functions folders_time_*() return time in absolute nanoseconds since start of OS epoch or 0 if failed or not available

uint64_t folders_time_created(folders_t dirs, int i) {
    return_time_field(ftCreationTime);
}

uint64_t folders_time_updated(folders_t dirs, int i) {
    return_time_field(ftLastWriteTime);
}

uint64_t folders_time_accessed(folders_t dirs, int i) {
    return_time_field(ftLastAccessTime);
}

int folders_enumerate(folders_t dirs, const char* folder) {
    folders_t_* d = (folders_t_*)dirs;
    WIN32_FIND_DATAA ffd = {0};
    int folder_length = (int)strlen(folder);
    if (folder_length > 0 && (folder[folder_length - 1] == '/' || folder[folder_length - 1] == '\\')) {
        assert(folder[folder_length - 1] != '/' && folder[folder_length - 1] != '\\',
            "folder name should not contain trailing [back] slash: %s", folder);
        folder_length--;
    }
    if (folder_length == 0) { return -1; }
    int pattern_length = folder_length + 3;
    char* pattern = (char*)stackalloc(pattern_length);
    crt.sformat(pattern, pattern_length, "%-*.*s/*", folder_length, folder_length, folder);
    if (d->folder != null) { free(d->folder); d->folder = null; }
    d->folder = (char*)malloc(folder_length + 1);
    if (d->folder == null) { return -1; }
    crt.sformat(d->folder, folder_length + 1, "%s", folder);
    assert(strequ(d->folder, folder));
    if (d->allocated == 0 && d->n == 0 && d->data == null) {
        d->allocated = 128;
        d->n = 0;
        d->data = (folders_data_t*)malloc(sizeof(folders_data_t) * d->allocated);
        if (d->data == null) {
            free(d->data);
            d->allocated = 0;
            d->data = null;
        }
    }
    assert(d->allocated > 0 && d->n <= d->allocated && d->data != null,
        "inconsitent values of n=%d allocated=%d", d->n, d->allocated);
    d->n = 0;
    if (d->allocated > 0 && d->n <= d->allocated && d->data != null) {
        int pathname_length = (int)(strlen(folder) + countof(ffd.cFileName) + 3);
        char* pathname = (char*)stackalloc(pathname_length);
        HANDLE h = FindFirstFileA(pattern, &ffd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (strequ(".", ffd.cFileName) || strequ("..", ffd.cFileName)) { continue; }
                if (d->n >= d->allocated) {
                    folders_data_t* r = (folders_data_t*)realloc(d->data,
                        sizeof(folders_data_t) * d->allocated * 2);
                    if (r != null) {
                        // out of memory - do the best we can, leave the rest for next pass
                        d->allocated = d->allocated * 2;
                        d->data = r;
                    }
                }
                if (d->n < d->allocated && d->data != null) {
                    crt.sformat(pathname, pathname_length, "%s/%s", folder, ffd.cFileName);
 //                 traceln("%s", pathname);
                    d->data[d->n].ffd = ffd;
                    d->n++;
                } else {
                    return -1; // keep the data we have so far intact
                }
            } while (FindNextFileA(h, &ffd));
            FindClose(h);
        }
        return 0;
    }
    return -1;
}

folders_if folders = {
    .open = folders_open,
    .enumerate = folders_enumerate,
    .foldername = folders_foldername,
    .count = folders_count,
    .name = folders_filename,
    .is_folder = folders_is_folder,
    .is_symlink = folders_is_symlink,
    .size = folders_file_size,
    .created = folders_time_created,
    .updated = folders_time_updated,
    .accessed = folders_time_accessed,
    .close = folders_close
};

end_c
