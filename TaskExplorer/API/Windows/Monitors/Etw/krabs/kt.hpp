// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include "compiler_check.hpp"
#include "trace.hpp"
#include "provider.hpp"

#include "ut.hpp"

#include <Evntrace.h>
#include "version_helpers.hpp"


namespace krabs { namespace details {

    /**
     * <summary>
     *   Used as a template argument to a trace instance. This class implements
     *   code paths for kernel traces. Should never be used or seen by client
     *   code.
     * </summary>
     */
    struct kt {

        typedef krabs::kernel_provider provider_type;

        /**
         * <summary>
         *   Used to assign a name to the trace instance that is being
         *   instantiated.
         * </summary>
         * <remarks>
         *   In pre-Win8 days, there could only be a single kernel trace
         *   instance on an entire machine, and that instance had to be named
         *   a particular name. This restriction was loosened in Win8, but
         *   the trace still needs to do the right thing on older OSes.
         * </remarks>
         */
        static const std::wstring enforce_name_policy(
            const std::wstring &name);

        /**
         * <summary>
         *   Generates a value that fills the EnableFlags field in an
         *   EVENT_TRACE_PROPERTIES structure. This controls the providers that
         *   get enabled for a kernel trace.
         * </summary>
         */
        static const unsigned long construct_enable_flags(
            const krabs::trace<krabs::details::kt> &trace);

        /** 
         * <summary>
         *   Enables the providers that are attached to the given trace.
         * </summary>
         * <remarks>
         *   This does a whole lot of nothing for kernel traces.
         * </remarks>
         */
        static void enable_providers(
            const krabs::trace<krabs::details::kt> &trace);

        /**
         * <summary>
         *   Decides to forward an event to any of the providers in the trace.
         * </summary>
         */
        static void forward_events(
            const EVENT_RECORD &record,
            const krabs::trace<krabs::details::kt> &trace);

        /**
         * <summary>
         *   Sets the ETW trace log file mode.
         * </summary>
         */
        static unsigned long augment_file_mode();

        /**
         * <summary>
         *   Returns the GUID of the trace session.
         * </summary>
         */
        static krabs::guid get_trace_guid();

    };

    // Implementation
    // ------------------------------------------------------------------------

    inline const std::wstring kt::enforce_name_policy(
        const std::wstring &name_hint)
    {
        if (IsWindows8OrGreater()) {
            return krabs::details::ut::enforce_name_policy(name_hint);
        }

        return KERNEL_LOGGER_NAME;
    }

    inline const unsigned long kt::construct_enable_flags(
        const krabs::trace<krabs::details::kt> &trace)
    {
        unsigned long flags = 0;
        for (auto &provider : trace.providers_) {
            flags |= provider.get().flags();
        }

        return flags;
    }

    inline void kt::enable_providers(
        const krabs::trace<krabs::details::kt> &)
    {
        return;
    }

    inline void kt::forward_events(
        const EVENT_RECORD &record,
        const krabs::trace<krabs::details::kt> &trace)
    {
        for (auto &provider : trace.providers_) {
            if (provider.get().id() == record.EventHeader.ProviderId) {
                provider.get().on_event(record);
            }
        }
    }

    inline unsigned long kt::augment_file_mode()
    {
        if (IsWindows8OrGreater()) {
            return EVENT_TRACE_SYSTEM_LOGGER_MODE;
        }

        return 0;
    }

    inline krabs::guid kt::get_trace_guid()
    {
        if (IsWindows8OrGreater()) {
            return krabs::guid::random_guid();
        }

        return krabs::guid(SystemTraceControlGuid);
    }



} /* namespace details */ } /* namespace krabs */
