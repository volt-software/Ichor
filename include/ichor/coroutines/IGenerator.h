#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>
#include <ichor/Defines.h>
#include <ichor/CoreTypes.h>
#ifdef ICHOR_HAVE_STD_STACKTRACE
#include <stacktrace>
#endif

namespace Ichor {
    class IAsyncGeneratorBeginOperation {
    public:
        constexpr virtual ~IAsyncGeneratorBeginOperation() = default;

        [[nodiscard]] constexpr virtual bool get_finished() const noexcept = 0;
        /// Whenever a consumer/producer routine suspends, the generator promise sets the value.
        /// If the coroutine never ends up in await_suspend, f.e. because of co_await'ing on another coroutine immediately,
        /// suspended is in an un-set state and is considered as suspended.
        /// \return true if suspended or never engaged
        [[nodiscard]] constexpr virtual bool get_has_suspended() const noexcept = 0;
        [[nodiscard]] constexpr virtual state get_op_state() const noexcept = 0;
        [[nodiscard]] constexpr virtual state get_promise_state() const noexcept = 0;
        [[nodiscard]] constexpr virtual uint64_t get_promise_id() const noexcept = 0;
    };

    /// Holds a type-erased AsyncGeneratorBeginOperation inline, avoiding a heap allocation per begin_interface() call.
    /// Neither copyable nor movable: it is only ever returned as a prvalue.
    class AsyncGeneratorBeginOperationHolder final {
    public:
        template <typename Op, typename... Args>
        explicit AsyncGeneratorBeginOperationHolder(std::in_place_type_t<Op>, Args&&... args) noexcept {
            static_assert(std::is_base_of_v<IAsyncGeneratorBeginOperation, Op>, "Op needs to implement IAsyncGeneratorBeginOperation");
            static_assert(sizeof(Op) <= sizeof(_storage), "Op does not fit in the inline storage");
            static_assert(alignof(Op) <= alignof(std::max_align_t), "Op is over-aligned");
            static_assert(std::is_nothrow_constructible_v<Op, Args&&...>, "Op construction needs to be noexcept");
            _op = ::new (static_cast<void*>(_storage)) Op(std::forward<Args>(args)...);
        }

        ~AsyncGeneratorBeginOperationHolder() {
            _op->~IAsyncGeneratorBeginOperation();
        }

        AsyncGeneratorBeginOperationHolder(const AsyncGeneratorBeginOperationHolder&) = delete;
        AsyncGeneratorBeginOperationHolder(AsyncGeneratorBeginOperationHolder&&) = delete;
        AsyncGeneratorBeginOperationHolder& operator=(const AsyncGeneratorBeginOperationHolder&) = delete;
        AsyncGeneratorBeginOperationHolder& operator=(AsyncGeneratorBeginOperationHolder&&) = delete;

        [[nodiscard]] IAsyncGeneratorBeginOperation* get() const noexcept {
            return _op;
        }

        [[nodiscard]] IAsyncGeneratorBeginOperation* operator->() const noexcept {
            return _op;
        }

        [[nodiscard]] IAsyncGeneratorBeginOperation& operator*() const noexcept {
            return *_op;
        }

    private:
        alignas(std::max_align_t) std::byte _storage[6 * sizeof(void*)];
        // Needed because the interface is not necessarily at offset 0 of Op.
        IAsyncGeneratorBeginOperation* _op;
    };

    class IGenerator {
    public:
        constexpr virtual ~IGenerator() = 0;
        [[nodiscard]] constexpr virtual bool done() const noexcept = 0;
        [[nodiscard]] virtual AsyncGeneratorBeginOperationHolder begin_interface() noexcept = 0;
        constexpr virtual void set_service_id(ServiceIdType svcId) noexcept = 0;
        [[nodiscard]] constexpr virtual ServiceIdType const get_service_id() const noexcept = 0;

#ifdef ICHOR_HAVE_STD_STACKTRACE
        [[nodiscard]] constexpr virtual std::stacktrace const &get_stacktrace() const noexcept = 0;
#endif
    };

    constexpr inline IGenerator::~IGenerator() = default;
}
