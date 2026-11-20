// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// TimeWindow.hpp — the time windows reconstruction runs over.
//
// SHiP has no discrete events, so read_digitised_hits cuts the continuous hit
// stream into time windows, one per data cell of the "spill" layer. Each
// window *owns* a slice [own_lo, own_hi) of the time axis, of width S (the
// step). The owned slices tile the time axis (the first one extends to -inf,
// the last one to +inf), so anything assigned by "which window owns its time"
// lands in exactly one window.
//
// Each detector then sees its own padded range around the owned slice,
// [own_lo - before, own_hi + after), sized to the time context that
// detector's reconstruction needs (see TimePadding, and the per-detector
// values in DigitisedTimeScan.hpp). Detectors that need no context use zero
// padding and get exactly the owned slice. Anything whose pieces span less
// than `after` past its owned time is fully contained in the owning window's
// padded range.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace SHiP::trout {

// How much time context [ns] a detector needs around a window's owned slice.
struct TimePadding {
    double before_ns{0.0};
    double after_ns{0.0};
};

// A half-open time range [lo, hi) [ns].
struct TimeRange {
    double lo{0};
    double hi{0};

    bool contains(double t) const { return t >= lo && t < hi; }
};

struct TimeWindow {
    std::size_t index{0};
    double own_lo{0};  ///< Start of the owned slice [ns], inclusive
    double own_hi{0};  ///< End of the owned slice [ns], exclusive

    bool owns(double t) const { return t >= own_lo && t < own_hi; }

    // The owned slice widened by `padding`.
    TimeRange padded(TimePadding const& padding) const {
        return TimeRange{own_lo - padding.before_ns, own_hi + padding.after_ns};
    }
};

// The full set of windows covering [t_min, t_max].
class TimeWindowing {
   public:
    TimeWindowing(double t_min, double t_max, double step) : t0_{t_min}, step_{step} {
        // Negated so that NaNs are rejected too.
        if (!(step_ > 0.0))
            throw std::invalid_argument("time windows need step_ns > 0");
        if (!(t_max >= t_min))
            throw std::invalid_argument("time windows need t_min <= t_max");
        // Slice i is [t0 + i * S, t0 + (i + 1) * S). The last slice extends
        // to +inf, so rounding here can never leave a hit uncovered; it only
        // decides how the tail is split.
        double const slices = std::ceil((t_max - t_min) / step_);
        n_ = std::max<std::size_t>(1, static_cast<std::size_t>(slices));
    }

    std::size_t size() const { return n_; }
    double step() const { return step_; }

    TimeWindow at(std::size_t i) const {
        double const inf = std::numeric_limits<double>::infinity();
        // Both edges come from edge(), so window i's own_hi is bit-for-bit
        // window i + 1's own_lo and no time falls between owned slices.
        return TimeWindow{.index = i,
                          .own_lo = i == 0 ? -inf : edge(i),
                          .own_hi = i + 1 == n_ ? inf : edge(i + 1)};
    }

   private:
    double edge(std::size_t i) const { return t0_ + static_cast<double>(i) * step_; }


    double t0_;
    double step_;
    std::size_t n_;
};

}  // namespace SHiP::trout
