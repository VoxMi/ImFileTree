#pragma once

#if defined(_MSVC_LANG)
#if _MSVC_LANG < 201402L
#error "This code requires C++14 or later. Please use /std:c++14 or higher."
#endif
#elif defined(__cplusplus)
#if __cplusplus < 201402L
#error "This code requires C++14 or later. Please use -std=c++14 or higher."
#endif
#endif

#ifdef _MSC_VER
#define FORCE_INLINE __forceinline
#elif (defined(__clang__) || defined(__GNUC__))
#define FORCE_INLINE inline __attribute__((__always_inline__))
#else
#define FORCE_INLINE inline
#endif

/*
 * constexpr_compile: A template wrappers to enforce compile-time evaluation.
 *
 * According to the C++ standard, a `constexpr` function is only evaluated at compile-time
 * if invoked within a context that strictly requires a constant expression. Passing it
 * directly to a runtime function (like `printf`) allows the compiler to defer evaluation.
 *
 * By utilizing a non-type template parameter (`template <uint32_t param>`), we artificially
 * create a mandatory compile-time context. The C++ standard dictates that non-type template
 * arguments must be core constant expressions, forcing the compiler to resolve the hash
 * during compilation, effectively bypassing the need for C++20's `consteval`.
 */
template <typename T, T V> constexpr static FORCE_INLINE T constexpr_compile() { return V; }
