// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#define INITGUID

#include <vector>
#include <algorithm>

#include "../compiler_check.hpp"
#include "../guid.hpp"
#include "../schema.hpp"
#include "../parser.hpp"

#include <evntrace.h>
#include <evntcons.h>
#include <memory>

namespace krabs { namespace testing {

    /**
     * <summary>
     *   Represents a property that is faked -- one that is built by hand for the
     *   purpose of testing event reaction code.
     * </summary>
     */
    class synth_record {
    public:

        /**
         * <summary>
         *   Constructs a synthetic property, given a partially filled
         *   EVENT_RECORD and a packed sequence of bytes that represent the
         *   event's user data.
         * </summary>
         * <remarks>
         *   This class should not be directly instantiated -- an record_builder
         *   should return this with its `pack` methods.
         * </remarks>
         */
        synth_record(const EVENT_RECORD &record,
                     const std::vector<BYTE> &user_data);

        /**
         * <summary>
         *   Copies a synth_record and updates the pointers
         *   in the EVENT_RECORD appropriately.
         * </summary>
         */
        synth_record(const synth_record& other);

        /**
         * <summary>
         *   Moves a synth_record into a new instance.
         * </summary>
         */
        synth_record(synth_record&& other);

        /**
         * <summary>
         *   Assigns a synth_record to another.
         * </summary>
         * <remarks>by value to take advantage of move ctor</remarks>
         */
        synth_record& operator=(synth_record);

        /**
         * <summary>
         *   Allows implicit casts to an EVENT_RECORD.
         * </summary>
         */
         operator const EVENT_RECORD&() const;

        /**
         * <summary>
         *   Swaps two synth_records.
         * </summary>
         */
        friend void swap(synth_record& left, synth_record& right)
        {
            using std::swap; // ADL

            swap(left.record_, right.record_);
            swap(left.data_, right.data_);
        }

    private:
        synth_record()
            : record_()
            , data_() { }

        EVENT_RECORD record_;
        std::vector<BYTE> data_;
    };

    // Implementation
    // ------------------------------------------------------------------------

    inline synth_record::synth_record(
        const EVENT_RECORD &record,
        const std::vector<BYTE> &user_data)
    : record_(record)
    , data_(user_data)
    {
        if (data_.size() > 0) {
            record_.UserData = &data_[0];
        } else {
            record_.UserData = 0;
        }

        record_.UserDataLength = static_cast<USHORT>(data_.size());
    }

    inline synth_record::synth_record(const synth_record& other)
        : synth_record(other.record_, other.data_)
    { }

    inline synth_record::synth_record(synth_record&& other)
        : synth_record()
    {
        swap(*this, other);
    }

    inline synth_record& synth_record::operator=(synth_record other)
    {
        swap(*this, other);
        return *this;
    }

    inline synth_record::operator const EVENT_RECORD&() const
    {
        return record_;
    }

} /* namespace testing */ } /* namespace krabs */
