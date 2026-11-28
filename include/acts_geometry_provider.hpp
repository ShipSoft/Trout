// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Acts/Geometry/TrackingGeometry.hpp>
#include <Acts/Surfaces/SurfacePlacementBase.hpp>

#include <memory>
#include <vector>

namespace SHiP::trout {

struct DetectorGeometry {
    /// Place the sensitive surfaces (one per station); the surfaces point
    /// back at them, so they live as long as the tracking geometry.
    std::vector<std::shared_ptr<const Acts::SurfacePlacementBase>> detectorElements;
    std::shared_ptr<const Acts::TrackingGeometry> trackingGeometry;
};

}  // namespace SHiP::trout
