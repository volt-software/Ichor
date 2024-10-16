#pragma once

#include <ichor/Common.h>
#include <ichor/stl/ReferenceCountedPointer.h>
#include <string>

namespace Ichor {

    template <typename T>
    class PropertiesFilterEntry final {
    public:
        PropertiesFilterEntry(std::string _key, T _val) noexcept : key(std::move(_key)), val(std::move(_val)) {}

        [[nodiscard]] bool matches(ILifecycleManager const &manager) const noexcept {
            auto const propVal = manager.getProperties().find(key);

            if(propVal == cend(manager.getProperties())) {
                return false;
            }

            if(propVal->second.type_hash() != typeNameHash<T>()) {
                return false;
            }

            return Ichor::any_cast<T&>(propVal->second) == val;
        }

        [[nodiscard]] std::string getDescription() const noexcept {
            return fmt::format("PropertiesFilterEntry {}:{}", key, val);
        }

        std::string key;
        T val;
    };

    class ServiceIdFilterEntry final {
    public:
        explicit ServiceIdFilterEntry(uint64_t _id) noexcept : id(_id) {}

        [[nodiscard]] bool matches(ILifecycleManager const &manager) const noexcept {
            return manager.serviceId() == id;
        }

        [[nodiscard]] std::string getDescription() const noexcept {
            return fmt::format("ServiceIdFilterEntry {}", id);
        }

        uint64_t id;
    };

    class ITemplatedFilter {
    public:
        virtual ~ITemplatedFilter() noexcept = default;
        [[nodiscard]] virtual bool compareTo(ILifecycleManager const &manager) const noexcept = 0;
        [[nodiscard]] virtual std::string getDescription() const noexcept = 0;
    };

    template <typename T>
    class TemplatedFilter final : public ITemplatedFilter {
    public:
        TemplatedFilter(T&& _entries) noexcept : entry(std::forward<T>(_entries)) {}
        ~TemplatedFilter() noexcept final = default;

        TemplatedFilter(const TemplatedFilter&) = default;
        TemplatedFilter(TemplatedFilter&&) noexcept = default;
        TemplatedFilter& operator=(const TemplatedFilter&) = default;
        TemplatedFilter& operator=(TemplatedFilter&&) noexcept = default;

        [[nodiscard]] bool compareTo(ILifecycleManager const &manager) const noexcept final {
            return entry.matches(manager);
        }

        [[nodiscard]] virtual std::string getDescription() const noexcept {
            return entry.getDescription();
        }

    private:
        T entry;
    };

    class Filter final {
    public:
        template <typename T>
        Filter(T&& entries) noexcept : _templatedFilter(new TemplatedFilter<T>(std::forward<T>(entries))) {}

        Filter(Filter&) = default;
        Filter(const Filter&) = default;
        Filter(Filter&&) noexcept = default;
        Filter& operator=(const Filter&) = default;
        Filter& operator=(Filter&&) noexcept = default;

        [[nodiscard]] bool compareTo(ILifecycleManager const &manager) const noexcept {
            return _templatedFilter->compareTo(manager);
        }

        [[nodiscard]] std::string getDescription() const noexcept {
            return _templatedFilter->getDescription();
        }

        ReferenceCountedPointer<ITemplatedFilter> _templatedFilter;
    };
}

template <>
struct fmt::formatter<Ichor::Filter> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::Filter& f, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", f.getDescription());
    }
};
