#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/events/RunFunctionEvent.h>
#include "UselessService.h"
#include <ichor/ScopedServiceProxy.h>

using namespace Ichor::v1;

namespace Ichor {
	struct IRemoveAfterAwaitedStopService {
	};

	template <bool PUSH_STOP>
	struct RemoveAfterAwaitedStopService final : public IRemoveAfterAwaitedStopService, public AdvancedService<RemoveAfterAwaitedStopService<PUSH_STOP>> {
		RemoveAfterAwaitedStopService(DependencyRegister &reg, Properties props) : AdvancedService<RemoveAfterAwaitedStopService<PUSH_STOP>>(std::move(props)) {
			reg.registerDependency<IUselessService>(this, DependencyFlags::REQUIRED);
		}
		~RemoveAfterAwaitedStopService() final = default;

		Task<tl::expected<void, Ichor::StartError>> start() final {
			if constexpr (PUSH_STOP) {
				GetThreadLocalEventQueue().pushPrioritisedEvent<StopServiceEvent>(AdvancedService<RemoveAfterAwaitedStopService<PUSH_STOP>>::getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, AdvancedService<RemoveAfterAwaitedStopService<PUSH_STOP>>::getServiceId(), true);
			}
			co_return {};
		}

		Task<void> stop() final {
			GetThreadLocalEventQueue().pushPrioritisedEvent<RunFunctionEvent>(AdvancedService<RemoveAfterAwaitedStopService<PUSH_STOP>>::getServiceId(), 1, [this]() {
				_evt.set();
			});
			co_await _evt;
			GetThreadLocalEventQueue().pushPrioritisedEvent<QuitEvent>(AdvancedService<RemoveAfterAwaitedStopService<PUSH_STOP>>::getServiceId(), 20'000);

			co_return;
		}

		void addDependencyInstance(Ichor::ScopedServiceProxy<IUselessService*>, IService &) {
		}

		void removeDependencyInstance(Ichor::ScopedServiceProxy<IUselessService*>, IService&) {
		}

		Ichor::AsyncManualResetEvent _evt;
	};

	struct DependingOnRemoveAfterAwaitedStopService final : public AdvancedService<DependingOnRemoveAfterAwaitedStopService> {
		DependingOnRemoveAfterAwaitedStopService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
			reg.registerDependency<IRemoveAfterAwaitedStopService>(this, DependencyFlags::REQUIRED);
		}
		~DependingOnRemoveAfterAwaitedStopService() final = default;

		Task<tl::expected<void, Ichor::StartError>> start() final {
			GetThreadLocalEventQueue().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, _svcId, true);
			co_return {};
		}

		Task<void> stop() final {
			GetThreadLocalEventQueue().pushPrioritisedEvent<RunFunctionEvent>(getServiceId(), 20'000, [this]() {
				_evt.set();
			});
			co_await _evt;
			co_return;
		}

		void addDependencyInstance(Ichor::ScopedServiceProxy<IRemoveAfterAwaitedStopService*>, IService &svc) {
			_svcId = svc.getServiceId();
		}

		void removeDependencyInstance(Ichor::ScopedServiceProxy<IRemoveAfterAwaitedStopService*>, IService&) {
		}

		ServiceIdType _svcId{};
		Ichor::AsyncManualResetEvent _evt;
	};
}
