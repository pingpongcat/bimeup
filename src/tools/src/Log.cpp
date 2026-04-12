#include "tools/Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace bimeup::tools {

std::shared_ptr<spdlog::logger> Log::s_logger;

void Log::Init(const std::string& appName) {
    if (s_logger) {
        spdlog::drop(s_logger->name());
    }

    s_logger = spdlog::stdout_color_mt(appName);
    s_logger->set_level(spdlog::level::trace);
    s_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
}

void Log::Shutdown() {
    if (s_logger) {
        spdlog::drop(s_logger->name());
        s_logger.reset();
    }
}

std::shared_ptr<spdlog::logger>& Log::GetLogger() {
    return s_logger;
}

} // namespace bimeup::tools
