#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "os_filesystem.h"
#include "path_utils.h"
#include "utf8_utils.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <stdio.h>

DIR* fs_opendir(const char* path_name)
{
#ifdef _WIN32
    if(path_name == NULL || path_name[0] == '\0')
        return NULL;

    // Get the absolute or full path name
    char full_path_name[MAX_PATH];
    if(!_fullpath(full_path_name, path_name, MAX_PATH))
        return NULL;

    // Remove trailing slashes
    char* full_path_name_end = full_path_name + strlen(full_path_name);
    while(full_path_name_end > full_path_name && (full_path_name_end[-1] == '\\' || full_path_name_end[-1] == '/'))
    {
        --full_path_name_end;
        *full_path_name_end = '\0';
    }

    DIR* dir = (DIR*)malloc(sizeof(DIR));
    if(!dir)
        return NULL;

    dir->current_entry.d_name[0] = '\0';

    int full_path_name_len = str_utf8_to_utf16(full_path_name, dir->initial_path, _countof(dir->initial_path));
    if(!full_path_name_len || full_path_name_len + 3 > MAX_PATH) // '3' is the length of the null-terminated wildcard ("\\*")
        goto on_error;

    DWORD attrs = GetFileAttributesW(dir->initial_path);
    if(attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
        goto on_error;

    // Append wildcard (asterisk) to search for all files and directories in the specified path
    dir->initial_path[full_path_name_len + 0] = '\\';
    dir->initial_path[full_path_name_len + 1] = '*';
    dir->initial_path[full_path_name_len + 2] = '\0';

    dir->find_handle = FindFirstFileW(dir->initial_path, &dir->find_data);
    if(dir->find_handle == INVALID_HANDLE_VALUE)
        goto on_error;

    return dir;

on_error:
    free(dir);
    return NULL;
#else
    return opendir(path_name);
#endif
}

dirent* fs_readdir(DIR* dir)
{
#ifdef _WIN32
    if(!dir || dir->find_handle == INVALID_HANDLE_VALUE)
        return NULL;

    dirent* entry = &dir->current_entry;

    // Determine the file type based on Windows file attributes.
    // Note: Explicit classification is only performed for directories (DT_DIR), regular files (DT_REG) and symlinks (DT_LNK). All other types (e.g., devices or pipes) fall back to DT_UNKNOWN.
    DWORD attrs = dir->find_data.dwFileAttributes;
    if(attrs & FILE_ATTRIBUTE_DIRECTORY)
        entry->d_type = DT_DIR;
    else if(attrs & FILE_ATTRIBUTE_REPARSE_POINT)
        entry->d_type = DT_LNK;
    else
    {
        DWORD non_reg_mask = FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_TEMPORARY;
#ifdef FILE_ATTRIBUTE_INTEGRITY_STREAM
        non_reg_mask |= FILE_ATTRIBUTE_INTEGRITY_STREAM;
#endif
#ifdef FILE_ATTRIBUTE_NO_SCRUB_DATA
        non_reg_mask |= FILE_ATTRIBUTE_NO_SCRUB_DATA;
#endif
        if(attrs & non_reg_mask)
            entry->d_type = DT_UNKNOWN;
        else
            entry->d_type = DT_REG;
    }

    str_utf16_to_utf8(dir->find_data.cFileName, entry->d_name, sizeof(entry->d_name));

    // Advance to the next file in the directory stream.
    // Note: If there are no more files, close the find handle and mark it as invalid.
    if(!FindNextFileW(dir->find_handle, &dir->find_data))
    {
        FindClose(dir->find_handle);
        dir->find_handle = INVALID_HANDLE_VALUE;
    }

    return entry;
#else
    return readdir(dir);
#endif
}

int fs_closedir(DIR* dir)
{
#ifdef _WIN32
    if(!dir)
        return -1;

    if(dir->find_handle != INVALID_HANDLE_VALUE)
    {
        FindClose(dir->find_handle);
        dir->find_handle = INVALID_HANDLE_VALUE;
    }

    free(dir);
    return 0;
#else
    return closedir(dir);
#endif
}

bool fs_path_exists(const char* path_name)
{
    if(path_name == NULL || path_name[0] == '\0')
        return false;
#ifdef _WIN32
    wchar_t path_name_w[PATH_MAX];
    str_utf8_to_utf16(path_name, path_name_w, _countof(path_name_w));

    DWORD attrs = GetFileAttributesW(path_name_w);
    return attrs != INVALID_FILE_ATTRIBUTES;
#else
    return access(path_name, F_OK) == 0;
#endif
}

bool fs_file_exists(const char* file_name)
{
    if(file_name == NULL || file_name[0] == '\0')
        return false;
#ifdef _WIN32
    wchar_t file_name_w[PATH_MAX];
    str_utf8_to_utf16(file_name, file_name_w, _countof(file_name_w));

    DWORD attrs = GetFileAttributesW(file_name_w);
    if(attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY))
        return false;
#else
    struct stat st;
    if(stat(file_name, &st) != 0 || !S_ISREG(st.st_mode))
        return false;
#endif
    return true;
}

int fs_file_create(const char* file_name)
{
    if(file_name == NULL || file_name[0] == '\0')
        return -1;
#ifdef _WIN32
    wchar_t file_name_w[PATH_MAX];
    str_utf8_to_utf16(file_name, file_name_w, _countof(file_name_w));
    FILE* file = _wfopen(file_name_w, L"ab");
#else
    FILE* file = fopen(file_name, "ab");
#endif
    if(file == NULL)
        return -1;

    fclose(file);
    return 0;
}

int fs_file_remove(const char* file_name)
{
    if(file_name == NULL || file_name[0] == '\0')
        return -1;
#ifdef _WIN32
    wchar_t file_name_w[PATH_MAX];
    str_utf8_to_utf16(file_name, file_name_w, _countof(file_name_w));

    DWORD attrs = GetFileAttributesW(file_name_w);
    if(attrs == INVALID_FILE_ATTRIBUTES)
        return -1;

    if(attrs & FILE_ATTRIBUTE_READONLY)
    {
        attrs &= ~FILE_ATTRIBUTE_READONLY;
        if(SetFileAttributesW(file_name_w, attrs) != TRUE)
            return -1;
    }

    return DeleteFileW(file_name_w) == TRUE ? 0 : -1;
#else
    return remove(file_name);
#endif
}

int fs_file_rename(const char* old_file_name, const char* new_file_name)
{
    if(old_file_name == NULL || old_file_name[0] == '\0')
        return -1;

    if(new_file_name == NULL || new_file_name[0] == '\0')
        return -1;
#ifdef _WIN32
    wchar_t file_old_name_w[PATH_MAX];
    wchar_t file_new_name_w[PATH_MAX];

    str_utf8_to_utf16(old_file_name, file_old_name_w, _countof(file_old_name_w));
    str_utf8_to_utf16(new_file_name, file_new_name_w, _countof(file_new_name_w));

    return MoveFileExW(file_old_name_w, file_new_name_w, MOVEFILE_REPLACE_EXISTING) == TRUE ? 0 : -1;
#else
    return rename(old_file_name, new_file_name);
#endif
}

int fs_file_copy(const char* src_file_name, const char* dst_file_name)
{
    if(src_file_name == NULL || src_file_name[0] == '\0')
        return -1;

    if(dst_file_name == NULL || dst_file_name[0] == '\0')
        return -1;

#ifdef _WIN32
    wchar_t file_src_name_w[PATH_MAX];
    wchar_t file_dst_name_w[PATH_MAX];
    str_utf8_to_utf16(src_file_name, file_src_name_w, _countof(file_src_name_w));
    str_utf8_to_utf16(dst_file_name, file_dst_name_w, _countof(file_dst_name_w));
    return CopyFileW(file_src_name_w, file_dst_name_w, FALSE) == TRUE ? 0 : -1;
#else
#define READ_BUF_SIZE 8192

    FILE* file_src = fopen(src_file_name, "rb");
    if(file_src == NULL)
        return -1;

    FILE* file_dst = fopen(dst_file_name, "wb");
    if(file_dst == NULL)
    {
        fclose(file_src);
        return -1;
    }

    size_t read_bytes;
    void* read_buf = malloc(READ_BUF_SIZE);
    if(read_buf)
    {
        while((read_bytes = fread(read_buf, 1, READ_BUF_SIZE, file_src)) > 0)
        {
            if(fwrite(read_buf, 1, read_bytes, file_dst) != read_bytes)
            {
                fclose(file_src);
                fclose(file_dst);
                return -1;
            }
        }
    }

    fclose(file_src);
    fclose(file_dst);

    if(read_buf)
        free(read_buf);

    return 0;
#endif
}

int fs_file_move(const char* src_file_name, const char* dst_file_name)
{
    if(fs_file_copy(src_file_name, dst_file_name) != 0)
        return -1;

   return fs_file_remove(src_file_name);
}

bool fs_dir_exists(const char* path_name)
{
    if(path_name == NULL || path_name[0] == '\0')
        return 0;
#ifdef _WIN32
    wchar_t path_name_w[PATH_MAX];
    str_utf8_to_utf16(path_name, path_name_w, _countof(path_name_w));

    DWORD attrs = GetFileAttributesW(path_name_w);
    if(attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
        return false;
#else
    struct stat st;
    if(stat(path_name, &st) != 0 || !S_ISDIR(st.st_mode))
        return false;
#endif
    return true;
}

int fs_dir_create(const char* path_name)
{
    if(path_name == NULL || path_name[0] == '\0')
        return -1;
#ifdef _WIN32
    wchar_t path_name_w[PATH_MAX];
    str_utf8_to_utf16(path_name, path_name_w, _countof(path_name_w));

    if(CreateDirectoryW(path_name_w, NULL) != TRUE)
        return -1;

    return 0;
#else
    return mkdir(path_name, 0755);
#endif
}

int fs_dir_remove(const char* path_name)
{
    if(path_name == NULL || path_name[0] == '\0')
        return -1;
#ifdef _WIN32
    wchar_t path_name_w[PATH_MAX];
    str_utf8_to_utf16(path_name, path_name_w, _countof(path_name_w));

    if(RemoveDirectoryW(path_name_w) != TRUE)
        return -1;

    return 0;
#else
    if(rmdir(path_name) != 0)
        return -1;

    return 0;
#endif
}

int fs_dir_remove_recursive(const char* path_name)
{
    if(path_name == NULL || path_name[0] == '\0')
        return -1;
#ifdef _WIN32
    wchar_t path_name_w[PATH_MAX];
    str_utf8_to_utf16(path_name, path_name_w, _countof(path_name_w));

    DWORD attrs = GetFileAttributesW(path_name_w);
    if(attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
        return -1;
#else
    struct stat st;
    if(stat(path_name, &st) != 0 || !S_ISDIR(st.st_mode))
        return -1;
#endif
    DIR* dir = fs_opendir(path_name);
    if(!dir)
        return -1;

    char sub_path[PATH_MAX];

    dirent* ent;
    while((ent = fs_readdir(dir)) != NULL)
    {
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        int len = snprintf(sub_path, sizeof(sub_path), "%s" DIR_SEPARATOR "%s", path_name, ent->d_name);
        if(len < 0 || len >= (int)sizeof(sub_path))
        {
            fs_closedir(dir);
            return -1;
        }
#ifdef _WIN32
        str_utf8_to_utf16(sub_path, path_name_w, _countof(path_name_w));

        attrs = GetFileAttributesW(path_name_w);
        if(attrs == INVALID_FILE_ATTRIBUTES)
        {
            fs_closedir(dir);
            return -1;
        }

        int is_dir = attrs & FILE_ATTRIBUTE_DIRECTORY;
#else
        struct stat st;
        if(stat(sub_path, &st) != 0)
        {
            fs_closedir(dir);
            return -1;
        }

        int is_dir = S_ISDIR(st.st_mode);
#endif
        if(is_dir)
        {
            if(fs_dir_remove_recursive(sub_path) != 0)
            {
                fs_closedir(dir);
                return -1;
            }
        }
        else
        {
            if(fs_file_remove(sub_path) != 0)
            {
                fs_closedir(dir);
                return -1;
            }
        }
    }

    fs_closedir(dir);

    if(fs_dir_remove(path_name) != 0)
        return -1;

    return 0;
}

int fs_dir_rename(const char* old_path_name, const char* new_path_name)
{
    return fs_file_rename(old_path_name, new_path_name);
}

int fs_dir_copy(const char* src_path_name, const char* dst_path_name)
{
    if(src_path_name == NULL || src_path_name[0] == '\0')
        return -1;

    if(dst_path_name == NULL || dst_path_name[0] == '\0')
        return -1;

#ifdef _WIN32
    wchar_t src_path_name_w[PATH_MAX];
    str_utf8_to_utf16(src_path_name, src_path_name_w, _countof(src_path_name_w));

    DWORD attrs = GetFileAttributesW(src_path_name_w);
    if(attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
        return -1;
#else
    struct stat st;
    if(stat(src_path_name, &st) != 0 || !S_ISDIR(st.st_mode))
        return -1;
#endif

    if(fs_dir_create(dst_path_name) != 0)
        return -1;

    DIR* dir = fs_opendir(src_path_name);
    if(!dir)
        return -1;

    char src_sub_path_name[PATH_MAX];
    char dst_sub_path_name[PATH_MAX];

    dirent* ent;
    while((ent = fs_readdir(dir)) != NULL)
    {
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        int src_len = snprintf(src_sub_path_name, sizeof(src_sub_path_name), "%s" DIR_SEPARATOR "%s", src_path_name, ent->d_name);
        int dst_len = snprintf(dst_sub_path_name, sizeof(dst_sub_path_name), "%s" DIR_SEPARATOR "%s", dst_path_name, ent->d_name);

        if(src_len < 0 || src_len >= (int)sizeof(src_sub_path_name) || dst_len < 0 || dst_len >= (int)sizeof(dst_sub_path_name))
        {
            fs_closedir(dir);
            return -1;
}
#ifdef _WIN32
        wchar_t src_sub_path_name_w[PATH_MAX];
        str_utf8_to_utf16(src_sub_path_name, src_sub_path_name_w, _countof(src_sub_path_name_w));

        DWORD attrs = GetFileAttributesW(src_sub_path_name_w);
        if(attrs == INVALID_FILE_ATTRIBUTES)
        {
            fs_closedir(dir);
            return -1;
        }

        int is_dir = attrs & FILE_ATTRIBUTE_DIRECTORY;
#else
        struct stat st;
        if(stat(src_sub_path_name, &st) != 0)
        {
            fs_closedir(dir);
            return -1;
        }

        int is_dir = S_ISDIR(st.st_mode);
#endif
        if(is_dir)
        {
            if(fs_dir_copy(src_sub_path_name, dst_sub_path_name) != 0)
            {
                fs_closedir(dir);
                return -1;
            }
        }
        else
        {
            if(fs_file_copy(src_sub_path_name, dst_sub_path_name) != 0)
            {
                fs_closedir(dir);
                return -1;
            }
        }
    }

    fs_closedir(dir);
    return 0;
}

int fs_dir_move(const char* src_path_name, const char* dst_path_name)
{
    if(fs_dir_copy(src_path_name, dst_path_name) != 0)
        return -1;

    return fs_dir_remove_recursive(src_path_name);
}

int64_t fs_get_file_mod_time_ns(const char* path_name)
{
    if(path_name == NULL || path_name[0] == '\0')
        return -1;
#if defined(_WIN32)
    wchar_t path_name_w[PATH_MAX];
    str_utf8_to_utf16(path_name, path_name_w, _countof(path_name_w));

    WIN32_FILE_ATTRIBUTE_DATA attr;
    if(!GetFileAttributesExW(path_name_w, GetFileExInfoStandard, &attr))
        return -1;

    ULARGE_INTEGER mod_time;
    mod_time.LowPart = attr.ftLastWriteTime.dwLowDateTime;
    mod_time.HighPart = attr.ftLastWriteTime.dwHighDateTime;

    // Re-bias to 1/1/1970
    mod_time.QuadPart -= 116444736000000000ULL;

    // Converting to Unix nanoseconds
    uint64_t ns_since_epoch = mod_time.QuadPart * 100ULL;
    return (int64_t)ns_since_epoch;

#else
#if defined(__linux__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 28))
    struct statx stx;
    if(statx(AT_FDCWD, path_name, AT_STATX_SYNC_AS_STAT, STATX_MTIME, &stx) != 0)
        return -1;

    // Converting timespec to nanoseconds
    return (int64_t)(stx.stx_mtime.tv_sec) * 1000000000LL + (int64_t)(stx.stx_mtime.tv_nsec);
#else
    struct stat st;
    if(stat(path_name, &st) != 0)
        return -1;
#if defined(__APPLE__) || defined(__FreeBSD__)
    return (int64_t)(st.st_mtimespec.tv_sec) * 1000000000LL + (int64_t)(st.st_mtimespec.tv_nsec);
#else
    return (int64_t)(st.st_mtim.tv_sec) * 1000000000LL + (int64_t)(st.st_mtim.tv_nsec);
#endif
#endif
#endif
}

int fs_get_unique_name_for_existing_path(const char* src_path, const char* dst_dir, char* out_path, size_t out_size, bool is_dir)
{
    const char* src_name = path_get_base_name_no_alloc(src_path);
    if(!src_name || !dst_dir)
    {
        out_path[0] = '\0';
        return -1;
    }

    char src_name_base[MAX_PATH]; // Base filename (no extension)
    char src_name_ext[MAX_PATH];  // Filename extension

    src_name_base[0] = '\0';
    src_name_ext[0] = '\0';

    if(is_dir)
        strncpy_s(src_name_base, sizeof(src_name_base), src_name, _TRUNCATE);
    else
    {
        const char* dot = strrchr(src_name, '.');
        if(dot && dot != src_name)
        {
            size_t name_len = (size_t)(dot - src_name);
            if(name_len >= MAX_PATH)
                name_len = MAX_PATH - 1;

            memcpy(src_name_base, src_name, name_len);
            src_name_base[name_len] = '\0';
            strncpy_s(src_name_ext, sizeof(src_name_ext), dot, _TRUNCATE);
        }
        else
            strncpy_s(src_name_base, sizeof(src_name_base), src_name, _TRUNCATE);
    }

    // Try "<name>"
    int len = snprintf(out_path, out_size, "%s" DIR_SEPARATOR "%s%s", dst_dir, src_name_base, src_name_ext);
    if(len < 0 || len >= (int)out_size)
        return -1;
    if(!fs_path_exists(out_path))
        return -1;

    // Try "<name> copy"
    len = snprintf(out_path, out_size, "%s" DIR_SEPARATOR "%s copy%s", dst_dir, src_name_base, src_name_ext);
    if(len < 0 || len >= (int)out_size)
        return -1;
    if(!fs_path_exists(out_path))
        return -1;

    int next_copy_num = 0;
    bool has_base_copy = false;

    DIR* dir = fs_opendir(dst_dir);
    if(dir)
    {
        size_t src_name_base_len = strlen(src_name_base);
        size_t src_name_ext_len = strlen(src_name_ext);

        struct dirent* entry;
        while((entry = fs_readdir(dir)) != NULL)
        {
            const char* entry_name = entry->d_name;
            size_t entry_name_len = strlen(entry_name);

            // Search for a file name by pattern: "<name> copy.<extension>"
            if(entry_name_len == src_name_base_len + 5 + src_name_ext_len)
            {
                if(strncmp(entry_name, src_name_base, src_name_base_len) == 0 && strncmp(entry_name + src_name_base_len, " copy", 5) == 0)
                {
                    if(strcmp(entry_name + entry_name_len - src_name_ext_len, src_name_ext) == 0)
                        has_base_copy = true;
                }
            }

            // Search for a file name by pattern: "<name> copy <N>.<extension>"
            if(entry_name_len > src_name_base_len + 6 + src_name_ext_len)
            {
                if(strncmp(entry_name, src_name_base, src_name_base_len) == 0 && strncmp(entry_name + src_name_base_len, " copy ", 6) == 0)
                {
                    if(strcmp(entry_name + entry_name_len - src_name_ext_len, src_name_ext) == 0)
                    {
                        size_t num_len = entry_name_len - src_name_base_len - 6 - src_name_ext_len;
                        if(num_len < 16)
                        {
                            char num_str[16];
                            memcpy(num_str, entry_name + src_name_base_len + 6, num_len);
                            num_str[num_len] = '\0';

                            bool is_num = true;
                            for(size_t i = 0; i < num_len; ++i)
                            {
                                if(!isdigit((unsigned char)num_str[i]))
                                {
                                    is_num = false;
                                    break;
                                }
                            }

                            if(is_num)
                            {
                                int n = atoi(num_str);
                                if(n > next_copy_num)
                                    next_copy_num = n;
                            }
                        }
                    }
                }
            }
        }

        fs_closedir(dir);
    }

    if(!has_base_copy)
        len = snprintf(out_path, out_size, "%s" DIR_SEPARATOR "%s copy%s", dst_dir, src_name_base, src_name_ext);
    else
        len = snprintf(out_path, out_size, "%s" DIR_SEPARATOR "%s copy %d%s", dst_dir, src_name_base, next_copy_num + 1, src_name_ext);

    if(len < 0 || len >= (int)out_size)
        return -1;

    return 0;
}
