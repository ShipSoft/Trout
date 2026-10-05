// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "PhlexModuleProxy.hpp"
#include "phlex/core/product_selector.hpp"

#include <string>

// count_tracks (fold over the track layer into `partitionLayer`): counts the
// fitted tracks under each partition data cell, for the track multiplicity
// histogram. A partition without any tracks still yields a count of 0.
//
// The partition is the spill for now. The count that matters for physics is
// tracks per reconstructed event window, and a spill will hold several of
// those. Once a window layer exists between the spill and the seeds, this
// fold should partition by it instead.
void register_count_tracks(ModuleProxy const& m, phlex::experimental::identifier const& trackLayer,
                           std::string const& partitionLayer);
