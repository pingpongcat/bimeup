#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace bimeup::ifc {

class IfcModel;

using ProgressCallback = std::function<void(float percent, std::string_view phase)>;

// AsyncLoader runs IfcModel::LoadFromFile on a worker thread, emitting
// progress events at phase boundaries: "starting" (0%), "parsing" (10%),
// "done" (100%). The cancel flag is sampled at every phase boundary —
// requesting cancellation mid-parse cannot abort web-ifc itself, so the
// parse will run to completion and the result will simply be discarded.
class AsyncLoader {
public:
    AsyncLoader();
    ~AsyncLoader();

    AsyncLoader(const AsyncLoader&) = delete;
    AsyncLoader& operator=(const AsyncLoader&) = delete;
    AsyncLoader(AsyncLoader&&) = delete;
    AsyncLoader& operator=(AsyncLoader&&) = delete;

    std::future<std::unique_ptr<IfcModel>> LoadAsync(
        const std::string& path,
        ProgressCallback onProgress = nullptr);

    void Cancel();
    [[nodiscard]] bool IsCancelled() const;

private:
    std::atomic<bool> cancelled_{false};
    std::thread worker_;
};

} // namespace bimeup::ifc
