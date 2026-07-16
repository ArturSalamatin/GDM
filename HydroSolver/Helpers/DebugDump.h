#pragma once

#include <string>

#ifdef GDM_DUMP_DEBUG
#include <filesystem>

#ifndef GDM_EXE_NAME
#define GDM_EXE_NAME "unknown"
#endif
#endif

namespace debug_dump {

class DebugDump {
public:
    static constexpr bool enabled() {
#ifdef GDM_DUMP_DEBUG
        return true;
#else
        return false;
#endif
    }

#ifdef GDM_DUMP_DEBUG
    static std::string path(const std::string& filename) {
        static const std::string base = init_base();
        return base + filename;
    }

    static std::string dir(const std::string& subdir) {
        static const std::string base = init_base();
        std::string full = base + subdir;
        std::filesystem::create_directories(full);
        return full + "/";
    }

private:
    static std::string init_base() {
        std::string base = "results/debug/" + std::string(GDM_EXE_NAME) + "/";
        std::filesystem::create_directories(base);
        return base;
    }
#endif
};

} // namespace debug_dump
