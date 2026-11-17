// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <Acts/MagneticField/MagneticFieldProvider.hpp>

#include <array>
#include <memory>
#include <string>

namespace SHiP::trout {

// The spectrometer's magnetic field, registered as a phlex resource by the
// acts_field_provider plugin, like DetectorGeometry (acts_geometry_provider.hpp):
// the map is loaded once and shared, rather than every consumer loading its own
// copy.
struct DetectorField {
    // Loads map `map` from `file`, with the map's origin placed at
    // `translation` (mm) in the global frame. Defined in acts_field_provider.cpp.
    DetectorField(std::string const& file, std::string const& map,
                  std::array<double, 3> const& translation);

    std::shared_ptr<Acts::MagneticFieldProvider> field;
};

}  // namespace SHiP::trout
