#pragma once

#include "Common.h"
#include <string>

namespace Cppelix {
    struct Filter {
        explicit CPPELIX_CONSTEXPR Filter() : filter() {}
        explicit CPPELIX_CONSTEXPR Filter(std::string _filter) : filter(_filter) {}
        CPPELIX_CONSTEXPR Filter(const Filter &other) noexcept = default;
        CPPELIX_CONSTEXPR Filter(Filter &&other) noexcept = default;
        CPPELIX_CONSTEXPR Filter& operator=(const Filter &other) noexcept = default;
        CPPELIX_CONSTEXPR Filter& operator=(Filter &&other) noexcept = default;
        CPPELIX_CONSTEXPR std::strong_ordering operator<=>(const Filter&) const noexcept = default;

        CPPELIX_CONSTEXPR Filter And(std::string key, std::string value) {
            return Filter("(&" + filter + "("  + key + "=" + value + "))");
        }

        CPPELIX_CONSTEXPR Filter And(std::string extra_filter) {
            return Filter("(&" + filter + extra_filter + ")");
        }

        CPPELIX_CONSTEXPR Filter Or(std::string key, std::string value) {
            return Filter("(|" + filter + "("  + key + "=" + value + "))");
        }

        CPPELIX_CONSTEXPR Filter Or(std::string extra_filter) {
            return Filter("(|" + filter + extra_filter + ")");
        }

        CPPELIX_CONSTEXPR std::string filter;
    };
}
