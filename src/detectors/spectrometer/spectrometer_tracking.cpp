// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// spectrometer_tracking.cpp — Phlex module plugin
//
// Splits pattern recognition into phlex-native stages so per-seed work is
// parallelized by the framework's own scheduling, instead of a single
// serial loop inside one transform (contrast keep_tracks.cpp, which does
// everything for a whole spill in one call). Each stage's implementation
// lives in its own file — this one just wires them together, in order:
//
//   1. prepare_measurements.{hpp,cpp} — spill layer, transform.
//        straw_tubes_hits + the geometry and field resources -> SpillContext,
//        built once per spill and shared read-only by every seed below.
//   2. generate_seeds.{hpp,cpp} — spill layer, unfold -> "seed" layer.
//        Seeding: pairs station-1/station-2 hits into candidate seeds.
//   3. fit_seed.{hpp,cpp} — seed layer, transform.
//        Finding (CKF pattern recognition) + fitting (KalmanFitter) for
//        one seed at a time.
//   4. resolve_tracks.{hpp,cpp} — seed layer -> spill layer, fold +
//        transform. Gathers the spill's tracks, removes duplicates and keeps
//        the ones this (overlapping) time window owns.
//
// See each header for what that stage actually does, and SpectrometerCkf.hpp
// for the underlying ACTS track-finding/fitting engine all of them share.
//
// Configuration:
//   max_shared_hits (default 1) tracks sharing more hits than this are
//                   duplicates, and only the best of them is kept.
// The straw-tube time constants (hit time selection, seed margin, and so the
// straw hits each window gets) are fixed in io/StrawTiming.hpp.

#include "detectors/spectrometer/fit_seed.hpp"
#include "detectors/spectrometer/generate_seeds.hpp"
#include "detectors/spectrometer/prepare_measurements.hpp"
#include "detectors/spectrometer/resolve_tracks.hpp"
#include "phlex/module.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

PHLEX_REGISTER_ALGORITHMS(m, config) {
    auto const layerName = config.get<std::string>("layer");
    auto const layer = phlex::experimental::identifier{layerName};
    auto const seedLayerName = std::string{"seed"};
    auto const seedLayer = phlex::experimental::identifier{seedLayerName};
    auto const maxSharedHits = config.get<int>("max_shared_hits", 1);
    if (maxSharedHits < 0)
        throw std::invalid_argument("spectrometer_tracking: max_shared_hits must be >= 0");

    SHiP::trout::register_prepare_measurements(m, layer);
    SHiP::trout::register_generate_seeds(m, layer, seedLayerName);
    SHiP::trout::register_fit_seed(m, layer, seedLayer);
    SHiP::trout::register_resolve_tracks(m, layerName, seedLayer,
                                         static_cast<std::size_t>(maxSharedHits));
}
