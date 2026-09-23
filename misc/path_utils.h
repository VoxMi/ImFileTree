#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Returns a pointer to a null-terminated byte string, which is the filename with extension of pathname
const char* path_get_file_name(const char* path);

// Returns a pointer to a null-terminated byte string, which is the extension of pathname
const char* path_get_file_name_ext(const char* path, bool is_omit_dot);

// Returns a pointer to a null-terminated byte string, which is the stem (name without extension) of pathname.
// Note: The returned pointer must be passed to 'free' function to avoid a memory leak.
char* path_get_file_name_stem(const char* path);

// Returns a pointer to a null-terminated byte string, which is the name of the last component of pathname.
// Note: The returned pointer must be passed to 'free' function to avoid a memory leak.
char* path_get_base_name(const char* path);

// Returns a pointer to a null-terminated byte string, which is the name of the last component of pathname.
// Note: The returned pointer points directly into the original 'path' string and its lifetime is tied to the lifetime of the original 'path' string.
const char* path_get_base_name_no_alloc(const char* path);

// Returns a pointer to a null-terminated byte string, which is the directory part of pathname.
// Note: The returned pointer must be passed to 'free' function to avoid a memory leak.
char* path_get_dir_name(const char* path);

#ifdef __cplusplus
}
#endif
