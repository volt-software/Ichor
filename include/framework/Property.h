#pragma once

#include "Common.h"
#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>

namespace Cppelix {
    class IProperty {
    public:
        virtual ~IProperty() = default;

        [[nodiscard]] virtual long getAsLong() = 0;
        [[nodiscard]] virtual double getAsDouble() = 0;
        [[nodiscard]] virtual std::string_view getAsString() = 0;
    };

    template <class T>
    class Property : public IProperty {
    public:
        Property() noexcept = default;

        [[nodiscard]] long getAsLong() final { throw std::runtime_error(""); }
        [[nodiscard]] virtual double getAsDouble() { throw std::runtime_error(""); }
        [[nodiscard]] virtual std::string_view getAsString() { throw std::runtime_error(""); }
    };

    template <>
    class Property<int> : public IProperty {
    public:
        Property(int _val) : val(_val) {}
        [[nodiscard]] long getAsLong() final { return val; }
        [[nodiscard]] virtual double getAsDouble() { throw std::runtime_error(""); }
        [[nodiscard]] virtual std::string_view getAsString() { throw std::runtime_error(""); }

    private:
        int val;
    };

    using CppelixProperties = std::unordered_map<std::string, std::unique_ptr<IProperty>>;
}