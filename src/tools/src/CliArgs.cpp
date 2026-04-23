#include <tools/CliArgs.h>

#include <charconv>
#include <sstream>
#include <string>
#include <string_view>

namespace bimeup::tools {

namespace {

bool ParseUint32(std::string_view text, std::uint32_t& out) {
    if (text.empty()) return false;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
    }
    std::uint32_t value = 0;
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || ptr != text.data() + text.size()) return false;
    out = value;
    return true;
}

CliArgs MakeError(std::string message) {
    CliArgs result;
    result.ok = false;
    result.error = std::move(message);
    return result;
}

}  // namespace

CliArgs ParseCliArgs(int argc, const char* const* argv) {
    CliArgs result;
    if (argc <= 1) return result;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            result.help = true;
            continue;
        }

        // `--device-id N` or `--device-id=N`.
        constexpr std::string_view kDeviceIdFlag = "--device-id";
        if (arg == kDeviceIdFlag) {
            if (i + 1 >= argc) {
                return MakeError("--device-id requires a non-negative integer value");
            }
            std::uint32_t value = 0;
            if (!ParseUint32(argv[i + 1], value)) {
                std::ostringstream os;
                os << "--device-id expects a non-negative integer, got '" << argv[i + 1] << "'";
                return MakeError(os.str());
            }
            result.deviceId = value;
            ++i;
            continue;
        }
        if (arg.size() > kDeviceIdFlag.size() &&
            arg.substr(0, kDeviceIdFlag.size() + 1) == std::string(kDeviceIdFlag) + "=") {
            std::string_view value = arg.substr(kDeviceIdFlag.size() + 1);
            std::uint32_t parsed = 0;
            if (!ParseUint32(value, parsed)) {
                std::ostringstream os;
                os << "--device-id expects a non-negative integer, got '" << value << "'";
                return MakeError(os.str());
            }
            result.deviceId = parsed;
            continue;
        }

        if (!arg.empty() && arg.front() == '-') {
            std::ostringstream os;
            os << "Unknown flag: '" << arg << "' (try --help)";
            return MakeError(os.str());
        }

        if (result.ifcPath.has_value()) {
            std::ostringstream os;
            os << "Unexpected extra positional argument: '" << arg
               << "' (only one IFC path is accepted)";
            return MakeError(os.str());
        }
        result.ifcPath = std::string(arg);
    }

    return result;
}

std::string CliHelpText(std::string_view programName) {
    std::ostringstream os;
    os << "Usage: " << programName << " [options] [<ifc-path>]\n"
       << "\n"
       << "Bimeup — Vulkan BIM viewer.\n"
       << "\n"
       << "Positional:\n"
       << "  <ifc-path>           Path to an IFC file to load at startup.\n"
       << "                       Defaults to the bundled sample when omitted.\n"
       << "\n"
       << "Options:\n"
       << "  -h, --help           Show this help text and exit.\n"
       << "  --device-id <N>      Use physical device #N (0-based) instead of the\n"
       << "                       auto-picked best GPU. Indices match the order of\n"
       << "                       the `Vulkan device: ...` lines printed at startup.\n";
    return os.str();
}

}  // namespace bimeup::tools
