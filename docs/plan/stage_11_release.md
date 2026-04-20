## Stage 11 — Polish, Packaging & Release

**Goal**: Production-ready application with installers, documentation, and final polish.

**Sessions**: 2–3

### Modules involved
- All modules (final integration)
- CI/CD (packaging)

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 11.1 | Implement drag-and-drop IFC file loading | Integration test: drop file → model loads | Platform integration |
| 11.2 | Implement recent files list in UI | Unit test: files added, persisted to config, loaded on restart | UI feature |
| 11.3 | Implement user preferences (theme, render settings, VR settings) | Unit test: settings save/load round-trip | Settings in tools/ |
| 11.4 | Create Linux AppImage packaging in CI | CI produces downloadable AppImage | `.github/workflows/`, CMake install rules |
| 11.5 | Create Windows NSIS installer in CI | CI produces downloadable .exe installer | NSIS script |
| 11.6 | Write user-facing README with screenshots, build instructions | README exists and is accurate | `README.md` |
| 11.7 | Run full integration test suite on both platforms | All tests pass on Linux + Windows CI | Test results |
| 11.8 | Performance profiling pass — identify and fix top 3 bottlenecks | Benchmark before/after shows improvement | Profiling results |
| 11.9 | Accessibility review — keyboard navigation, screen reader hints | All panels navigable by keyboard | UI improvements |

---

