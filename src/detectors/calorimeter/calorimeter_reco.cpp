// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "detectors/identity_reco.hpp"

#include <SHiP/detectors/CaloHit.hpp>

PHLEX_REGISTER_ALGORITHMS(m, config) {
    register_identity_reco<SHiP::CaloHit>(m, config, "calorimeter_reco", "calorimeter_hits");
}
