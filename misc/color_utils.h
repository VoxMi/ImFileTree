#pragma once

#include "misc/constexpr_utils.h"

// Extracting the value (unsigned int) from a string with the format "#RRGGBB" or "#RRGGBBAA"
constexpr static unsigned int rgba_from_hex_string(const char* str)
{
    if(!str || str[0] != '#')
        return 0;

    size_t str_len = 0;
    while(str[str_len + 1] != '\0')
        ++str_len;

    // #RRGGBB or #RRGGBBAA
    if(str_len != 6 && str_len != 8)
        return 0;

    unsigned int color_comps[4] = { 0, 0, 0, 0xFF };

    for(size_t i = 0; i < str_len / 2; ++i)
    {
        unsigned int byte_val = 0;

        for(int j = 0; j < 2; ++j)
        {
            const char c = str[1 + i * 2 + j];
            unsigned int digit = 0;

            if(c >= '0' && c <= '9')
                digit = (unsigned int)(c - '0');
            else if(c >= 'a' && c <= 'f')
                digit = 10u + (unsigned int)(c - 'a');
            else if(c >= 'A' && c <= 'F')
                digit = 10u + (unsigned int)(c - 'A');
            else
                return 0;

            byte_val = (byte_val << 4) | digit;
        }

        color_comps[i] = byte_val;
    }

    // Pack the final value
    return ((color_comps[0] << 0) | (color_comps[1] << 8) | (color_comps[2] << 16) | ((color_comps[3] << 24)));
}

#define CONSTEXPR_STR_COLOR_EXTRACT_HEX(str) (constexpr_compile<unsigned int, rgba_from_hex_string(str)>())
