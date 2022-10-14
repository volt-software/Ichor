#pragma once

namespace Ichor {
    class IAsyncGeneratorBeginOperation {
    public:
        virtual ~IAsyncGeneratorBeginOperation() = default;

        [[nodiscard]] virtual bool get_finished() const noexcept = 0;
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