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

#include <FieldService/FieldMap.h>
#include <FieldService/FieldMapIO.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

PHLEX_REGISTER_PROVIDERS(m, config) {
    // `file` names the map file (bare filenames resolve under
    // $SHIPFIELD_ROOT/share/field/, see ship::resolveFieldMapPath) and `map`
    // the map inside it.
    auto const file = config.get<std::string>("file");
    auto const map_name = config.get<std::string>("map");

    // Maps are stored in their own frame; `translation` (mm) places the
    // map's origin in the global frame. Omitted means the origins coincide.
    std::array<double, 3> translation{};
    if (auto const t = config.get_if_present<std::vector<double>>("translation")) {
        if (t->size() != translation.size())
            throw std::invalid_argument("acts_field_provider: translation needs 3 values (mm)");
        auto const finite = [](double v) { return std::isfinite(v); };
        if (!std::ranges::all_of(*t, finite))
            throw std::invalid_argument("acts_field_provider: translation values must be finite");
        std::ranges::copy(*t, translation.begin());
    }

    // Read the map itself, not just an evaluator, so the lookup bounds come
    // from its grid.
    auto const map = ship::readFieldMap(ship::resolveFieldMapPath(file), map_name);
    auto const bounds = fieldBounds(map.grid, map.symmetry, translation);
    auto detector = std::make_shared<DetectorField>();
    detector->field =
        std::make_shared<ShipActsFieldProvider>(ship::makeFieldEvaluator(map), bounds, translation);

    m.provide(
         "read_field",
         [detector](phlex::data_cell_index const&) -> std::shared_ptr<DetectorField> {
             return detector;
         },
         phlex::concurrency::unlimited)
        .output_product("spectrometer_field", phlex::experimental::identifier{"field"},
                        phlex::experimental::identifier{"job"});
}
