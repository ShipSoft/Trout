#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) CERN for the benefit of the SHiP Collaboration
#
# Cut a release: bump CMakeLists.txt VERSION, regenerate CHANGELOG.md via
# git-cliff, create a release commit and an annotated tag. Does NOT push.

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/release.sh <version>

  <version>   semver without leading 'v', e.g. 0.2.0

The script must be run from a clean working tree. It will:
  1. bump the VERSION line in CMakeLists.txt
  2. bump version and date-released in CITATION.cff (if present)
  3. regenerate CHANGELOG.md with `git cliff --tag v<version>`
  4. create commit `chore(release): v<version>`
  5. create annotated tag `v<version>`

Pushing is left to the operator:
  git push origin <branch> && git push origin v<version>
EOF
}

if [[ $# -ne 1 ]]; then
    usage >&2
    exit 64
fi

case "$1" in
    -h|--help) usage; exit 0 ;;
esac

VERSION="$1"
TAG="v${VERSION}"

if ! [[ "${VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "error: version must match MAJOR.MINOR.PATCH (got: ${VERSION})" >&2
    exit 64
fi

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "${REPO_ROOT}"

if ! git cliff --version >/dev/null 2>&1; then
    echo "error: 'git cliff' not available; install git-cliff (https://git-cliff.org/)" >&2
    exit 69
fi

if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "error: working tree is dirty; commit or stash changes first" >&2
    exit 65
fi

BRANCH="$(git rev-parse --abbrev-ref HEAD)"
if [[ "${BRANCH}" != "main" ]]; then
    echo "warning: on branch '${BRANCH}', not 'main'" >&2
fi

if git rev-parse --verify --quiet "refs/tags/${TAG}" >/dev/null; then
    echo "error: tag ${TAG} already exists" >&2
    exit 65
fi

CMAKE_FILE="CMakeLists.txt"
if ! grep -qE '^[[:space:]]*VERSION[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+' "${CMAKE_FILE}"; then
    echo "error: could not find VERSION line in ${CMAKE_FILE}" >&2
    exit 70
fi

sed -i -E "s/^([[:space:]]*VERSION[[:space:]]+)[0-9]+\.[0-9]+\.[0-9]+/\1${VERSION}/" "${CMAKE_FILE}"

if ! grep -qE "^[[:space:]]*VERSION[[:space:]]+${VERSION//./\\.}([[:space:]]|$)" "${CMAKE_FILE}"; then
    echo "error: failed to update VERSION in ${CMAKE_FILE}" >&2
    git checkout -- "${CMAKE_FILE}"
    exit 70
fi

CITATION_FILE="CITATION.cff"
if [[ -f "${CITATION_FILE}" ]]; then
    sed -i -E "s/^version: .*/version: ${VERSION}/" "${CITATION_FILE}"
    sed -i -E "s/^date-released: .*/date-released: \"$(date -u +%Y-%m-%d)\"/" "${CITATION_FILE}"
fi

git cliff --tag "${TAG}" -o CHANGELOG.md

git add "${CMAKE_FILE}" CHANGELOG.md
if [[ -f "${CITATION_FILE}" ]]; then
    git add "${CITATION_FILE}"
fi
git commit -m "chore(release): ${TAG}"
git tag -a "${TAG}" -m "Release ${TAG}"

cat <<EOF

Release ${TAG} prepared on branch '${BRANCH}'.

Next steps:
  git push origin ${BRANCH}
  git push origin ${TAG}

(or:  git push --follow-tags origin ${BRANCH})
EOF
