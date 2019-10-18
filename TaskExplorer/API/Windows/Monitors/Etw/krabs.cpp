// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include "stdafx.h"

#include "krabs.hpp"

// Note: This file exists to define the schema_locator
// cache static variable. Due to issues with C++14 magic
// static and C++/CLI, we're unable to rely solely on
// using magic static.

// DO NOT ADD THINGS TO THIS FILE

namespace krabs {
    schema_locator schema_locator::singleton_;
}