// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// DigitisedTimeScan.hpp — the first pass over a digitised-hit file's time
// columns, shared by read_digitised_hits (which serves the windows) and
// time_window_driver (which emits one "spill" data cell per window), so both
// derive the same TimeWindowing from the same input and configuration.
//
// It also holds each RNTuple's time padding: how much context around a
// window's owned slice that detector's reconstruction needs (see
// TimeWindow.hpp). These are fixed here in C++ on purpose, not workflow
// parameters.

#pragma once

#include "io/TimeWindow.hpp"

#include <ROOT/RNTupleReader.hxx>
#include <ROOT/RNTupleView.hxx>

#include <SHiP/SimHit.hpp>
#include <SHiP/SimParticle.hpp>
#include <SHiP/detectors/CaloHit.hpp>
#include <SHiP/detectors/SBTHit.hpp>
#include <SHiP/detectors/StrawTubesHit.hpp>
#include <SHiP/detectors/TimeDetHit.hpp>
#include <SHiP/detectors/UBTHit.hpp>
#include <algorithm>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace SHiP::trout {

// Field holding each entry's time [ns]: digitised hits carry it in their
// RecHit, the sim types directly.
template <typename Hit>
constexpr char const* time_field = "hit.rec_hit.time";
template <>
constexpr char const* time_field<SHiP::SimHit> = "hit.time";
template <>
constexpr char const* time_field<SHiP::SimParticle> = "hit.time";

// What the first pass learns about one RNTuple's time column.
struct TimeScan {
    double min{std::numeric_limits<double>::infinity()};
    double max{-std::numeric_limits<double>::infinity()};
    // The most any entry is earlier than the latest entry before it.
    double max_lag{0.0};
};

inline TimeScan scan_times(std::string const& input_file, std::string const& ntuple_name,
                           char const* field) {
    auto reader = ROOT::RNTupleReader::Open(ntuple_name, input_file);
    auto view = reader->GetView<double>(field);
    TimeScan scan;
    for (auto const i : reader->GetEntryRange()) {
        double const t = view(i);
        scan.max_lag = std::max(scan.max_lag, scan.max - t);
        scan.min = std::min(scan.min, t);
        scan.max = std::max(scan.max, t);
    }
    return scan;
}

// Time context each detector's reconstruction needs around a window's owned
// slice. The straw tubes need the most: tracking groups hits up to
// strawHitTimeSelection (2000 ns, detectors/spectrometer/HitUncertainty.hpp)
// apart, so a track owned at the end of the slice must still find its hits.
// The other detectors have no reconstruction that looks beyond the owned
// slice yet, so they get exactly the owned slice.
inline constexpr TimePadding kStrawTubesPadding{.before_ns = 2000.0, .after_ns = 2000.0};
inline constexpr TimePadding kNoPadding{};

// One RNTuple read from the input.
struct DigitisedNtuple {
    std::string name;
    char const* time_field;
    TimePadding padding;
    // Whether its times set the span the windows cover. Only detector hits
    // do: truth (sim_hits, sim_particles) must not move the windows, and can
    // reach far later times (e.g. late Geant4 decays). Truth outside the span
    // still lands in the first or last window, whose owned slices extend to
    // -inf and +inf.
    bool sets_span;
};

// The RNTuples read for a given configuration, with their time fields.
// Mirrors the fixed set of RNTuples HitRNTupleWriter writes in shannon's
// digitised_output_module.cpp — field name "hit" is constant for all of them
// (TypedHitWriter<Hit> always names its field "hit").
inline std::vector<DigitisedNtuple> digitised_ntuples(bool simulation, bool sim_particles) {
    std::vector<DigitisedNtuple> ntuples{
        {"ubt_hits", time_field<SHiP::UBTHit>, kNoPadding, true},
        {"sbt_hits", time_field<SHiP::SBTHit>, kNoPadding, true},
        {"straw_tubes_hits", time_field<SHiP::StrawTubesHit>, kStrawTubesPadding, true},
        {"calorimeter_hits", time_field<SHiP::CaloHit>, kNoPadding, true},
        {"timing_detector_hits", time_field<SHiP::TimeDetHit>, kNoPadding, true}};
    if (simulation)
        ntuples.push_back({"sim_hits", time_field<SHiP::SimHit>, kNoPadding, false});
    if (simulation && sim_particles)
        ntuples.push_back({"sim_particles", time_field<SHiP::SimParticle>, kNoPadding, false});
    return ntuples;
}

struct InputTimeScan {
    std::map<std::string, TimeScan> per_ntuple;
    TimeScan all;  ///< Over the ntuples that set the span only
    TimeWindowing windowing;
};

// Scans every RNTuple's time column (each one's max_lag is needed to choose
// how to read it) and cuts the span of those that set it into windows.
inline InputTimeScan scan_input(std::string const& input_file,
                                std::vector<DigitisedNtuple> const& ntuples, double step_ns) {
    std::map<std::string, TimeScan> scans;
    TimeScan all;
    for (auto const& ntuple : ntuples) {
        auto const scan = scan_times(input_file, ntuple.name, ntuple.time_field);
        scans[ntuple.name] = scan;
        if (!ntuple.sets_span)
            continue;
        all.min = std::min(all.min, scan.min);
        all.max = std::max(all.max, scan.max);
    }
    if (all.min > all.max) {
        all.min = 0.0;  // no detector hits at all: one window covering everything
        all.max = 0.0;
    }
    TimeWindowing const windowing{all.min, all.max, step_ns};
    return InputTimeScan{std::move(scans), all, windowing};
}

}  // namespace SHiP::trout
