# Contributing to trout

Thank you for your interest in contributing to trout! As part of the SHiP Collaboration, we follow a set of standards to ensure code quality and maintainability.

## Development workflow

1. **Fork and clone** the repository.
2. **Environment**: the supported way to obtain build dependencies (ROOT, ACTS, phlex, the SHiP geometry/field/data-model packages, a recent CMake/Ninja/compiler) is [pixi](https://pixi.sh):
   ```bash
   pixi install
   pixi run build
   ```
   See `pixi.toml` for the full list of tasks (`configure`, `build`, `install`, `test`, `smoke`, `clean`).
3. **Pre-commit hooks**: we enforce style and licensing via [`prek`](https://github.com/j178/prek) (a drop-in `pre-commit` replacement). The hook tools come from the pixi `lint` environment, so versions are tracked in `pixi.lock` and run identically everywhere. Install the pre-commit and commit-msg hooks once:
   ```bash
   pixi run install-hooks
   ```
   Run all hooks manually at any time with `pixi run lint`.
4. **Branching**: create a feature branch for your changes.
5. **Coding standards**:
   - C++23; style enforced by `clang-format` (`.clang-format`) and `cpplint` (`CPPLINT.cfg`).
   - CMake formatting enforced by `gersemi`.
   - Every new file must carry an SPDX header (REUSE-compliant; verified by `reuse lint`).
6. **Commits**: we follow [Conventional Commits](https://www.conventionalcommits.org/), validated by `cz check` (commitizen). This also drives automated changelog generation via `git-cliff`.
7. **Testing**: run the end-to-end smoke test with `pixi run smoke`. Add new workflows or validation checks if you introduce new features.
8. **Submission**: open a pull request against `main`. Ensure the CI passes.

## Coding style

- **C++**: We use C++23. Style is defined in `.clang-format`.
- **Python**: Follow PEP 8.
- **Configuration**: Workflows are defined using [Jsonnet](https://jsonnet.org/); shared structure lives in `workflows/lib/`.

## Licensing

This project is licensed under the **LGPL-3.0-or-later**. All contributions must be compatible with this license. Each new file must include an SPDX identifier and copyright notice.
