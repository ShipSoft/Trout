// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <Acts/Definitions/Algebra.hpp>
#include <Acts/EventData/VectorMultiTrajectory.hpp>
#include <Acts/EventData/VectorTrackContainer.hpp>
#include <Acts/Geometry/GeometryContext.hpp>

#include <SHiP/TrackFitResult.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace SHiP::trout {

inline TrackFitResult fromActsFitResult(
    const Acts::Result<Acts::TrackContainer<Acts::VectorTrackContainer, Acts::VectorMultiTrajectory,
                                            Acts::ValueHolder>::TrackProxy>& actsResult,
    Acts::TrackContainer<Acts::VectorTrackContainer, Acts::VectorMultiTrajectory,
                         Acts::ValueHolder>& tracks,
    Acts::GeometryContext const& gctx) {
    TrackFitResult result;
    if (!actsResult.ok()) {
        result.fit_status = 1;
        return result;
    }

    auto track = actsResult.value();
    const auto& params = track.parameters();
    auto& states = tracks.trackStateContainer();

    result.fit_status = 0;
    result.chi2 = track.chi2();
    result.ndf = track.nDoF();
    result.q_over_p = params[Acts::eBoundQOverP];
    result.phi = params[Acts::eBoundPhi];
    result.theta = params[Acts::eBoundTheta];
    result.time = params[Acts::eBoundTime];

    // ref_loc used to be {loc0, loc1, 0.0} — the LOCAL position on the
    // reference surface with a dead placeholder third component, not a
    // global position at all (so e.g. it never read anywhere near the
    // first station's real z, no matter how good the fit was). Convert to
    // the surface's actual global position instead.
    if (track.hasReferenceSurface()) {
        Acts::Vector3 const direction(std::sin(result.theta) * std::cos(result.phi),
                                      std::sin(result.theta) * std::sin(result.phi),
                                      std::cos(result.theta));
        auto const global = track.referenceSurface().localToGlobal(
            gctx, Acts::Vector2{params[Acts::eBoundLoc0], params[Acts::eBoundLoc1]}, direction);
        result.ref_loc = {global.x(), global.y(), global.z()};
    } else {
        // No reference surface set — fall back to the old (local-position)
        // behaviour rather than reporting a meaningless global position.
        result.ref_loc = {params[Acts::eBoundLoc0], params[Acts::eBoundLoc1], 0.0};
    }

    states.visitBackwards(track.tipIndex(), [&](auto ts) {
        if (!ts.typeFlags().isMeasurement()) {
            return true;
        }
        if (!(ts.hasCalibrated())) {
            return true;
        }
        if (!ts.hasSmoothed()) {
            return true;
        }

        auto meas = ts.template calibrated<2>();
        auto smooth = ts.smoothed();

        Acts::Vector2 fitted;
        fitted << smooth[Acts::eBoundLoc0], smooth[Acts::eBoundLoc1];

        Acts::Vector2 residual = meas - fitted;
        result.input_measurements_x.push_back(meas[0]);
        result.input_measurements_y.push_back(meas[1]);
        result.fitted_measurements_x.push_back(fitted[0]);
        result.fitted_measurements_y.push_back(fitted[1]);
        result.residuals_x.push_back(residual[0]);
        result.residuals_y.push_back(residual[1]);
        return true;
    });
    result.n_meas = static_cast<std::int32_t>(result.input_measurements_x.size());

    return result;
}

}  // namespace SHiP::trout
