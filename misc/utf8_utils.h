#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int str_utf8_to_utf16(const char* in_text, wchar_t* buf_out, int buf_out_size);
int str_utf16_to_utf8(const wchar_t* in_text, char* buf_out, int buf_out_size);

#ifdef __cplusplus
}
#endif
