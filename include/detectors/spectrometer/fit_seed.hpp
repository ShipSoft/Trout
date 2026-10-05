// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "PhlexModuleProxy.hpp"
#include "phlex/core/product_selector.hpp"

// fit_seed (seed layer, transform): takes one seed (a SeedHitPair) and the
// SpillContext of the spill it came from (a spill-layer input, which phlex
// hands to every seed of that spill), runs ckf.findCandidateHits() for just
// that seed, and refits every surviving
// CKF candidate branch (SpectrometerCkf::refit(), a real
// forward+backward-smoothed Acts::KalmanFitter — the CKF exploration itself
// has no smoother, see SpectrometerCkf.hpp) in a plain loop, returning all
// of them as one track_fit_result vector.
//
// A seed can yield zero, one, or several CKF candidates, and fit_seed loops
// over them in plain C++ rather than unfolding them into a third "track"
// layer. The loop was originally a workaround: phlex 0.3.2 crashed when an
// unfold produced no children for some parents while a product from a
// higher layer was also fed into those children, which is what a chained
// track unfold does. phlex 0.4.1 handles that combination, so a track layer
// is now possible if per-track data cells are wanted.
void register_fit_seed(ModuleProxy const& m, phlex::experimental::identifier const& layer,
                       phlex::experimental::identifier const& seedLayer);
