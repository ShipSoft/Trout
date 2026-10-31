// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "detectors/spectrometer/resolve_tracks.hpp"

#include "detectors/spectrometer/SpillContext.hpp"

#include <tuple>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>

namespace SHiP::trout {

namespace {

std::size_t count_shared(std::vector<std::uint64_t> const& a, std::vector<std::uint64_t> const& b) {
    // Both sorted (see TrackCandidate::hitIds).
    std::size_t shared = 0;
    auto ia = a.begin();
    auto ib = b.begin();
    while (ia != a.end() && ib != b.end()) {
        if (*ia < *ib) {
            ++ia;
        } else if (*ib < *ia) {
            ++ib;
        } else {
            ++shared;
            ++ia;
            ++ib;
        }
    }
    return shared;
}

// Infinity rather than NaN for failed fits, so the sort keeps a strict weak
// ordering.
double chi2_per_ndf(SHiP::TrackFitResult const& f) {
    double const r = f.ndf > 0 ? f.chi2 / f.ndf : std::numeric_limits<double>::infinity();
    return std::isnan(r) ? std::numeric_limits<double>::infinity() : r;
}

// Whether a ranks before b (see resolve_tracks.hpp).
bool better(TrackCandidate const& a, TrackCandidate const& b) {
    // fit_status 0 is success, so lower is better; more hits is better.
    auto const ka = std::tuple{a.fit.fit_status, b.hitIds.size(), chi2_per_ndf(a.fit)};
    auto const kb = std::tuple{b.fit.fit_status, a.hitIds.size(), chi2_per_ndf(b.fit)};
    if (ka != kb)
        return ka < kb;
    if (a.hitIds != b.hitIds)
        return a.hitIds < b.hitIds;
    return std::tie(a.fit.q_over_p, a.fit.phi, a.fit.theta) <
           std::tie(b.fit.q_over_p, b.fit.phi, b.fit.theta);
}

}  // namespace

std::vector<SHiP::TrackFitResult> resolve_tracks(std::vector<TrackCandidate> const& candidates,
                                                 TimeWindow const& window,
                                                 std::size_t maxSharedHits) {
    std::vector<std::size_t> order(candidates.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::sort(
        order, [&](std::size_t a, std::size_t b) { return better(candidates[a], candidates[b]); });

    std::vector<TrackCandidate const*> accepted;
    for (auto const i : order) {
        auto const& c = candidates[i];
        bool const duplicate = std::ranges::any_of(accepted, [&](TrackCandidate const* a) {
            return a->hitIds == c.hitIds || count_shared(a->hitIds, c.hitIds) > maxSharedHits;
        });
        if (!duplicate)
            accepted.push_back(&c);
    }

    std::vector<SHiP::TrackFitResult> tracks;
    for (auto const* a : accepted) {
        if (window.owns(a->fit.time))
            tracks.push_back(a->fit);
    }
    return tracks;
}

void register_resolve_tracks(ModuleProxy const& m, std::string const& layerName,
                             phlex::experimental::identifier const& seedLayer,
                             std::size_t maxSharedHits) {
    auto const layer = phlex::experimental::identifier{layerName};

    m.fold(
         "collect_tracks",
         [](std::vector<TrackCandidate>& all, std::vector<TrackCandidate> const& fromSeed) {
             all.insert(all.end(), fromSeed.begin(), fromSeed.end());
         },
         phlex::concurrency::serial, layerName)
        .input_family(phlex::product_selector{
            .creator = "fit_seed", .layer = seedLayer, .suffix = "track_candidates"})
        .output_product_suffixes("track_candidates");

    m.transform(
         "resolve_tracks",
         [maxSharedHits](
             std::vector<TrackCandidate> const& candidates,
             std::shared_ptr<SpillContext> const& ctx) -> std::vector<SHiP::TrackFitResult> {
             if (!ctx)
                 return {};
             return resolve_tracks(candidates, ctx->window, maxSharedHits);
         },
         phlex::concurrency::unlimited)
        .input_family(
            phlex::product_selector{
                .creator = "collect_tracks", .layer = layer, .suffix = "track_candidates"},
            phlex::product_selector{
                .creator = "prepare_measurements", .layer = layer, .suffix = "spill_context"})
        .output_product_suffixes("track_fit_result");
}

}  // namespace SHiP::trout
