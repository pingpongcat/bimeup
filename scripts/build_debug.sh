#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build_debug"

cmake -B "${BUILD_DIR}" -S "${PROJECT_ROOT}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBIMEUP_BUILD_TESTS=ON

cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo "Debug build complete. Binary: ${BUILD_DIR}/bin/bimeup"
