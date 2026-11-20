// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// StrawTiming.hpp — the straw-tube time constants, in one place, so the time
// window reader (how many straw hits a window gets) and tracking (which
// hits it may use) can't drift apart. Plain ns, with no ACTS dependency, so
// the io plugins can use it; tracking converts to ACTS units
// (strawHitTimeSelection in detectors/spectrometer/HitUncertainty.hpp).
//
// These are fixed in C++ on purpose, not workflow parameters.

#pragma once

#include "io/TimeWindow.hpp"

namespace SHiP::trout {

// How far apart in time straw hits may be and still belong to one track: the
// station-1/station-2 seed coincidence and the CKF time gate, both of which
// cut in both directions. A reflection of the straw tubes' time resolution.
inline constexpr double kStrawHitTimeSelectionNs = 2000.0;

// How far outside a window's owned slice station-1 hits are still seeded
// from. Tracks found there aren't kept by this window; they're context for
// resolve_tracks, so a clash across a window boundary is seen, and decided
// the same way, by both windows.
inline constexpr double kStrawSeedMarginNs = 2000.0;

// The straw hits each window gets around its owned slice: every hit a track
// seeded anywhere in the seed range (owned slice +- kStrawSeedMarginNs) can
// use, so no track, owned or context, is cut short at the window edge.
inline constexpr TimePadding kStrawTubesPadding{
    .before_ns = kStrawSeedMarginNs + kStrawHitTimeSelectionNs,
    .after_ns = kStrawSeedMarginNs + kStrawHitTimeSelectionNs};

}  // namespace SHiP::trout
