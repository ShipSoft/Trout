// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "detectors/spectrometer/fit_seed.hpp"

#include "detectors/spectrometer/SpectrometerCkf.hpp"
#include "detectors/spectrometer/SpillContext.hpp"
#include "detectors/spectrometer/generate_seeds.hpp"

#include <SHiP/TrackFitResult.hpp>
#include <cstddef>
#include <memory>
#include <vector>

namespace {

// The spectrometer has 4 stations — demand a candidate use all of them
// before it's worth a refit. Most CKF candidates are landing at only 1-2
// hits (well under this), which is a strong sign the crude straight-line
// seed / nearest-measurement selector (see SpectrometerCkf.hpp) is picking
// up unrelated-track hits rather than that this cut is too strict.
constexpr std::size_t kMinHitsPerTrack = 4;

}  // namespace

void register_fit_seed(ModuleProxy const& m, phlex::experimental::identifier const& layer,
                       phlex::experimental::identifier const& seedLayer) {
    m.transform(
         "fit_seed",
         [](SeedHitPair const& pair,
            std::shared_ptr<SpillContext> const& ctx) -> std::vector<SHiP::TrackFitResult> {
             std::vector<SHiP::TrackFitResult> results;
             if (!ctx || !ctx->measurements || !ctx->ckf)
                 return results;
             auto const& m0 = ctx->measurements->measurements()[pair.idx0];
             auto const& m1 = ctx->measurements->measurements()[pair.idx1];
             auto seed = makeSeedFromHitPair(m0, m1);
             if (!seed)
                 return results;
             auto candidates = ctx->ckf->findCandidateHits(*seed, *ctx->measurements);
             for (auto const& hits : candidates) {
                 if (hits.size() < kMinHitsPerTrack)
                     continue;
                 results.push_back(ctx->ckf->refit(*seed, *ctx->measurements, hits));
             }
             return results;
         },
         phlex::concurrency::unlimited)
        .input_family(
            phlex::product_selector{
                .creator = "generate_seeds", .layer = seedLayer, .suffix = "seed_hit_pair"},
            phlex::product_selector{
                .creator = "prepare_measurements", .layer = layer, .suffix = "spill_context"})
        .output_product_suffixes("track_fit_result");
}
