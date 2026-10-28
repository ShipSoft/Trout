// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "detectors/spectrometer/fit_seed.hpp"

#include "detectors/spectrometer/SpectrometerCkf.hpp"
#include "detectors/spectrometer/SpillContext.hpp"
#include "detectors/spectrometer/TrackCandidate.hpp"
#include "detectors/spectrometer/generate_seeds.hpp"

#include <Acts/Definitions/Units.hpp>

#include <SHiP/TrackFitResult.hpp>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace SHiP::trout {

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
            std::shared_ptr<SpillContext> const& ctx) -> std::vector<TrackCandidate> {
             std::vector<TrackCandidate> results;
             if (!ctx || !ctx->measurements || !ctx->ckf)
                 return results;
             auto const& meas = ctx->measurements->measurements();
             auto const& m0 = meas[pair.idx0];
             auto const& m1 = meas[pair.idx1];
             auto seed = makeSeedFromHitPair(m0, m1);
             if (!seed)
                 return results;
             auto candidates = ctx->ckf->findCandidateHits(*seed, *ctx->measurements);
             for (auto const& hits : candidates) {
                 if (hits.size() < kMinHitsPerTrack)
                     continue;
                 TrackCandidate candidate{ctx->ckf->refit(*seed, *ctx->measurements, hits), {}};
                 // The track's time is its earliest hit's, which is also what
                 // resolve_tracks assigns it to a window by.
                 double earliest = std::numeric_limits<double>::infinity();
                 candidate.hitIds.reserve(hits.size());
                 for (auto const idx : hits) {
                     candidate.hitIds.push_back(meas[idx].hitId);
                     earliest = std::min(earliest, meas[idx].time);
                 }
                 std::ranges::sort(candidate.hitIds);
                 candidate.fit.time = earliest / Acts::UnitConstants::ns;
                 results.push_back(std::move(candidate));
             }
             return results;
         },
         phlex::concurrency::unlimited)
        .input_family(
            phlex::product_selector{
                .creator = "generate_seeds", .layer = seedLayer, .suffix = "seed_hit_pair"},
            phlex::product_selector{
                .creator = "prepare_measurements", .layer = layer, .suffix = "spill_context"})
        .output_product_suffixes("track_candidates");
}

}  // namespace SHiP::trout
