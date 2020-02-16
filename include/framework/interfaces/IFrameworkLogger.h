#pragma once

#include <fmt/format.h>

namespace Cppelix {
    class IFrameworkLogger {
    public:
        virtual void trace(const char *filename_in, int line_in, const char *funcname_in, fmt::basic_string_view<char> buf) = 0;
        virtual void debug(const char *filename_in, int line_in, const char *funcname_in, fmt::basic_string_view<char> buf) = 0;
        virtual void info(const char *filename_in, int line_in, const char *funcname_in, fmt::basic_string_view<char> buf) = 0;
        virtual void warn(const char *filename_in, int line_in, const char *funcname_in, fmt::basic_string_view<char> buf) = 0;
        virtual void error(const char *filename_in, int line_in, const char *funcname_in, fmt::basic_string_view<char> buf) = 0;
    protected:
        virtual ~IFrameworkLogger() = default;
    };
}