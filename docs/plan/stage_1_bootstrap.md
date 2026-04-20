## Stage 1 — Project Bootstrap & Build System

**Goal**: Empty project that compiles on Linux and Windows, runs tests, produces a "hello world" log message.

**Sessions**: 1–2

### Modules involved
- `tools/` (logging wrapper)
- Root CMake

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 1.1 | Create root `CMakeLists.txt` with C++20, module structure, option flags (`BIMEUP_BUILD_TESTS`, `BIMEUP_ENABLE_VR`, `BIMEUP_ENABLE_RAYTRACING`) | CMake configures without errors | `CMakeLists.txt` |
| 1.2 | Add git submodules: googletest, spdlog, glm | Submodules clone and build | `external/`, `.gitmodules` |
| 1.3 | Create `tools/` module with `Log` wrapper around spdlog | Unit test: `Log::Init()` succeeds, `LOG_INFO("test")` doesn't crash | `src/tools/` |
| 1.4 | Create `tools/Config` — simple key-value config loader (INI or JSON) | Unit test: parse config string, get values by key, default values | `src/tools/include/tools/Config.h` |
| 1.5 | Create `app/main.cpp` — initializes logging, prints version | Builds and runs, outputs version to stdout | `app/main.cpp` |
| 1.6 | Create build scripts: `scripts/build_debug.sh`, `scripts/build_debug.bat`, `scripts/build_release.sh`, `scripts/build_release.bat` | Scripts run and produce binaries | `scripts/` |
| 1.7 | Set up GitHub Actions CI: Linux + Windows, Debug + Release, run tests | CI passes on push | `.github/workflows/ci.yml` |
| 1.8 | Enable clang-tidy and sanitizers (ASan, UBSan) in Debug | CI runs clang-tidy, sanitizers active in Debug tests | `cmake/Sanitizers.cmake`, `.clang-tidy` |

### Expected APIs after Stage 1

```cpp
// tools/include/tools/Log.h
namespace bimeup::tools {
    class Log {
    public:
        static void Init(const std::string& appName);
        static void Shutdown();
        // Macros: LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL
    };
}

// tools/include/tools/Config.h
namespace bimeup::tools {
    class Config {
    public:
        bool Load(const std::string& path);
        bool LoadFromString(const std::string& content);
        std::string GetString(const std::string& key, const std::string& defaultVal = "") const;
        int GetInt(const std::string& key, int defaultVal = 0) const;
        float GetFloat(const std::string& key, float defaultVal = 0.0f) const;
        bool GetBool(const std::string& key, bool defaultVal = false) const;
    };
}
```

---

