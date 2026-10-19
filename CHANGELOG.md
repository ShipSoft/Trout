# Changelog

All notable changes to this project will be documented in this file.

## [0.3.0] - 2026-10-19

### Refactor

- [**breaking**] Move Trout into SHiP::trout and onto data-model 0.6

### Miscellaneous

- Add advisory builds against ShipSoft library mains

### Build

- Resolve SHiP packages from ship-experiment/stable
- Keep ship-random below 0.3
- Move to ship-random 0.3
- Move to ship-random 0.4
## [0.2.0] - 2026-10-14

### Features

- [**breaking**] Require phlex 0.4.1
- [**breaking**] Load field maps in the SHiP field-map format

### Bug fixes

- *(release)* Tag the release only after it lands on main
- *(tracking)* Measure both local coordinates of a station hit
- *(geometry)* Let tracks leave the station volume

### Refactor

- Take PhiloxRng from ship-random
- Use field_service's ACTS adapter and placement helpers

### Documentation

- *(tracking)* Note that phlex 0.4.1 lifts the unfold workarounds

### Testing

- *(smoke)* Require every generated track to be reconstructed
- *(smoke)* Count repeated hits and duplicate tracks in truth check

### Miscellaneous

- *(lint)* Move gersemi to 0.29.1
- Sync shared configs
- Update pixi lock file
- Update pixi lock file

### Build

- *(pixi)* Keep shipdatamodel out of the base dependencies
- Keep shipdatamodel below 0.6
## [0.1.0] - 2026-09-21

### Features

- Spectrometer tracking
- Spectrometer tracking
- Spectrometer tracking
- Add all detectors
- Reconstruction
- Reconstruction
- Reconstruction
- Reconstruction
- Reconstruction
- Trout. Assisted by claude sonnet 5

### Bug fixes

- *(build)* Install all reconstruction plugins, enable warnings
- *(geometry)* Make container bounds unit-consistent, search stations in one pass
- *(field)* Fail anomalous field lookups instead of silently returning zero
- *(field)* Catch non-standard exceptions from the field evaluator
- *(smoke)* Generate findable tracks and all smoke inputs in-tree
- *(smoke)* Point the smoke workflow at the inputs it generates
- *(smoke)* Load the field map from the working directory
- *(smoke)* Reject event counts with trailing characters
- *(pixi)* Keep the committed lock resolvable without a sibling checkout
- *(release)* Match the CMake project() version declaration

### Refactor

- Remove dead code and tighten namespace hygiene
- *(workflows)* Factor shared jsonnet body, fix field-map typo
- Deduplicate the placeholder per-subdetector modules

### Documentation

- Describe the actual project and toolchain

### Performance

- Share the digitised hit collections between consumers

### Styling

- End README.md with a newline

### Miscellaneous

- *(licensing)* Trim vendored ACTS headers and label them MPL-2.0
- Adopt the standard SHiP workflow set
- Spell out the token scopes each caller needs
- Gate merges on uniform aggregator checks

### Build

- *(pixi)* Make the default environment self-sufficient, add lint env
