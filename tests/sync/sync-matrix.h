/* Shared declarations for bounded additional U/S acceptance cases. */
#ifndef CIDER_SYNC_MATRIX_H
#define CIDER_SYNC_MATRIX_H
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
typedef void (*matrix_check_fn)(const char *name,int ok,int64_t actual,int64_t expected);
void matrix_server_cases(matrix_check_fn check,DWORD timeout_ms);
#endif
