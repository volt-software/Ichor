#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <cassert>
#include <spdlog/spdlog.h>
#include <framework/interfaces/IFrameworkLogger.h>
#include "FrameworkListener.h"
#include "BundleListener.h"
#include "ServiceListener.h"
#include "Bundle.h"
#include "ComponentLifecycleManager.h"

namespace Cppelix {

    class DependencyManager {
    public:
        DependencyManager() : _components(), _logger(nullptr) {}

        template<class Interface, class Impl>
            requires Derived<Impl, Bundle>
        [[nodiscard]] // bug in gcc 9.2 prevents usage here
        std::shared_ptr<ComponentLifecycleManager<Interface, Impl>> createComponentManager(DependencyInfo dependencies) {
            auto cmpMgr = ComponentLifecycleManager<Interface, Impl>::create(_logger, "", dependencies);

            if constexpr (std::is_same<Interface, IFrameworkLogger>::value) {
                _logger = &cmpMgr->getComponent();
            }

            if(_logger != nullptr) {
                LOG_DEBUG(_logger, "added ComponentManager<{}, {}>", typeName<Interface>(), typeName<Impl>());
            }

            _components.emplace_back(cmpMgr);
            return cmpMgr;
        }

        void start() {
            assert(_logger != nullptr);
            LOG_DEBUG(_logger, "starting dm");
            for(auto &lifecycleManager : _components) {
                if(!lifecycleManager->shouldStart()) {
                    LOG_DEBUG(_logger, "lifecycleManager {} not ready to start yet", lifecycleManager->name());
                    continue;
                }

                LOG_DEBUG(_logger, "starting lifecycleManager {}", lifecycleManager->name());

                if (!lifecycleManager->start()) {
                    LOG_TRACE(_logger, "Couldn't start ComponentManager {}", lifecycleManager->name());
                    continue;
                }

                for(auto &possibleDependentLifecycleManager : _components) {
                    possibleDependentLifecycleManager->dependencyOnline(lifecycleManager->getSelfAsDependency());
                }
            }
        }

    private:
        std::vector<std::shared_ptr<LifecycleManager>> _components;
        IFrameworkLogger *_logger;
    };
}