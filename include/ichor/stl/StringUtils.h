#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <iterator>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <algorithm>
#include <tl/optional.h>
#include <fmt/base.h>
#include <cstring>
#include <cstdint>
#include <locale>
#include <ichor/stl/CompilerSpecific.h>

#ifdef _GLIBCXX_DEBUG
#define ICHOR_NO_DEBUG_CONSTEXPR
#else
#define ICHOR_NO_DEBUG_CONSTEXPR constexpr
#endif

namespace Ichor::v1 {

    /// Code modified from https://stackoverflow.com/a/73078442/1460998
    /// converts a string to an integer with little error checking. Only use if you're very sure that the string is actually a number.
    /// Values outside of the int64_t range wrap around instead of being undefined behaviour.
    ICHOR_PURE_FUNC_ATTR inline constexpr int64_t FastAtoi(const char* str) noexcept {
        // Accumulate in unsigned arithmetic: the magnitude of INT64_MIN doesn't fit in an int64_t and signed overflow is UB.
        uint64_t const neg = str[0] == '-';
        str += neg;
        // Peeling off the first digit saves a multiply and benchmarks faster on both gcc and clang.
        uint64_t val = uint8_t(*str - '0');
        if(val > 9) {
            return 0;
        }
        ++str;
        uint8_t x;
        while ((x = uint8_t(*str++ - '0')) <= 9) val = val * 10 + x;
        // Branchless conditional two's complement negation, (val ^ ~0) + 1 == -val. Converting to int64_t is well-defined modulo 2^64 since C++20.
        return static_cast<int64_t>((val ^ (0 - neg)) + neg);
    }

    /// Code from https://stackoverflow.com/a/73078442/1460998
    /// converts a string to an unsigned integer with little error checking. Only use if you're very sure that the string is actually a number.
    ICHOR_PURE_FUNC_ATTR inline constexpr uint64_t FastAtoiu(const char* str) noexcept {
        uint64_t val = 0;
        uint8_t  x;
        while ((x = uint8_t(*str++ - '0')) <= 9) val = val * 10 + x;
        return val;
    }
    ICHOR_PURE_FUNC_ATTR inline constexpr uint64_t FastAtoiu(std::string_view str) noexcept {
        uint64_t val = 0;
        uint8_t  x;
        size_t pos{};
        while (pos < str.size() && (x = uint8_t(str[pos] - '0')) <= 9) {
            val = val * 10 + x;
            pos++;
        }
        return val;
    }

    inline constexpr signed char digittoval[256] = {
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
    inline constexpr uint64_t FastHexToUint(std::string_view str) noexcept {
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
    /// @return Nothing if input is empty, not hex or if input would cause value to be more than uint64_t::max
    inline constexpr tl::optional<uint64_t> SafeHexToUint(std::string_view str) noexcept {
        if(str.empty() || str.size() > 16) {
            return tl::nullopt;
        }

        uint64_t ret{};
        uint64_t shift{};
        auto it = str.crbegin();
        while(it != str.crend()) {
            if(digittoval[static_cast<unsigned char>(*it)] == -1) {
                return tl::nullopt;
            }

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
    // An empty delimiter does not split, the whole (non-empty) string is returned.
    inline ICHOR_NO_DEBUG_CONSTEXPR std::vector<std::string_view> split(std::string_view str, std::string_view delims, bool includeDelims) {
        std::vector<std::string_view> output;
        if(delims.empty()) {
            if(!str.empty()) {
                output.push_back(str);
            }
            return output;
        }
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

    // An empty delimiter does not split, cb is called once with the whole (non-empty) string.
    template <typename CB>
    constexpr void split(std::string_view str, std::string_view delims, bool includeDelims, CB&& cb) {
        if(delims.empty()) {
            if(!str.empty()) {
                cb(str);
            }
            return;
        }
        for(auto first = str.data(), second = str.data(), last = first + str.size(); second != last && first != last; first = second + delims.size()) {
            second = std::search(first, last, std::cbegin(delims), std::cend(delims));

            auto end = static_cast<size_t>(second - first);
            if(includeDelims && second != last) {
                end += delims.size();
            }
            cb(std::string_view{first, end});
        }
    }

    inline constexpr bool IsOnlyDigits(std::string_view str) {
        return std::all_of(str.cbegin(), str.cend(), [](char const c) {
            return c >= '0' && c <= '9';
        });
    }

    /// Parses a non-empty string of only digits into a uint64_t, returns nothing on invalid input or overflow.
    inline constexpr tl::optional<uint64_t> parseUint64(std::string_view str) noexcept {
        if(str.empty()) {
            return tl::nullopt;
        }
        uint64_t val{};
        for(char const c : str) {
            if(c < '0' || c > '9') {
                return tl::nullopt;
            }
            auto const digit = static_cast<uint64_t>(c - '0');
            if(val > (std::numeric_limits<uint64_t>::max() - digit) / 10) {
                return tl::nullopt;
            }
            val = val * 10 + digit;
        }
        return val;
    }

    inline constexpr tl::optional<Version> parseStringAsVersion(std::string_view str) {
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

        // string_views into str are not null-terminated, so parse them with their lengths. Also rejects empty components ("1..2") and overflow.
        auto major = parseUint64(splitStr[0]);
        auto minor = parseUint64(splitStr[1]);
        auto patch = parseUint64(splitStr[2]);
        if(!major || !minor || !patch) {
            return {};
        }

        return Version{*major, *minor, *patch};
    }

    // Copied and modified from spdlog
    ICHOR_PURE_FUNC_ATTR inline constexpr const char* basename(const char* filename) {
#ifdef _WIN32
        const char* last = filename;
        for (const char* p = filename; *p != '\0'; ++p) {
            if (*p == '/' || *p == '\\') last = p + 1;
        }
        return last;
        //const std::reverse_iterator<const char *> begin(filename + std::strlen(filename));
        //const std::reverse_iterator<const char *> end(filename);
        //const std::string_view seperator{"\\/"};

        //const auto it = std::find_first_of(begin, end, std::begin(seperator), std::end(seperator));
        //return it != end ? it.base() : filename;
#else
        if (std::is_constant_evaluated()) {
            // strrchr isn't constexpr
            const char* last = filename;
            for (const char* p = filename; *p != '\0'; ++p) {
                if (*p == '/') last = p + 1;
            }
            return last;
        }
        const char *rv = std::strrchr(filename, '/');
        return rv != nullptr ? rv + 1 : filename;
#endif
    }

    // inserter to use with fmt and inserting into an uint8_t vector
    struct FmtU8Inserter {
        std::vector<uint8_t>* v;
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = void;
        FmtU8Inserter(std::vector<uint8_t>& vv) : v(&vv) {}
        FmtU8Inserter& operator=(char c) { v->push_back(static_cast<uint8_t>(c)); return *this; }
        FmtU8Inserter& operator*()  { return *this; }
        FmtU8Inserter& operator++() { return *this; }
        FmtU8Inserter  operator++(int) { return *this; }
    };
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

#undef ICHOR_NO_DEBUG_CONSTEXPR
