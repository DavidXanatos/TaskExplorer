/*
 * Copyright (c) 2023 Xanasoft.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     DavidXanatos   2022-2025
 *
 */

#pragma once


#define TRACE_LEVEL_ERROR 0
#define TRACE_LEVEL_WARNING 1
#define TRACE_LEVEL_VERBOSE 2
#define TRACE_LEVEL_INFORMATION 3
#define TRACE_LEVEL_CRITICAL 4

#define GENERAL 0
#define TRACKING 1
#define COMMS 2
#define HASH 3
#define INFORMER 4
#define PROTECTION 5
#define UTIL 6
#define VERIFY 7
#define SOCKET 8

#define WPP_INIT_TRACING(x,y)
#define WPP_CLEANUP(z) UNREFERENCED_PARAMETER(z)

void KphTracePrint(ULONG level, ULONG event, const char* message, ...);
