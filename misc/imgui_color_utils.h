#pragma once

#ifndef IMGUI_DISABLE
#include <imgui/imgui.h>

#include "misc/constexpr_utils.h"
#include "misc/hash_utils.h"

// Extracting a color value from a string in "#RRGGBB" or "#RRGGBBAA" format
constexpr static ImU32 ColorConvertStringToU32(const char* str)
{
    if(!str || str[0] != '#')
        return 0;

    size_t str_len = 0;
    while(str[str_len + 1] != '\0')
        ++str_len;

    // #RRGGBB or #RRGGBBAA
    if(str_len != 6 && str_len != 8)
        return 0;

    unsigned char color_comps[4] = { 0, 0, 0, 0xFF };

    for(size_t i = 0; i < str_len / 2; ++i)
    {
        unsigned char byte_val = 0;

        for(int j = 0; j < 2; ++j)
        {
            const char c = str[1 + i * 2 + j];
            unsigned char digit = 0;

            if(c >= '0' && c <= '9')
                digit = (unsigned char)(c - '0');
            else if(c >= 'a' && c <= 'f')
                digit = 10u + (unsigned char)(c - 'a');
            else if(c >= 'A' && c <= 'F')
                digit = 10u + (unsigned char)(c - 'A');
            else
                return 0;

            byte_val = (unsigned char)((byte_val << 4) | digit);
        }

        color_comps[i] = byte_val;
    }

    ImU32 out = 0;
    out  = ((ImU32)color_comps[0]) << IM_COL32_R_SHIFT;
    out |= ((ImU32)color_comps[1]) << IM_COL32_G_SHIFT;
    out |= ((ImU32)color_comps[2]) << IM_COL32_B_SHIFT;
    out |= ((ImU32)color_comps[3]) << IM_COL32_A_SHIFT;
    return out;
}

#define CONSTEXPR_STRING_TO_COLOR(str)                  (constexpr_compile<ImU32, ColorConvertStringToU32(str)>())
#define CONSTEXPR_KEYWORD_COLOR(keyword, hex_color)     CONSTEXPR_HASH_FNV1A_32(keyword), CONSTEXPR_STRING_TO_COLOR(hex_color)
#define CONSTEXPR_KEY_GLYPH_COLOR(key, glyph, str)      CONSTEXPR_HASH_FNV1A_32(key), {glyph, CONSTEXPR_STRING_TO_COLOR(str)}
#endif // IMGUI_DISABLE
