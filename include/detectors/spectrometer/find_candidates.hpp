// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "GeometryContainers.hpp"
#include "PhlexModuleProxy.hpp"
#include "detectors/spectrometer/SpectrometerCkf.hpp"
#include "detectors/spectrometer/SpectrometerMeasurements.hpp"
#include "detectors/spectrometer/generate_seeds.hpp"
#include "phlex/core/product_selector.hpp"

#include <optional>
#include <string>
#include <vector>

// One CKF candidate: the seed it grew from and the ordered measurement
// indices it picked up. Carrying the seed lets fit_track refit the candidate
// from the "track" layer and the spill's SpillContext alone.
struct CkfCandidate {
    SeedHitPair seed;
    std::vector<ActsExamples::Index> hits;
};

// Rebuilds a seed's BoundTrackParameters from its two hit indices.
inline std::optional<Acts::BoundTrackParameters> makeSeed(
    SeedHitPair const& pair, SpectrometerMeasurements const& measurements) {
    auto const& meas = measurements.measurements();
    return makeSeedFromHitPair(meas[pair.idx0], meas[pair.idx1]);
}

// find_candidates (seed layer, unfold -> "track" layer): takes one seed (a
// SeedHitPair) and the SpillContext of its spill, runs
// ckf.findCandidateHits() for that seed, and spawns one track-layer child
// data cell per candidate that passes the hit-count cut. A seed can yield
// zero, one or several children.
//
// An unfold nested under another unfold that also takes a higher-layer
// input needs phlex 0.5.0. phlex 0.4.1 never flushed the spill layer in
// that case (Framework-R-D/phlex#955).
void register_find_candidates(ModuleProxy const& m, phlex::experimental::identifier const& layer,
                              phlex::experimental::identifier const& seedLayer,
                              std::string const& trackLayerName);
