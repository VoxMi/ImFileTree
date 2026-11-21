#pragma once

#include "constexpr_utils.h"

#include <stdint.h>

namespace Hash
{
    constexpr static uint32_t fnv1a_32(const char* key)
    {
        if(!key)
            return 0;

        uint32_t hash = 0x811C9DC5U;
        while(*key)
        {
            hash = (hash ^ *key++) * 0x1000193U;
        }

        return hash;
    }

    constexpr static uint32_t fnv1a_32(const char* key, size_t len)
    {
        if(!key || len == 0)
            return 0;

        uint32_t hash = 0x811C9DC5U;
        const char* end = key + len;
        while(key < end)
            hash = (hash ^ *key++) * 0x1000193U;

        return hash;
    }

    constexpr static uint64_t fnv1a_64(const char* key)
    {
        if(!key)
            return 0;

        uint64_t hash = 0xCBF29CE484222325ULL;
        while(*key)
            hash = (hash ^ *key++) * 0x100000001B3ULL;

        return hash;
    }

    constexpr static uint64_t fnv1a_64(const char* key, size_t len)
    {
        if(!key || len == 0)
            return 0;

        uint64_t hash = 0xCBF29CE484222325ULL;
        const char* end = key + len;
        while(key < end)
            hash = (hash ^ *key++) * 0x100000001B3ULL;

        return hash;
    }

    constexpr static uint32_t murmur_oaat_32(const char* key)
    {
        if(!key)
            return 0;

        uint32_t hash = 0xC613FC15U;
        while(*key)
        {
            hash ^= *key++;
            hash *= 0x5BD1E995U;
            hash ^= hash >> 15U;
        }

        return hash;
    }

    constexpr static uint32_t murmur_oaat_32(const char* key, int len)
    {
        if(!key || len == 0)
            return 0;

        uint32_t hash = 0xC613FC15U;

        const char* end = key + len;
        while(key < end)
        {
            hash ^= *key++;
            hash *= 0x5BD1E995U;
            hash ^= hash >> 15U;
        }

        return hash;
    }

    constexpr static uint64_t murmur_oaat_64(const char* key)
    {
        if(!key)
            return 0;

        uint64_t hash = 0x749E3E6989DF617ULL;
        while(*key)
        {
            hash ^= *key++;
            hash *= 0x5BD1E9955BD1E995ULL;
            hash ^= hash >> 47U;
        }
        return hash;
    }

    constexpr static uint64_t murmur_oaat_64(const char* key, int len)
    {
        if(!key || len == 0)
            return 0;

        uint64_t hash = 0x749E3E6989DF617ULL;
        const char* end = key + len;
        while(key < end)
        {
            hash ^= *key++;
            hash *= 0x5BD1E9955BD1E995ULL;
            hash ^= hash >> 47U;
        }
        return hash;
    }
}

// Use this macro if you need to evaluate values ​​from strings at compile time.
#define CONSTEXPR_HASH_FNV1A_32(key)                    (constexpr_compile<uint32_t, Hash::fnv1a_32(key)>())
#define CONSTEXPR_HASH_FNV1A_32_S(key, len)             (constexpr_compile<uint32_t, Hash::fnv1a_32(key, len)>())
#define CONSTEXPR_HASH_FNV1A_64(key)                    (constexpr_compile<uint64_t, Hash::fnv1a_64(key)>())
#define CONSTEXPR_HASH_FNV1A_64_S(key, len)             (constexpr_compile<uint64_t, Hash::fnv1a_64(key, len)>())

#define CONSTEXPR_HASH_MURMUR_AOAT_32(key)              (constexpr_compile<uint32_t, Hash::murmur_oaat_32(key)>())
#define CONSTEXPR_HASH_MURMUR_AOAT_32_S(key, len)       (constexpr_compile<uint32_t, Hash::murmur_oaat_32(key, len)>())
#define CONSTEXPR_HASH_MURMUR_AOAT_64(key)              (constexpr_compile<uint64_t, Hash::murmur_oaat_64(key)>())
#define CONSTEXPR_HASH_MURMUR_AOAT_64_S(key, len)       (constexpr_compile<uint64_t, Hash::murmur_oaat_64(key, len)>())
