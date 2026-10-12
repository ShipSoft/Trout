// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "PhlexModuleProxy.hpp"
#include "phlex/core/product_selector.hpp"

// fit_track (track layer, transform): takes one CKF candidate and the
// SpillContext of its spill, and refits the candidate's hits with
// SpectrometerCkf::refit() (a real forward+backward-smoothed
// Acts::KalmanFitter; the CKF exploration itself has no smoother, see
// SpectrometerCkf.hpp). Produces one track_fit_result per candidate.
void register_fit_track(ModuleProxy const& m, phlex::experimental::identifier const& layer,
                        phlex::experimental::identifier const& trackLayer);
