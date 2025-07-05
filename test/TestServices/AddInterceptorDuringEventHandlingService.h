#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/events/Event.h>
#include "../TestEvents.h"

using namespace Ichor;
using namespace Ichor::v1;

struct AddInterceptorDuringEventHandlingService final : public AdvancedService<AddInterceptorDuringEventHandlingService> {
    AddInterceptorDuringEventHandlingService() = default;

    Task<tl::expected<void, Ichor::StartError>> start() final {
        _interceptor = GetThreadLocalManager().registerEventInterceptor<TestEvent>(this, this);
        _interceptorAll = GetThreadLocalManager().registerEventInterceptor<Event>(this, this);

        co_return {};
    }

    Task<void> stop() final {
        _interceptor.reset();
        _interceptorAll.reset();

        co_return;
    }

    bool preInterceptEvent(TestEvent const &evt) {
        if(!_addedPreIntercept) {
            GetThreadLocalManager().createServiceManager<AddInterceptorDuringEventHandlingService>();
            _addedPreIntercept = true;
        }

        return AllowOthersHandling;
    }

    void postInterceptEvent(TestEvent const &evt, bool processed) {
        if(!_addedPostIntercept) {
            GetThreadLocalManager().createServiceManager<AddInterceptorDuringEventHandlingService>();
            _addedPostIntercept = true;
        }
    }

    bool preInterceptEvent(Event const &evt) {
        if(!_addedPreInterceptAll) {
            GetThreadLocalManager().createServiceManager<AddInterceptorDuringEventHandlingService>();
            _addedPreInterceptAll = true;
        }

        return AllowOthersHandling;
    }

    void postInterceptEvent(Event const &evt, bool processed) {
        if(!_addedPostInterceptAll) {
            GetThreadLocalManager().createServiceManager<AddInterceptorDuringEventHandlingService>();
            _addedPostInterceptAll = true;
        }
    }

    EventInterceptorRegistration _interceptor{};
    EventInterceptorRegistration _interceptorAll{};
    static bool _addedPreIntercept;
    static bool _addedPostIntercept;
    static bool _addedPreInterceptAll;
    static bool _addedPostInterceptAll;
};
