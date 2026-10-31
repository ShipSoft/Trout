// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "detectors/spectrometer/prepare_measurements.hpp"

#include "acts_field_provider.hpp"
#include "acts_geometry_provider.hpp"
#include "detectors/spectrometer/SpillContext.hpp"

#include <Acts/Geometry/GeometryContext.hpp>

#include <SHiP/RecHit.hpp>
#include <SHiP/detectors/StrawTubesHit.hpp>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace SHiP::trout {

void register_prepare_measurements(ModuleProxy const& m,
                                   phlex::experimental::identifier const& layer,
                                   double seedMarginNs) {
    m.transform(
         "prepare_measurements",
         [seedMarginNs](
             std::shared_ptr<std::vector<SHiP::StrawTubesHit>> const& ip,
             std::shared_ptr<std::vector<std::uint64_t>> const& ids, TimeWindow const& window,
             std::shared_ptr<DetectorGeometry> const& detector,
             std::shared_ptr<DetectorField> const& field) -> std::shared_ptr<SpillContext> {
             auto ctx = std::make_shared<SpillContext>();
             ctx->window = window;
             ctx->seedTimeLo = window.own_lo - seedMarginNs;
             ctx->seedTimeHi = window.own_hi + seedMarginNs;

             // A track owned near the end of the slice must still fit inside the window.
             if (std::isfinite(window.own_hi) &&
                 (window.hi - window.own_hi) * Acts::UnitConstants::ns < strawHitTimeSelection)
                 throw std::invalid_argument(
                     "window margin is smaller than the straw hit time selection");

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
