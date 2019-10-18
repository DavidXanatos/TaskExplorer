// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifndef  WIN32_LEAN_AND_MEAN
#define  WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <objbase.h>

#include <new>
#include <memory>
#include <sstream>
#include <iomanip>

#include "compiler_check.hpp"

namespace krabs {

    /** <summary>
     * Represents a GUID, allowing simplified construction from a string or
     * Windows GUID structure.
     * </summary>
     */
    class guid {
    public:
        guid(GUID guid);
        guid(const std::wstring &guid);

        bool operator==(const guid &rhs) const;
        bool operator==(const GUID &rhs) const;

        bool operator<(const guid &rhs) const;
        bool operator<(const GUID &rhs) const;

        operator GUID() const;
        operator const GUID*() const;

        /** <summary>
          * Constructs a new random guid.
          * </summary>
          */
        static inline guid random_guid();

    private:
        GUID guid_;
    };

    // Implementation
    // ------------------------------------------------------------------------

    inline guid::guid(GUID guid)
        : guid_(guid)
    {}

    inline guid::guid(const std::wstring &guid)
    {
        HRESULT hr = CLSIDFromString(guid.c_str(), &guid_);
        if (FAILED(hr)) {
            std::string guidStr(guid.begin(), guid.end());
            std::stringstream stream;
            stream << "Error in constructing guid from string (";
            stream << guidStr;
            stream << "), hr = 0x";
            stream << std::hex << hr;
            throw std::runtime_error(stream.str());
        }
    }

    inline bool guid::operator==(const guid &rhs) const
    {
        return (0 == memcmp(&guid_, &rhs.guid_, sizeof(GUID)));
    }

    inline bool guid::operator==(const GUID &rhs) const
    {
        return (0 == memcmp(&guid_, &rhs, sizeof(GUID)));
    }

    inline bool guid::operator<(const guid &rhs) const
    {
        return (memcmp(&guid_, &rhs.guid_, sizeof(guid_)) < 0);
    }

    inline bool guid::operator<(const GUID &rhs) const
    {
        return (memcmp(&guid_, &rhs, sizeof(guid_)) < 0);
    }

    inline guid::operator GUID() const
    {
        return guid_;
    }

    inline guid::operator const GUID*() const
    {
        return &guid_;
    }

    inline guid guid::random_guid()
    {
        GUID tmpGuid;
        CoCreateGuid(&tmpGuid);
        return guid(tmpGuid);
    }

    struct CoTaskMemDeleter {
        void operator()(wchar_t *mem) {
            CoTaskMemFree(mem);
        }
    };
}

namespace std
{
    inline std::wstring to_wstring(const krabs::guid& guid)
    {
        wchar_t* guidString;
        HRESULT hr = StringFromCLSID(guid, &guidString);

        if (FAILED(hr)) throw std::bad_alloc();

        std::unique_ptr<wchar_t, krabs::CoTaskMemDeleter> managed(guidString);

        return { managed.get() };
    }
}
