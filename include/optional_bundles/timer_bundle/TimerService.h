#pragma once

#include "framework/Common.h"
#include "framework/Service.h"
#include "framework/DependencyManager.h"
#include <chrono>
#include <thread>
#include "ITimer.h"

namespace Cppelix {

    // Rather shoddy implementation, setting the interval does not reset the insertEventLoop function and the sleep_for is sketchy at best.
    class Timer : public ITimer, public Service {
    public:
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        Timer() : _id(_timerId.fetch_add(1, std::memory_order_acq_rel)), _intervalNanosec(0), _eventInsertionThread(nullptr), _quit(true) {
        }

        ~Timer() {
            stopTimer();
        }

        bool start() final {
            return true;
        }

        bool stop() final {
            stopTimer();
            return true;
        }

        void startTimer() {
            bool expected = true;
            if(_quit.compare_exchange_strong(expected, false)) {
                _eventInsertionThread = std::make_unique<std::thread>([this]() { this->insertEventLoop(); });
            }
        }

        void stopTimer() {
            bool expected = false;
            if(_quit.compare_exchange_strong(expected, true)) {
                _eventInsertionThread->join();
                _eventInsertionThread = nullptr;
            }
        }

        template <typename Dur>
        void setInterval(Dur duration) {
            _intervalNanosec = std::chrono::nanoseconds(duration).count();
        }

        bool running() const {
            return !_quit;
        };

        uint64_t timerId() const {
            return _id;
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
                _manager->pushEvent<TimerEvent>(getServiceId(), _id);

                next += std::chrono::nanoseconds(_intervalNanosec.load(std::memory_order_acquire));
            }
        }

        uint64_t _id;
        std::atomic<uint64_t> _intervalNanosec;
        std::unique_ptr<std::thread> _eventInsertionThread;
        std::atomic<bool> _quit;
        static std::atomic<uint64_t> _timerId;
    };
}