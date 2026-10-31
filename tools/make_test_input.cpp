// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// make_test_input.cpp — deterministic simulation-like input for smoke tests
//
// Writes one RNTuple per detector type (sim_particles, sim_hits, ubt_hits,
// sbt_hits, straw_tubes_hits, calorimeter_hits, timing_detector_hits), each
// a flat record-per-hit stream — the same shape shannon's
// digitised_output_module.cpp writes and read_digitised_hits.cpp reads (see
// TypedHitWriter/TypedHitWriters in typed_rntuple_writer.hpp) — so trout's
// reconstruction can be smoke tested without Geant4, aegir, or shannon.
//
// Usage: make_test_input [file] [n_events] [event_spacing_ns] [noise_offset_ns]
//   event_spacing_ns (default 0) time between the starts of successive events;
//                    use a sizeable fraction of step_ns or more to spread the
//                    input over several time windows.
//   noise_offset_ns  (default 0: off) adds, per track, one station-3 hit right
//                    on the track's path but this much later than it, with
//                    track_id -1: as close as the track's own hit in space, so
//                    only its time can reject it. Use more than the 100 ns
//                    spread of an event's tracks, or it can be in time for
//                    another track.

#include "io/typed_rntuple_writer.hpp"

#include <SHiP/SimHit.hpp>
#include <SHiP/SimParticle.hpp>
#include <SHiP/detectors/CaloHit.hpp>
#include <SHiP/detectors/SBTHit.hpp>
#include <SHiP/detectors/StrawTubesHit.hpp>
#include <SHiP/detectors/TimeDetHit.hpp>
#include <SHiP/detectors/UBTHit.hpp>
#include <SHiP/detectors/detector_id.hpp>
#include <SHiP/random/philox_rng.hpp>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <limits>
#include <optional>
#include <string>

using SHiP::trout::NamedHit;
using SHiP::trout::RNTupleFileService;
using SHiP::trout::TypedHitWriters;

// Global z positions [mm] of the four straw-tube stations
// (/SHiP/trackers/station_1..4 in the geometry DB).
constexpr std::array<double, 4> stat_z = {84070, 86070, 93070, 95070};

constexpr double speed_of_light = 299.792458;  // mm/ns

using SmokeTestWriters = TypedHitWriters<
    NamedHit<"sim_particles", SHiP::SimParticle>, NamedHit<"sim_hits", SHiP::SimHit>,
    NamedHit<"ubt_hits", SHiP::UBTHit>, NamedHit<"sbt_hits", SHiP::SBTHit>,
    NamedHit<"straw_tubes_hits", SHiP::StrawTubesHit>, NamedHit<"calorimeter_hits", SHiP::CaloHit>,
    NamedHit<"timing_detector_hits", SHiP::TimeDetHit>>;

// A finite, non-negative time argument [ns], or nullopt if arg isn't one.
std::optional<double> parse_time(char const* arg) {
    std::size_t consumed = 0;
    double value = 0.0;
    try {
        value = std::stod(arg, &consumed);
    } catch (std::exception const&) {
        return std::nullopt;
    }
    if (consumed == 0 || arg[consumed] != '\0' || !std::isfinite(value) || value < 0.0)
        return std::nullopt;
    return value;
}

int main(int argc, char* argv[]) {
    std::string const filename = argc > 1 ? argv[1] : "smoke_input.root";
    unsigned long n_events = 10UL;
    if (argc > 2) {
        std::size_t consumed = 0;
        try {
            n_events = std::stoul(argv[2], &consumed);
        } catch (std::exception const&) {
            consumed = 0;
        }
        if (consumed == 0 || argv[2][consumed] != '\0') {
            std::fprintf(stderr, "invalid event count: %s\n", argv[2]);
            return 1;
        }
    }
    // The event loop counts (and seeds the RNG) with a uint32_t.
    if (n_events > std::numeric_limits<std::uint32_t>::max()) {
        std::fprintf(stderr, "n_events out of range: %lu\n", n_events);
        return 1;
    }
    double event_spacing_ns = 0.0;
    if (argc > 3) {
        auto const parsed = parse_time(argv[3]);
        if (!parsed) {
            std::fprintf(stderr, "invalid event spacing: %s\n", argv[3]);
            return 1;
        }
        event_spacing_ns = *parsed;
    }
    double noise_offset_ns = 0.0;
    if (argc > 4) {
        auto const parsed = parse_time(argv[4]);
        if (!parsed) {
            std::fprintf(stderr, "invalid noise offset: %s\n", argv[4]);
            return 1;
        }
        noise_offset_ns = *parsed;
    }

    RNTupleFileService file_service{filename};
    SmokeTestWriters writers{file_service};

    constexpr SHiP::detector_id detectors[] = {
        SHiP::detector_id::UpstreamTagger, SHiP::detector_id::SurroundTagger,
        SHiP::detector_id::StrawTubes, SHiP::detector_id::Calorimeter,
        SHiP::detector_id::TimingDetector};

    for (std::uint32_t event = 0; event < n_events; ++event) {
        SHiP::random::PhiloxRng rng{0, 0x7E57DA7A, event};
        double const event_time = static_cast<double>(event) * event_spacing_ns;

        auto const n_tracks = 1 + static_cast<int>(rng.uniform(0.0, 3.0));
        for (int track = 0; track < n_tracks; ++track) {
            SHiP::SimParticle particle;
            particle.track_id = track;
            particle.parent_id = -1;
            particle.pdg_code = 13;
            particle.momentum = {rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5),
                                 rng.uniform(10.0, 100.0)};
            particle.energy = particle.momentum[2];
            writers.get<SHiP::SimParticle>().write(particle);

            // Straight-line track through the four straw stations, each hit
            // timed as Geant4's global time would be: the track's time plus
            // the flight from station 1 at the speed of light (the slopes are
            // too small to matter), up to jitter. Seeding pairs station-1 and
            // station-2 hits by time and requires the implied direction to be
            // forward (generate_seeds.cpp); the CKF then gates on the time the
            // track predicts at each station (SpectrometerMeasurements.hpp).
            double const z_ref = 0.5 * (stat_z.front() + stat_z.back());
            double const x_ref = rng.uniform(-300.0, 300.0);
            double const y_ref = rng.uniform(-300.0, 300.0);
            double const slope_x = particle.momentum[0] / particle.momentum[2];
            double const slope_y = particle.momentum[1] / particle.momentum[2];
            double const track_time = event_time + rng.uniform(0.0, 100.0);

            for (auto const detector : detectors) {
                if (detector == SHiP::detector_id::StrawTubes) {
                    for (double const z : stat_z) {
                        SHiP::SimHit sim_hit;
                        sim_hit.detector_id = static_cast<std::int32_t>(detector);
                        sim_hit.track_id = track;
                        sim_hit.pdg_code = particle.pdg_code;
                        sim_hit.position = {x_ref + slope_x * (z - z_ref) + rng.gaussian(0.0, 1.0),
                                            y_ref + slope_y * (z - z_ref) + rng.gaussian(0.0, 1.0),
                                            z};
                        sim_hit.momentum = particle.momentum;
                        sim_hit.energy_deposit = rng.uniform(0.0, 0.1);
                        sim_hit.time = track_time + (z - stat_z.front()) / speed_of_light +
                                       rng.uniform(-1.0, 1.0);
                        sim_hit.path_length = rng.uniform(0.0, 10.0);
                        writers.get<SHiP::SimHit>().write(sim_hit);
                        SHiP::StrawTubesHit hit;
                        hit.rec_hit = SHiP::from_sim_hit(sim_hit);
                        writers.get<SHiP::StrawTubesHit>().write(hit);
                    }
                    if (noise_offset_ns > 0.0) {
                        // Unsmeared, so it's as close to the track as the
                        // track's own station-3 hit; only time can reject it.
                        double const z = stat_z[2];
                        SHiP::SimHit sim_hit;
                        sim_hit.detector_id = static_cast<std::int32_t>(detector);
                        sim_hit.track_id = -1;
                        sim_hit.position = {x_ref + slope_x * (z - z_ref),
                                            y_ref + slope_y * (z - z_ref), z};
                        sim_hit.time = track_time + (z - stat_z.front()) / speed_of_light +
                                       noise_offset_ns;
                        writers.get<SHiP::SimHit>().write(sim_hit);
                        SHiP::StrawTubesHit hit;
                        hit.rec_hit = SHiP::from_sim_hit(sim_hit);
                        writers.get<SHiP::StrawTubesHit>().write(hit);
                    }
                } else {
                    SHiP::SimHit sim_hit;
                    sim_hit.detector_id = static_cast<std::int32_t>(detector);
                    sim_hit.track_id = track;
                    sim_hit.pdg_code = particle.pdg_code;
                    sim_hit.position = {rng.uniform(-500.0, 500.0), rng.uniform(-500.0, 500.0),
                                        rng.uniform(0.0, 10000.0)};
                    sim_hit.momentum = particle.momentum;
                    sim_hit.energy_deposit = rng.uniform(0.0, 0.1);
                    sim_hit.time = event_time + rng.uniform(0.0, 100.0);
                    sim_hit.path_length = rng.uniform(0.0, 10.0);
                    writers.get<SHiP::SimHit>().write(sim_hit);

                    switch (detector) {
                        case SHiP::detector_id::UpstreamTagger: {
                            SHiP::UBTHit hit;
                            hit.rec_hit = SHiP::from_sim_hit(sim_hit);
                            writers.get<SHiP::UBTHit>().write(hit);
                            break;
                        }
                        case SHiP::detector_id::SurroundTagger: {
                            SHiP::SBTHit hit;
                            hit.rec_hit = SHiP::from_sim_hit(sim_hit);
                            writers.get<SHiP::SBTHit>().write(hit);
                            break;
                        }
                        case SHiP::detector_id::Calorimeter: {
                            SHiP::CaloHit hit;
                            hit.rec_hit = SHiP::from_sim_hit(sim_hit);
                            writers.get<SHiP::CaloHit>().write(hit);
                            break;
                        }
                        case SHiP::detector_id::TimingDetector: {
                            SHiP::TimeDetHit hit;
                            hit.rec_hit = SHiP::from_sim_hit(sim_hit);
                            writers.get<SHiP::TimeDetHit>().write(hit);
                            break;
                        }
                        default:
                            break;  // StrawTubes handled above
                    }
                }
            }
        }
    }

    std::printf("Wrote %lu events to %s\n", n_events, filename.c_str());
    return 0;
}
