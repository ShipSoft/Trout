// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// spectrometer_tracking.cpp — Phlex module plugin
//
// Splits pattern recognition into phlex-native stages so per-seed and
// per-candidate work is parallelized by the framework's own scheduling,
// instead of a single serial loop inside one transform (contrast
// keep_tracks.cpp, which does everything for a whole spill in one call).
// Each stage's implementation lives in its own file — this one just wires
// them together, in order:
//
//   1. prepare_measurements.{hpp,cpp} — spill layer, transform.
//        straw_tubes_hits + tracking_geometry -> SpillContext, built once
//        per spill and shared read-only by every seed and track below.
//   2. generate_seeds.{hpp,cpp} — spill layer, unfold -> "seed" layer.
//        Seeding: pairs station-1/station-2 hits into candidate seeds.
//   3. find_candidates.{hpp,cpp} — seed layer, transform, then unfold ->
//        "track" layer. Finding: CKF pattern recognition for one seed,
//        then one child per candidate that passes the hit-count cut.
//   4. fit_track.{hpp,cpp} — track layer, transform.
//        Fitting: KalmanFitter refit of one candidate.
//   5. count_tracks.{hpp,cpp} — fold of the track layer into the spill
//        layer, for the track multiplicity.
//
// See each header for what that stage actually does, and SpectrometerCkf.hpp
// for the underlying ACTS track-finding/fitting engine all of them share.

#include "detectors/spectrometer/count_tracks.hpp"
#include "detectors/spectrometer/find_candidates.hpp"
#include "detectors/spectrometer/fit_track.hpp"
#include "detectors/spectrometer/generate_seeds.hpp"
#include "detectors/spectrometer/prepare_measurements.hpp"
#include "phlex/module.hpp"

#include <string>

PHLEX_REGISTER_ALGORITHMS(m, config) {
    auto const layerName = config.get<std::string>("layer");
    auto const layer = phlex::experimental::identifier{layerName};
    auto const seedLayerName = std::string{"seed"};
    auto const seedLayer = phlex::experimental::identifier{seedLayerName};
    auto const trackLayerName = std::string{"track"};
    auto const trackLayer = phlex::experimental::identifier{trackLayerName};

    register_prepare_measurements(m, layer);
    register_generate_seeds(m, layer, seedLayerName);
    register_find_candidates(m, layer, seedLayer, trackLayerName);
    register_fit_track(m, layer, trackLayer);
    register_count_tracks(m, trackLayer, layerName);
}
