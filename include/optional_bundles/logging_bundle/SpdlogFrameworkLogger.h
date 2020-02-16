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

        static constexpr std::string_view version = "1.0.0";

        void trace(const char *filename_in, int line_in, const char *funcname_in, fmt::basic_string_view<char> buf) final;
        void debug(const char *filename_in, int line_in, const char *funcname_in, fmt::basic_string_view<char> buf) final;
        void info(const char *filename_in, int line_in, const char *funcname_in, fmt::basic_string_view<char> buf) final;
        void warn(const char *filename_in, int line_in, const char *funcname_in, fmt::basic_string_view<char> buf) final;
        void error(const char *filename_in, int line_in, const char *funcname_in, fmt::basic_string_view<char> buf) final;


        bool start() final;
        bool stop() final;
    };
}

#endif //USE_SPDLOG