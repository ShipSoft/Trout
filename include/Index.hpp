// This file is part of the ACTS project.
//
// Copyright (C) 2016 CERN for the benefit of the ACTS project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// Trimmed copy from the ACTS Examples framework: only the Index alias is
// used here; the IndexMultimap helpers were dropped.

#pragma once

#include <cstdint>

namespace ActsExamples {

/// Index type to reference elements in a container.
///
/// We do not expect to have more than 2^32 elements in any given container so a
/// fixed sized integer type is sufficient.
using Index = std::uint32_t;

}  // namespace ActsExamples
