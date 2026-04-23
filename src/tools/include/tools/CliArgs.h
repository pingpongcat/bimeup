#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bimeup::tools {

/// Parsed command-line arguments for the bimeup viewer.
///
/// `ok == false` with `error` populated means the parse failed; the caller
/// should print `error` (and optionally the help text) and exit non-zero.
/// `help == true` means the user asked for `-h` / `--help`; the caller
/// should print `CliHelpText` and exit zero.
struct CliArgs {
    bool help = false;
    std::optional<std::uint32_t> deviceId;
    std::optional<std::string> ifcPath;

    bool ok = true;
    std::string error;
};

/// Parse `argv` (including `argv[0]` program name).
CliArgs ParseCliArgs(int argc, const char* const* argv);

/// Human-readable help text describing every supported flag.
std::string CliHelpText(std::string_view programName = "bimeup");

}  // namespace bimeup::tools
