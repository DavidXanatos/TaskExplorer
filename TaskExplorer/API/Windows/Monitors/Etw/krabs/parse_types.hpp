// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifndef  WIN32_LEAN_AND_MEAN
#define  WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <vector>
#include <type_traits>

#include "compiler_check.hpp"

namespace krabs {

    /**
     * <summary>
     *   Provided entirely for code clarity purposes.
     *   Indicates that the number is intended to be used as an ID
     * </summary>
     * <remarks>
     *   This should be turned into an _id user defined literal when our
     *   compiler decides to catch up to the times.
     * </remarks>
     * <example>
     *     id(1000);
     * </example>
     */
     template <typename T>
     T id(T n)
     {
        return n;
     }

    /**
     * <summary>
     *   Provided entirely for code clarity purposes.
     *   Indicates that the number is intended to be used as a version
     * </summary>
     * <remarks>
     *   This should be turned into a _vers user defined literal when our
     *   compiler decides to catch up to the times.
     * </remarks>
     * <example>
     *     id(1000);
     * </example>
     */
     template <typename T>
     T version(T n)
     {
        return n;
     }

    /**
     * <summary>
     *   Provided entirely for code clarity purposes.
     *   Indicates that the number is intended to be used as an opcode
     * </summary>
     * <remarks>
     *   This should be turned into a _opcode user defined literal when our
     *   compiler decides to catch up to the times.
     * </remarks>
     * <example>
     *     opcode(1000);
     * </example>
     */
     template <typename T>
     T opcode(T n)
     {
        return n;
     }


    /**
     * <summary>
     * Used to discriminate between hex ints and regular ints in ETW events.
     * </summary>
     * <remarks>
     * Q: Why in the world? I can't even.
     * A: ETW differentiates between hexints and regular ints. When
     *    record_builder validates that the input type matches the type
     *    specified in the schema, getting this wrong will cause an
     *    exception. A quick little type wrapper like this lets us
     *    discriminate based on the type and everything turns out better.
     * </remarks>
     */
    struct hexint32 {
        hexint32(int v)
        : value(v)
        {}

        int value;
    };

    struct hexint64 {
        hexint64(long long v)
        : value(v)
        {}

        long long value;
    };

    /**
     * <summary>
     * Used to support parsing and creation of binary ETW fields.
     * </summary>
     */
    struct binary {
    public:
        binary() : bytes_() { }

        binary(const BYTE* start, size_t n)
        : bytes_(start, start + n)
        { }

        const std::vector<BYTE>& bytes() const
        {
            return bytes_;
        }

    private:
        std::vector<BYTE> bytes_;
    };

    template<typename T>
    binary make_binary(const T& value, size_t n)
    {
        const auto start = (BYTE*)&value;
        return binary(start, n);
    }

    /**
     * <summary>
     * Used to handle parsing of IPv4 and IPv6 fields in an ETW record.
     * This is used in the parser class in a template specialization.
     * </summary>
     */
    struct ip_address {
        union {
            DWORD v4;
            BYTE v6[16];
        };
        bool is_ipv6;

        static ip_address from_ipv6(const BYTE* bytes)
        {
            ip_address addr;
            addr.is_ipv6 = true;
            memcpy_s(addr.v6, 16, bytes, 16);
            return addr;
        }

        static ip_address from_ipv4(DWORD val)
        {
            ip_address addr;
            addr.is_ipv6 = false;
            addr.v4 = val;
            return addr;
        }

        ip_address() {}
     };

    /**
    * <summary>
    * Used to handle parsing of socket addresses in
    * network order. This union is a convenient wrapper
    * around the type IPv4 and IPv6 types provided by
    * the Winsock (v2) APIs.
    * </summary>
    */
    struct socket_address {
        union {
            struct sockaddr sa;
            struct sockaddr_in sa_in;
            struct sockaddr_in6 sa_in6;
            struct sockaddr_storage sa_stor;
        };

        static socket_address from_bytes(const BYTE* bytes, size_t size_in_bytes)
        {
            socket_address sa;
            memcpy_s(&(sa.sa_stor), sizeof sa.sa_stor, bytes, size_in_bytes);
            return sa;
        }
    };

    /**
     * <summary>
     * Holds information about an property extracted from the etw schema
     * </summary>
     */
    struct property_info {
        const BYTE *pPropertyIndex_;
        const EVENT_PROPERTY_INFO *pEventPropertyInfo_;
        ULONG length_;

        property_info(
            const BYTE *offset,
            const EVENT_PROPERTY_INFO &evtPropInfo,
            ULONG length)
            : pPropertyIndex_(offset)
            , pEventPropertyInfo_(&evtPropInfo)
            , length_(length)
        { }

        property_info()
            : pPropertyIndex_(nullptr)
            , pEventPropertyInfo_(nullptr)
            , length_(0)
        { }

        inline bool found() const
        {
            return pPropertyIndex_ != nullptr;
        }
    };

    /**
     * <summary>
     * Used to handle parsing of CountedStrings in an ETW Record.
     * This is used in the parser class in a template specialization.
     * </summary>
     */
#pragma pack(push,1)
    struct counted_string {
        using value_type = wchar_t;
        using reference = value_type&;
        using pointer = value_type*;
        using const_reference = const value_type&;
        using const_pointer = const value_type*;
        using iterator = value_type*;
        using const_iterator = const value_type*;

        /**
         * size of the string in bytes
         */
        uint16_t size_;
        wchar_t string_[1];

        const_pointer string() const
        {
            return string_;
        }

        size_t length() const
        {
            return size_ / sizeof(value_type);
        }
    };
#pragma pack(pop)

    static_assert(std::is_pod<counted_string>::value, "Do not modify counted_string");

} /* namespace krabs */
