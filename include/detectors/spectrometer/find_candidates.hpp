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

// Registers two stages that together give each CKF candidate its own
// "track"-layer data cell:
//
//   find_candidates (seed layer, transform): takes one seed (a SeedHitPair)
//     and the SpillContext of its spill, runs ckf.findCandidateHits() for
//     that seed, and returns the candidates that pass the hit-count cut.
//   split_candidates (seed layer, unfold -> track layer): one child per
//     candidate. A seed can yield zero, one or several children.
//
// The CKF can't run inside the unfold itself: in phlex 0.4.1, an unfold
// below another unfold whose Object also takes a product from a higher layer
// (here the SpillContext) stops the framework from ever flushing that
// higher layer. Every join on a spill-layer product then holds it until the
// end of the job, and folds into the spill never emit. An unfold with just
// the seed-layer input doesn't have the problem.
void register_find_candidates(ModuleProxy const& m, phlex::experimental::identifier const& layer,
                              phlex::experimental::identifier const& seedLayer,
                              std::string const& trackLayerName);
