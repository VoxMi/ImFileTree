#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void* dynlib_load(const char* file_path);
void  dynlib_unload(void* lib_handle);
void* dynlib_get_symbol(void* lib_handle, const char* symbol_name);

#ifdef __cplusplus
}
#endif
