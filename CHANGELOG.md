# Changelog

All notable changes to this project will be documented in this file.

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
