#pragma once

#include <concepts>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <new>
#include <utility>
#include <ichor/events/Event.h>

namespace Ichor::v1 {
    /// Owns a single Event (or derived type) stored inline, without heap allocation.
    // NOLINTBEGIN(cppcoreguidelines-pro-type-member-init): _buffer is the storage the event is constructed in, zeroing it would cost every event a memset
    class [[nodiscard]] EventStackUniquePtr final {
    public:
        static constexpr std::size_t bufferSize = 128;

        EventStackUniquePtr() noexcept = default;

        template<typename T, typename... Args>
        requires std::derived_from<T, Event>
        static EventStackUniquePtr create(Args &&... args) {
            static_assert(sizeof(T) <= bufferSize, "size not big enough to hold T");
            static_assert(alignof(T) <= alignof(std::max_align_t), "T is over-aligned");
            static_assert(T::TYPE != 0, "type of T cannot be 0");
            static_assert(std::is_move_constructible_v<T>, "T has to be move constructible");
            EventStackUniquePtr ptr;
            ::new(static_cast<void*>(ptr._buffer.data())) T(std::forward<Args>(args)...);
            ptr._type = T::TYPE;
            ptr._moveFn = [](void *dst, void *src) noexcept {
                // Move-construct into dst and destroy the source, a plain byte copy is not valid for non-trivially copyable types.
                auto *srcT = std::launder(static_cast<T*>(src));
                ::new(dst) T(std::move(*srcT));
                std::destroy_at(srcT);
            };
            return ptr;
        }

        EventStackUniquePtr(const EventStackUniquePtr &) = delete;

        EventStackUniquePtr(EventStackUniquePtr &&other) noexcept {
            takeFrom(other);
        }
    // NOLINTEND(cppcoreguidelines-pro-type-member-init)

        EventStackUniquePtr &operator=(const EventStackUniquePtr &) = delete;

        EventStackUniquePtr &operator=(EventStackUniquePtr &&other) noexcept {
            if(this == &other) [[unlikely]] {
                return *this;
            }
            reset();
            takeFrom(other);
            return *this;
        }

        ~EventStackUniquePtr() {
            reset();
        }

        template<typename T>
        requires std::derived_from<T, Event>
        [[nodiscard]] T *getT() {
            if (_moveFn == nullptr || _type != T::TYPE) {
                std::terminate();
            }

            return std::launder(reinterpret_cast<T *>(_buffer.data()));
        }

        [[nodiscard]] Event *get() {
            if (_moveFn == nullptr) {
                std::terminate();
            }

            return std::launder(reinterpret_cast<Event *>(_buffer.data()));
        }

        [[nodiscard]] uint64_t getType() const noexcept {
            return _type;
        }

        [[nodiscard]] bool empty() const noexcept {
            return _moveFn == nullptr;
        }

    private:
        using MoveFn = void(*)(void *dst, void *src) noexcept;

        void reset() noexcept {
            if (_moveFn != nullptr) {
                std::destroy_at(std::launder(reinterpret_cast<Event *>(_buffer.data())));
                _moveFn = nullptr;
                _type = 0;
            }
        }

        void takeFrom(EventStackUniquePtr &other) noexcept {
            if(other._moveFn == nullptr) {
                return;
            }
            other._moveFn(_buffer.data(), other._buffer.data());
            _moveFn = other._moveFn;
            _type = other._type;
            other._moveFn = nullptr;
            other._type = 0;
        }

        alignas(std::max_align_t) std::array<std::byte, bufferSize> _buffer;
        uint64_t _type{0};
        MoveFn _moveFn{nullptr}; // nullptr means empty
    };
}
