#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build_release"

cmake -B "${BUILD_DIR}" -S "${PROJECT_ROOT}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBIMEUP_BUILD_TESTS=OFF

cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo "Release build complete. Binary: ${BUILD_DIR}/bin/bimeup"
