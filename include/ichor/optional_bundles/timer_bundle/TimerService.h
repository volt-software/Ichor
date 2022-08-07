#pragma once

#include <ichor/Common.h>
#include <ichor/Service.h>
#include <ichor/DependencyManager.h>
#include <chrono>
#include <thread>
#include <ichor/optional_bundles/timer_bundle/ITimer.h>

namespace Ichor {

    // Rather shoddy implementation, setting the interval does not reset the insertEventLoop function and the sleep_for is sketchy at best.
    class Timer final : public ITimer, public Service<Timer> {
    public:
        Timer() noexcept = default;

        ~Timer() noexcept final {
            stopTimer();
        }

        StartBehaviour start() final {
            _timerEventRegistration = getManager()->registerEventHandler<TimerEvent>(this, getServiceId());
            return Ichor::StartBehaviour::SUCCEEDED;
        }

        StartBehaviour stop() final {
            _timerEventRegistration.reset();
            stopTimer();
            return Ichor::StartBehaviour::SUCCEEDED;
        }

        void startTimer() final {
            if(!_fn) {
                throw std::runtime_error("No callback set.");
            }

            bool expected = true;
            if(_quit.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
                _eventInsertionThread = std::make_unique<std::thread>([this]() { this->insertEventLoop(); });
#ifdef __linux__
                pthread_setname_np(_eventInsertionThread->native_handle(), fmt::format("Tmr #{}", getServiceId()).c_str());
#endif
            }
        }

        void stopTimer() final {
            bool expected = false;
            if(_quit.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                _eventInsertionThread->join();
                _eventInsertionThread = nullptr;
            }
        }

        bool running() const noexcept final {
            return !_quit.load(std::memory_order_acquire);
        };

        void setCallback(std::function<Generator<bool>(TimerEvent const * const)> fn) {
            _fn = std::move(fn);
        }

        void setInterval(uint64_t nanoseconds) noexcept final {
            _intervalNanosec.store(nanoseconds, std::memory_order_release);
        }


        void setPriority(uint64_t priority) noexcept final {
            _priority.store(priority, std::memory_order_release);
        }

        uint64_t getPriority() const noexcept final {
            return _priority.load(std::memory_order_acquire);
        }

        Generator<bool> handleEvent(TimerEvent const * const evt) {
            return _fn(evt);
        }

    private:
        void insertEventLoop() {
            auto now = std::chrono::steady_clock::now();
            auto next = now + std::chrono::nanoseconds(_intervalNanosec.load(std::memory_order_acquire));
            while(!_quit.load(std::memory_order_acquire)) {
                while(now < next && !_quit.load(std::memory_order_acquire)) {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(_intervalNanosec.load(std::memory_order_acquire)/10));
                    now = std::chrono::steady_clock::now();
                }
                getManager()->pushPrioritisedEvent<TimerEvent>(getServiceId(), _priority.load(std::memory_order_acquire));

                next += std::chrono::nanoseconds(_intervalNanosec.load(std::memory_order_acquire));
            }
        }

        std::atomic<uint64_t> _intervalNanosec{};
        std::unique_ptr<std::thread> _eventInsertionThread{};
        EventHandlerRegistration _timerEventRegistration{};
        std::function<Generator<bool>(TimerEvent const * const)> _fn{};
        std::atomic<bool> _quit{true};
        std::atomic<uint64_t> _priority{INTERNAL_EVENT_PRIORITY};
    };
}