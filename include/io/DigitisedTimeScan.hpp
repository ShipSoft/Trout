// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// DigitisedTimeScan.hpp — the first pass over a digitised-hit file's time
// columns, shared by read_digitised_hits (which serves the windows) and
// time_window_driver (which emits one "spill" data cell per window), so both
// derive the same TimeWindowing from the same input and configuration.

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

// The RNTuples read for a given configuration, with their time fields.
// Mirrors the fixed set of RNTuples HitRNTupleWriter writes in shannon's
// digitised_output_module.cpp — field name "hit" is constant for all of them
// (TypedHitWriter<Hit> always names its field "hit").
inline std::vector<std::pair<std::string, char const*>> digitised_ntuples(bool simulation,
                                                                          bool sim_particles) {
    std::vector<std::pair<std::string, char const*>> ntuples{
        {"ubt_hits", time_field<SHiP::UBTHit>},
        {"sbt_hits", time_field<SHiP::SBTHit>},
        {"straw_tubes_hits", time_field<SHiP::StrawTubesHit>},
        {"calorimeter_hits", time_field<SHiP::CaloHit>},
        {"timing_detector_hits", time_field<SHiP::TimeDetHit>}};
    if (simulation)
        ntuples.emplace_back("sim_hits", time_field<SHiP::SimHit>);
    if (simulation && sim_particles)
        ntuples.emplace_back("sim_particles", time_field<SHiP::SimParticle>);
    return ntuples;
}

struct InputTimeScan {
    std::map<std::string, TimeScan> per_ntuple;
    TimeScan all;
    TimeWindowing windowing;
};

// Scans every RNTuple's time column and cuts the overall span into windows.
inline InputTimeScan scan_input(std::string const& input_file,
                                std::vector<std::pair<std::string, char const*>> const& ntuples,
                                double window_ns, double step_ns) {
    std::map<std::string, TimeScan> scans;
    TimeScan all;
    for (auto const& [name, field] : ntuples) {
        auto const scan = scan_times(input_file, name, field);
        scans[name] = scan;
        all.min = std::min(all.min, scan.min);
        all.max = std::max(all.max, scan.max);
    }
    if (all.min > all.max) {
        all.min = 0.0;  // no hits at all: one empty window
        all.max = 0.0;
    }
    TimeWindowing const windowing{all.min, all.max, window_ns, step_ns};
    return InputTimeScan{std::move(scans), all, windowing};
}

}  // namespace SHiP::trout
