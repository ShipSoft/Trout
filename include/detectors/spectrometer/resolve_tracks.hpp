// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "PhlexModuleProxy.hpp"
#include "TrackCandidate.hpp"
#include "io/TimeWindow.hpp"
#include "phlex/core/product_selector.hpp"

#include <SHiP/TrackFitResult.hpp>
#include <cstddef>
#include <string>
#include <vector>

namespace SHiP::trout {

// Removes duplicate tracks among one window's candidates and keeps those the
// window owns:
//
//   1. Rank the candidates: successful fits first, then more hits, then lower
//      chi2/ndf (remaining ties broken by hit ids and track parameters, so the
//      result doesn't depend on the order the seeds were processed in).
//   2. Greedily accept them in that order, rejecting any candidate that
//      shares more than maxSharedHits hits with one already accepted, or has
//      exactly the same hits as one.
//   3. Of the accepted tracks, return those whose time (earliest hit) the
//      window owns.
//
// Step 2 runs over every track the window found, including ones another
// window owns, so that a clash across a window boundary is seen — and
// decided the same way — by both windows.
std::vector<SHiP::TrackFitResult> resolve_tracks(std::vector<TrackCandidate> const& candidates,
                                                 TimeWindow const& window,
                                                 std::size_t maxSharedHits);

// collect_tracks (fold, seed layer -> spill layer): gathers every seed's
// track_candidates into one vector per spill (time window).
// resolve_tracks (spill layer, transform): that vector + the SpillContext's
// window -> the window's final track_fit_result vector, via resolve_tracks()
// above.
void register_resolve_tracks(ModuleProxy const& m, std::string const& layerName,
                             phlex::experimental::identifier const& seedLayer,
                             std::size_t maxSharedHits);

}  // namespace SHiP::trout
