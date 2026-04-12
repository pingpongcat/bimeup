#pragma once

#include <spdlog/spdlog.h>
#include <string>
#include <memory>

namespace bimeup::tools {

class Log {
public:
    static void Init(const std::string& appName);
    static void Shutdown();

    static std::shared_ptr<spdlog::logger>& GetLogger();

private:
    static std::shared_ptr<spdlog::logger> s_logger;
};

} // namespace bimeup::tools

#define LOG_TRACE(...) ::bimeup::tools::Log::GetLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::bimeup::tools::Log::GetLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...)  ::bimeup::tools::Log::GetLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)  ::bimeup::tools::Log::GetLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::bimeup::tools::Log::GetLogger()->error(__VA_ARGS__)
#define LOG_FATAL(...) ::bimeup::tools::Log::GetLogger()->critical(__VA_ARGS__)
