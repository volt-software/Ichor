#pragma once

#include <memory>
#include <vector>
#include <ichor/Defines.h>
#include <ichor/ServiceExecutionScope.h>

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

    class IGenerator {
    public:
        constexpr virtual ~IGenerator() = 0;
        [[nodiscard]] constexpr virtual bool done() const noexcept = 0;
        [[nodiscard]] constexpr virtual std::unique_ptr<IAsyncGeneratorBeginOperation> begin_interface() noexcept = 0;
        [[nodiscard]] constexpr virtual const std::vector<Detail::ServiceExecutionScopeContents> &get_service_id_stack() noexcept = 0;
    };

    constexpr inline IGenerator::~IGenerator() = default;
}
