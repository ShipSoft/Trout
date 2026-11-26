// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Acts/Geometry/TrackingGeometry.hpp>

#include <SHiP/GeometryService/GeometryService.h>
#include <memory>
#include <string>

namespace SHiP::trout {

// The tracking geometry, registered as a phlex resource by the
// acts_geometry_provider plugin and handed to algorithms that declare
// phlex::resource<DetectorGeometry>{}. It is built once, before any module
// loads, and destroyed after all of them.
struct DetectorGeometry {
    // Loads the GeoModel geometry in `dbFile` and builds the tracking geometry
    // from the station placements. Defined in acts_geometry_provider.cpp.
    explicit DetectorGeometry(std::string const& dbFile);

    DetectorGeometry(DetectorGeometry const&) = delete;
    DetectorGeometry& operator=(DetectorGeometry const&) = delete;
    DetectorGeometry(DetectorGeometry&&) = delete;
    DetectorGeometry& operator=(DetectorGeometry&&) = delete;
    ~DetectorGeometry() = default;

    // Empties the Geant4 geometry stores on the geometry thread when
    // destroyed. Declared first, so it is constructed before the geometry is
    // built and destroyed last, also when the constructor throws part-way.
    struct G4StoreCleanup {
        G4StoreCleanup() = default;
        G4StoreCleanup(G4StoreCleanup const&) = delete;
        G4StoreCleanup& operator=(G4StoreCleanup const&) = delete;
        G4StoreCleanup(G4StoreCleanup&&) = delete;
        G4StoreCleanup& operator=(G4StoreCleanup&&) = delete;
        ~G4StoreCleanup();
    };
    G4StoreCleanup cleanup;

    std::shared_ptr<SHiP::GeometryService> geometryService;
    std::shared_ptr<const Acts::TrackingGeometry> trackingGeometry;
};

}  // namespace SHiP::trout
