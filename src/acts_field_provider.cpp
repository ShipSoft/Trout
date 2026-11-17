// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// acts_field_provider.cpp — Phlex resource plugin
//
// Loads the spectrometer's magnetic field map once, in the SHiP field-map
// format (field_service docs/field_map_format.md), and registers it as the
// DetectorField resource, an Acts::MagneticFieldProvider shared by every
// algorithm that uses it. acts_geometry_provider.cpp does the same for the
// tracking geometry.

#include "acts_field_provider.hpp"

#include "phlex/configuration.hpp"
#include "phlex/resource.hpp"

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

namespace SHiP::trout {

DetectorField::DetectorField(std::string const& file, std::string const& map,
                             std::array<double, 3> const& translation) {
    // Outside the mapped volume the field is genuinely negligible (the map
    // covers the full magnetic region by construction), so lookups there get
    // B = 0 instead of the evaluator's clamped edge value — the stepper's trial
    // steps routinely poke past the map edges. Anomalies (non-finite values,
    // evaluator exceptions) fail the lookup instead, and the first is logged.
    // SHiP::translated rejects non-finite translations.
    auto const fieldMap = SHiP::readFieldMap(SHiP::resolveFieldMapPath(file), map);
    field = std::make_shared<SHiP::ActsFieldAdapter>(
        SHiP::translated(
            SHiP::zeroOutside(SHiP::makeFieldEvaluator(fieldMap), SHiP::extent(fieldMap)),
            translation),
        [](std::string_view what) {
            spdlog::warn("acts_field_provider: {} — failing field lookups", what);
        });
}

}  // namespace SHiP::trout

PHLEX_REGISTER_RESOURCES(r, config) {
    // `file` names the map file (bare filenames resolve under
    // $SHIPFIELD_ROOT/share/field/, see SHiP::resolveFieldMapPath) and `map`
    // the map inside it.
    auto const file = config.get<std::string>("file");
    auto const map_name = config.get<std::string>("map");

    // Maps are stored in their own frame; `translation` (mm) places the
    // map's origin in the global frame. Omitted means the origins coincide.
    std::array<double, 3> translation{};
    if (auto const t = config.get_if_present<std::vector<double>>("translation")) {
        if (t->size() != translation.size())
            throw std::invalid_argument("acts_field_provider: translation needs 3 values (mm)");
        std::ranges::copy(*t, translation.begin());
    }

    r.add_unlimited_resource<SHiP::trout::DetectorField>(file, map_name, translation);
}
