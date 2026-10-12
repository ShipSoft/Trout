// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "GeometryContainers.hpp"
#include "PhlexModuleProxy.hpp"
#include "SpillContext.hpp"
#include "phlex/core/product_selector.hpp"

#include <string>

// generate_seeds' per-child output, a single seed's identity: two hit
// indices into SpillContext::measurements (one at station 1, one at station
// 2). find_candidates takes it together with the spill's SpillContext and
// rebuilds the actual BoundTrackParameters via makeSeed.
struct SeedHitPair {
    ActsExamples::Index idx0{0};
    ActsExamples::Index idx1{0};
};

// generate_seeds (spill layer, unfold): walks every station-1/station-2 hit
// pair in the spill, applying a time-coincidence + forward-direction cut,
// and spawns one "seed"-layer child data cell per surviving pair. No static
// `total` is declared for the "seed" layer — phlex::unfold sizes it
// dynamically at runtime.
void register_generate_seeds(ModuleProxy const& m, phlex::experimental::identifier const& layer,
                             std::string const& seedLayerName);
