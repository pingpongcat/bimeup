#include "ifc/AsyncLoader.h"

#include "ifc/IfcModel.h"

namespace bimeup::ifc {

AsyncLoader::AsyncLoader() = default;

AsyncLoader::~AsyncLoader() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

std::future<std::unique_ptr<IfcModel>> AsyncLoader::LoadAsync(
    const std::string& path,
    ProgressCallback onProgress) {

    if (worker_.joinable()) {
        worker_.join();
    }
    cancelled_.store(false, std::memory_order_release);

    auto promise = std::make_shared<std::promise<std::unique_ptr<IfcModel>>>();
    auto future = promise->get_future();
    std::string pathCopy = path;

    worker_ = std::thread(
        [this, path = std::move(pathCopy), onProgress = std::move(onProgress), promise]() mutable {
            auto emit = [&](float pct, std::string_view phase) {
                if (onProgress) onProgress(pct, phase);
            };

            emit(0.0F, "starting");
            if (cancelled_.load(std::memory_order_acquire)) {
                promise->set_value(nullptr);
                return;
            }

            emit(10.0F, "parsing");
            auto model = std::make_unique<IfcModel>();
            const bool ok = model->LoadFromFile(path);

            if (!ok) {
                emit(100.0F, "failed");
                promise->set_value(nullptr);
                return;
            }

            if (cancelled_.load(std::memory_order_acquire)) {
                promise->set_value(nullptr);
                return;
            }

            emit(100.0F, "done");
            promise->set_value(std::move(model));
        });

    return future;
}

void AsyncLoader::Cancel() {
    cancelled_.store(true, std::memory_order_release);
}

bool AsyncLoader::IsCancelled() const {
    return cancelled_.load(std::memory_order_acquire);
}

} // namespace bimeup::ifc
