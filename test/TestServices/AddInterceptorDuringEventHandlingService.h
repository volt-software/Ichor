#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/events/Event.h>
#include "../TestEvents.h"

using namespace Ichor;

struct AddInterceptorDuringEventHandlingService final : public AdvancedService<AddInterceptorDuringEventHandlingService> {
    AddInterceptorDuringEventHandlingService() = default;

    AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
        _interceptor = getManager().registerEventInterceptor<TestEvent>(this);
        _interceptorAll = getManager().registerEventInterceptor<Event>(this);

        co_return {};
    }

    AsyncGenerator<void> stop() final {
        _interceptor.reset();
        _interceptorAll.reset();

        co_return;
    }

    bool preInterceptEvent(TestEvent const &evt) {
        if(!_addedPreIntercept) {
            getManager().createServiceManager<AddInterceptorDuringEventHandlingService>();
            _addedPreIntercept = true;
        }

        return AllowOthersHandling;
    }

    void postInterceptEvent(TestEvent const &evt, bool processed) {
        if(!_addedPostIntercept) {
            getManager().createServiceManager<AddInterceptorDuringEventHandlingService>();
            _addedPostIntercept = true;
        }
    }

    bool preInterceptEvent(Event const &evt) {
        if(!_addedPreInterceptAll) {
            getManager().createServiceManager<AddInterceptorDuringEventHandlingService>();
            _addedPreInterceptAll = true;
        }

        return AllowOthersHandling;
    }

    void postInterceptEvent(Event const &evt, bool processed) {
        if(!_addedPostInterceptAll) {
            getManager().createServiceManager<AddInterceptorDuringEventHandlingService>();
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
