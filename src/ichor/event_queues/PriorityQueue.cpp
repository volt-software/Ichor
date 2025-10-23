#include <csignal>
#include <shared_mutex>
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/DependencyManager.h>

namespace Ichor::Detail {
    extern std::atomic<bool> sigintQuit;
    extern std::atomic<bool> registeredSignalHandler;
    void on_sigint([[maybe_unused]] int sig);
}

namespace Ichor {
    template <typename COMPARE>
    TemplatePriorityQueue<COMPARE>::TemplatePriorityQueue() = default;
    template <typename COMPARE>
    TemplatePriorityQueue<COMPARE>::TemplatePriorityQueue(uint64_t quitTimeoutMs, bool spinlock) : _spinlock(spinlock), _quitTimeoutMs(quitTimeoutMs) {
    }

    template <typename COMPARE>
    TemplatePriorityQueue<COMPARE>::~TemplatePriorityQueue() {
        if(_dm) {
            stopDm();
        }

        if(Detail::registeredSignalHandler) {
            if (::signal(SIGINT, SIG_DFL) == SIG_ERR) {
                fmt::print("Couldn't unset signal handler\n");
            }
        }
    }

    template <typename COMPARE>
    void TemplatePriorityQueue<COMPARE>::pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) {
        if(!event) [[unlikely]] {
#if ICHOR_EXCEPTIONS_ENABLED
            throw std::runtime_error("Pushing nullptr");
#else
            fmt::println("Pushing nullptr");
            std::terminate();
#endif
        }

//        INTERNAL_DEBUG("inserted event of type {} priority {} into manager {}", event->get_name(), event->priority, _dm->getId());

        // TODO hardening
//#ifdef ICHOR_USE_HARDENING
//            if(originatingServiceId != 0 && _services.find(originatingServiceId) == _services.end()) [[unlikely]] {
//                std::terminate();
//            }
//#endif

        {
            std::lock_guard const l(_eventQueueMutex);
            _eventQueue.push(std::move(event));
        }
        _wakeup.notify_all();
    }

    template <typename COMPARE>
    bool TemplatePriorityQueue<COMPARE>::empty() const noexcept {
        std::shared_lock const l(_eventQueueMutex);
        return _eventQueue.empty() && !_processingEvt.load(std::memory_order_acquire);
    }

    template <typename COMPARE>
    uint64_t TemplatePriorityQueue<COMPARE>::size() const noexcept {
        std::shared_lock const l(_eventQueueMutex);
        return static_cast<uint64_t>(_eventQueue.size()) + _processingEvt.load(std::memory_order_acquire);
    }

    template <typename COMPARE>
    bool TemplatePriorityQueue<COMPARE>::is_running() const noexcept {
        return !_quit.load(std::memory_order_acquire);
    }

    template <typename COMPARE>
    ICHOR_CONST_FUNC_ATTR NameHashType TemplatePriorityQueue<COMPARE>::get_queue_name_hash() const noexcept {
        return typeNameHash<TemplatePriorityQueue<COMPARE>>();
    }

    template <typename COMPARE>
    bool TemplatePriorityQueue<COMPARE>::start(bool captureSigInt) {
        if(!_dm) [[unlikely]] {
            fmt::println("Please create a manager first!");
            return false;
        }

        if(captureSigInt && !Ichor::Detail::registeredSignalHandler.exchange(true)) {
            if (::signal(SIGINT, Ichor::Detail::on_sigint) == SIG_ERR) {
                fmt::println("Couldn't set signal");
                return false;
            }
        }

        startDm();

        while(!shouldQuit()) [[likely]] {
            std::unique_lock l(_eventQueueMutex);
            while(!shouldQuit() && _eventQueue.empty()) {
                // Spinlock 10ms before going to sleep, improves latency in high workload cases at the expense of CPU usage
                if(_spinlock) {
                    l.unlock();
                    auto start = std::chrono::steady_clock::now();
                    while(std::chrono::steady_clock::now() < start + 10ms) {
                        l.lock();
                        if(!_eventQueue.empty()) {
                            goto spinlockBreak;
                        }
                        l.unlock();
                    }
                    l.lock();
                }
                // Being woken up from another thread incurs a cost of ~0.4ms on my machine (see benchmarks/README.md for specs)
                _wakeup.wait_for(l, 500ms, [this]() {
                    shouldAddQuitEvent();
                    return shouldQuit() || !_eventQueue.empty();
                });
            }
            spinlockBreak:

            shouldAddQuitEvent();

            if(shouldQuit()) [[unlikely]] {
                break;
            }

            auto evt = _eventQueue.pop();
            l.unlock();
            _processingEvt.store(true, std::memory_order_release);
            processEvent(evt);
            _processingEvt.store(false, std::memory_order_release);
        }

        stopDm();

        return true;
    }

    template <typename COMPARE>
    bool TemplatePriorityQueue<COMPARE>::shouldQuit() {
        bool const shouldQuit = Detail::sigintQuit.load(std::memory_order_acquire);

//        INTERNAL_DEBUG("shouldQuit() {} {:L}", shouldQuit, (std::chrono::steady_clock::now() - _whenQuitEventWasSent).count());
        if (shouldQuit && _quitEventSent && std::chrono::steady_clock::now() - _whenQuitEventWasSent >= std::chrono::milliseconds(_quitTimeoutMs)) [[unlikely]] {
            _quit.store(true, std::memory_order_release);
        }

        return _quit.load(std::memory_order_acquire);
    }

    template <typename COMPARE>
    void TemplatePriorityQueue<COMPARE>::shouldAddQuitEvent() {
        bool const shouldQuit = Detail::sigintQuit.load(std::memory_order_acquire);

        if(shouldQuit && !_quitEventSent) {
            // assume _eventQueueMutex is locked
            _eventQueue.push(std::make_unique<QuitEvent>(getNextEventId(), ServiceIdType{0}, INTERNAL_EVENT_PRIORITY));
            _quitEventSent = true;
            _whenQuitEventWasSent = std::chrono::steady_clock::now();
        }
    }

    template <typename COMPARE>
    void TemplatePriorityQueue<COMPARE>::quit() {
        _quit.store(true, std::memory_order_release);
    }
}

template class Ichor::TemplatePriorityQueue<Ichor::PriorityQueueCompare>;
template class Ichor::TemplatePriorityQueue<Ichor::OrderedPriorityQueueCompare>;
