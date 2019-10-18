// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifndef  WIN32_LEAN_AND_MEAN
#define  WIN32_LEAN_AND_MEAN
#endif

#define INITGUID

#include <windows.h>
#include <tdh.h>
#include <evntrace.h>

#include <memory>
#include <unordered_map>

#include "compiler_check.hpp"
#include "guid.hpp"
#include "lock.hpp"

#pragma comment(lib, "tdh.lib")

namespace krabs {

    /**
     * <summary>
     * Type used as the key for cache lookup in a schema_locator.
     * </summary>
     */
    struct schema_key
    {
        guid      provider;
        uint16_t  id;
        uint8_t   opcode;
        uint8_t   version;
        uint8_t   level;

        schema_key(const EVENT_RECORD& record)
            : provider(record.EventHeader.ProviderId)
            , id(record.EventHeader.EventDescriptor.Id)
            , opcode(record.EventHeader.EventDescriptor.Opcode)
            , level(record.EventHeader.EventDescriptor.Level)
            , version(record.EventHeader.EventDescriptor.Version) { }

        bool operator==(const schema_key& rhs) const
        {
            return provider == rhs.provider &&
                   id == rhs.id &&
                   opcode == rhs.opcode &&
                   level == rhs.level &&
                   version == rhs.version;
        }

        bool operator!=(const schema_key& rhs) const { return !(*this == rhs); }
    };
}

namespace std {

    /**
     * <summary>
     * Builds a hash code for a schema_key
     * </summary>
     */
    template<>
    struct std::hash<krabs::schema_key>
    {
        size_t operator()(const krabs::schema_key& key) const
        {
            // Shift-Add-XOR hash - good enough for the small sets we deal with
            const char* p = (const char*)&key;
            size_t h = 2166136261;

            for(auto i = 0; i < sizeof(key); ++i)
            {
                h ^= (h << 5) + (h >> 2) + p[i];
            }

            return h;
        }
    };
}

namespace krabs {

    /**
     * <summary>
     * Get event schema from TDH.
     * </summary>
     */
    std::unique_ptr<char[]> get_event_schema_from_tdh(const EVENT_RECORD&);

    /**
     * <summary>
     * Fetches and caches schemas from TDH. Also has a singleton
     * instance. It is preferable to inject the locator, but the
     * current krabs implementation isn't built in a way that
     * allows the schema_locator to be injected in all of the
     * places where Schemas or Parsers are constructed.
     * </summary>
     */
    class schema_locator {
    public:

        /**
         * <summary>
         * Retrieves the event schema from the cache or falls back to
         * TDH to load the schema.
         * </summary>
         */
        const PTRACE_EVENT_INFO get_event_schema(const EVENT_RECORD& record);

        /**
         * <summary>
         * Reset the cache. IMPORTANT! This cannot be called while other
         * objects have references to cache items. This basically means
         * don't call this unless tracing is stopped or you'll probably
         * cause an AV. TODO: could be addressed with shared_ptr but
         * requires C++17 features or some rewrite of the buffer handling.
         * </summary>
         */
        void reset();

        /**
         * Get the schema_locator singleton instance.
         * WARNING: if compiled into managed code, do NOT get a reference
         * to this in a static initializer or it will cause loader lock.
         */
        static schema_locator& get_instance()
        {
            return singleton_;
        }

    private:
        static schema_locator singleton_;
        critical_section sync_;
        std::unordered_map<schema_key, std::unique_ptr<char[]>> cache_;
    };

    // Implementation
    // ------------------------------------------------------------------------

    inline const PTRACE_EVENT_INFO schema_locator::get_event_schema(const EVENT_RECORD& record)
    {
        // check the cache
        auto key = schema_key(record);
        auto& buffer = cache_[key];

        // return if there's a cache hit
        if(buffer) return (PTRACE_EVENT_INFO)(&buffer[0]);

        auto temp = get_event_schema_from_tdh(record);

        // multiple threads may end up trying to save their
        // temp objects to the cache so we need to lock
        // so only the first entry should be cached.

        if(!buffer)
        {
            scope_lock l(sync_);
            if (!buffer) buffer.swap(temp);
        }

        return (PTRACE_EVENT_INFO)(&buffer[0]);
    }

    inline void schema_locator::reset()
    {
        cache_.clear();
    }

    inline std::unique_ptr<char[]> get_event_schema_from_tdh(const EVENT_RECORD& record)
    {
        // get required size
        ULONG bufferSize = 0;
        ULONG status = TdhGetEventInformation(
            (PEVENT_RECORD)&record,
            0,
            NULL,
            NULL,
            &bufferSize);

        if (status != ERROR_INSUFFICIENT_BUFFER) {
            error_check_common_conditions(status);
        }

        // allocate and fill the schema from TDH
        auto buffer = std::unique_ptr<char[]>(new char[bufferSize]);

        error_check_common_conditions(
            TdhGetEventInformation(
            (PEVENT_RECORD)&record,
            0,
            NULL,
            (PTRACE_EVENT_INFO)&buffer[0],
            &bufferSize));

        return buffer;
    }
}
