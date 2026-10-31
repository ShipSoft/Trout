#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Quick smoke test for CI: generate all inputs (geometry DB, field map,
# simulation-like hits), run the reconstruction workflow end to end, and
# check that the outputs were produced. Self-contained: everything comes
# from the pixi environment and this repository.
set -euo pipefail

# Geometry DB from the installed shipgeometry package.
build_geometry -o smoke_geometry.db

# Constant zero field over the field-map volume: make_test_input generates
# straight-line tracks, so a field-free propagation matches them exactly.
generate_constant_fieldmap smoke_field.root smoke_field 0 0 0 -4000 4000 -4000 4000 0 100000

# Ten events 1 us apart, spanning several of the default 3 us time windows,
# with a noise hit along each track, 200 ns late: more than the 100 ns spread
# of an event's tracks, so it is out of time for all of them.
./build/make_test_input smoke_input.root 10 1000 200

# Remove outputs from previous runs so stale files cannot pass the checks.
rm -f smoke_reco_output.root smoke_reco_validation.root
rm -f smoke_indexed_output.root smoke_indexed_validation.root

# Runs a workflow and checks that the reader read the straw-tube hits as
# expected ("streamed" or "indexed"), so that comparing the two runs below
# can't end up comparing one mode with itself.
run_workflow() {
    local log
    log=$(mktemp)
    phlex -c "$1" 2>&1 | tee "$log"
    if ! grep -q "straw_tubes_hits is up to .* so it is $2" "$log"; then
        echo "$1: straw_tubes_hits were not $2" >&2
        rm -f "$log"
        exit 1
    fi
    rm -f "$log"
}

run_workflow workflows/smoke.jsonnet streamed
run_workflow workflows/smoke_indexed.jsonnet indexed

for f in smoke_reco_output.root smoke_reco_validation.root \
    smoke_indexed_output.root smoke_indexed_validation.root; do
    if ! [ -s "$f" ]; then
        echo "missing output: $f" >&2
        exit 1
    fi
done
python scripts/validate_smoke.py smoke_reco_output.root smoke_reco_validation.root smoke_input.root
python scripts/validate_smoke.py smoke_indexed_output.root smoke_indexed_validation.root \
    smoke_input.root smoke_reco_output.root
echo "Smoke test OK"
