#pragma once

#ifdef USE_SPDLOG

#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <framework/interfaces/IFrameworkLogger.h>
#include <framework/Bundle.h>

namespace Cppelix {

    class SpdlogFrameworkLogger : public IFrameworkLogger, public Bundle {
    public:
        SpdlogFrameworkLogger();
        ~SpdlogFrameworkLogger() final = default;

        void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;
        void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) final;


        bool start() final;
        bool stop() final;
        void injectDependencyManager(DependencyManager *mng) final {}
    };
}

#endif //USE_SPDLOG