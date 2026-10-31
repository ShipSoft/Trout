// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <SHiP/TrackFitResult.hpp>
#include <cstdint>
#include <vector>

namespace SHiP::trout {

// A fitted track together with the hits it was built from, as passed from
// fit_seed to resolve_tracks. The hit ids aren't part of TrackFitResult, so
// they only live inside trout and are dropped at output.
struct TrackCandidate {
    SHiP::TrackFitResult fit;
    // Sorted ids (Measurement::hitId) of the track's hits — stable across
    // windows, so overlapping windows' tracks can be compared.
    std::vector<std::uint64_t> hitIds;
};

}  // namespace SHiP::trout
