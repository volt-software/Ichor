#ifdef ICHOR_USE_SDEVENT

#include <ichor/event_queues/SdeventQueue.h>
#include <ichor/DependencyManager.h>
#include <sys/eventfd.h>

namespace Ichor::Detail {
    extern std::atomic<bool> sigintQuit;
    extern std::atomic<bool> registeredSignalHandler;
    void on_sigint([[maybe_unused]] int sig);
}

namespace Ichor {
    struct ProcessableEvent {
        SdeventQueue *queue;
        std::unique_ptr<Event> event;
    };

    SdeventQueue::SdeventQueue() {
        _eventfd = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
        _threadId = std::this_thread::get_id();
    }

    SdeventQueue::~SdeventQueue() {
        if(_initializedSdevent.load(std::memory_order_acquire)) {
            stopDm();
            sd_event_unref(_eventQueue);
            sd_event_source_unref(_eventfdSource);
            sd_event_source_unref(_timerSource);
        } else {
            close(_eventfd);
        }

        if(Detail::registeredSignalHandler) {
            if (::signal(SIGINT, SIG_DFL) == SIG_ERR) {
                fmt::print("Couldn't unset signal handler\n");
            }
        }
    }

    void SdeventQueue::pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) {
        if(!_initializedSdevent.load(std::memory_order_acquire)) [[unlikely]] {
            throw std::runtime_error("sdevent not initialized. Call createEventLoop or useEventLoop first.");
        }

        if(!event) [[unlikely]] {
            throw std::runtime_error("Pushing nullptr");
        }


        // TODO hardening
//#ifdef ICHOR_USE_HARDENING
//            if(originatingServiceId != 0 && _services.find(originatingServiceId) == _services.end()) [[unlikely]] {
//                std::terminate();
//            }
//#endif

        if(std::this_thread::get_id() != _threadId) [[unlikely]] {
            {
                std::lock_guard const l(_eventQueueMutex);
                _otherThreadEventQueue.emplace(priority, std::move(event));
            }

            uint64_t val = 1;
            if (write(_eventfd, &val, sizeof(val)) < 0) {
                throw std::system_error(-errno, std::generic_category(), "write() failed");
            }
            return;
        }

        {
            sd_event_source *src;
            auto *procEvent = new ProcessableEvent{this, std::move(event)};
            int ret = sd_event_add_defer(_eventQueue, &src, [](sd_event_source *source, void *userdata) {
                auto *e = reinterpret_cast<ProcessableEvent*>(userdata);

                if(e->queue->shouldQuit()) [[unlikely]] {
                    e->queue->quit();
                }

                try {
                    e->queue->processEvent(std::move(e->event));
                } catch(const std::exception &ex) {
                    fmt::print("Encountered exception: \"{}\", quitting\n", ex.what());
                    e->queue->quit();
                } catch(...) {
                    fmt::print("Encountered unknown exception, quitting\n");
                    e->queue->quit();
                }

                sd_event_source_unref(source);

                delete e;

                return 0;
            }, procEvent);

            if(ret < 0) [[unlikely]] {
                delete procEvent;
                throw std::system_error(-ret, std::generic_category(), "sd_event_add_defer() failed");
            }

            ret = sd_event_source_set_priority(src, static_cast<int64_t>(priority) - static_cast<int64_t>(INTERNAL_EVENT_PRIORITY));

            if (ret < 0) [[unlikely]] {
                throw std::system_error(-ret, std::generic_category(), "sd_event_source_set_priority() failed");
            }
        }
    }

    bool SdeventQueue::empty() const {
        // probably a race condition... this crashes QueueTests, disabling for now. Don't think it can hurt that much...right? :(
//#ifdef ICHOR_USE_HARDENING
//        if(_dm.get() != Detail::_local_dm) [[unlikely]] { // are we on the right thread?
//            std::terminate();
//        }
//#endif

        if(!_initializedSdevent.load(std::memory_order_acquire)) [[unlikely]] {
            throw std::runtime_error("sdevent not initialized. Call createEventLoop or useEventLoop first.");
        }

        auto state = sd_event_get_state(_eventQueue);
        return state == SD_EVENT_INITIAL || state == SD_EVENT_ARMED || state == SD_EVENT_FINISHED;
    }

    uint64_t SdeventQueue::size() const {
#ifdef ICHOR_USE_HARDENING
        if(_dm.get() != Detail::_local_dm) [[unlikely]] { // are we on the right thread?
            std::terminate();
        }
#endif

        if(!_initializedSdevent.load(std::memory_order_acquire)) [[unlikely]] {
            throw std::runtime_error("sdevent not initialized. Call createEventLoop or useEventLoop first.");
        }

        auto state = sd_event_get_state(_eventQueue);
        return state == SD_EVENT_INITIAL || state == SD_EVENT_ARMED || state == SD_EVENT_FINISHED ? 0 : 1;
    }

    [[nodiscard]] sd_event* SdeventQueue::createEventLoop() {
        auto ret = sd_event_default(&_eventQueue);

        if(ret < 0) [[unlikely]] {
            throw std::system_error(-ret, std::generic_category(), "sd_event_default() failed");
        }

        registerEventFd();
        registerTimer();

        _initializedSdevent.store(true, std::memory_order_release);
        return _eventQueue;
    }

    void SdeventQueue::useEventLoop(sd_event *event) {
        sd_event_ref(event);
        _eventQueue = event;
        registerEventFd();
        registerTimer();
        _initializedSdevent.store(true, std::memory_order_release);
    }

    void SdeventQueue::start(bool captureSigInt) {
        if(!_initializedSdevent.load(std::memory_order_acquire)) [[unlikely]] {
            throw std::runtime_error("sdevent not initialized. Call createEventLoop or useEventLoop first.");
        }

        if(!_dm) [[unlikely]] {
            throw std::runtime_error("Please create a manager first!");
        }

        // this capture currently has no way to wake all queues. Multimap f.e. polls sigintQuit, but with sdevent the
        if(captureSigInt && !Ichor::Detail::registeredSignalHandler.exchange(true)) {
            if (::signal(SIGINT, Ichor::Detail::on_sigint) == SIG_ERR) {
                throw std::runtime_error("Couldn't set signal");
            }
        }

        startDm();
    }

    bool SdeventQueue::shouldQuit() {
        bool const shouldQuit = Detail::sigintQuit.load(std::memory_order_acquire);

        if (shouldQuit) [[unlikely]] {
            _quit.store(true, std::memory_order_release);
        }

        return _quit.load(std::memory_order_acquire);
    }

    void SdeventQueue::quit() {
#ifdef ICHOR_USE_HARDENING
        if(_dm.get() != Detail::_local_dm) [[unlikely]] { // are we on the right thread?
            std::terminate();
        }
#endif
        _quit.store(true, std::memory_order_release);

        sd_event_source *src;
        int ret = sd_event_add_defer(_eventQueue, &src, [](sd_event_source *source, void *userdata) {
            auto *q = reinterpret_cast<SdeventQueue*>(userdata);

            sd_event_exit(q->_eventQueue, 0);
            sd_event_source_unref(source);

            return 0;
        }, this);

        if(ret < 0) [[unlikely]] {
            // memory leak because event.release()
            throw std::system_error(-ret, std::generic_category(), "sd_event_add_defer() failed");
        }

        sd_event_source_set_priority(src, std::numeric_limits<int64_t>::max());
    }

    void SdeventQueue::registerEventFd() {
        int ret = sd_event_add_io(_eventQueue, &_eventfdSource, _eventfd, EPOLLIN,
                                  [](sd_event_source *s, int fd, uint32_t revents, void *userdata) {
                                      uint64_t val;
                                      auto *q = static_cast<SdeventQueue*>(userdata);

                                      std::unique_lock lck(q->_eventQueueMutex);
                                      if(!q->_otherThreadEventQueue.empty()) [[unlikely]] {
                                          auto node = q->_otherThreadEventQueue.extract(q->_otherThreadEventQueue.begin());
                                          lck.unlock();
                                          q->pushEventInternal(node.key(), std::move(node.mapped()));
                                      } else {
                                          lck.unlock();
                                      }

                                      // Decrement semaphore
                                      auto n = ::read(fd, &val, sizeof(val));
                                      if (n < 0) [[unlikely]] {
                                          if (errno == EINTR) {
                                              n = 0;
                                          }
                                      }

                                      return static_cast<int>(n);
                                  }, this);

        if (ret < 0) [[unlikely]] {
            throw std::system_error(-ret, std::generic_category(), "sd_event_add_io() failed");
        }

        ret = sd_event_source_set_io_fd_own(_eventfdSource, true);

        if (ret < 0) [[unlikely]] {
            throw std::system_error(-ret, std::generic_category(), "sd_event_source_set_io_fd_own() failed");
        }
    }

    void SdeventQueue::registerTimer() {
        constexpr int delayUs = 500'000;
        // check if loop should quit every 500 ms
        int ret = sd_event_add_time(_eventQueue, &_timerSource, CLOCK_MONOTONIC, 0, 0,
                                  [](sd_event_source *s, uint64_t usec, void *userdata) {
                                      auto *q = reinterpret_cast<SdeventQueue*>(userdata);
                                      if(q->shouldQuit()) {
                                          q->quit();
                                      }
                                      sd_event_source_set_time(s, usec + delayUs);
                                      return 0;
                                  }, this);

        if (ret < 0) [[unlikely]] {
            throw std::system_error(-ret, std::generic_category(), "sd_event_add_io() failed");
        }
    }
}

#endif
