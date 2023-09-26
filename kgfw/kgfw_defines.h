#ifndef KRISVERS_KGFW_DEFINES_H
#define KRISVERS_KGFW_DEFINES_H

/* platform/os macros */
#if defined(_WIN32) || defined(__WIN32__) || defined(_MSC_VER)
#define KGFW_WINDOWS 1
#define KGFW_WIN32 1
#endif

#if defined(_WIN64)
#ifndef KGFW_WINDOWS
#define KGFW_WINDOWS 1
#endif
#define KGFW_WIN64 1
#endif

#if defined(__linux__)
#define KGFW_LINUX 1
#endif

#if defined(__unix__)
#define KGFW_UNIX 1
#endif

#if defined(__posix__)
#define KGFW_POSIX 1
#endif

#if defined(__APPLE__)
#define KGFW_APPLE 1
#include <TargetConditionals.h>

#if defined(TARGET_OS_IPHONE)
#define KGFW_APPLE_IPHONE
#endif

#if defined(TARGET_OS_MAC)
#define KGFW_APPLE_MACOS
#endif

#if defined(TARGET_OS_UNIX)
#define KGFW_APPLE_UNIX
#endif

#if defined(TARGET_OS_EMBEDDED)
#define KGFW_APPLE_EMBEDDED
#endif
#endif

/* compiler macros */
#if defined(_MSC_VER)
#define KGFW_MSVC 1
#endif

#if defined(__GNUC__)
#define KGFW_GCC
#endif

#if defined(__clang__)
#define KGFW_CLANG
#endif

#if defined(__MINGW32__)
#define KGFW_MINGW 1
#define KGFW_MINGW32 1
#endif

#if defined(__MINGW64__)
#ifndef KGFW_MINGW
#define KGFW_MINGW 1
#endif
#define KGFW_MINGW32 1
#endif

#if defined(__BORLANDC__)
#define KGFW_BORLANDC 1
#endif

/* dynamic libray macros */
#if defined(KGFW_DYNAMIC_EXPORT)
#if defined(KGFW_WINDOWS)
#define KGFW_PUBLIC __declspec(dllexport)
#define KGFW_PRIVATE
#else
#define KGFW_PUBLIC __attribute__((__visibility__("default"))
#define KGFW_PRIVATE __attribute__((__visibility__("hidden"))
#endif
#elif defined(KGFW_DYNAMIC_IMPORT)
#if defined(KGFW_WINDOWS)
#define KGFW_PUBLIC __declspec(dllimport)
#define KGFW_PRIVATE
#else
#define KGFW_PUBLIC __attribute__((__visibility__("default"))
#define KGFW_PRIVATE __attribute__((__visibility__("hidden"))
#endif
#else
#define KGFW_PUBLIC
#define KGFW_PRIVATE
#endif

#ifndef NULL
#define NULL ((void *) 0)
#endif

#endif