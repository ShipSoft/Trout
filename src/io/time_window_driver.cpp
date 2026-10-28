// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// time_window_driver.cpp — Phlex driver plugin
//
// Emits one "spill"-layer data cell per sliding time window of a digitised-hit
// file, so the number of windows comes from the input rather than from a
// hand-set total. It runs the same first pass over the time columns as
// read_digitised_hits (io/DigitisedTimeScan.hpp) and must be given the same
// input_file, window_ns, step_ns, simulation and sim_particles, which
// workflows/lib/reconstruction.libsonnet passes to both from one set of
// parameters.
//
// TODO: the time columns are scanned twice at startup, once here and once in
// read_digitised_hits. Phlex drivers can take source objects as arguments
// (phlex::detail::driver_proxy, the data_cell_cursor-plus-sources form), so
// the cleaner design is to turn read_digitised_hits into a phlex::source that
// does the scan once and owns the windowing, and have this driver take that
// source and read windowing().size() from it. That needs read_digitised_hits
// reworked from PHLEX_REGISTER_PROVIDERS into a source class, so it is left
// for later; the double scan only reads the time columns and is small next to
// a reconstruction job.

#include "io/DigitisedTimeScan.hpp"
#include "phlex/configuration.hpp"
#include "phlex/driver.hpp"
#include "phlex/model/fixed_hierarchy.hpp"

#include <cstddef>
#include <cstdint>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <vector>

PHLEX_REGISTER_DRIVER(d, config) {
    auto const input_file = config.get<std::string>("input_file");
    auto const layer = config.get<std::string>("layer", std::string{"spill"});
    // Same defaults as read_digitised_hits.
    auto const simulation = config.get<bool>("simulation", false);
    auto const sim_particles = simulation && config.get<bool>("sim_particles", false);
    auto const window_ns = config.get<double>("window_ns", 3000.0);
    auto const step_ns = config.get<double>("step_ns", 1000.0);

    auto const scan = SHiP::trout::scan_input(
        input_file, SHiP::trout::digitised_ntuples(simulation, sim_particles), window_ns, step_ns);
    std::size_t n_windows = scan.windowing.size();
    spdlog::info("time_window_driver: hits span [{}, {}] ns, giving {} '{}' data cells",
                 scan.all.min, scan.all.max, n_windows, layer);

    // Optional cap, to process just the first part of the input.
    if (auto const max_spills = config.get_if_present<std::int64_t>("max_spills")) {
        if (*max_spills < 1)
            throw std::invalid_argument("time_window_driver: max_spills must be >= 1");
        if (static_cast<std::size_t>(*max_spills) < n_windows) {
            spdlog::info("time_window_driver: max_spills limits this to the first {} of {} windows",
                         *max_spills, n_windows);
            n_windows = static_cast<std::size_t>(*max_spills);
        }
    }

    // phlex matches the first parameter's type exactly, so the cursor (a cheap,
    // copyable view) is taken by value rather than const&.
    return d.driver(phlex::fixed_hierarchy{std::vector<std::vector<std::string>>{{"job", layer}}},
                    [n_windows, layer](phlex::data_cell_cursor job) {
                        for (std::size_t i = 0; i < n_windows; ++i)
                            job.yield_child(layer, i);
                    });
}
