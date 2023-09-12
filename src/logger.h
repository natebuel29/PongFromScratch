#pragma once

#include <platform.h>

#include <stdio.h>

template <typename... Args>
void _log(char *prefix, TextColor color, const char *msg, Args... args)
{
    char fmtBuffer[32000] = {};
    char msgBuffer[3200] = {};
    sprintf(fmtBuffer, "%s: %s\n", prefix, msg);
    sprintf(msgBuffer, fmtBuffer, args...);
    platform_log(msgBuffer, color);
}

#define NB_TRACE(msg, ...) _log("TRACE", TEXT_COLOR_GREEN, msg, __VA_ARGS__)
#define NB_WARN(msg, ...) _log("WARN", TEXT_COLOR_YELLOW, msg, __VA_ARGS__)
#define NB_ERROR(msg, ...) _log("ERROR", TEXT_COLOR_RED, msg, __VA_ARGS__)
#define NB_FATAL(msg, ...) _log("FATAL", TEXT_COLOR_LIGHT_READ, msg, __VA_ARGS__)

#ifdef DEBUG
#define NB_ASSERT(x, msg, ...)          \
    {                                   \
        if (!(x))                       \
        {                               \
            NB_ERROR(msg, __VA_ARGS__); \
            __debugbreak();             \
        }                               \
    }
#elif
#define NB_ASSERT(x, msg, ...)
#endif