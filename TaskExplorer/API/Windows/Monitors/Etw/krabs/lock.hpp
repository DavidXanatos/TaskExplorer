// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifndef  WIN32_LEAN_AND_MEAN
#define  WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include "compiler_check.hpp"

// Use:
// static critical_section cs;
//
//  scope_lock lock(cs);
//  ... /* code is synchronized in this scope */

namespace krabs {

    // Wraps a critical section for code synchronization
    class critical_section
    {
    private:
        mutable CRITICAL_SECTION critical;

        critical_section(const critical_section&) = delete;
        critical_section& operator=(const critical_section&) = delete;

    public:
        critical_section()
        {
            InitializeCriticalSection(&critical);
        }

        ~critical_section()
        {
            DeleteCriticalSection(&critical);
        }

        bool try_enter() const
        {
            return TryEnterCriticalSection(&critical) != FALSE;
        }

        void enter() const
        {
            EnterCriticalSection(&critical);
        }

        void leave() const
        {
            LeaveCriticalSection(&critical);
        }
    };

    // RAII Wrapper for a entering and leaving a critical section
    class scope_lock
    {
    private:
        const critical_section&  criticalInst;

        scope_lock& operator=(const scope_lock&) = delete;

    public:
        scope_lock(const critical_section& cs) : criticalInst(cs)
        {
            criticalInst.enter();
        }

        scope_lock(const critical_section* cs) : criticalInst(*cs)
        {
            criticalInst.enter();
        }

        ~scope_lock()
        {
            criticalInst.leave();
        }
    };
}