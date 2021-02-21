#pragma once

#include <memory_resource>

// Differs from std::pmr::PolymorphicAllocator by propagating the allocator on move and swap. Otherwise as equal as possible.
namespace Ichor {
    template<typename T>
    struct PolymorphicAllocator
    {
        using value_type = T;
        typedef std::true_type propagate_on_container_move_assignment;
        typedef std::true_type propagate_on_container_swap;

        PolymorphicAllocator() noexcept
                : _resource(std::pmr::get_default_resource())
        { }

        PolymorphicAllocator(std::pmr::memory_resource* _r) noexcept
        __attribute__((__nonnull__))
                : _resource(_r)
        { }

        PolymorphicAllocator(const PolymorphicAllocator& _other) = default;

        template<typename UP>
        PolymorphicAllocator(const PolymorphicAllocator<UP>& _x) noexcept
                : _resource(_x.resource())
        { }

        PolymorphicAllocator&
        operator=(const PolymorphicAllocator&) = default;

        [[nodiscard]]
        T* allocate(size_t _n) __attribute__((__returns_nonnull__)) {
            if (_n > (std::numeric_limits<size_t>::max() / sizeof(T))) {
                throw (std::bad_array_new_length());
            }
            return static_cast<T*>(_resource->allocate(_n * sizeof(T),
                                                           alignof(T)));
        }

        void deallocate(T* _p, size_t _n) noexcept __attribute__((__nonnull__)) {
            _resource->deallocate(_p, _n * sizeof(T), alignof(T));
        }

        [[nodiscard]]
        void* allocate_bytes(size_t _nbytes, size_t _alignment = alignof(max_align_t)) {
            return _resource->allocate(_nbytes, _alignment);
        }

        void deallocate_bytes(void* _p, size_t _nbytes, size_t _alignment = alignof(max_align_t)) {
            _resource->deallocate(_p, _nbytes, _alignment);
        }

        template<typename UP>
        [[nodiscard]]
        UP* allocate_object(size_t _n = 1) {
            if ((std::numeric_limits<size_t>::max() / sizeof(UP)) < _n) {
                throw(std::bad_array_new_length());
            }
            return static_cast<UP*>(allocate_bytes(_n * sizeof(UP),
                                                    alignof(UP)));
        }

        template<typename UP>
        void deallocate_object(UP* _p, size_t _n = 1) {
            deallocate_bytes(_p, _n * sizeof(UP), alignof(UP));
        }

        template<typename UP, typename... CtorArgs>
        [[nodiscard]]
        UP* new_object(CtorArgs&&... _ctor_args) {
            UP* _p = allocate_object<UP>();
            try {
                construct(_p, std::forward<CtorArgs>(_ctor_args)...);
            } catch (...) {
                deallocate_object(_p);
                throw;
            }
            return _p;
        }

        template<typename UP>
        void delete_object(UP* _p) {
            destroy(_p);
            deallocate_object(_p);
        }

        template<typename T1, typename... Args>
        __attribute__((__nonnull__))
        void construct(T1* _p, Args&&... _args) {
            std::uninitialized_construct_using_allocator(_p, *this, std::forward<Args>(_args)...);
        }

        template<typename UP>
        __attribute__((__nonnull__))
        void destroy(UP* _p) {
            _p->~UP();
        }

        PolymorphicAllocator select_on_container_copy_construction() const noexcept {
            return PolymorphicAllocator();
        }

        [[nodiscard]]
        std::pmr::memory_resource* resource() const noexcept __attribute__((__returns_nonnull__)) {
            return _resource;
        }

    private:
        std::pmr::memory_resource* _resource;
    };
}