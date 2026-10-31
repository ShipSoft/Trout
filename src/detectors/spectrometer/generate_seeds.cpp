// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "detectors/spectrometer/generate_seeds.hpp"

#include "detectors/spectrometer/HitUncertainty.hpp"

#include <Acts/Definitions/Units.hpp>
#include <Acts/Geometry/GeometryIdentifier.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <utility>
#include <vector>

namespace SHiP::trout {

namespace {

// Minimum cos(angle from the beam/z axis) a station1->station2 pair's
// implied direction must have to be considered a plausible seed. With
// hundreds of hits per station in a busy spill, blind time-coincidence
// pairing alone produces massive combinatorial background — diagnostics
// showed "seed" directions up to ~65 degrees off-axis, landing tens of
// meters away from station 3 (which is only ever a few meters across).
// 0.9 (~26 degrees half-angle) is a first-pass guess, not a measured
// acceptance cut — tighten or loosen based on how it affects yield.
constexpr double kMinForwardCosine = 0.9;

// unfold's "Object": built once per spill from SpillContext, holds the
// station-1/station-2 hit index lists, and does the "find the next
// time-coincident, forward-going pair" scan itself. phlex's unfold calls
// its predicate and unfold function once per *produced* child — there's no
// built-in "skip this one" — so to apply the cuts without spawning a child
// per rejected pair, the running state is "the next known-valid flat index
// (or none)", pre-scanned here and after each produced seed.
class SeedGenObject {
   public:
    explicit SeedGenObject(std::shared_ptr<SpillContext> const& ctx) : ctx_{ctx} {
        if (!ctx_ || !ctx_->measurements)
            return;

        // Station geometry ids assigned in acts_geometry_provider.cpp: volume 1,
        // sensitive = station index (1-based).
        auto const station1Id = Acts::GeometryIdentifier{}.withVolume(1).withSensitive(1);
        auto const station2Id = Acts::GeometryIdentifier{}.withVolume(1).withSensitive(2);
        // Only seed from station-1 hits near what this window owns: the rest
        // are seeded by the windows owning them (resolve_tracks keeps each
        // track only in its owner), and seeding them here too would just
        // repeat the CKF work in every overlapping window.
        auto const& meas = ctx_->measurements->measurements();
        double const seedLo = ctx_->seedTimeLo * Acts::UnitConstants::ns;
        double const seedHi = ctx_->seedTimeHi * Acts::UnitConstants::ns;
        for (auto const& sl :
             ActsExamples::selectModule(ctx_->measurements->sourceLinks(), station1Id)) {
            double const t = meas[sl.index()].time;
            if (t >= seedLo && t < seedHi)
                hits1_.push_back(sl.index());
        }
        for (auto const& sl :
             ActsExamples::selectModule(ctx_->measurements->sourceLinks(), station2Id))
            hits2_.push_back(sl.index());
        log_counts();
    }

    std::optional<std::size_t> initial_value() const { return find_next_valid(0); }

    // Scans forward from flat index `from` (over hits1_.size() x hits2_.size())
    // for the next pair within the time-coincidence window AND whose implied
    // direction is forward-going enough (see kMinForwardCosine), or nullopt
    // if none remain. Shared by initial_value() and the unfold function.
    std::optional<std::size_t> find_next_valid(std::size_t from) const {
        if (hits1_.empty() || hits2_.empty())
            return std::nullopt;
        auto const& meas = ctx_->measurements->measurements();
        auto const total = hits1_.size() * hits2_.size();

        for (std::size_t k = from; k < total; ++k) {
            auto const i = k / hits2_.size();
            auto const j = k % hits2_.size();
            auto const& m0 = meas[hits1_[i]];
            auto const& m1 = meas[hits2_[j]];
            double const dt = std::abs(
                (m1.time - m0.time) -
                (m1.global - m0.global).norm());  // This is the time difference with the expected
                                                  // from straight line flight at c
            if (dt > strawHitTimeSelection)
                continue;
            Acts::Vector3 const dir = (m1.global - m0.global).normalized();
            if (dir.z() >= kMinForwardCosine)
                return k;
        }
        return std::nullopt;
    }

    SeedHitPair seedAt(std::size_t k) const {
        auto const i = k / hits2_.size();
        auto const j = k % hits2_.size();
        return SeedHitPair{hits1_[i], hits2_[j]};
    }

   private:
    // Diagnostic: how many seeds this window can produce. Counting them
    // exactly needs the full hits1 x hits2 scan, so count the time-coincident
    // pairs instead — an upper bound, since it skips the forward cut — in
    // n1 log n2.
    void log_counts() const {
        auto const& meas = ctx_->measurements->measurements();
        std::vector<double> times2;
        times2.reserve(hits2_.size());
        for (auto const idx : hits2_)
            times2.push_back(meas[idx].time);
        std::ranges::sort(times2);

        std::uint64_t pairs = 0;
        for (auto const idx : hits1_) {
            double const t = meas[idx].time;
            pairs += static_cast<std::uint64_t>(
                std::ranges::upper_bound(times2, t + strawHitTimeSelection) -
                std::ranges::lower_bound(times2, t - strawHitTimeSelection));
        }
        auto const& w = ctx_->window;
        spdlog::info(
            "generate_seeds: window {} [{}, {}) ns: {} measurements, {} station-1 hits in seed "
            "range, {} station-2 hits, {} time-coincident pairs (upper bound on seeds)",
            w.index, w.lo, w.hi, meas.size(), hits1_.size(), hits2_.size(), pairs);
    }

    std::shared_ptr<SpillContext> ctx_;
    std::vector<ActsExamples::Index> hits1_;
    std::vector<ActsExamples::Index> hits2_;
};

}  // namespace

void register_generate_seeds(ModuleProxy const& m, phlex::experimental::identifier const& layer,
                             std::string const& seedLayerName) {
    m.unfold<SeedGenObject>(
         "generate_seeds",
         [](SeedGenObject const&, std::optional<std::size_t> next) { return next.has_value(); },
         [](SeedGenObject const& obj, std::optional<std::size_t> current) {
             auto const seed = obj.seedAt(*current);
             auto const next = obj.find_next_valid(*current + 1);
             return std::make_pair(next, seed);
         },
         seedLayerName, phlex::concurrency::unlimited)
        .input_family(phlex::product_selector{
            .creator = "prepare_measurements", .layer = layer, .suffix = "spill_context"})
        .output_product_suffixes("seed_hit_pair");
}

}  // namespace SHiP::trout
