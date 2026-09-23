#include "utf8_utils.h"

#define UNICODE_CODEPOINT_INVALID   0xFFFDu
#define UNICODE_CODEPOINT_MAX       0x10FFFFu
#define UTF16_HIGH_SURROGATE_MIN    0xD800u
#define UTF16_HIGH_SURROGATE_MAX    0xDBFFu
#define UTF16_LOW_SURROGATE_MIN     0xDC00u
#define UTF16_LOW_SURROGATE_MAX     0xDFFFu
#define UTF16_LEAD_OFFSET           0xD7C0u
#define UTF8_ACCEPT                 0
#define UTF8_REJECT                 12

int str_utf8_to_utf16(const char* in_text, wchar_t* buf_out, int buf_out_size)
{
    if(!buf_out || buf_out_size <= 0 || !in_text)
        return 0;

    wchar_t* buf_start = buf_out;
    const wchar_t* buf_end = buf_out + buf_out_size;

    unsigned char byte = 0;
    unsigned int type = 0;
    unsigned int codepoint = 0;
    unsigned int state = UTF8_ACCEPT;

    while(*in_text && buf_out < buf_end - 1)
    {
        // Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
        // See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
        const unsigned char utf8d[] =
        {
            // The first part of the table maps bytes to character classes that to reduce the size of the transition table and create bitmasks.
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
            8, 8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
            10, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 3, 3, 11, 6, 6, 6, 5, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,

            // The second part is a transition table that maps a combinationof a state of the automaton and a character class to a state.
            0, 12, 24, 36, 60, 96, 84, 12, 12, 12, 48, 72, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
            12, 0, 12, 12, 12, 12, 12,  0, 12, 0, 12, 12, 12, 24, 12, 12, 12, 12, 12, 24, 12, 24, 12, 12,
            12, 12, 12, 12, 12, 12, 12, 24, 12, 12, 12, 12, 12, 24, 12, 12, 12, 12, 12, 12, 12, 24, 12, 12,
            12, 12, 12, 12, 12, 12, 12, 36, 12, 36, 12, 12, 12, 36, 12, 12, 12, 12, 12, 36, 12, 36, 12, 12,
            12, 36, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
        };

        byte = (unsigned char)*in_text++;
        type = utf8d[byte];
        codepoint = (state != UTF8_ACCEPT) ? (byte & 0x3Fu) | (codepoint << 6u) : (0xFFu >> type) & (byte);
        state = utf8d[256 + state + type];

        if(state == UTF8_ACCEPT)
        {
            if(codepoint > UNICODE_CODEPOINT_MAX || (codepoint >= UTF16_HIGH_SURROGATE_MIN && codepoint <= UTF16_LOW_SURROGATE_MAX))
            {
                if(buf_out < buf_end)
                    *buf_out++ = (wchar_t)(UNICODE_CODEPOINT_INVALID);
            }
            else if(codepoint > 0xFFFFu)
            {
                if(buf_out + 1 >= buf_end)
                    break;

                *buf_out++ = (wchar_t)((codepoint >> 10u) + UTF16_LEAD_OFFSET);
                *buf_out++ = (wchar_t)((codepoint & 0x3FFu) | UTF16_LOW_SURROGATE_MIN);
            }
            else
            {
                if(buf_out < buf_end)
                    *buf_out++ = (wchar_t)codepoint;
            }

            state = UTF8_ACCEPT;
            codepoint = 0;
        }
        else if(state == UTF8_REJECT)
        {
            if(buf_out < buf_end)
                *buf_out++ = (wchar_t)(UNICODE_CODEPOINT_INVALID);

            state = UTF8_ACCEPT;
            codepoint = 0;
        }
    }

    *buf_out = L'\0';
    return (int)(buf_out - buf_start);
}

int str_utf16_to_utf8(const wchar_t* in_text, char* buf_out, int buf_out_size)
{
    if(!buf_out || buf_out_size <= 0 || !in_text)
        return 0;

    char* buf_start = buf_out;
    const char* buf_end = buf_out + buf_out_size;

    while(*in_text && buf_start < buf_end - 1)
    {
        unsigned int c = (unsigned int)(*in_text++);

        if(c >= UTF16_HIGH_SURROGATE_MIN && c <= UTF16_HIGH_SURROGATE_MAX)
        {
            unsigned int next_c = (unsigned int)(*in_text);
            if(next_c >= UTF16_LOW_SURROGATE_MIN && next_c <= UTF16_LOW_SURROGATE_MAX)
            {
                in_text++;
                c = 0x10000u + ((c - UTF16_HIGH_SURROGATE_MIN) << 10) + (next_c - UTF16_LOW_SURROGATE_MIN);
            }
            else
                c = UNICODE_CODEPOINT_INVALID;
        }
        else if(c >= UTF16_LOW_SURROGATE_MIN && c <= UTF16_LOW_SURROGATE_MAX)
            c = UNICODE_CODEPOINT_INVALID;

        int bytes_needed = 1;
        if(c >= 0x10000u)      bytes_needed = 4;
        else if(c >= 0x800u)   bytes_needed = 3;
        else if(c >= 0x80u)    bytes_needed = 2;

        if(buf_start + bytes_needed >= buf_end)
            break;

        if(c < 0x80u)
        {
            *buf_start++ = (char)c;
        }
        else if(c < 0x800u)
        {
            *buf_start++ = (char)(0xC0u | (c >> 6));
            *buf_start++ = (char)(0x80u | (c & 0x3Fu));
        }
        else if(c < 0x10000u)
        {
            *buf_start++ = (char)(0xE0u | (c >> 12));
            *buf_start++ = (char)(0x80u | ((c >> 6) & 0x3Fu));
            *buf_start++ = (char)(0x80u | (c & 0x3Fu));
        }
        else
        {
            *buf_start++ = (char)(0xF0u | (c >> 18));
            *buf_start++ = (char)(0x80u | ((c >> 12) & 0x3Fu));
            *buf_start++ = (char)(0x80u | ((c >> 6) & 0x3Fu));
            *buf_start++ = (char)(0x80u | (c & 0x3Fu));
        }
    }

    *buf_start = '\0';
    return (int)(buf_start - buf_out);
}
