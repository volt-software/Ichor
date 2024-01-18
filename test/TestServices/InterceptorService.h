#pragma once

#include <ichor/dependency_management/AdvancedService.h>
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
struct InterceptorService final : public IInterceptorService, public AdvancedService<InterceptorService<InterceptorT, allowProcessing>> {
    InterceptorService() = default;

    Task<tl::expected<void, Ichor::StartError>> start() final {
        _interceptor = GetThreadLocalManager().template registerEventInterceptor<InterceptorT>(this, this);

        co_return {};
    }

    Task<void> stop() final {
        _interceptor.reset();

        co_return;
    }

    bool preInterceptEvent(InterceptorT const &evt) {
        auto evtType = evt.get_type();
        auto counter = preinterceptedCounters.find(evtType);

        INTERNAL_DEBUG("---------------------------- pre intercepted {}:{}", evt.id, evt.get_name());

        if(counter == end(preinterceptedCounters)) {
            preinterceptedCounters.template emplace<>(evtType, 1);
        } else {
            counter->second++;
        }

        return allowProcessing;
    }

    void postInterceptEvent(InterceptorT const &evt, bool processed) {
        auto evtType = evt.get_type();
        if(processed) {
            INTERNAL_DEBUG("---------------------------- post intercepted {}:{}", evt.id, evt.get_name());

            auto counter = postinterceptedCounters.find(evtType);

            if (counter == end(postinterceptedCounters)) {
                postinterceptedCounters.template emplace<>(evtType, 1);
            } else {
                counter->second++;
            }
        } else {
            INTERNAL_DEBUG("---------------------------- unproc intercepted {}:{}", evt.id, evt.get_name());

            auto counter = unprocessedInterceptedCounters.find(evtType);

            if(counter == end(unprocessedInterceptedCounters)) {
                unprocessedInterceptedCounters.template emplace<>(evtType, 1);
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
