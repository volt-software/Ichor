#pragma once

#include <ichor/Service.h>
#include <ichor/events/Event.h>

using namespace Ichor;

struct IInterceptorService {
    virtual std::unordered_map<uint64_t, uint64_t>& getPreinterceptedCounters() = 0;
    virtual std::unordered_map<uint64_t, uint64_t>& getPostinterceptedCounters() = 0;
    virtual std::unordered_map<uint64_t, uint64_t>& getUnprocessedInterceptedCounters() = 0;

protected:
    ~IInterceptorService() = default;
};

template <Derived<Event> InterceptorT, bool allowProcessing = true>
struct InterceptorService final : public IInterceptorService, public Service<InterceptorService<InterceptorT, allowProcessing>> {
    InterceptorService() = default;

    AsyncGenerator<void> start() final {
        _interceptor = this->getManager().template registerEventInterceptor<InterceptorT>(this);

        co_return;
    }

    AsyncGenerator<void> stop() final {
        _interceptor.reset();

        co_return;
    }

    bool preInterceptEvent(InterceptorT const &evt) {
        auto counter = preinterceptedCounters.find(evt.type);

        INTERNAL_DEBUG("---------------------------- pre intercepted {}:{}", evt.id, evt.name);

        if(counter == end(preinterceptedCounters)) {
            preinterceptedCounters.template emplace<>(evt.type, 1);
        } else {
            counter->second++;
        }

        return allowProcessing;
    }

    void postInterceptEvent(InterceptorT const &evt, bool processed) {
        if(processed) {
            INTERNAL_DEBUG("---------------------------- post intercepted {}:{}", evt.id, evt.name);

            auto counter = postinterceptedCounters.find(evt.type);

            if (counter == end(postinterceptedCounters)) {
                postinterceptedCounters.template emplace<>(evt.type, 1);
            } else {
                counter->second++;
            }
        } else {
            INTERNAL_DEBUG("---------------------------- unproc intercepted {}:{}", evt.id, evt.name);

            auto counter = unprocessedInterceptedCounters.find(evt.type);

            if(counter == end(unprocessedInterceptedCounters)) {
                unprocessedInterceptedCounters.template emplace<>(evt.type, 1);
            } else {
                counter->second++;
            }
        }
    }

    std::unordered_map<uint64_t, uint64_t>& getPreinterceptedCounters() final {
        return preinterceptedCounters;
    }

    std::unordered_map<uint64_t, uint64_t>& getPostinterceptedCounters() final {
        return postinterceptedCounters;
    }

    std::unordered_map<uint64_t, uint64_t>& getUnprocessedInterceptedCounters() final {
        return unprocessedInterceptedCounters;
    }

    EventInterceptorRegistration _interceptor{};
    std::unordered_map<uint64_t, uint64_t> preinterceptedCounters;
    std::unordered_map<uint64_t, uint64_t> postinterceptedCounters;
    std::unordered_map<uint64_t, uint64_t> unprocessedInterceptedCounters;
};