// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <deque>

#include "compiler_check.hpp"
#include "guid.hpp"
#include "provider.hpp"
#include "nightmare.hpp"


namespace krabs { namespace details {
    template <typename T> class trace_manager;
} /* namespace details */ } /* namespace krabs */

namespace krabs { namespace testing {
    template <typename T> class trace_proxy;
} /* namespace testing */ } /* namespace krabs */


namespace krabs {

    template <typename T>
    class provider;

    /**
     * <summary>
     * Selected statistics about an ETW trace
     * </summary>
     */
    class trace_stats
    {
    public:
        const uint32_t buffersCount;
        const uint32_t buffersFree;
        const uint32_t buffersWritten;
        const uint32_t buffersLost;
        const uint64_t eventsTotal;
        const uint64_t eventsHandled;
        const uint32_t eventsLost;

        trace_stats(uint64_t eventsHandled, const EVENT_TRACE_PROPERTIES& props)
            : buffersCount(props.NumberOfBuffers)
            , buffersFree(props.FreeBuffers)
            , buffersWritten(props.BuffersWritten)
            , buffersLost(props.RealTimeBuffersLost)
            , eventsTotal(eventsHandled + props.EventsLost)
            , eventsHandled(eventsHandled)
            , eventsLost(props.EventsLost)
        { }
    };

    /**
     * <summary>
     *    Represents a single trace session that can have multiple
     *    enabled providers. Ideally, there should only need to be a
     *    single trace instance for all ETW user traces.
     * </summary>
     */
    template <typename T>
    class trace {
    public:

        typedef T trace_type;

        /**
         * <summary>
         *   Constructs a trace with an optional trace name, which can be
         *   any arbitrary, unique name.
         * </summary>
         *
         * <example>
         *   trace trace;
         *   trace namedTrace(L"Some special name");
         * </example>
         */
        trace(const std::wstring &name);
        trace(const wchar_t *name = L"");

        /**
         * <summary>
         *   Destructs the trace session and unregisters the session, if
         *   applicable.
         * </summary>
         *
         * <example>
         *   trace trace;
         *   // ~trace implicitly called
         * </example>
         */
        ~trace();

        /**
         * <summary>
         * Enables the provider on the given user trace.
         * </summary>
         * <example>
         *    krabs::trace trace;
         *    krabs::guid id(L"{A0C1853B-5C40-4B15-8766-3CF1C58F985A}");
         *    provider<> powershell(id);
         *    trace.enable(powershell);
         * </example>
         */
        void enable(const typename T::provider_type &p);

        /**
         * <summary>
         * Starts a trace session.
         * </summary>
         * <example>
         *    krabs::trace trace;
         *    krabs::guid id(L"{A0C1853B-5C40-4B15-8766-3CF1C58F985A}");
         *    provider<> powershell(id);
         *    trace.enable(powershell);
         *    trace.start();
         * </example>
         */
        void start();

        /**
         * <summary>
         * Closes a trace session.
         * </summary>
         * <example>
         *    krabs::trace trace;
         *    krabs::guid id(L"{A0C1853B-5C40-4B15-8766-3CF1C58F985A}");
         *    provider<> powershell(id);
         *    trace.enable(powershell);
         *    trace.start();
         *    trace.stop();
         * </example>
         */
        void stop();

        /**
        * <summary>
        * Opens a trace session.
        * </summary>
        * <example>
        *    krabs::trace trace;
        *    krabs::guid id(L"{A0C1853B-5C40-4B15-8766-3CF1C58F985A}");
        *    provider<> powershell(id);
        *    trace.enable(powershell);
        *    auto logfile = trace.open();
        * </example>
        */
        EVENT_TRACE_LOGFILE open();

        /**
        * <summary>
        * Initiates the processing loop for a trace session.
        * </summary>
        * <example>
        *    krabs::trace trace;
        *    krabs::guid id(L"{A0C1853B-5C40-4B15-8766-3CF1C58F985A}");
        *    provider<> powershell(id);
        *    trace.enable(powershell);
        *    auto logfile = trace.open();
        *    trace.process();
        * </example>
        */
        void process();
        /**
         * <summary>
         * Queries the trace session to get stats about
         * events lost and buffers handled.
         * </summary>
         */
        trace_stats query_stats();

        /**
         * <summary>
         * Returns the number of buffers that were processed.
         * </summary>
         * <example>
         *    krabs::trace trace;
         *    krabs::guid id(L"{A0C1853B-5C40-4B15-8766-3CF1C58F985A}");
         *    provider<> powershell(id);
         *    trace.enable(powershell);
         *    trace.start();
         *    trace.stop();
         *    std::wcout << trace.buffers_processed() << std::endl;
         * </example>
         */
        size_t buffers_processed() const;

    private:

        /**
         * <summary>
         *   Invoked when an event occurs in the underlying ETW session.
         * </summary>
         */
        void on_event(const EVENT_RECORD &);

    private:
        std::wstring name_;
        std::deque<std::reference_wrapper<const typename T::provider_type>> providers_;

        TRACEHANDLE registrationHandle_;
        TRACEHANDLE sessionHandle_;

        size_t buffersRead_;
        uint64_t eventsHandled_;

    private:
        template <typename T>
        friend class details::trace_manager;

        template <typename T>
        friend class testing::trace_proxy;

        friend typename T;
    };

    // Implementation
    // ------------------------------------------------------------------------

    template <typename T>
    trace<T>::trace(const std::wstring &name)
    : registrationHandle_(INVALID_PROCESSTRACE_HANDLE)
    , sessionHandle_(INVALID_PROCESSTRACE_HANDLE)
    , eventsHandled_(0)
    , buffersRead_(0)
    {
        name_ = T::enforce_name_policy(name);
    }

    template <typename T>
    trace<T>::trace(const wchar_t *name)
    : registrationHandle_(INVALID_PROCESSTRACE_HANDLE)
    , sessionHandle_(INVALID_PROCESSTRACE_HANDLE)
    , eventsHandled_(0)
    , buffersRead_(0)
    {
        name_ = T::enforce_name_policy(name);
    }

    template <typename T>
    trace<T>::~trace()
    {
        stop();
    }

    template <typename T>
    void trace<T>::on_event(const EVENT_RECORD &record)
    {
        ++eventsHandled_;
        T::forward_events(record, *this);
    }

    template <typename T>
    void trace<T>::enable(const typename T::provider_type &p)
    {
        providers_.push_back(std::ref(p));
    }

    template <typename T>
    void trace<T>::start()
    {
        eventsHandled_ = 0;

        details::trace_manager<trace> manager(*this);
        manager.start();
    }

    template <typename T>
    void trace<T>::stop()
    {
        details::trace_manager<trace> manager(*this);
        manager.stop();
    }

    template <typename T>
    EVENT_TRACE_LOGFILE trace<T>::open()
    {
        eventsHandled_ = 0;

        details::trace_manager<trace> manager(*this);
        return manager.open();
    }

    template <typename T>
    void trace<T>::process()
    {
        details::trace_manager<trace> manager(*this);
        return manager.process();
    }

    template <typename T>
    trace_stats trace<T>::query_stats()
    {
        details::trace_manager<trace> manager(*this);
        return { eventsHandled_, manager.query() };
    }

    template <typename T>
    size_t trace<T>::buffers_processed() const
    {
        return buffersRead_;
    }

}
