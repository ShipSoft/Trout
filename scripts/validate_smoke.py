#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Validate the smoke-test outputs of the digitisation workflow.

Checks that the digitised-output file contains all per-detector RNTuples and
that the validation file contains all histograms, each with entries.

Given the input file as well, also checks that the spectrometer tracking
found every generated track and nothing else: make_test_input writes each
track's four straw-tube hits consecutively, one per station, and each must
come back as exactly one output track built from exactly those four hits, even
though time windows overlap and out-of-time noise hits (track_id -1) lie along
the tracks. It also checks that the overlapping windows write every simulated
hit and particle out exactly once.

Given another output as well, also checks that both hold the same tracks and
as many entries in each RNTuple: streamed and indexed reading must agree.
"""

import collections
import os
import sys

import numpy as np
import ROOT

NTUPLES = [
    "calorimeter",
    "surround_tagger",
    "spectrometer_tracks",
    "timing_detector",
    "upstream_tagger",
    "sim_hits",
    "sim_particles"
]

HISTOGRAMS = [
    "h_upstream_tagger_multiplicity",
    "h_surround_tagger_multiplicity",
    "h_spectrometer_track_multiplicity",
    "h_calorimeter_multiplicity",
    "h_timing_detector_multiplicity",
    "h_sim_hit_multiplicity",
    "h_sim_particle_multiplicity",
    "h_sim_hit_x",
    "h_sim_hit_y",
    "h_sim_hit_z",
    "h_ref_x",
    "h_ref_y",
    "h_ref_z",
    "h_sim_particle_vtx_x",
    "h_sim_particle_vtx_y",
    "h_sim_particle_vtx_z",
]


def check_ntuples(path):
    errors = []
    for name in NTUPLES:
        try:
            reader = ROOT.RNTupleReader.Open(name, path)
        except Exception as e:
            errors.append(f"{path}: cannot open RNTuple '{name}': {e}")
            continue
        if reader.GetNEntries() == 0:
            errors.append(f"{path}: RNTuple '{name}' has no entries")
    return errors


def check_histograms(path):
    # TFile.Open reports failure through nullptr/IsZombie rather than a
    # Python exception.
    file = ROOT.TFile.Open(path)
    if not file or file.IsZombie():
        return [f"{path}: cannot open file"]
    errors = []
    for name in HISTOGRAMS:
        hist = file.Get(name)
        if not hist:
            errors.append(f"{path}: missing histogram '{name}'")
        elif hist.GetEntries() == 0:
            errors.append(f"{path}: histogram '{name}' has no entries")
    file.Close()
    return errors


HITS_PER_TRACK = 4  # one straw-tube hit per tracking station


def hit_key(xs, ys):
    """Order-independent key for a track's hits, robust to float noise."""
    return tuple(sorted(zip(np.round(xs, 3), np.round(ys, 3))))


def found_tracks(output_path):
    """The output's tracks, each as the key of the hits it was built from."""
    tracks = ROOT.RDataFrame("spectrometer_tracks", output_path).AsNumpy(
        ["hit.input_measurements_x", "hit.input_measurements_y"])
    return collections.Counter(hit_key(np.asarray(xs), np.asarray(ys))
                               for xs, ys in zip(tracks["hit.input_measurements_x"],
                                                 tracks["hit.input_measurements_y"]))


def check_truth(output_path, input_path):
    hits = ROOT.RDataFrame("straw_tubes_hits", input_path).AsNumpy(
        ["hit.rec_hit.position", "hit.rec_hit.track_id"])
    positions = np.array([np.asarray(p) for p in hits["hit.rec_hit.position"]])
    on_track = np.asarray(hits["hit.rec_hit.track_id"]) >= 0
    n_noise = np.count_nonzero(~on_track)
    positions = positions[on_track]
    if len(positions) == 0 or len(positions) % HITS_PER_TRACK:
        return [f"{input_path}: expected a multiple of {HITS_PER_TRACK} straw-tube hits, "
                f"found {len(positions)}"]
    generated = collections.Counter(hit_key(t[:, 0], t[:, 1])
                                    for t in positions.reshape(-1, HITS_PER_TRACK, 3))
    found = found_tracks(output_path)

    # Counter subtraction keeps only positive counts, so each generated track
    # needs its own output track, even if another track has the same hits.
    n_generated = sum(generated.values())
    missing = sum((generated - found).values())
    surplus = found - generated
    duplicated = sum(n for key, n in surplus.items() if key in generated)
    others = sum(surplus.values()) - duplicated
    print(f"spectrometer tracking: found {n_generated - missing} of {n_generated} "
          f"generated tracks ({duplicated} duplicates, {others} other hit combinations, "
          f"{n_noise} noise hits in the input)")
    errors = []
    if missing:
        errors.append(f"{output_path}: {missing} of {n_generated} generated tracks "
                      "were not reconstructed")
    if duplicated:
        errors.append(f"{output_path}: {duplicated} extra copies of generated tracks")
    if others:
        errors.append(f"{output_path}: {others} tracks match no generated track")
    return errors


def check_written_once(output_path, input_path):
    """Windows overlap, but each hit is owned by one, so comes out exactly once."""
    errors = []
    times = [np.sort(ROOT.RDataFrame("sim_hits", path).AsNumpy(["hit.time"])["hit.time"])
             for path in (input_path, output_path)]
    if not np.array_equal(times[0], times[1]):
        errors.append(f"{output_path}: sim_hits differ from the input "
                      f"({len(times[1])} entries, {len(times[0])} in the input)")
    n_particles = [ROOT.RNTupleReader.Open("sim_particles", path).GetNEntries()
                   for path in (input_path, output_path)]
    if n_particles[0] != n_particles[1]:
        errors.append(f"{output_path}: {n_particles[1]} sim_particles, "
                      f"{n_particles[0]} in the input")
    return errors


def check_same_output(output_path, other_path):
    """Streamed and indexed reading must give the same reconstruction."""
    errors = []
    if found_tracks(output_path) != found_tracks(other_path):
        errors.append(f"{output_path}: tracks differ from those in {other_path}")
    for name in NTUPLES:
        entries = [ROOT.RNTupleReader.Open(name, path).GetNEntries()
                   for path in (output_path, other_path)]
        if entries[0] != entries[1]:
            errors.append(f"{output_path}: {entries[0]} entries in '{name}', "
                          f"{entries[1]} in {other_path}")
    return errors


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4, 5):
        print(f"usage: {sys.argv[0]} <smoke_results.root> <reco_validation.root> [input.root "
              "[other_results.root]]", file=sys.stderr)
        sys.exit(2)
    errors = check_ntuples(sys.argv[1]) + check_histograms(sys.argv[2])
    if len(sys.argv) >= 4:
        errors += check_truth(sys.argv[1], sys.argv[3])
        errors += check_written_once(sys.argv[1], sys.argv[3])
    if len(sys.argv) == 5:
        errors += check_same_output(sys.argv[1], sys.argv[4])
    for error in errors:
        print(error, file=sys.stderr)
    sys.stdout.flush()
    sys.stderr.flush()
    # Skip interpreter shutdown to dodge a PyROOT RNTuple teardown crash.
    os._exit(1 if errors else 0)
