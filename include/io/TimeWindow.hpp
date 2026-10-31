// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// TimeWindow.hpp — the sliding time windows reconstruction runs over.
//
// SHiP has no discrete events, so read_digitised_hits cuts the continuous hit
// stream into overlapping windows of width W stepping by S (S <= W); each
// window is one data cell of the "spill" layer. Every window also *owns* the
// central slice [lo + m, lo + m + S) of its range, with m = (W - S) / 2. The
// owned slices tile the time axis (the first window's extends to -inf, the
// last one's to +inf), so anything assigned by "which window owns its time"
// lands in exactly one window. Anything whose pieces span less than m in time
// after its owned time is also fully contained in the owning window.

#pragma once

#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace SHiP::trout {

struct TimeWindow {
    std::size_t index{0};
    double lo{0};      ///< Start of the window [ns], inclusive
    double hi{0};      ///< End of the window [ns], exclusive
    double own_lo{0};  ///< Start of the owned slice [ns], inclusive
    double own_hi{0};  ///< End of the owned slice [ns], exclusive

    bool contains(double t) const { return t >= lo && t < hi; }
    bool owns(double t) const { return t >= own_lo && t < own_hi; }
};

// The full set of windows covering [t_min, t_max].
class TimeWindowing {
   public:
    TimeWindowing(double t_min, double t_max, double width, double step)
        : t0_{t_min}, width_{width}, step_{step} {
        // Negated so that NaNs are rejected too.
        if (!(step_ > 0.0) || !(width_ >= step_))
            throw std::invalid_argument("time windows need 0 < step_ns <= window_ns");
        if (!(t_max >= t_min))
            throw std::invalid_argument("time windows need t_min <= t_max");
        double const span = t_max - t_min;
        // Smallest n with t0 + (n - 1) * S + W > t_max.
        n_ = span < width_ ? 1 : static_cast<std::size_t>(std::floor((span - width_) / step_)) + 2;
    }

    std::size_t size() const { return n_; }

    TimeWindow at(std::size_t i) const {
        double const inf = std::numeric_limits<double>::infinity();
        double const lo = t0_ + static_cast<double>(i) * step_;
        double const margin = 0.5 * (width_ - step_);
        return TimeWindow{.index = i,
                          .lo = lo,
                          .hi = lo + width_,
                          .own_lo = i == 0 ? -inf : lo + margin,
                          .own_hi = i + 1 == n_ ? inf : lo + margin + step_};
    }

   private:
    double t0_;
    double width_;
    double step_;
    std::size_t n_;
};

}  // namespace SHiP::trout
