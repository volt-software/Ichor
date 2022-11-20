#pragma once

namespace Ichor {
    class IAsyncGeneratorBeginOperation {
    public:
        virtual ~IAsyncGeneratorBeginOperation() = default;

        [[nodiscard]] virtual bool get_finished() const noexcept = 0;
        /// Whenever a consumer/producer routine suspends, the generator promise sets the value.
        /// If the coroutine never ends up in await_suspend, f.e. because of co_await'ing on another coroutine immediately,
        /// suspended is in an un-set state and is considered as suspended.
        /// \return true if suspended or never engaged
        [[nodiscard]] virtual bool get_has_suspended() const noexcept = 0;
        [[nodiscard]] virtual state get_op_state() const noexcept = 0;
        [[nodiscard]] virtual state get_promise_state() const noexcept = 0;
        [[nodiscard]] virtual uint64_t get_promise_id() const noexcept = 0;
    };

    class IGenerator {
    public:
        virtual ~IGenerator() = 0;
        [[nodiscard]] virtual bool done() const noexcept = 0;
        [[nodiscard]] virtual std::unique_ptr<IAsyncGeneratorBeginOperation> begin_interface() noexcept = 0;
    };

    inline IGenerator::~IGenerator() = default;
}