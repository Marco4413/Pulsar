#ifndef _PULSARTOOLS_CORE_H
#define _PULSARTOOLS_CORE_H

#include <cinttypes>
#include <cstdlib>

#include "fmt/color.h"

#define PULSARTOOLS_PRINTF fmt::print

#define PULSARTOOLS_ASSERT(cond, msg)                 \
    do {                                              \
        if (!(cond)) {                                \
            PULSARTOOLS_PRINTF(stderr,                \
                fmt::fg(fmt::color::red),             \
                "{}:{}: Assertion Failed ({}): {}\n", \
                __FILE__, __LINE__, #cond, msg);      \
            std::exit(1);                             \
        }                                             \
    } while (false)

#define PULSARTOOLS_ASSERTF(cond, msg, ...)           \
    do {                                              \
        if (!(cond)) {                                \
            PULSARTOOLS_PRINTF(stderr,                \
                fmt::fg(fmt::color::red),             \
                "{}:{}: Assertion Failed ({}): {}\n", \
                __FILE__, __LINE__, #cond,            \
                fmt::format(msg, __VA_ARGS__));       \
            std::exit(1);                             \
        }                                             \
    } while (false)

#define PULSARTOOLS_ERROR(msg)        \
    do {                              \
        PULSARTOOLS_PRINTF(stderr,    \
            fmt::fg(fmt::color::red), \
            "[ERROR]: {}\n", msg);    \
    } while (false)

#define PULSARTOOLS_ERRORF(msg, ...)        \
    do {                                    \
        PULSARTOOLS_PRINTF(stderr,          \
            fmt::fg(fmt::color::red),       \
            "[ERROR]: {}\n",                \
            fmt::format(msg, __VA_ARGS__)); \
    } while (false)

#define PULSARTOOLS_WARN(msg)            \
    do {                                 \
        PULSARTOOLS_PRINTF(stdout,       \
            fmt::fg(fmt::color::orange), \
            "[WARN]: {}\n", msg);        \
    } while (false)

#define PULSARTOOLS_WARNF(msg, ...)         \
    do {                                    \
        PULSARTOOLS_PRINTF(stdout,          \
            fmt::fg(fmt::color::orange),    \
            "[WARN]: {}\n",                 \
            fmt::format(msg, __VA_ARGS__)); \
    } while (false)

#define PULSARTOOLS_INFO(msg)                     \
    do {                                          \
        PULSARTOOLS_PRINTF(stdout,                \
            "{} {}\n",                            \
            fmt::styled("[INFO]:",                \
                fmt::fg(fmt::color::light_blue)), \
            msg);                                 \
    } while (false)

#define PULSARTOOLS_INFOF(msg, ...)               \
    do {                                          \
        PULSARTOOLS_PRINTF(stdout,                \
            "{} {}\n",                            \
            fmt::styled("[INFO]:",                \
                fmt::fg(fmt::color::light_blue)), \
            fmt::format(msg, __VA_ARGS__));       \
    } while (false)

#endif // _PULSARTOOLS_CORE_H
