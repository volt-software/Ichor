#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include <framework/interfaces/IFrameworkLogger.h>
#include "FrameworkListener.h"
#include "BundleListener.h"
#include "ServiceListener.h"
#include "Bundle.h"
#include "ComponentManager.h"

namespace Cppelix {

    struct ComponentManagerLifecycleLambdas {
        std::function<bool(void)> start;
        std::function<bool(void)> stop;
        std::function<bool(void)> dependencyOnline;
        std::function<bool(void)> dependencyOffline;
    };

    class DependencyManager {
    public:
        DependencyManager() : _components(), _logger(nullptr) {}

        template<class Interface, class Impl>
            requires Derived<Impl, Bundle>
        // [[nodiscard]] // bug in gcc 9.2 prevents usage here
        constexpr ComponentManager<Interface, Impl> createComponentManager() {
            auto cmpMgr = ComponentManager<Interface, Impl>::create("");

            if constexpr(std::is_same<Interface, IFrameworkLogger>::value) {
                _logger = &cmpMgr.getComponent();
            }

            if(_logger != nullptr) {
                LOG_DEBUG(_logger, "added ComponentManager<{}, {}>", typeName<Interface>(), typeName<Impl>());
            }
            _components.emplace_back(&cmpMgr);
            return cmpMgr;
        }

        void start() {
            if(_logger != nullptr) {
                LOG_DEBUG(_logger, "starting dm");
            }
            for(auto &lifecycleManager : _components) {
                if(_logger != nullptr) {
                    LOG_DEBUG(_logger, "starting lifecycleManager {}", lifecycleManager->name());
                }

                if(lifecycleManager->start()) {
                    continue;
                }

                for(auto &possibleDependentLifecycleManager : _components) {
                    possibleDependentLifecycleManager->dependencyOnline(lifecycleManager->name());
                }
            }
        }

    private:
        std::vector<LifecycleManager*> _components;
        IFrameworkLogger *_logger;
    };
}