// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "detectors/spectrometer/find_candidates.hpp"

#include "detectors/spectrometer/SpillContext.hpp"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace {

// The spectrometer has 4 stations — demand a candidate use all of them
// before it's worth a refit. Most CKF candidates are landing at only 1-2
// hits (well under this), which is a strong sign the crude straight-line
// seed / nearest-measurement selector (see SpectrometerCkf.hpp) is picking
// up unrelated-track hits rather than that this cut is too strict.
constexpr std::size_t kMinHitsPerTrack = 4;

// split_candidates' unfold "Object": walks find_candidates' output by index.
class CandidateSplitter {
   public:
    explicit CandidateSplitter(std::vector<CkfCandidate> const& candidates)
        : candidates_{candidates} {}

    std::size_t initial_value() const { return 0; }

    std::size_t size() const { return candidates_.size(); }

    CkfCandidate const& at(std::size_t k) const { return candidates_[k]; }

   private:
    std::vector<CkfCandidate> candidates_;
};

}  // namespace

void register_find_candidates(ModuleProxy const& m, phlex::experimental::identifier const& layer,
                              phlex::experimental::identifier const& seedLayer,
                              std::string const& trackLayerName) {
    m.transform(
         "find_candidates",
         [](SeedHitPair const& pair,
            std::shared_ptr<SpillContext> const& ctx) -> std::vector<CkfCandidate> {
             std::vector<CkfCandidate> candidates;
             if (!ctx || !ctx->measurements || !ctx->ckf)
                 return candidates;
             auto seed = makeSeed(pair, *ctx->measurements);
             if (!seed)
                 return candidates;
             for (auto& hits : ctx->ckf->findCandidateHits(*seed, *ctx->measurements)) {
                 if (hits.size() >= kMinHitsPerTrack)
                     candidates.push_back(CkfCandidate{pair, std::move(hits)});
             }
             return candidates;
         },
         phlex::concurrency::unlimited)
        .input_family(
            phlex::product_selector{
                .creator = "generate_seeds", .layer = seedLayer, .suffix = "seed_hit_pair"},
            phlex::product_selector{
                .creator = "prepare_measurements", .layer = layer, .suffix = "spill_context"})
        .output_product_suffixes("ckf_candidates");

    m.unfold<CandidateSplitter>(
         "split_candidates",
         [](CandidateSplitter const& obj, std::size_t k) { return k < obj.size(); },
         [](CandidateSplitter const& obj, std::size_t k) {
             return std::make_pair(k + 1, obj.at(k));
         },
         trackLayerName, phlex::concurrency::unlimited)
        .input_family(phlex::product_selector{
            .creator = "find_candidates", .layer = seedLayer, .suffix = "ckf_candidates"})
        .output_product_suffixes("ckf_candidate");
}
