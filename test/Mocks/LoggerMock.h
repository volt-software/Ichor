#pragma once

#include <ichor/services/logging/Logger.h>
#include <fmt/format.h>

struct LoggerMock : public ILogger, public AdvancedService<LoggerMock> {
    LoggerMock(DependencyRegister &reg, Properties props) : AdvancedService<LoggerMock>(std::move(props)) {}
    void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) override {
        if(level <= LogLevel::LOG_TRACE) {
            fmt::basic_memory_buffer<char> buf{};
            fmt::vformat_to(std::back_inserter(buf), format_str, args);
            logs.emplace_back(buf.data(), buf.size());
        }
    }

    void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) override {
        if(level <= LogLevel::LOG_DEBUG) {
            fmt::basic_memory_buffer<char> buf{};
            fmt::vformat_to(std::back_inserter(buf), format_str, args);
            logs.emplace_back(buf.data(), buf.size());
        }
    }

    void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) override {
        if(level <= LogLevel::LOG_INFO) {
            fmt::basic_memory_buffer<char> buf{};
            fmt::vformat_to(std::back_inserter(buf), format_str, args);
            logs.emplace_back(buf.data(), buf.size());
        }
    }

    void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) override {
        if(level <= LogLevel::LOG_WARN) {
            fmt::basic_memory_buffer<char> buf{};
            fmt::vformat_to(std::back_inserter(buf), format_str, args);
            logs.emplace_back(buf.data(), buf.size());
        }
    }

    void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) override {
        if(level <= LogLevel::LOG_ERROR) {
            fmt::basic_memory_buffer<char> buf{};
            fmt::vformat_to(std::back_inserter(buf), format_str, args);
            logs.emplace_back(buf.data(), buf.size());
        }
    }

    void setLogLevel(LogLevel _level) noexcept override {
        level = _level;
    }
    [[nodiscard]] LogLevel getLogLevel() const noexcept override {
        return level;
    }
    LogLevel level{LogLevel::LOG_WARN};
    std::vector<std::string> logs;
};
