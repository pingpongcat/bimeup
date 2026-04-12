#include <tools/Log.h>

#include <cstdio>

int main() {
    std::printf("bimeup v%s\n", BIMEUP_VERSION);

    bimeup::tools::Log::Init("bimeup");
    LOG_INFO("Bimeup v{} starting", BIMEUP_VERSION);
    LOG_INFO("Bimeup shutting down");
    bimeup::tools::Log::Shutdown();

    return 0;
}
