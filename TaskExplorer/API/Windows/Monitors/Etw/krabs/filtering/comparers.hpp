// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#if(_MANAGED)
#pragma warning(push)
#pragma warning(disable: 4800) // bool warning in boost when _MANAGED flag is set
#endif

#include "../compiler_check.hpp"
//#include <boost/algorithm/string.hpp>
//#include <boost/range/iterator_range_core.hpp>

#if(_MANAGED)
#pragma warning(pop)
#endif

namespace krabs { namespace predicates {

    namespace comparers {

        // Algorithms
        // --------------------------------------------------------------------

        /**
         * Iterator based equals
         */
        template <typename Comparer>
        struct equals
        {
            template <typename Iter1, typename Iter2>
            bool operator()(Iter1 begin1, Iter1 end1, Iter2 begin2, Iter2 end2)
            {
                auto r1 = boost::make_iterator_range(begin1, end1);
                auto r2 = boost::make_iterator_range(begin2, end2);

                return boost::equals(r1, r2, Comparer());
            }
        };

        /**
         * Iterator based search
         */
        template <typename Comparer>
        struct contains
        {
            template <typename Iter1, typename Iter2>
            bool operator()(Iter1 begin1, Iter1 end1, Iter2 begin2, Iter2 end2)
            {
                auto r1 = boost::make_iterator_range(begin1, end1);
                auto r2 = boost::make_iterator_range(begin2, end2);

                return boost::contains(r1, r2, Comparer());
            }
        };

        /**
         * Iterator based starts_with
         */
        template <typename Comparer>
        struct starts_with
        {
            template <typename Iter1, typename Iter2>
            bool operator()(Iter1 begin1, Iter1 end1, Iter2 begin2, Iter2 end2)
            {
                auto r1 = boost::make_iterator_range(begin1, end1);
                auto r2 = boost::make_iterator_range(begin2, end2);

                return boost::starts_with(r1, r2, Comparer());
            }
        };

        /**
        * Iterator based ends_with
        */
        template <typename Comparer>
        struct ends_with
        {
            template <typename Iter1, typename Iter2>
            bool operator()(Iter1 begin1, Iter1 end1, Iter2 begin2, Iter2 end2)
            {
                auto r1 = boost::make_iterator_range(begin1, end1);
                auto r2 = boost::make_iterator_range(begin2, end2);

                return boost::ends_with(r1, r2, Comparer());
            }
        };

        // Custom Comparison
        // --------------------------------------------------------------------

        template <typename T>
        struct iequal_to
        {
            bool operator()(const T& a, const T& b)
            {
                static_assert(sizeof(T) == 0,
                    "iequal_to needs a specialized overload for type");
            }
        };

        /**
        * <summary>
        *   Binary predicate for comparing two wide characters case insensitively
        *   Does not handle all locales
        * </summary>
        */
        template <>
        struct iequal_to<wchar_t>
        {
            bool operator()(const wchar_t& a, const wchar_t& b) const
            {
                return towupper(a) == towupper(b);
            }
        };

        /**
        * <summary>
        *   Binary predicate for comparing two characters case insensitively
        *   Does not handle all locales
        * </summary>
        */
        template <>
        struct iequal_to<char>
        {
            bool operator()(const char& a, const char& b) const
            {
                return toupper(a) == toupper(b);
            }
        };

    } /* namespace comparers */

} /* namespace predicates */ } /* namespace krabs */