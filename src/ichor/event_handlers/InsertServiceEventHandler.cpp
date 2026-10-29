#include <ichor/DependencyManager.h>
#include <ichor/Filter.h>
#include <ichor/stl/Any.h>
#include <ichor/dependency_management/IServiceInterestedLifecycleManager.h>
#include "EventHandlerHelpers.h"

void Ichor::DependencyManager::handleEvent(InsertServiceEvent &evt, ProcessEventContext &) {
    INTERNAL_DEBUG("InsertServiceEvent {} {} {}:{}", evt.id, evt.priority, evt.originatingService, evt.mgr->implementationName());
    auto svcIt = _services.emplace(evt.originatingService, std::move(evt.mgr));
    auto &cmpMgr = svcIt.first->second;

    // If a service requests IService, we interpret it to mean a reference to itself, not just all services in existence.
    Ichor::v1::Detail::IServiceInterestedLifecycleManager selfMgr{cmpMgr->getIService()};
    {
        auto startBehaviour = cmpMgr->dependencyOnline(&selfMgr);

        if(startBehaviour == StartBehaviour::STARTED) {
            auto gen = cmpMgr->startAfterDependencyOnline();
            gen.set_service_id(cmpMgr->serviceId());
            gen.set_priority(std::min(cmpMgr->getPriority(), INTERNAL_DEPENDENCY_EVENT_PRIORITY));
            auto it = gen.begin();

            if(!it.get_finished()) {
                _scopedGenerators.emplace(it.get_promise_id(), ScopedGenerator{std::make_unique<AsyncGenerator<StartBehaviour>>(std::move(gen)), v1::make_reference_counted<DependencyOnlineEvent>(_eventQueue.getNextEventId(), cmpMgr->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt.priority))});
                // create new event that will be inserted upon finish of coroutine in ContinuableStartEvent
                // _scopedEvents.emplace(it.get_promise_id(), v1::make_reference_counted<DependencyOnlineEvent>(_eventQueue.getNextEventId(), cmpMgr->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt.priority)));
            } else if(it.get_value() == StartBehaviour::STARTED) {
                _eventQueue.pushPrioritisedEvent<DependencyOnlineEvent>(cmpMgr->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt.priority));
            }
        }
    }

    if(cmpMgr->getDependencyRegistry() == nullptr || cmpMgr->getDependencyRegistry()->empty()) {
        return;
    }

    // loop over all services, check if cmpMgr is interested in the active ones and inject them if so
    // Services that aren't active yet are only counted, a line per service would be quadratic in the number of services. They
    // get injected later on, when their own DependencyOnlineEvent visits cmpMgr.
    [[maybe_unused]] uint64_t notActive{};
    for (auto &[key, mgr] : _services) {
        if (mgr->getInterfaces().empty()) {
            continue;
        }
        if (mgr->getServiceState() != ServiceState::ACTIVE) {
            notActive++;
            continue;
        }

        auto const filterProp = mgr->getProperties().find("Filter");
        const Filter *filter = nullptr;
        if (filterProp != cend(mgr->getProperties())) {
            filter = Ichor::v1::any_cast<Filter const *>(&filterProp->second);
        }

        if (filter != nullptr && !filter->compareTo(*cmpMgr.get())) {
            if constexpr (DO_INTERNAL_DEBUG) {
                if (v1::Detail::requestsAnyInterfaceOf(*cmpMgr, *mgr)) {
                    INTERNAL_DEBUG("InsertServiceEvent {} {}:{} is interested in {}:{} but rejected by its filter '{}'", evt.id, cmpMgr->serviceId(), cmpMgr->implementationName(), key, mgr->implementationName(), filter->getDescription());
                }
            }
            continue;
        }

        auto startBehaviour = cmpMgr->dependencyOnline(mgr.get());

        INTERNAL_DEBUG("InsertServiceEvent {} {}:{} interested service is {}:{} startBehaviour {}", evt.id, cmpMgr->serviceId(), cmpMgr->implementationName(), key, mgr->implementationName(), startBehaviour);

        if(startBehaviour == StartBehaviour::DONE) {
            continue;
        }

        auto gen = cmpMgr->startAfterDependencyOnline();
        gen.set_service_id(cmpMgr->serviceId());
        gen.set_priority(std::min(cmpMgr->getPriority(), INTERNAL_DEPENDENCY_EVENT_PRIORITY));
        auto it = gen.begin();

        if(!it.get_finished()) {
            _scopedGenerators.emplace(it.get_promise_id(), ScopedGenerator{std::make_unique<AsyncGenerator<StartBehaviour>>(std::move(gen)), v1::make_reference_counted<DependencyOnlineEvent>(_eventQueue.getNextEventId(), cmpMgr->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt.priority))});
            // create new event that will be inserted upon finish of coroutine in ContinuableStartEvent
            // _scopedEvents.emplace(it.get_promise_id(), v1::make_reference_counted<DependencyOnlineEvent>(_eventQueue.getNextEventId(), cmpMgr->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt.priority)));
        } else if(it.get_value() == StartBehaviour::STARTED) {
            _eventQueue.pushPrioritisedEvent<DependencyOnlineEvent>(cmpMgr->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt.priority));
        }
    }

    INTERNAL_DEBUG("InsertServiceEvent {} {}:{} skipped {} services with interfaces that are not active yet", evt.id, cmpMgr->serviceId(), cmpMgr->implementationName(), notActive);
}
