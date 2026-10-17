// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Acts/Geometry/TrackingGeometry.hpp>

#include <SHiP/GeometryService/GeometryService.h>
#include <memory>

namespace SHiP::trout {

struct DetectorGeometry {
    std::shared_ptr<SHiP::GeometryService> geometryService;
    std::shared_ptr<const Acts::TrackingGeometry> trackingGeometry;
};

}  // namespace SHiP::trout
