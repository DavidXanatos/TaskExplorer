// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifndef  WIN32_LEAN_AND_MEAN
#define  WIN32_LEAN_AND_MEAN
#endif

#define INITGUID

#include <memory>

#include <windows.h>
#include <tdh.h>
#include <evntrace.h>

#include "compiler_check.hpp"
#include "schema_locator.hpp"

#pragma comment(lib, "tdh.lib")


namespace krabs { namespace testing {
    class record_builder;
} /* namespace testing */ } /* namespace krabs */


namespace krabs {

    class schema;
    class parser;

    /**
     * <summary>
     * Used to query events for detailed information. Creation is rather
     * costly, so client code should try hard to delay creation of this.
     * </summary>
     */
    class schema {
    public:

        /**
         * <summary>
         * Constructs a schema from an event record instance.
         * </summary>
         *
         * <example>
         *   void event(const EVENT_RECORD &record)
         *   {
         *       krabs::schema schema(record);
         *   }
         * </example>
         */
        schema(const EVENT_RECORD &);

        /**
         * <summary>Compares two schemas for equality.<summary>
         *
         * <example>
         *   schema1 == schema2;
         *   schema1 != schema2;
         * </example>
         */
        bool operator==(const schema &other) const;
        bool operator!=(const schema &other) const;

        /*
         * <summary>
         * Returns the name of an event via its schema.
         * </summary>
         * <example>
         *    void on_event(const EVENT_RECORD &record)
         *    {
         *        krabs::schema schema(record);
         *        std::wstring name = krabs::event_name(schema);
         *    }
         * </example>
         */
        const wchar_t *event_name() const;

        /**
         * <summary>
         * Returns the event ID via its schema.
         * </summary>
         * <example>
         *    void on_event(const EVENT_RECORD &record)
         *    {
         *        krabs::schema schema(record);
         *        int id = schema.event_id();
         *    }
         * </example>
         */
        int event_id() const;

        /**
         * <summary>
         * Returns the event opcode.
         * </summary>
         * <example>
         *    void on_event(const EVENT_RECORD &record)
         *    {
         *        krabs::schema schema(record);
         *        int opcode = schema.event_opcode();
         *    }
         * </example>
         */
        int event_opcode() const;

        /**
         * <summary>
         * Returns the version of the event.
         * </summary>
         */
        unsigned int event_version() const;

        /**
         * <summary>
         * Returns the flags of the event.
         * </summary>
         */
        unsigned int event_flags() const;

        /**
         * <summary>
         * Returns the provider name of an event via its schema.
         * </summary>
         * <example>
         *    void on_event(const EVENT_RECORD &record)
         *    {
         *        krabs::schema schema(record);
         *        std::wstring name = krabs::provider_name(schema);
         *    }
         * </example>
         */
        const wchar_t *provider_name() const;

        /**
        * <summary>
        * Returns the PID associated with the event via its schema.
        * </summary>
        * <example>
        *    void on_event(const EVENT_RECORD &record)
        *    {
        *        krabs::schema schema(record);
        *        unsigned int name = krabs::process_id(schema);
        *    }
        * </example>
        */
        unsigned int process_id() const;

        /**
        * <summary>
        * Returns the Thread ID associated with the event via its schema.
        * </summary>
        * <example>
        *    void on_event(const EVENT_RECORD &record)
        *    {
        *        krabs::schema schema(record);
        *        unsigned int name = krabs::thread_id(schema);
        *    }
        * </example>
        */
        unsigned int thread_id() const;

        /**
        * <summary>
        * Returns the timestamp associated with the event via its schema.
        * </summary>
        * <example>
        *    void on_event(const EVENT_RECORD &record)
        *    {
        *        krabs::schema schema(record);
        *        LARGE_INTEGER time = krabs::timestamp(schema);
        *    }
        * </example>
        */
        LARGE_INTEGER timestamp() const;

    private:
        const EVENT_RECORD &record_;
        TRACE_EVENT_INFO *pSchema_;

    private:
        friend std::wstring event_name(const schema &);
        friend std::wstring provider_name(const schema &);
        friend unsigned int process_id(const schema &);
        friend LARGE_INTEGER timestamp(const schema &);
        friend int event_id(const EVENT_RECORD &);
        friend int event_id(const schema &);

        friend class parser;
        friend class property_iterator;
        friend class record_builder;
    };


    // Implementation
    // ------------------------------------------------------------------------

    inline schema::schema(const EVENT_RECORD &record)
        : record_(record)
        , pSchema_(schema_locator::get_instance().get_event_schema(record))
    { }

    inline bool schema::operator==(const schema &other) const
    {
        return (pSchema_->ProviderGuid == other.pSchema_->ProviderGuid &&
                pSchema_->EventDescriptor.Id == other.pSchema_->EventDescriptor.Id &&
                pSchema_->EventDescriptor.Version == other.pSchema_->EventDescriptor.Version);
    }

    inline bool schema::operator!=(const schema &other) const
    {
        return !(*this == other);
    }

    inline const wchar_t *schema::event_name() const
    {
        return reinterpret_cast<const wchar_t*>(
            reinterpret_cast<const char*>(pSchema_) +
            pSchema_->OpcodeNameOffset);
    }

    inline int schema::event_id() const
    {
        return record_.EventHeader.EventDescriptor.Id;
    }

    inline int schema::event_opcode() const
    {
        return record_.EventHeader.EventDescriptor.Opcode;
    }

    inline unsigned int schema::event_version() const
    {
        return record_.EventHeader.EventDescriptor.Version;
    }

    inline unsigned int schema::event_flags() const
    {
        return record_.EventHeader.Flags;
    }

    inline const wchar_t *schema::provider_name() const
    {
       return reinterpret_cast<const wchar_t*>(
           reinterpret_cast<const char*>(pSchema_) +
           pSchema_->ProviderNameOffset);
    }

    inline unsigned int schema::process_id() const
    {
        return record_.EventHeader.ProcessId;
    }

    inline unsigned int schema::thread_id() const
    {
        return record_.EventHeader.ThreadId;
    }

    inline LARGE_INTEGER schema::timestamp() const
    {
        return record_.EventHeader.TimeStamp;
    }
}
