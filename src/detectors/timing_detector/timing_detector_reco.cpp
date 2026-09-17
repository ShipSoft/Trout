// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "detectors/identity_reco.hpp"

#include <SHiP/detectors/TimeDetHit.hpp>

PHLEX_REGISTER_ALGORITHMS(m, config) {
    register_identity_reco<SHiP::TimeDetHit>(m, config, "timing_detector_reco",
                                             "timing_detector_hits");
}
