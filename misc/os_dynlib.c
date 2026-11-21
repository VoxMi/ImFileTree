#include "os_dynlib.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

void* dynlib_load(const char* file_path)
{
    if(!file_path)
        return (void*)0;
#ifdef _WIN32
    return (void*)LoadLibraryA(file_path);
#else
    return dlopen(file_name, RTLD_NOW | RTLD_LOCAL);
#endif
}

void dynlib_unload(void* lib_handle)
{
    if(!lib_handle)
        return;
#ifdef _WIN32
    FreeLibrary((HMODULE)(lib_handle));
#else
    dlclose(lib_handle);
#endif
}

void* dynlib_get_symbol(void* lib_handle, const char* symbol_name)
{
    if(!lib_handle || !symbol_name)
        return NULL;
#ifdef _WIN32
    return (void*)GetProcAddress((HMODULE)lib_handle, symbol_name);
#else
    return (void*)dlsym(lib_handle, symbol_name);
#endif
}
