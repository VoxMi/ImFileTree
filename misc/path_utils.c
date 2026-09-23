#include "path_utils.h"

#include <stdlib.h>
#include <string.h>

#define IS_UPPER_CASE(n)    ((n) >= 'A' && (n) <= 'Z')
#define IS_LOWER_CASE(n)    ((n) >= 'a' && (n) <= 'z')
#define IS_ALPHABETIC(n)    (IS_UPPER_CASE(n) || IS_LOWER_CASE(n))
#define IS_DIR_SEPARATOR(c) ((c) == '/' || (c) == '\\')

const char* path_get_file_name(const char* path)
{
    if(!path || path[0] == '\0')
        return NULL;

    if(IS_ALPHABETIC(path[0]) && path[1] == ':')
        path += 2;

    const char* p = path;
    while(*path++)
    {
        if(IS_DIR_SEPARATOR(*path))
            p = path + 1;
    }

    return p;
}

const char* path_get_file_name_ext(const char* path, bool is_omit_dot)
{
    const char* p = path_get_file_name(path);
    if(!p)
        return NULL;

    const char* p1 = NULL;
    while(*p++)
    {
        if(*p == '.')
            p1 = p;
    }

    if(!p1 || *(p1 + 1) == '\0')
        return NULL;

    return is_omit_dot ? p1 + 1 : p1;
}

char* path_get_file_name_stem(const char* path)
{
    if(!path || path[0] == '\0')
        return NULL;

    char* result = NULL;

    size_t len = strlen(path);
    if(len == 0)
    {
        result = (char*)malloc(1);
        result[0] = '\0';
        return result;
    }

    size_t end = len;
    while(end > 0 && IS_DIR_SEPARATOR(path[end - 1]))
        end--;

    if(end == 0)
    {
        result = (char*)malloc(2);
        result[0] = '/';
        result[1] = '\0';
        return result;
    }

    size_t start = end;
    while(start > 0 && !IS_DIR_SEPARATOR(path[start - 1]))
        start--;

    size_t last_dot = end;
    for(size_t i = start; i < end; i++)
    {
        if(path[i] == '.')
        {
            if(i == start)
                continue;

            last_dot = i;
        }
    }

    size_t new_len = last_dot - start;
    result = (char*)malloc(new_len + 1);
    memcpy(result, path + start, new_len);
    result[new_len] = '\0';
    return result;
}

char* path_get_base_name(const char* path)
{
    if(!path || path[0] == '\0')
        return NULL;

    char* result = NULL;

    size_t len = strlen(path);
    if(len == 0)
    {
        result = (char*)malloc(1);
        result[0] = '\0';
        return result;
    }

    size_t end = len;
    while(end > 0 && IS_DIR_SEPARATOR(path[end - 1]))
        end--;

    if(end == 0)
    {
        result = (char*)malloc(2);
        result[0] = '/';
        result[1] = '\0';
        return result;
    }

    size_t start = end;
    while(start > 0 && !IS_DIR_SEPARATOR(path[start - 1]))
        start--;

    size_t new_len = end - start;
    result = (char*)malloc(new_len + 1);
    memcpy(result, path + start, new_len);
    result[new_len] = '\0';
    return result;
}

const char* path_get_base_name_no_alloc(const char* path)
{
    if(!path || path[0] == '\0')
        return NULL;

    size_t len = strlen(path);
    if(len == 0)
        return path;

    size_t end = len;
    while(end > 0 && IS_DIR_SEPARATOR(path[end - 1]))
        end--;

    if(end == 0)
        return path;

    size_t start = end;
    while(start > 0 && !IS_DIR_SEPARATOR(path[start - 1]))
        start--;

    return path + start;
}

char* path_get_dir_name(const char* path)
{
    if(!path || path[0] == '\0')
        return NULL;

    size_t len = strlen(path);
    if(len == 0)
        return NULL;

    size_t prefix_len = 0;
    if(IS_ALPHABETIC(path[0]) && path[1] == ':')
    {
        prefix_len = 2;

        if(len > 2 && IS_DIR_SEPARATOR(path[2]))
            prefix_len = 3;
    }
    else if(IS_DIR_SEPARATOR(path[0]))
        prefix_len = 1;

    size_t pos = len;
    while(pos > prefix_len && !IS_DIR_SEPARATOR(path[pos - 1]))
        --pos;

    size_t new_len = pos;
    if(new_len == 0)
        return NULL;

    if(new_len < prefix_len)
        new_len = prefix_len;

    while(new_len > prefix_len && IS_DIR_SEPARATOR(path[new_len - 1]))
        --new_len;

    char* result = (char*)malloc(new_len + 1);
    if(result)
    memcpy(result, path, new_len);
    result[new_len] = '\0';
    return result;
}
