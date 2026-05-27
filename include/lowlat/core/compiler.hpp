// Compiler attribute helpers for hot-path code.
//
// LOWLAT_LIKELY / LOWLAT_UNLIKELY:
//   Branch prediction hints. Use sparingly — only when you know the
//   true distribution and the compiler can't infer it.
//
// LOWLAT_ALWAYS_INLINE / LOWLAT_NEVER_INLINE:
//   Force or forbid inlining. Useful for keeping cold code (asserts,
//   error paths) out of the hot path's instruction cache.

#pragma once

#if defined(__GNUC__) || defined(__clang__)
    #define LOWLAT_LIKELY(x)       __builtin_expect(!!(x), 1)
    #define LOWLAT_UNLIKELY(x)     __builtin_expect(!!(x), 0)
    #define LOWLAT_ALWAYS_INLINE   inline __attribute__((always_inline))
    #define LOWLAT_NEVER_INLINE    __attribute__((noinline))
    #define LOWLAT_HOT             __attribute__((hot))
    #define LOWLAT_COLD            __attribute__((cold))
#else
    #define LOWLAT_LIKELY(x)       (x)
    #define LOWLAT_UNLIKELY(x)     (x)
    #define LOWLAT_ALWAYS_INLINE   inline
    #define LOWLAT_NEVER_INLINE
    #define LOWLAT_HOT
    #define LOWLAT_COLD
#endif
