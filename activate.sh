# SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
#
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Pixi activation script for trout.
# Sourced automatically by `pixi run` / `pixi shell`.

export TROUT_ROOT="$PIXI_PROJECT_ROOT"

# Locally built plugins first, then installed plugins from the pixi env.
export PHLEX_PLUGIN_PATH="$PIXI_PROJECT_ROOT/build:${CONDA_PREFIX}/lib${PHLEX_PLUGIN_PATH:+:$PHLEX_PLUGIN_PATH}"
export LD_LIBRARY_PATH="$PIXI_PROJECT_ROOT/build${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# shipgeometry installs geometry DBs under $CONDA_PREFIX/share/geometry/.
# acts_geometry_provider resolves bare db_file names via this variable.
export SHIPGEOMETRY_ROOT="${SHIPGEOMETRY_ROOT:-$CONDA_PREFIX}"

# SHiPFieldService resolves bare .cvf filenames via $SHIPFIELD_ROOT/share/field/.
# Kept distinct from SHIPGEOMETRY_ROOT so field maps and geometry can be
# versioned independently.
export SHIPFIELD_ROOT="${SHIPFIELD_ROOT:-$CONDA_PREFIX}"
