#pragma once

#include <ichor/services/logging/Logger.h>
#include <fmt/format.h>
#include <iostream>

struct LoggerMock : public ILogger, public AdvancedService<LoggerMock> {
    LoggerMock(DependencyRegister &reg, Properties props) : AdvancedService<LoggerMock>(std::move(props)) {}
    void trace(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) override {
        if(level <= LogLevel::LOG_TRACE) {
            fmt::basic_memory_buffer<char> buf{};
            fmt::vformat_to(std::back_inserter(buf), format_str, args);
            logs.emplace_back(buf.data(), buf.size());
            fmt::print("TRACE ");
            if (filename_in != nullptr) {
                const char *base = Ichor::v1::basename(filename_in);
                fmt::print("[{}:{}] ", base, line_in);
            }
            std::cout.write(buf.data(), static_cast<ptrdiff_t>(buf.size())) << "\n";
        }
    }

    void debug(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) override {
        if(level <= LogLevel::LOG_DEBUG) {
            fmt::basic_memory_buffer<char> buf{};
            fmt::vformat_to(std::back_inserter(buf), format_str, args);
            logs.emplace_back(buf.data(), buf.size());
            fmt::print("DEBUG ");
            if (filename_in != nullptr) {
                const char *base = Ichor::v1::basename(filename_in);
                fmt::print("[{}:{}] ", base, line_in);
            }
            std::cout.write(buf.data(), static_cast<ptrdiff_t>(buf.size())) << "\n";
        }
    }

    void info(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) override {
        if(level <= LogLevel::LOG_INFO) {
            fmt::basic_memory_buffer<char> buf{};
            fmt::vformat_to(std::back_inserter(buf), format_str, args);
            logs.emplace_back(buf.data(), buf.size());
            fmt::print("INFO ");
            if (filename_in != nullptr) {
                const char *base = Ichor::v1::basename(filename_in);
                fmt::print("[{}:{}] ", base, line_in);
            }
            std::cout.write(buf.data(), static_cast<ptrdiff_t>(buf.size())) << "\n";
        }
    }

    void warn(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) override {
        if(level <= LogLevel::LOG_WARN) {
            fmt::basic_memory_buffer<char> buf{};
            fmt::vformat_to(std::back_inserter(buf), format_str, args);
            logs.emplace_back(buf.data(), buf.size());
            fmt::print("WARN ");
            if (filename_in != nullptr) {
                const char *base = Ichor::v1::basename(filename_in);
                fmt::print("[{}:{}] ", base, line_in);
            }
            std::cout.write(buf.data(), static_cast<ptrdiff_t>(buf.size())) << "\n";
        }
    }

    void error(const char *filename_in, int line_in, const char *funcname_in, std::string_view format_str, fmt::format_args args) override {
        fmt::println("logger mock level {}", level);
        if(level <= LogLevel::LOG_ERROR) {
            fmt::basic_memory_buffer<char> buf{};
            fmt::vformat_to(std::back_inserter(buf), format_str, args);
            logs.emplace_back(buf.data(), buf.size());
            fmt::print("ERROR ");
            if (filename_in != nullptr) {
                const char *base = Ichor::v1::basename(filename_in);
                fmt::print("[{}:{}] ", base, line_in);
            }
            std::cout.write(buf.data(), static_cast<ptrdiff_t>(buf.size())) << "\n";
        }
    }

    void setLogLevel(LogLevel _level) noexcept override {
        level = _level;
    }
    [[nodiscard]] LogLevel getLogLevel() const noexcept override {
        return level;
    }
    LogLevel level{LogLevel::LOG_TRACE};
    std::vector<std::string> logs;
};
