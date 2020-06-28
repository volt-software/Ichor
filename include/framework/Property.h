#pragma once

#include "Common.h"
#include <string>
#include <memory>
#include <unordered_map>

namespace Cppelix {
    // TODO optimize getAsString, reference or std::string_view in CppelixProperties
    class IProperty {
    public:
        virtual ~IProperty() = default;

        [[nodiscard]] virtual long getAsLong() = 0;
        [[nodiscard]] virtual double getAsDouble() = 0;
        [[nodiscard]] virtual std::string getAsString() = 0;
    };

    template <class T>
    class Property : public IProperty {
    public:
        Property() noexcept = default;
        ~Property() final = default;

        [[nodiscard]] long getAsLong() final { throw std::runtime_error(""); }
        [[nodiscard]] double getAsDouble() final { throw std::runtime_error(""); }
        [[nodiscard]] std::string getAsString() final { throw std::runtime_error(""); }
    };

    template <>
    class Property<int> : public IProperty {
    public:
        Property(int _val) : val(_val) {}
        ~Property() final = default;

        [[nodiscard]] long getAsLong() final { return val; }
        [[nodiscard]] double getAsDouble() final { throw std::runtime_error(""); }
        [[nodiscard]] std::string getAsString() final { throw std::runtime_error(""); }

    private:
        int val;
    };

    template <>
    class Property<double> : public IProperty {
    public:
        Property(double _val) : val(_val) {}
        ~Property() final = default;

        [[nodiscard]] long getAsLong() final { throw std::runtime_error(""); }
        [[nodiscard]] double getAsDouble() final { return val; }
        [[nodiscard]] std::string getAsString() final { throw std::runtime_error(""); }

    private:
        double val;
    };

    template <>
    class Property<std::string> : public IProperty {
    public:
        Property(std::string _val) : val(_val) {}
        ~Property() final = default;

        [[nodiscard]] long getAsLong() final { throw std::runtime_error(""); }
        [[nodiscard]] double getAsDouble() final { throw std::runtime_error(""); }
        [[nodiscard]] std::string getAsString() final { return val; }

    private:
        std::string val;
    };

    using CppelixProperties = std::unordered_map<std::string, std::shared_ptr<IProperty>>;
}