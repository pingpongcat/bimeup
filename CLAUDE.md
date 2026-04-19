# Bimeup — Session Instructions

## Project
Bimeup is a C++ BIM viewer (Vulkan + ImGui + OpenXR). Full plan: `PLAN.md`. Progress: `PROGRESS.md`.

## Session Protocol

At the START of every session:
1. Read `PROGRESS.md` — find the current stage and next unchecked task
2. Read only the PLAN.md section for that stage — do NOT read the whole plan
3. Read only the headers/APIs of modules you will touch — do NOT explore the full codebase
4. State what you will do and which task(s) you will complete

For EACH task:
1. Write the test FIRST (TDD — test must fail before implementation)
2. Implement minimal code to pass the test
3. Run the test — confirm it passes
4. Run the tests for the module(s) you touched — confirm no local regressions
   (e.g. `ctest -L renderer -j4`, or a gtest filter directly on the binary
   like `./bin/renderer_tests --gtest_filter='ClipPlane*'` — note that
   `--gtest_filter` via ctest no longer narrows execution because each
   ctest entry is already a single test or fixture)
5. Mark the task done in `PROGRESS.md` (change `- [ ]` to `- [x]`)
6. Commit with message: `[stage.task] description`

At the END of every session:
1. Update `PROGRESS.md` — check off completed tasks, update "Current Task" to the next one
2. If this session completed the last task of a stage:
   - Update "Current Stage" to the next stage
   - Run the FULL test suite (`ctest -j$(nproc) --output-on-failure`) as the stage gate
3. Summarize what was done and what comes next

## Hard Rules
- NEVER read more than 2 modules per session
- NEVER implement without a failing test first
- NEVER leave tests failing at end of session
- NEVER skip the full test suite at stage completion (it's the gate for moving to the next stage)
- Keep each task to a single commit
- If a task is too large for one session, split it and update PROGRESS.md

## Build & Test Commands
```bash
# Configure (from bimeup/)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBIMEUP_BUILD_TESTS=ON

# Build
cmake --build build -j$(nproc)

# Run all tests
cd build && ctest -j$(nproc) --output-on-failure

# Run specific module tests
cd build && ctest -R tools --output-on-failure
cd build && ctest -R platform --output-on-failure

# Renderer suite (per-test ctest entries with cpu/gpu labels)
cd build && ctest -L renderer -j4 --output-on-failure   # all renderer tests (~15 s)
cd build && ctest -L cpu -j$(nproc)                     # CPU/math only (~2 s)
cd build && ctest -L gpu -j4                            # GPU only (~13 s)
cd build && ctest -R RenderLoopTest                     # one fixture
```

## Dependencies
All in `external/` as git submodules. Never install system-wide.

## Reference Code
- `../engine_web-ifc/` — C++ IFC parser (geometry, parsing, schema modules)
- `../web-ifc-viewer/` — TypeScript BIM viewer (UI patterns, feature reference)
