#pragma once

#include <ichor/Common.h>
#include <ichor/stl/ReferenceCountedPointer.h>
#include <string>

namespace Ichor {

    template <typename T, bool MissingOk>
    class PropertiesFilterEntry final {
    public:
        PropertiesFilterEntry(std::string_view _key, T _val) noexcept : key(_key), val(std::move(_val)) {}

        [[nodiscard]] bool matches(ILifecycleManager const &manager) const noexcept {
            auto const propVal = manager.getProperties().find(key);

            if(propVal == cend(manager.getProperties())) {
                return MissingOk;
            }

            if(propVal->second.type_hash() != typeNameHash<T>()) {
                return MissingOk;
            }

            return Ichor::v1::any_cast<T const &>(propVal->second) == val;
        }

        [[nodiscard]] std::string getDescription() const noexcept {
            std::string s;
            fmt::format_to(std::back_inserter(s), "PropertiesFilterEntry {}:{}", key, val);
            return s;
        }

        std::string_view key;
        T val;
    };

    template <typename DepT, typename PropT, bool MissingOk>
    class DependencyPropertiesFilterEntry final {
    public:
        DependencyPropertiesFilterEntry(std::string_view _key, PropT _val) noexcept : key(_key), val(std::move(_val)) {}

        [[nodiscard]] bool matches(ILifecycleManager const &manager) const noexcept {
            auto registry = manager.getDependencyRegistry();

            if(registry == nullptr) {
                return MissingOk;
            }

            auto it = registry->find(typeNameHash<DepT>());

            if(it == registry->end()) {
                return MissingOk;
            }

            auto const &props = std::get<tl::optional<Properties>>(it->second);

            if(!props) {
                return MissingOk;
            }

            auto const propVal = (*props).find(key);

            if(propVal == cend(*props)) {
                return MissingOk;
            }

            if(propVal->second.type_hash() != typeNameHash<PropT>()) {
                return MissingOk;
            }

            return Ichor::v1::any_cast<PropT const &>(propVal->second) == val;
        }

        [[nodiscard]] std::string getDescription() const noexcept {
            std::string s;
            fmt::format_to(std::back_inserter(s), "DependencyPropertiesFilterEntry {}:{}:{}", typeName<DepT>, key, val);
            return s;
        }

        std::string_view key;
        PropT val;
    };

    class ServiceIdFilterEntry final {
    public:
        explicit ServiceIdFilterEntry(ServiceIdType _id) noexcept : id(_id) {}

        [[nodiscard]] bool matches(ILifecycleManager const &manager) const noexcept {
            return manager.serviceId() == id;
        }

        [[nodiscard]] std::string getDescription() const noexcept {
            std::string s;
            fmt::format_to(std::back_inserter(s), "ServiceIdFilterEntry {}", id);
            return s;
        }

        ServiceIdType id;
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

        v1::ReferenceCountedPointer<ITemplatedFilter> _templatedFilter;
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
