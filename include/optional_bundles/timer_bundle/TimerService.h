#pragma once

#include "framework/Common.h"
#include "framework/Service.h"
#include "framework/DependencyManager.h"
#include <chrono>
#include <thread>
#include "ITimer.h"

namespace Cppelix {

    // Rather shoddy implementation, setting the interval does not reset the insertEventLoop function and the sleep_for is sketchy at best.
    class Timer final : public ITimer, public Service {
    public:
        Timer() : _intervalNanosec(0), _eventInsertionThread(nullptr), _quit(true) {
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

    private:
        void insertEventLoop() {
            auto now = std::chrono::system_clock::now();
            auto next = now + std::chrono::nanoseconds(_intervalNanosec.load(std::memory_order_acquire));
            while(!_quit.load(std::memory_order_acquire)) {
                while(now < next && !_quit.load(std::memory_order_acquire)) {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(_intervalNanosec.load(std::memory_order_acquire)/10));
                    now = std::chrono::system_clock::now();
                }
                getManager()->pushEvent<TimerEvent>(getServiceId(), INTERNAL_EVENT_PRIORITY+1);

                next += std::chrono::nanoseconds(_intervalNanosec.load(std::memory_order_acquire));
            }
        }

        std::atomic<uint64_t> _intervalNanosec;
        std::unique_ptr<std::thread> _eventInsertionThread;
        std::atomic<bool> _quit;
    };
}