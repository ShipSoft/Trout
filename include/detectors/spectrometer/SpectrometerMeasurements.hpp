// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// SpectrometerMeasurements.hpp — builds the per-surface measurement and
// source-link containers a CombinatorialKalmanFilter needs, from a flat
// vector of RecHit and a real Acts::TrackingGeometry (see
// acts_geometry_provider.cpp). Unlike ToyDetector, every hit is kept — not
// just one per station — since pattern recognition (not this class) is what
// decides which hits belong to which candidate track.

#pragma once

#include "GeometryContainers.hpp"
#include "HitUncertainty.hpp"
#include "Index.hpp"
#include "IndexSourceLink.hpp"

#include <Acts/Definitions/Algebra.hpp>
#include <Acts/Definitions/Units.hpp>
#include <Acts/EventData/SourceLink.hpp>
#include <Acts/Geometry/GeometryContext.hpp>
#include <Acts/Geometry/GeometryIdentifier.hpp>
#include <Acts/Geometry/TrackingGeometry.hpp>
#include <Acts/Surfaces/Surface.hpp>
#include <Acts/Utilities/CalibrationContext.hpp>

#include <SHiP/RecHit.hpp>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

class SpectrometerMeasurements {
   public:
    struct Measurement {
        // Non-owning; points into the TrackingGeometry that SpillContext
        // keeps alive alongside this container (see SpillContext.hpp).
        const Acts::Surface* surface = nullptr;
        Acts::Vector3 global;  // kept alongside local for seed construction
        Acts::Vector2 local;
        Acts::SquareMatrix2 covariance;
        double time;  // ns, for the time-coincidence cut in seeding
    };

    // zWindowMm: a hit is assigned to the nearest station surface only if
    // within this z distance — the same tolerance selectOneHitPerStation used,
    // now applied per-hit instead of picking a single best hit per station.
    // The same value is reused as the on-surface tolerance for globalToLocal,
    // since both express "how far off the station plane a hit may sit".
    SpectrometerMeasurements(const Acts::TrackingGeometry& geometry,
                             const Acts::GeometryContext& gctx,
                             const std::vector<SHiP::RecHit>& hits, double zWindowMm = 510.0) {
        double const zWindow = zWindowMm * Acts::UnitConstants::mm;
        std::vector<std::pair<const Acts::Surface*, double>> stationSurfaces;
        geometry.visitSurfaces(
            [&](const Acts::Surface* s) { stationSurfaces.emplace_back(s, s->center(gctx).z()); });

        for (auto const& hit : hits) {
            Acts::Vector3 const global(hit.position[0] * Acts::UnitConstants::mm,
                                       hit.position[1] * Acts::UnitConstants::mm,
                                       hit.position[2] * Acts::UnitConstants::mm);

            const Acts::Surface* best = nullptr;
            double bestDz = std::numeric_limits<double>::max();
            for (auto const& [s, zSurface] : stationSurfaces) {
                double const dz = std::abs(global.z() - zSurface);
                if (dz < zWindow && dz < bestDz) {
                    bestDz = dz;
                    best = s;
                }
            }
            if (!best)
                continue;  // no station within window — drop the hit
            auto locRes = best->globalToLocal(gctx, global, Acts::Vector3::UnitZ(), zWindow);
            if (!locRes.ok())
                continue;
            double const time = hit.time * Acts::UnitConstants::ns;
            auto const cov = hitCovariance(HitUncertaintyContext{global, time});
            auto const idx = static_cast<ActsExamples::Index>(m_measurements.size());
            m_measurements.push_back(Measurement{best, global, locRes.value(), cov, time});
            m_sourceLinks.emplace(best->geometryId(), idx);
        }
    }

    const std::vector<Measurement>& measurements() const { return m_measurements; }

    const ActsExamples::GeometryIdMultiset<IndexSourceLink>& sourceLinks() const {
        return m_sourceLinks;
    }

    // Delegate: write calibrated measurement (same shape as
    // ToyDetector::calibrator, looked up by IndexSourceLink instead of
    // MySourceLink).
    template <typename trajectory_t>
    void calibrator(const Acts::GeometryContext&, const Acts::CalibrationContext&,
                    const Acts::SourceLink& sl, typename trajectory_t::TrackStateProxy ts) const {
        auto const& indexSourceLink = sl.get<IndexSourceLink>();
        auto const& m = m_measurements[indexSourceLink.index()];
        ts.allocateCalibrated(m.local, m.covariance);
        // Without this, states this calibrator services never carry an
        // uncalibrated source link at all — findCandidateHits() (SpectrometerCkf.hpp)
        // needs it to recover which measurement index a CKF-found state
        // corresponds to.
        ts.setUncalibratedSourceLink(Acts::SourceLink{sl});
    }

   private:
    std::vector<Measurement> m_measurements;
    ActsExamples::GeometryIdMultiset<IndexSourceLink> m_sourceLinks;
};
