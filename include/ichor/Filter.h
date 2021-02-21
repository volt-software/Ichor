#pragma once

#include <ichor/Common.h>
#include <string>

namespace Ichor {
    template <typename T>
    class PropertiesFilterEntry final {
    public:
        PropertiesFilterEntry(std::pmr::string _key, T _val) : key(std::move(_key)), val(std::move(_val)) {}

        [[nodiscard]] bool matches(const std::shared_ptr<ILifecycleManager> &manager) const {
            auto propVal = manager->getProperties()->find(key);

            if(propVal == end(*manager->getProperties())) {
                return false;
            }

            if(propVal->second.type_hash() != typeNameHash<T>()) {
                return false;
            }

            return Ichor::any_cast<T&>(propVal->second) == val;
        }

        const std::pmr::string key;
        const T val;
    };

    class ServiceIdFilterEntry final {
    public:
        explicit ServiceIdFilterEntry(uint64_t _id) : id(_id) {}

        [[nodiscard]] bool matches(const std::shared_ptr<ILifecycleManager> &manager) const noexcept {
            return manager->serviceId() == id;
        }

        const uint64_t id;
    };

    class ITemplatedFilter {
    public:
        virtual ~ITemplatedFilter() = default;
        [[nodiscard]] virtual bool compareTo(const std::shared_ptr<ILifecycleManager> &manager) const = 0;
    };

    // workaround std::any not supporting polymorphism
    template <typename... T>
    class TemplatedFilter final : public ITemplatedFilter {
    public:
        TemplatedFilter(T&&... _entries) noexcept : entries(std::forward<T>(_entries)...) {}
        ~TemplatedFilter() noexcept final = default;

        TemplatedFilter(const TemplatedFilter&) = default;
        TemplatedFilter(TemplatedFilter&&) noexcept = default;
        TemplatedFilter& operator=(const TemplatedFilter&) = default;
        TemplatedFilter& operator=(TemplatedFilter&&) noexcept = default;

        [[nodiscard]] bool compareTo(const std::shared_ptr<ILifecycleManager> &manager) const final {
            bool matches = true;
            std::apply([&manager, &matches](auto ...x){
                ((matches = matches && x.matches(manager)), ...);
            }, entries);
            return matches;
        }

        std::tuple<T...> entries;
    };

    class Filter final {
    public:
        template <typename... T>
        Filter(std::pmr::memory_resource *rsrc, T&&... entries) : _templatedFilter(new (rsrc->allocate(sizeof(TemplatedFilter<T...>))) TemplatedFilter<T...>(std::forward<T>(entries)...), Deleter{rsrc, sizeof(TemplatedFilter<T...>)}, std::pmr::polymorphic_allocator<TemplatedFilter<T...>>(rsrc)) {}

        Filter(const Filter&) = default;
        Filter(Filter&&) noexcept = default;
        Filter& operator=(const Filter&) = default;
        Filter& operator=(Filter&&) noexcept = default;

        [[nodiscard]] bool compareTo(const std::shared_ptr<ILifecycleManager> &manager) const {
            return _templatedFilter->compareTo(manager);
        }

        std::shared_ptr<ITemplatedFilter> _templatedFilter;
    };
}
