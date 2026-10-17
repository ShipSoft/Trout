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

PHLEX_REGISTER_PROVIDERS(m, config) {
    // `file` names the map file (bare filenames resolve under
    // $SHIPFIELD_ROOT/share/field/, see SHiP::resolveFieldMapPath) and `map`
    // the map inside it.
    auto const file = config.get<std::string>("file");
    auto const map_name = config.get<std::string>("map");

    // Maps are stored in their own frame; `translation` (mm) places the
    // map's origin in the global frame. Omitted means the origins coincide.
    // SHiP::translated rejects non-finite values.
    std::array<double, 3> translation{};
    if (auto const t = config.get_if_present<std::vector<double>>("translation")) {
        if (t->size() != translation.size())
            throw std::invalid_argument("acts_field_provider: translation needs 3 values (mm)");
        std::ranges::copy(*t, translation.begin());
    }

    // Outside the mapped volume the field is genuinely negligible (the map
    // covers the full magnetic region by construction), so lookups there get
    // B = 0 instead of the evaluator's clamped edge value — the stepper's trial
    // steps routinely poke past the map edges. Anomalies (non-finite values,
    // evaluator exceptions) fail the lookup instead, and the first is logged.
    auto const map = SHiP::readFieldMap(SHiP::resolveFieldMapPath(file), map_name);
    auto const detector = std::make_shared<SHiP::trout::DetectorField>();
    detector->field = std::make_shared<SHiP::ActsFieldAdapter>(
        SHiP::translated(SHiP::zeroOutside(SHiP::makeFieldEvaluator(map), SHiP::extent(map)),
                         translation),
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
