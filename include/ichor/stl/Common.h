#pragma once

#include <memory>
#include <memory_resource>
#include <ichor/ConstevalHash.h>

#define DEBUG_DELETER false

#if DEBUG_DELETER
#include <iostream>
#endif

namespace Ichor {
    struct DeleterBase {
        DeleterBase() noexcept = default;
        virtual ~DeleterBase() noexcept = default;

        DeleterBase(const DeleterBase& d) noexcept = default;
        DeleterBase(DeleterBase&& d) noexcept = default;

        DeleterBase& operator=(const DeleterBase&) noexcept = default;
        DeleterBase& operator=(DeleterBase&&) noexcept = default;

        virtual void dealloc(void *c) const noexcept = 0;
    };

    template <typename T>
    struct InternalDeleter final : public DeleterBase {
        explicit InternalDeleter(std::pmr::memory_resource *rsrc) noexcept : _rsrc(rsrc) {
        }

#if DEBUG_DELETER
        ~InternalDeleter() noexcept final {
            std::cout << "~InternalDeleter<" << typeName<T>() << ">" << std::endl;
        }
#endif

        InternalDeleter(const InternalDeleter& d) noexcept = default;
        InternalDeleter(InternalDeleter&& d) noexcept = default;

        InternalDeleter& operator=(const InternalDeleter&) noexcept = default;
        InternalDeleter& operator=(InternalDeleter&&) noexcept = default;

        //Called by unique_ptr or shared_ptr to destroy/free the Resource
        void dealloc(void *c) const noexcept final {
            if(_rsrc == nullptr) {
                std::terminate();
            }

            if(c != nullptr) {
#if DEBUG_DELETER
                std::cout << "deallocating " << typeName<T>() << std::endl;
#endif
                reinterpret_cast<T*>(c)->~T();
                _rsrc->deallocate(c, sizeof(T));
            }
        }

        std::pmr::memory_resource *_rsrc{nullptr};
    };

    struct Deleter final {
        template <typename T>
        explicit Deleter(InternalDeleter<T> &&id) noexcept {
            new (wrapper.data()) InternalDeleter<T>{std::forward<InternalDeleter<T>>(id)};
        }
        Deleter() noexcept = default;

        Deleter(const Deleter& d) noexcept = default;
        Deleter(Deleter&& d) noexcept = default;

        Deleter& operator=(const Deleter&) noexcept = default;
        Deleter& operator=(Deleter&&) noexcept = default;

        //Called by unique_ptr or shared_ptr to destroy/free the Resource
        void operator()(void *c) const noexcept {
            reinterpret_cast<DeleterBase const *>(wrapper.data())->dealloc(c);
        }

        std::array<std::byte, sizeof(InternalDeleter<int>)> wrapper;
    };

    template <typename T, typename... Args>
    std::unique_ptr<T, Deleter> make_unique(std::pmr::memory_resource *rsrc, Args&&... args) {
        return {new (rsrc->allocate(sizeof(T))) T(std::forward<Args>(args)...), Deleter{InternalDeleter<T>{rsrc}}};
    }

    template <typename T>
    using unique_ptr = std::unique_ptr<T, Deleter>;
}