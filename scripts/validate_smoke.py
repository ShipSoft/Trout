#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Validate the smoke-test outputs of the digitisation workflow.

Checks that the digitised-output file contains all per-detector RNTuples and
that the validation file contains all histograms, each with entries.

Given the input file as well, also checks that the spectrometer tracking
found every generated track: make_test_input writes each track's four
straw-tube hits consecutively, one per station, and each must come back as
one output track built from exactly those four hits.
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


def check_truth(output_path, input_path):
    hits = ROOT.RDataFrame("straw_tubes_hits", input_path).AsNumpy(["hit.recHit.position"])
    positions = np.array([np.asarray(p) for p in hits["hit.recHit.position"]])
    if len(positions) == 0 or len(positions) % HITS_PER_TRACK:
        return [f"{input_path}: expected a multiple of {HITS_PER_TRACK} straw-tube hits, "
                f"found {len(positions)}"]
    generated = collections.Counter(hit_key(t[:, 0], t[:, 1])
                                    for t in positions.reshape(-1, HITS_PER_TRACK, 3))

    tracks = ROOT.RDataFrame("spectrometer_tracks", output_path).AsNumpy(
        ["hit.inputMeasurementsX", "hit.inputMeasurementsY"])
    found = collections.Counter(hit_key(np.asarray(xs), np.asarray(ys))
                                for xs, ys in zip(tracks["hit.inputMeasurementsX"],
                                                  tracks["hit.inputMeasurementsY"]))

    # Counter subtraction keeps only positive counts, so each generated track
    # needs its own output track, even if another track has the same hits.
    n_generated = sum(generated.values())
    missing = sum((generated - found).values())
    print(f"spectrometer tracking: found {n_generated - missing} of {n_generated} "
          f"generated tracks ({sum((found - generated).values())} other hit combinations)")
    if missing:
        return [f"{output_path}: {missing} of {n_generated} generated tracks "
                "were not reconstructed"]
    return []


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        print(f"usage: {sys.argv[0]} <smoke_results.root> <reco_validation.root> [input.root]",
              file=sys.stderr)
        sys.exit(2)
    errors = check_ntuples(sys.argv[1]) + check_histograms(sys.argv[2])
    if len(sys.argv) == 4:
        errors += check_truth(sys.argv[1], sys.argv[3])
    for error in errors:
        print(error, file=sys.stderr)
    sys.stdout.flush()
    sys.stderr.flush()
    # Skip interpreter shutdown to dodge a PyROOT RNTuple teardown crash.
    os._exit(1 if errors else 0)
