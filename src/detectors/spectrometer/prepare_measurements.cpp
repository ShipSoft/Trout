// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "detectors/spectrometer/prepare_measurements.hpp"

#include "acts_field_provider.hpp"
#include "acts_geometry_provider.hpp"
#include "detectors/spectrometer/HitUncertainty.hpp"
#include "detectors/spectrometer/SpillContext.hpp"
#include "io/StrawTiming.hpp"

#include <Acts/Geometry/GeometryContext.hpp>

#include <SHiP/RecHit.hpp>
#include <SHiP/detectors/StrawTubesHit.hpp>
#include <cstdint>
#include <memory>
#include <vector>

namespace SHiP::trout {

// A track seeded anywhere in the seed range (owned slice +- the seed margin)
// can use hits up to the hit time selection beyond its seed, on either side,
// so the straw tubes' padded range must cover all of them.
static_assert(kStrawTubesPadding.before_ns >= kStrawSeedMarginNs + kStrawHitTimeSelectionNs &&
                  kStrawTubesPadding.after_ns >= kStrawSeedMarginNs + kStrawHitTimeSelectionNs,
              "straw-tube time padding doesn't cover every hit a margin-seeded track can use");

void register_prepare_measurements(ModuleProxy const& m,
                                   phlex::experimental::identifier const& layer) {
    m.transform(
         "prepare_measurements",
         [](std::shared_ptr<std::vector<SHiP::StrawTubesHit>> const& ip,
            std::shared_ptr<std::vector<std::uint64_t>> const& ids, TimeWindow const& window,
            std::shared_ptr<DetectorGeometry> const& detector,
            std::shared_ptr<DetectorField> const& field) -> std::shared_ptr<SpillContext> {
             auto ctx = std::make_shared<SpillContext>();
             ctx->window = window;
             ctx->seedTimeLo = window.own_lo - kStrawSeedMarginNs;
             ctx->seedTimeHi = window.own_hi + kStrawSeedMarginNs;

             if (!ip || !ids || !detector || !detector->trackingGeometry || !field || !field->field)
                 return ctx;

             std::vector<SHiP::RecHit> hits;
             hits.reserve(ip->size());
             for (auto const& h : *ip)
                 hits.push_back(h.rec_hit);

             auto gctx = Acts::GeometryContext::dangerouslyDefaultConstruct();
             ctx->measurements = std::make_shared<SpectrometerMeasurements>(
                 *detector->trackingGeometry, gctx, hits, *ids);
             ctx->ckf = std::make_shared<SpectrometerCkf>(detector->trackingGeometry, field->field);
             return ctx;
         },
         phlex::concurrency::unlimited)
        .input_family(
            phlex::product_selector{
                .creator = "rntuple_source", .layer = layer, .suffix = "straw_tubes_hits"},
            phlex::product_selector{
                .creator = "rntuple_source", .layer = layer, .suffix = "straw_tubes_hit_ids"},
            phlex::product_selector{
                .creator = "rntuple_source", .layer = layer, .suffix = "window"},
            phlex::product_selector{
                .creator = "tracking_geometry", .layer = "job", .suffix = "detector"},
            phlex::product_selector{
                .creator = "spectrometer_field", .layer = "job", .suffix = "field"})
        .output_product_suffixes("spill_context");
}

}  // namespace SHiP::trout
