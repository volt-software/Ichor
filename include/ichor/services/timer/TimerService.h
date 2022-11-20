#pragma once

#include <ichor/Service.h>
#include <thread>
#include <ichor/services/timer/ITimer.h>
#include <ichor/events/RunFunctionEvent.h>

namespace Ichor {

    // Rather shoddy implementation, setting the interval does not reset the insertEventLoop function and the sleep_for is sketchy at best.
    class Timer final : public ITimer, public Service<Timer> {
    public:
        Timer() noexcept = default;

        ~Timer() noexcept final;

        void startTimer() final;
        void startTimer(bool fireImmediately) final;
        void stopTimer() final;

        [[nodiscard]] bool running() const noexcept final;

        void setCallback(IService *svc, decltype(RunFunctionEvent::fun) fn);
        void setInterval(uint64_t nanoseconds) noexcept final;

        void setPriority(uint64_t priority) noexcept final;
        [[nodiscard]] uint64_t getPriority() const noexcept final;

    private:
        AsyncGenerator<void> start() final;
        AsyncGenerator<void> stop() final;

        void insertEventLoop(bool fireImmediately);

        std::atomic<uint64_t> _intervalNanosec{1'000'000'000};
        std::unique_ptr<std::thread> _eventInsertionThread{};
        decltype(RunFunctionEvent::fun) _fn{};
        std::atomic<bool> _quit{true};
        std::atomic<uint64_t> _priority{INTERNAL_EVENT_PRIORITY};
        uint64_t _requestingServiceId{};
    };
}