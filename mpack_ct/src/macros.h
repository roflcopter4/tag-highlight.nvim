#pragma once

#include "p99/p99.h"

#define P_DEFARG(...)                    \
        P99_DECLARE_DEFARG(__VA_ARGS__); \
        P99_DEFINE_DEFARG(__VA_ARGS__)

/* 
 99_PROTOTYPE(int, arse, int, int, char *);
#define arse(...) P99_CALL_DEFARG(arse, 3, __VA_ARGS__)
P_DEFARG(arse, (int)1, (int)1, "hello");
 */
