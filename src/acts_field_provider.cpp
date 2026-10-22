// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// acts_field_provider.cpp — Phlex provider plugin
//
// Loads the spectrometer's magnetic field map once, in the SHiP field-map
// format (field_service docs/field_map_format.md), and provides it as a
// shared Acts::MagneticFieldProvider — the same "build once on layer job"
// pattern acts_geometry_provider.cpp uses for the tracking geometry.

#include "acts_field_provider.hpp"

#include "phlex/configuration.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"

#include <SHiP/FieldService/ActsFieldAdapter.h>
#include <SHiP/FieldService/FieldMap.h>
#include <SHiP/FieldService/FieldMapIO.h>
#include <algorithm>
#include <array>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

/// The spectrometer's map and placement: from a placement manifest
/// (`placements`, field_service docs/placements_format.md) when given,
/// otherwise from `file`, `map` and `translation`.
SHiP::FieldMapSource::MagnetConfig spectrometerMagnet(phlex::configuration const& config) {
    if (auto const placements = config.get_if_present<std::string>("placements")) {
        if (config.get_if_present<std::string>("file") || config.get_if_present<std::string>("map"))
            throw std::invalid_argument(
                "acts_field_provider: set either 'placements' or 'file' and 'map', not both");
        // A bare manifest name resolves via $SHIPFIELD_ROOT/share/field/, where
        // the ship-field-maps package installs placements.json and its maps.
        auto const name = config.get_if_present<std::string>("magnet").value_or("Spectrometer");
        auto magnets = SHiP::readFieldMapPlacements(*placements).magnets;
        auto const it = std::ranges::find(magnets, name, &SHiP::FieldMapSource::MagnetConfig::name);
        if (it == magnets.end())
            throw std::invalid_argument("acts_field_provider: " + *placements + " has no magnet '" +
                                        name + "'");
        return std::move(*it);
    }

    // `file` names the map file (bare filenames resolve under
    // $SHIPFIELD_ROOT/share/field/, see SHiP::resolveFieldMapPath) and `map`
    // the map inside it.
    SHiP::FieldMapSource::MagnetConfig magnet;
    magnet.file = SHiP::resolveFieldMapPath(config.get<std::string>("file"));
    magnet.map = config.get<std::string>("map");
    // Maps are stored in their own frame; `translation` (mm) places the
    // map's origin in the global frame. Omitted means the origins coincide.
    if (auto const t = config.get_if_present<std::vector<double>>("translation")) {
        if (t->size() != magnet.translation.size())
            throw std::invalid_argument("acts_field_provider: translation needs 3 values (mm)");
        std::ranges::copy(*t, magnet.translation.begin());
    }
    return magnet;
}

}  // namespace

PHLEX_REGISTER_PROVIDERS(m, config) {
    auto const magnet = spectrometerMagnet(config);

    // Outside the mapped volume the field is genuinely negligible (the map
    // covers the full magnetic region by construction), so lookups there get
    // B = 0 instead of the evaluator's clamped edge value — the stepper's trial
    // steps routinely poke past the map edges. Anomalies (non-finite values,
    // evaluator exceptions) fail the lookup instead, and the first is logged.
    // SHiP::translated rejects a non-finite translation.
    auto const map = SHiP::readFieldMap(magnet.file, magnet.map);
    auto const detector = std::make_shared<SHiP::trout::DetectorField>();
    detector->field = std::make_shared<SHiP::ActsFieldAdapter>(
        SHiP::translated(SHiP::zeroOutside(SHiP::makeFieldEvaluator(map), SHiP::extent(map)),
                         magnet.translation),
        [](std::string_view what) {
            spdlog::warn("acts_field_provider: {} — failing field lookups", what);
        });

    m.provide(
         "read_field",
         [detector](phlex::data_cell_index const&) -> std::shared_ptr<SHiP::trout::DetectorField> {
             return detector;
         },
         phlex::concurrency::unlimited)
        .output_product("spectrometer_field", phlex::experimental::identifier{"field"},
                        phlex::experimental::identifier{"job"});
}
