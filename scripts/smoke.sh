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
build_geometry smoke_geometry.db

# Constant zero field over the field-map volume: make_test_input generates
# straight-line tracks, so a field-free propagation matches them exactly.
generate_constant_cvf smoke_field.cvf 0 0 0 -4000 4000 -4000 4000 0 100000

./build/make_test_input smoke_input.root 10

# Remove outputs from previous runs so stale files cannot pass the checks.
rm -f smoke_reco_output.root smoke_reco_validation.root

phlex -c workflows/smoke.jsonnet

for f in smoke_reco_output.root smoke_reco_validation.root; do
    if ! [ -s "$f" ]; then
        echo "missing output: $f" >&2
        exit 1
    fi
done
python scripts/validate_smoke.py smoke_reco_output.root smoke_reco_validation.root
echo "Smoke test OK"
