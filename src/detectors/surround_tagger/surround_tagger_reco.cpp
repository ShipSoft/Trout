// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "detectors/identity_reco.hpp"

#include <SHiP/detectors/SBTHit.hpp>

PHLEX_REGISTER_ALGORITHMS(m, config) {
    register_identity_reco<SHiP::SBTHit>(m, config, "surround_tagger_reco", "sbt_hits");
}
