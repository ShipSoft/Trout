// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "detectors/spectrometer/count_tracks.hpp"

#include <SHiP/TrackFitResult.hpp>
#include <atomic>
#include <cstddef>

void register_count_tracks(ModuleProxy const& m, phlex::experimental::identifier const& trackLayer,
                           std::string const& partitionLayer) {
    // The fold body runs concurrently for tracks of the same partition, so
    // the accumulator is atomic; phlex sends it on as a plain std::size_t.
    m.fold(
         "count_tracks",
         [](std::atomic<std::size_t>& count, SHiP::TrackFitResult const&) {
             count.fetch_add(1, std::memory_order_relaxed);
         },
         phlex::concurrency::unlimited, partitionLayer)
        .input_family(phlex::product_selector{
            .creator = "fit_track", .layer = trackLayer, .suffix = "track_fit_result"})
        .output_product_suffixes("track_count");
}
