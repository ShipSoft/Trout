// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "detectors/spectrometer/fit_track.hpp"

#include "detectors/spectrometer/SpillContext.hpp"
#include "detectors/spectrometer/find_candidates.hpp"

#include <SHiP/TrackFitResult.hpp>
#include <memory>

void register_fit_track(ModuleProxy const& m, phlex::experimental::identifier const& layer,
                        phlex::experimental::identifier const& trackLayer) {
    m.transform(
         "fit_track",
         [](CkfCandidate const& candidate,
            std::shared_ptr<SpillContext> const& ctx) -> SHiP::TrackFitResult {
             // find_candidates only emits candidates whose spill context and
             // seed were valid, so both are known good here.
             auto const seed = makeSeed(candidate.seed, *ctx->measurements);
             return ctx->ckf->refit(*seed, *ctx->measurements, candidate.hits);
         },
         phlex::concurrency::unlimited)
        .input_family(
            phlex::product_selector{
                .creator = "split_candidates", .layer = trackLayer, .suffix = "ckf_candidate"},
            phlex::product_selector{
                .creator = "prepare_measurements", .layer = layer, .suffix = "spill_context"})
        .output_product_suffixes("track_fit_result");
}
