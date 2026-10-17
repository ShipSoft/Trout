// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <Acts/MagneticField/MagneticFieldProvider.hpp>

#include <memory>

namespace SHiP::trout {

// Output of the acts_field_provider phlex module, mirroring DetectorGeometry
// (acts_geometry_provider.hpp): the field is loaded once and shared as a
// single product, rather than every consumer loading its own copy.
struct DetectorField {
    std::shared_ptr<Acts::MagneticFieldProvider> field;
};

}  // namespace SHiP::trout
