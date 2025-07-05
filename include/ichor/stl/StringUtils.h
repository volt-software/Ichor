#pragma once

#include <vector>
#include <string_view>
#include <algorithm>
#include <tl/optional.h>
#include <fmt/base.h>
#include <cstring>
#include <cstdint>
#include <locale>

#ifdef _GLIBCXX_DEBUG
#define NO_DEBUG_CONSTEXPR
#else
#define NO_DEBUG_CONSTEXPR constexpr
#endif

namespace Ichor::v1 {

    /// Code modified from https://stackoverflow.com/a/73078442/1460998
    /// converts a string to an integer with little error checking. Only use if you're very sure that the string is actually a number.
    static constexpr int64_t FastAtoi(const char* str) noexcept {
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
    static constexpr uint64_t FastAtoiu(const char* str) noexcept {
        uint64_t val = 0;
        uint8_t  x;
        while ((x = uint8_t(*str++ - '0')) <= 9) val = val * 10 + x;
        return val;
    }
    static constexpr uint64_t FastAtoiu(std::string_view str) noexcept {
        uint64_t val = 0;
        uint8_t  x;
        size_t pos{};
        while (pos < str.size() && (x = uint8_t(str[pos] - '0')) <= 9) {
            val = val * 10 + x;
            pos++;
        }
        return val;
    }

    static constexpr signed char digittoval[256] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,
        9,  -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1};

    /// Unsafe function to convert hex string to uint, assumes input has been checked. Does not overflow.
    /// @param str
    /// @return uint64_t
    static constexpr uint64_t FastHexToUint(std::string_view str) noexcept {
        if(str.size() > 16) {
            return -1ull;
        }

        uint64_t ret{};
        uint64_t shift{};
        auto it = str.crbegin();
        while(it != str.crend()) {
            ret += static_cast<uint64_t>(digittoval[static_cast<unsigned char>(*it)]) << shift;
            shift += 4;
            ++it;
        }
        return ret;
    }

    /// Function to convert hex string to uint. Safe to use with unsanitized input and with values that may overflow.
    /// @param str
    /// @return Nothing if input not hex or if input would cause value to be more than uint64_t::max
    static constexpr tl::optional<uint64_t> SafeHexToUint(std::string_view str) noexcept {
        if(str.size() > 16) {
            return tl::nullopt;
        }

        uint64_t ret{};
        uint64_t shift{};
        auto it = str.crbegin();
        while(it != str.crend()) {
            if(digittoval[static_cast<unsigned char>(*it)] == -1) {
                return tl::nullopt;
            }

            auto prev = ret;
            ret += static_cast<uint64_t>(digittoval[static_cast<unsigned char>(*it)]) << shift;
            shift += 4;
            ++it;
        }
        return ret;
    }

    struct Version final {
        uint64_t major;
        uint64_t minor;
        uint64_t patch;

        constexpr auto operator<=>(Version const &v) const = default;

        [[nodiscard]] std::string toString() const;
    };

    // Taken & adapted from https://www.cppstories.com/2018/07/string-view-perf-followup/
    static NO_DEBUG_CONSTEXPR std::vector<std::string_view> split(std::string_view str, std::string_view delims, bool includeDelims) {
        std::vector<std::string_view> output;
        if(delims.size() == 1) {
            auto count = std::count(str.cbegin(), str.cend(), delims[0]);
            output.reserve(static_cast<size_t>(count));
        }

        for(auto first = str.data(), second = str.data(), last = first + str.size(); second != last && first != last; first = second + delims.size()) {
            second = std::search(first, last, std::cbegin(delims), std::cend(delims));

            auto end = static_cast<size_t>(second - first);
            if(includeDelims && second != last) {
                end += delims.size();
            }
            output.emplace_back(first, end);
        }

        return output;
    }

    template <typename CB>
    static constexpr void split(std::string_view str, std::string_view delims, bool includeDelims, CB&& cb) {
        for(auto first = str.data(), second = str.data(), last = first + str.size(); second != last && first != last; first = second + delims.size()) {
            second = std::search(first, last, std::cbegin(delims), std::cend(delims));

            auto end = static_cast<size_t>(second - first);
            if(includeDelims && second != last) {
                end += delims.size();
            }
            cb(std::string_view{first, end});
        }
    }

    static constexpr bool IsOnlyDigits(std::string_view str) {
        return std::all_of(str.cbegin(), str.cend(), [](char const c) {
            return c >= '0' && c <= '9';
        });
    }

    static constexpr tl::optional<Version> parseStringAsVersion(std::string_view str) {
        if(str.length() < 5) {
            return {};
        }

        auto wrongLetterCount = std::any_of(str.cbegin(), str.cend(), [](char const c) {
            return c != '0' && c != '1' && c != '2' && c != '3' && c != '4' && c != '5' && c != '6' && c != '7' && c != '8' && c != '9' && c != '.';
        });

        if(wrongLetterCount) {
            return {};
        }

        if(*str.crbegin() == '.') {
            return {};
        }

        auto splitStr = split(str, ".", false);

        if(splitStr.size() != 3) {
            return {};
        }

        return Version{FastAtoiu(splitStr[0].data()), FastAtoiu(splitStr[1].data()), FastAtoiu(splitStr[2].data())};
    }

    // Copied and modified from spdlog
    static constexpr const char *basename(const char *filename) {
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
struct fmt::formatter<Ichor::v1::Version> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::v1::Version& v, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}.{}.{}", v.major, v.minor, v.patch);
    }
};
