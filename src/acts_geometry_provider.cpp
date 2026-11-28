// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// acts_geometry_provider.cpp — Phlex provider plugin
//
// Builds a minimal ACTS tracking geometry for the straw-tube spectrometer
// from the real SHiP geometry, replacing the hardcoded plane z-positions in
// ToyDetector/keep_tracks.cpp. One Acts::PlaneSurface per tracker station
// (/SHiP/trackers/station_1..4), positioned at each station's real global
// placement.
//
// The geometry database publishes the stations as full physical volumes
// under "Trackers" (see Publishing.h in the geometry repository). ACTS's
// GeoModel plugin reads them with their absolute transforms and converts
// each station box into a plane surface with rectangle bounds.

#include "acts_geometry_provider.hpp"

#include "SHiP/GeometryService/GeometryService.h"
#include "phlex/configuration.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"

#include <Acts/Definitions/Units.hpp>
#include <Acts/Geometry/Blueprint.hpp>
#include <Acts/Geometry/BlueprintOptions.hpp>
#include <Acts/Geometry/CuboidVolumeBounds.hpp>
#include <Acts/Geometry/Extent.hpp>
#include <Acts/Geometry/GeometryContext.hpp>
#include <Acts/Geometry/GeometryIdentifier.hpp>
#include <Acts/Geometry/NavigationPolicyFactory.hpp>
#include <Acts/Geometry/StaticBlueprintNode.hpp>
#include <Acts/Geometry/TrackingGeometry.hpp>
#include <Acts/Geometry/TrackingVolume.hpp>
#include <Acts/Navigation/TryAllNavigationPolicy.hpp>
#include <Acts/Surfaces/PlaneSurface.hpp>
#include <Acts/Surfaces/RectangleBounds.hpp>
#include <Acts/Utilities/BoundFactory.hpp>
#include <Acts/Utilities/Logger.hpp>
#include <Acts/Utilities/TransformHelpers.hpp>

#include <ActsPlugins/GeoModel/GeoModelConverters.hpp>
#include <ActsPlugins/GeoModel/GeoModelReader.hpp>
#include <ActsPlugins/GeoModel/GeoModelTree.hpp>
#include <GeoModelKernel/GeoFullPhysVol.h>
#include <GeoModelKernel/GeoLogVol.h>
#include <GeoModelKernel/GeoShape.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kNumStations = 4;
// The GeoPublisher system the geometry publishes the stations under.
constexpr auto kStationSystem = "Trackers";

std::string station_name(int i) {
    return "/SHiP/trackers/station_" + std::to_string(i + 1);
}

// Converts the published station volumes into sensitive plane surfaces, in
// station order. The detector elements own the surfaces' placement, so they
// go into `detector` to outlive the tracking geometry's use of them.
std::array<std::shared_ptr<Acts::Surface>, kNumStations> convert_stations(
    ActsPlugins::GeoModelTree const& tree, SHiP::trout::DetectorGeometry& detector) {
    auto const& published = tree.publisher->getPublishedVol(kStationSystem);
    Acts::SurfaceBoundFactory boundFactory;
    std::array<std::shared_ptr<Acts::Surface>, kNumStations> surfaces;
    for (int i = 0; i < kNumStations; ++i) {
        auto const it = published.find(station_name(i));
        if (it == published.end() || !it->second) {
            throw std::runtime_error("acts_geometry_provider: '" + station_name(i) +
                                     "' is not published under '" + kStationSystem +
                                     "' in the geometry database");
        }
        auto const& station = it->second;
        auto const converter =
            ActsPlugins::geoShapesConverters(station->getLogVol()->getShape()->typeID());
        if (!converter) {
            throw std::runtime_error(
                "acts_geometry_provider: no ACTS converter for the shape of '" + station_name(i) +
                "'");
        }
        auto converted = converter->toSensitiveSurface(
            station, Acts::makeTransform3(station->getAbsoluteTransform()), boundFactory);
        if (!converted.ok()) {
            throw std::runtime_error("acts_geometry_provider: cannot convert '" + station_name(i) +
                                     "': " + converted.error().message());
        }
        auto [element, surface] = std::move(converted.value());
        detector.detectorElements.push_back(std::move(element));
        surfaces[i] = std::move(surface);
    }
    return surfaces;
}

std::shared_ptr<Acts::TrackingGeometry> build_tracking_geometry(
    std::array<std::shared_ptr<Acts::Surface>, kNumStations> const& stations) {
    auto const gctx = Acts::GeometryContext::dangerouslyDefaultConstruct();

    // Container volume spanning all stations, plus a margin. Station centres
    // and half-extents come from the surfaces, in ACTS units (mm).
    double minZ = stations.front()->center(gctx).z();
    double maxZ = minZ;
    double halfX = 0;
    double halfY = 0;
    for (auto const& s : stations) {
        auto const z = s->center(gctx).z();
        minZ = std::min(minZ, z);
        maxZ = std::max(maxZ, z);
        auto const& bounds = dynamic_cast<Acts::RectangleBounds const&>(s->bounds());
        halfX = std::max(halfX, bounds.halfLengthX());
        halfY = std::max(halfY, bounds.halfLengthY());
    }
    double const margin_mm = 100.;
    double const margin = margin_mm * Acts::UnitConstants::mm;
    double const containerHalfX = halfX + margin;
    double const containerHalfY = halfY + margin;
    double const containerHalfZ = (0.5 * (maxZ - minZ)) + margin;
    double const containerZ = 0.5 * (maxZ + minZ);

    auto containerBounds =
        std::make_shared<Acts::CuboidVolumeBounds>(containerHalfX, containerHalfY, containerHalfZ);
    Acts::Transform3 containerTrf = Acts::Transform3::Identity();
    containerTrf.translate(Acts::Vector3(0., 0., containerZ));

    auto worldVolume = std::make_unique<Acts::TrackingVolume>(containerTrf, containerBounds,
                                                              "SpectrometerStations");

    for (int i = 0; i < kNumStations; ++i) {
        auto const& surface = stations[i];
        surface->assignGeometryId(Acts::GeometryIdentifier{}.withVolume(1).withSensitive(i + 1));
        // Independent of the GeometryIdentifier's sensitive() component above:
        // Surface::isSensitive() is what CombinatorialKalmanFilter::filter()
        // checks before ever attempting createTrackStates; a non-sensitive
        // station would never calibrate a hit. The converter's detector
        // element makes the surface sensitive (and forbids setting it here).
        if (!surface->isSensitive()) {
            throw std::logic_error("acts_geometry_provider: '" + station_name(i) +
                                   "' converted to a non-sensitive surface");
        }

        worldVolume->addSurface(surface);
    }

    // TrackingVolume::addSurface only registers these for enumeration
    // (visitSurfaces/apply — what SpectrometerMeasurements uses to match
    // hits); it doesn't make them navigable. Building via the raw
    // TrackingVolume constructor gives a volume with zero portals, which
    // TrackingGeometry::geometryVersion() classifies as legacy Gen1 — and for
    // Gen1, Acts::Navigator ignores navigation policies entirely and only
    // searches for the next surface via the (nonexistent, since we never
    // built any) Layer/LayerArray system. Going through the Blueprint/Gen3
    // construction path instead gives the volume real portals, so the
    // Navigator actually consults the navigation policy — TryAllNavigationPolicy
    // tries every surface in volume->surfaces() directly, which is exactly the
    // addSurface-populated list above, plus this volume's portals. Passives
    // aren't needed for this model.
    //
    // A track that misses the remaining stations has to leave through a
    // portal: without portals as candidates, or with a world volume that
    // shares this volume's boundary, the navigator reports itself lost
    // instead of reaching the end of the world. The CKF then keeps stepping
    // (it ignores PropagatorPlainOptions::pathLimit, and its own loop
    // protection does nothing in a field-free region), and the growing steps
    // run the position off to infinity. The envelope gives the world volume
    // room around this one, so leaving it ends the propagation.
    Acts::Blueprint::Config blueprintCfg;
    double const envelope = margin_mm * Acts::UnitConstants::mm;
    blueprintCfg.envelope = Acts::ExtentEnvelope{
        {.x = {envelope, envelope}, .y = {envelope, envelope}, .z = {envelope, envelope}}};
    auto root = std::make_unique<Acts::Blueprint>(blueprintCfg);
    root->addStaticVolume(std::move(worldVolume), [](auto& node) {
        Acts::TryAllNavigationPolicy::Config navPolicyCfg;
        navPolicyCfg.portals = true;
        navPolicyCfg.passives = false;
        node.setNavigationPolicyFactory(std::shared_ptr<Acts::NavigationPolicyFactory>(
            Acts::NavigationPolicyFactory{}
                .add<Acts::TryAllNavigationPolicy>(navPolicyCfg)
                .asUniquePtr()));
    });

    Acts::BlueprintOptions blueprintOptions;
    return root->construct(blueprintOptions, gctx);
}

}  // namespace

PHLEX_REGISTER_PROVIDERS(m, config) {
    // A bare file name is looked up under $SHIPGEOMETRY_ROOT/share/geometry.
    auto const db_file = SHiP::resolveGeometryDbPath(config.get<std::string>("db_file")).string();

    auto const tree = ActsPlugins::GeoModelReader::readFromDb(db_file);
    auto const detector = std::make_shared<SHiP::trout::DetectorGeometry>();
    detector->trackingGeometry = build_tracking_geometry(convert_stations(tree, *detector));

    m.provide(
         "read_tracking_geometry",
         [detector](phlex::data_cell_index const&)
             -> std::shared_ptr<SHiP::trout::DetectorGeometry> { return detector; },
         phlex::concurrency::unlimited)
        .output_product("tracking_geometry", phlex::experimental::identifier{"detector"},
                        phlex::experimental::identifier{"job"});
}
