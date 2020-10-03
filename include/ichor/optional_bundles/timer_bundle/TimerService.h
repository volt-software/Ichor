#pragma once

#include <ichor/Common.h>
#include <ichor/Service.h>
#include <ichor/DependencyManager.h>
#include <chrono>
#include <thread>
#include <ichor/optional_bundles/timer_bundle/ITimer.h>

namespace Ichor {

    // Rather shoddy implementation, setting the interval does not reset the insertEventLoop function and the sleep_for is sketchy at best.
    class Timer final : public ITimer, public Service {
    public:
        Timer() : _intervalNanosec(0), _eventInsertionThread(nullptr), _quit(true), _priority(INTERNAL_EVENT_PRIORITY) {
        }

        ~Timer() final {
            stopTimer();
        }

        bool start() final {
            return true;
        }

        bool stop() final {
            stopTimer();
            return true;
        }

        void startTimer() final {
            bool expected = true;
            if(_quit.compare_exchange_strong(expected, false)) {
                _eventInsertionThread = std::make_unique<std::thread>([this]() { this->insertEventLoop(); });
            }
        }

        void stopTimer() final {
            bool expected = false;
            if(_quit.compare_exchange_strong(expected, true)) {
                _eventInsertionThread->join();
                _eventInsertionThread = nullptr;
            }
        }

        bool running() const final {
            return !_quit;
        };

        void setInterval(uint64_t nanoseconds) final {
            _intervalNanosec = nanoseconds;
        }


        void setPriority(uint64_t priority) final {
            _priority.store(priority, std::memory_order_release);
        }

        uint64_t getPriority() const final {
            return _priority.load(std::memory_order_acquire);
        }

    private:
        void insertEventLoop() {
            auto now = std::chrono::system_clock::now();
            auto next = now + std::chrono::nanoseconds(_intervalNanosec.load(std::memory_order_acquire));
            while(!_quit.load(std::memory_order_acquire)) {
                while(now < next && !_quit.load(std::memory_order_acquire)) {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(_intervalNanosec.load(std::memory_order_acquire)/10));
                    now = std::chrono::system_clock::now();
                }
                getManager()->pushPrioritisedEvent<TimerEvent>(getServiceId(), _priority.load(std::memory_order_acquire));

                next += std::chrono::nanoseconds(_intervalNanosec.load(std::memory_order_acquire));
            }
        }

        std::atomic<uint64_t> _intervalNanosec;
        std::unique_ptr<std::thread> _eventInsertionThread;
        std::atomic<bool> _quit;
        std::atomic<uint64_t> _priority;
    };
}