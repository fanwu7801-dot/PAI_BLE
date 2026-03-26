/********************************************************************************
** Copyright <2022>[-<2023>]                                                    *
** Licensed under the Unisoc General Software License                           *
** version 1.0 (the License);                                                   *
** you may not use this file except in compliance with the License              *
** You may obtain a copy of the License at                                      *
** https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US       *
** Software distributed under the License is distributed on an "AS IS" BASIS,   *
** WITHOUT WARRANTIES OF ANY KIND, either express or implied.                   *
** See the Unisoc General Software License, version 1.0 for more details.       *
********************************************************************************/
/********************************************************************************
** File Name:       uos_type.h
** Author: Qiang He
** Copyright (C) 2002 Unisoc Communications Inc.
** Description:
********************************************************************************/
#ifndef _UOS_TYPE_H_
#define _UOS_TYPE_H_

#include <stdlib.h>
//#include <stdio.h>
#include <string.h>
//#include <stdbool.h>
#include <sys/types.h>

#ifdef  __cplusplus
extern "C"
{
#endif

typedef void *uv_poll_t;



/* data type definitions */
typedef signed   char                   uos_int8_t;      /* 8bit integer type */
typedef signed   short                  uos_int16_t;     /* 16bit integer type */
typedef signed   int                    uos_int32_t;     /* 32bit integer type */
typedef unsigned char                   uos_uint8_t;     /* 8bit unsigned integer type */
typedef unsigned short                  uos_uint16_t;    /* 16bit unsigned integer type */
typedef unsigned int                    uos_uint32_t;    /* 32bit unsigned integer type */

#ifdef ARCH_CPU_64BIT
typedef signed long                     uos_int64_t;     /* 64bit integer type */
typedef unsigned long                   uos_uint64_t;    /* 64bit unsigned integer type */
typedef unsigned long                   uos_uint32_t;    /* Type for size number */
#else
typedef signed long long                uos_int64_t;     /* 64bit integer type */
typedef unsigned long long              uos_uint64_t;    /* 64bit unsigned integer type */
#endif

//typedef uos_int32_t                     off_t;
//typedef uos_uint32_t                    size_t;
//typedef uos_int32_t                     ssize_t;
typedef uos_int32_t                     uos_bool_t;
typedef uos_int32_t                     uos_err_t;
typedef uos_uint32_t                    uos_tick_t;
typedef uos_uint32_t                    uos_thread_id_t;

typedef uos_uint8_t                     uint8;
typedef uos_uint16_t                    uint16;
typedef uos_uint32_t                    uint32;
typedef uos_int8_t                      int8;
typedef uos_int16_t                     int16;
typedef uos_int32_t                     int32;
typedef unsigned short                  wchar;
typedef char                            boolean;
typedef uos_uint8_t                     BOOLEAN;
typedef int                             errno_t;
/* typedef unsigned int                    BOOL; */
typedef float                           FLOAT;
typedef double                          DOUBLE;
typedef char                            CHAR;
typedef unsigned int                    UINTPTR;
typedef signed int                      INTPTR;
typedef char                            MC_BOOL;
typedef void                            VOID;
#define MC_FALSE 0

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define PNULL                           0

#define UOS_PIN_HIGH                    1
#define UOS_PIN_LOW                     0

/* boolean type definitions */
#define UOS_TRUE                        1         /* boolean true  */
#define UOS_FALSE                       0         /* boolean fails */

#define UOS_SUCCESS  0
#define UOS_FAILURE  1
/* error code definitions */
#define UOS_OK                          0          /* There is no error */
#define UOS_ERROR                       1          /* A generic error happens */
#define UOS_ERR_TIMEOUT                 2          /* Timed out */
#define UOS_ERR_FULL                    3          /* The resource is full */
#define UOS_ERR_EMPTY                   4          /* The resource is empty */
#define UOS_ERR_NOMEM                   5          /* No memory */
#define UOS_ERR_NOSYS                   6          /* No system */
#define UOS_ERR_BUSY                    7          /* Busy */
#define UOS_ERR_IO                      8          /* IO error */
#define UOS_ERR_INTR                    9          /* Interrupted system call */
#define UOS_ERR_INVAL                   10         /* Invalid argument */

#ifndef BIT
#define BIT(x) (0x01UL << (x))
#endif

#ifndef min
#define min(x,y) ((x) < (y)?(x):(y))
#endif

#ifndef max
#define max(x,y) (x)>(y)?(x):(y)
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#endif

//#define container_of(ptr, type, member) ((type *)((char *)ptr = offsetof(type, member)))

#ifndef NULL
#define NULL                            0L
#endif

#define UOS_NULL                        0L
#define UOS_NULL_PTR                    ((void *)0)
#define UOS_UNUSED(x)                   ((void)x)
#define UOS_UNUSED_PTR(x)               ((void*)x)
#define UOS_ALIGN_SIZE                  (4)

#define __uos_align(x)                  __attribute__((aligned(x)))


#define uos_inline                       static __inline
/**
 *
 * @def UOS_ALIGN(size, align)
 * Return the most contiguous size aligned at specified width. UOS_ALIGN(13, 4)
 * would return 16.
 */
#define UOS_ALIGN(size, align)           (((size) + (align) - 1) & ~((align) - 1))

/**
 * @def UOS_ALIGN_DOWN(size, align)
 * Return the down number of aligned at specified width. UOS_ALIGN_DOWN(13, 4)
 * would return 12.
 */
#define UOS_ALIGN_DOWN(size, align)      ((size) & ~((align) - 1))

#if 0
/* Compiler Related Definitions */
#if defined(__ARMCC_VERSION)           /* ARM Compiler */
#include <stdarg.h>
#define UOS_SECTION(x)              __attribute__((section(x)))
#define UOS_USED                    __attribute__((used))
//#define ALIGN(n)                    __attribute__((aligned(n)))
#define __align(n)                    __attribute__((aligned(n)))
#define UOS_WEAK                    __attribute__((weak))
#define uos_inline                  static __inline
#elif defined (__IAR_SYSTEMS_ICC__)     /* for IAR Compiler */
#include <stdarg.h>
#define UOS_SECTION(x)              @ x
#define UOS_USED                    __root
#define PRAGMA(x)                   _Pragma(#x)
//#define ALIGN(n)                    PRAGMA(data_alignment=n)
#define __align(n)                    PRAGMA(data_alignment=n)
#define UOS_WEAK                    __weak
#define uos_inline                  static inline
#elif defined (__GNUC__)                /* GNU GCC Compiler */
/* the version of GNU GCC must be greater than 4.x */
typedef __builtin_va_list           __gnuc_va_list;
typedef __gnuc_va_list              va_list;
#define va_start(v,l)               __builtin_va_start(v,l)
#define va_end(v)                   __builtin_va_end(v)
#define va_arg(v,l)                 __builtin_va_arg(v,l)
#define UOS_SECTION(x)              __attribute__((section(x)))
#define UOS_USED                    __attribute__((used))
//#define ALIGN(n)                    __attribute__((aligned(n)))
#define __align(n)                    __attribute__((aligned(n)))
#define UOS_WEAK                    __attribute__((weak))
#define uos_inline                  static __inline
#else
#error not supported tool chain
#endif
#endif

#ifdef  __cplusplus
}
#endif

#endif
