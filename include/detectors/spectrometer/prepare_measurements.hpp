// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "PhlexModuleProxy.hpp"
#include "phlex/core/product_selector.hpp"

namespace SHiP::trout {

// prepare_measurements (spill layer, transform): straw_tubes_hits (+ their
// ids and the time window) + tracking_geometry -> SpillContext
// (SpectrometerMeasurements + SpectrometerCkf + the window), built ONCE per
// spill and shared by every seed spawned
// downstream (rather than rebuilt per seed, which would also reload the
// field file every time). seedMarginNs widens the window's owned slice into
// the time range generate_seeds seeds from (see SpillContext).
void register_prepare_measurements(ModuleProxy const& m,
                                   phlex::experimental::identifier const& layer,
                                   double seedMarginNs);

}  // namespace SHiP::trout
