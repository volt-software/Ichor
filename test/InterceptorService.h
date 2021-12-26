#pragma once

#include <ichor/Service.h>
#include <ichor/Events.h>

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

    StartBehaviour start() final {
        _interceptor = this->getManager()->template registerEventInterceptor<InterceptorT>(this);

        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        _interceptor.reset();

        return StartBehaviour::SUCCEEDED;
    }

    bool preInterceptEvent(InterceptorT const * const evt) {
        auto counter = preinterceptedCounters.find(evt->type);

        if(counter == end(preinterceptedCounters)) {
            preinterceptedCounters.template emplace(evt->type, 1);
        } else {
            counter->second++;
        }

        return allowProcessing;
    }

    void postInterceptEvent(InterceptorT const * const evt, bool processed) {
        if(processed) {
            auto counter = postinterceptedCounters.find(evt->type);

            if (counter == end(postinterceptedCounters)) {
                postinterceptedCounters.template emplace(evt->type, 1);
            } else {
                counter->second++;
            }
        } else {
            auto counter = unprocessedInterceptedCounters.find(evt->type);

            if(counter == end(unprocessedInterceptedCounters)) {
                unprocessedInterceptedCounters.template emplace(evt->type, 1);
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

    Ichor::unique_ptr<EventInterceptorRegistration> _interceptor{};
    std::unordered_map<uint64_t, uint64_t> preinterceptedCounters;
    std::unordered_map<uint64_t, uint64_t> postinterceptedCounters;
    std::unordered_map<uint64_t, uint64_t> unprocessedInterceptedCounters;
};