// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "SpectrometerCkf.hpp"
#include "SpectrometerMeasurements.hpp"
#include "io/TimeWindow.hpp"

#include <memory>

namespace SHiP::trout {

// Output of prepare_measurements, shared read-only by every seed spawned
// from the same spill (time window).
struct SpillContext {
    std::shared_ptr<SpectrometerMeasurements> measurements;
    std::shared_ptr<SpectrometerCkf> ckf;
    TimeWindow window;
    // generate_seeds only seeds from station-1 hits in [seedTimeLo,
    // seedTimeHi) ns: the window's owned slice widened by the seed margin.
    // Set here rather than in generate_seeds since phlex's unfold can't pass
    // configuration to the SeedGenObject it constructs.
    double seedTimeLo{0};
    double seedTimeHi{0};
};

}  // namespace SHiP::trout
