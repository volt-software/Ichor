#pragma once

#include <vector>
#include <string_view>
#include <algorithm>
#include <tl/optional.h>
#include <fmt/core.h>

namespace Ichor {

    /// Code modified from https://stackoverflow.com/a/73078442/1460998
    /// converts a string to an integer with little error checking. Only use if you're very sure that the string is actually a number.
    static constexpr inline int64_t FastAtoi(const char* str) noexcept {
        int64_t val = 0;
        uint8_t x;
        bool neg{};
        if(str[0] == '-') {
            str++;
            neg = true;
        }
        while ((x = uint8_t(*str++ - '0')) <= 9) val = val * 10 + x;
        return neg ? -val : val;
    }

    /// Code from https://stackoverflow.com/a/73078442/1460998
    /// converts a string to an unsigned integer with little error checking. Only use if you're very sure that the string is actually a number.
    static constexpr inline uint64_t FastAtoiu(const char* str) noexcept {
        uint64_t val = 0;
        uint8_t  x;
        while ((x = uint8_t(*str++ - '0')) <= 9) val = val * 10 + x;
        return val;
    }

    struct Version {
        uint64_t major;
        uint64_t minor;
        uint64_t patch;

        auto operator<=>(Version const &v) const = default;
    };

    // Taken from https://www.cppstories.com/2018/07/string-view-perf-followup/
    static inline std::vector<std::string_view> split(std::string_view str, std::string_view delims) {
        std::vector<std::string_view> output;
        if(delims.size() == 1) {
            auto count = std::count(str.cbegin(), str.cend(), delims[0]);
            output.reserve(static_cast<uint64_t>(count));
        }

        for(auto first = str.data(), second = str.data(), last = first + str.size(); second != last && first != last; first = second + 1) {
            second = std::find_first_of(first, last, std::cbegin(delims), std::cend(delims));

            if(first != second) {
                output.emplace_back(first, second - first);
            }
        }

        return output;
    }

    static inline tl::optional<Version> parseStringAsVersion(std::string_view str) {
        if(str.length() < 5) {
            return {};
        }

        auto wrongLetterCount = std::count_if(str.cbegin(), str.cend(), [](char const c) {
            return c != '0' && c != '1' && c != '2' && c != '3' && c != '4' && c != '5' && c != '6' && c != '7' && c != '8' && c != '9' && c != '.';
        });

        if(wrongLetterCount != 0) {
            return {};
        }

        if(*str.crbegin() == '.') {
            return {};
        }

        auto splitStr = split(str, ".");

        if(splitStr.size() != 3) {
            return {};
        }

        return Version{FastAtoiu(splitStr[0].data()), FastAtoiu(splitStr[1].data()), FastAtoiu(splitStr[2].data())};
    }

    // Copied and modified from spdlog
    static inline const char *basename(const char *filename) {
#ifdef _WIN32
        const std::reverse_iterator<const char *> begin(filename + std::strlen(filename));
        const std::reverse_iterator<const char *> end(filename);
        const std::string_view seperator{"\\/"};

        const auto it = std::find_first_of(begin, end, std::begin(seperator), std::end(seperator));
        return it != end ? it.base() : filename;
#else
        const char *rv = std::strrchr(filename, '/');
        return rv != nullptr ? rv + 1 : filename;
#endif
    }
}



template <>
struct fmt::formatter<Ichor::Version> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::Version& v, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "{}.{}.{}", v.major, v.minor, v.patch);
    }
};
