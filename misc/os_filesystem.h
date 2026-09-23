#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#include <Windows.h>
#include <sys/stat.h>

#if !defined(PATH_MAX)
#define PATH_MAX MAX_PATH
#endif

#define DIR_SEPARATOR   "\\"

#if !defined(_S_IFLNK)
#define _S_IFLNK 0xA000
#endif

#define DT_UNKNOWN  0
#define DT_DIR      _S_IFDIR // Directory
#define DT_REG      _S_IFREG // Regular
#define DT_LNK      _S_IFLNK // Symbolic link, junction point or mount point

#define S_ISDIR(mode)   ((mode & _S_IFMT) == _S_IFDIR)
#define S_ISREG(mode)   ((mode & _S_IFMT) == _S_IFREG)
#define S_ISLNK(mode)   ((mode & _S_IFMT) == _S_IFLNK)
#else
#define DIR_SEPARATOR   "/"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
typedef struct dirent
{
    int     d_type;
    char    d_name[PATH_MAX];
} dirent;

typedef struct DIR
{
    HANDLE              find_handle;
    WIN32_FIND_DATAW    find_data;
    dirent              current_entry;
    wchar_t             initial_path[PATH_MAX];
} DIR;
#endif

DIR* fs_opendir(const char* path_name);
dirent* fs_readdir(DIR* dir);
int fs_closedir(DIR* dir);

bool fs_path_exists(const char* path_name);

bool fs_file_exists(const char* file_name);
int fs_file_create(const char* file_name);
int fs_file_remove(const char* file_name);
int fs_file_rename(const char* old_file_name, const char* new_file_name);
int fs_file_copy(const char* src_file_name, const char* dst_file_name);
int fs_file_move(const char* src_file_name, const char* dst_file_name);

bool fs_dir_exists(const char* path_name);
int fs_dir_create(const char* path_name);
int fs_dir_remove(const char* path_name);
int fs_dir_remove_recursive(const char* path_name);
int fs_dir_rename(const char* old_path_name, const char* new_path_name);
int fs_dir_copy(const char* src_path_name, const char* dst_path_name);
int fs_dir_move(const char* src_path_name, const char* dst_path_name);

int64_t fs_get_file_mod_time_ns(const char* path_name);

int fs_get_unique_name_for_existing_path(const char* src_path, const char* dst_dir, char* out_path, size_t out_size, bool is_dir);

#ifdef __cplusplus
}
#endif
