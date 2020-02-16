#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
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

        template<class Interface, class Impl>
            requires Derived<Impl, Bundle>
        // [[nodiscard]] // bug in gcc 9.2 prevents usage here
        constexpr ComponentManager<Interface, Impl> createComponentManager() {
            auto cmpMgr = ComponentManager<Interface, Impl>::create("");
            SPDLOG_DEBUG("added lifecycleManager {}", lifecycleManager->name());
            _components.emplace_back(&cmpMgr);
            return cmpMgr;
        }

        void start() {
            SPDLOG_DEBUG("starting dm");
            for(auto &lifecycleManager : _components) {
                SPDLOG_DEBUG("starting lifecycleManager {}", lifecycleManager->name());
                if(lifecycleManager->start()) {
                    for(auto &possibleDependentLifecycleManager : _components) {
                        possibleDependentLifecycleManager->dependencyOnline(lifecycleManager->name());
                    }
                }
            }
        }

    private:
        std::vector<LifecycleManager*> _components;
    };
}