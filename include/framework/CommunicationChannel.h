#pragma once

#include "DependencyManager.h"
#include <mutex>

namespace Cppelix {
    class CommunicationChannel {
    public:
        void addManager(DependencyManager* manager) {
            std::unique_lock l(_mutex);
            manager->setCommunicationChannel(this);
            _managers.try_emplace(manager->getId(), manager);
        }

        void removeManager(DependencyManager *manager) {
            std::unique_lock l(_mutex);
            manager->setCommunicationChannel(nullptr);
            _managers.erase(manager->getId());
        }

        template <typename EventT, typename... Args>
        requires Derived<EventT, Event>
        void broadcastEvent(DependencyManager *originatingManager, Args&&... args) {
            std::unique_lock l(_mutex);
            for(auto manager : _managers) {
                if(manager.second->getId() == originatingManager->getId()) {
                    continue;
                }

                manager.second->pushEvent<EventT>(std::forward<Args>(args)...);
            }
        }

        template <typename EventT, typename... Args>
        requires Derived<EventT, Event>
        void sendEventTo(uint64_t id, Args&&... args) {
            std::unique_lock l(_mutex);
            auto manager = _managers.find(id);

            if(manager == end(_managers)) {
                throw std::runtime_error("Couldn't find manager");
            }

            manager->second->pushEvent<EventT>(std::forward<Args>(args)...);
        }
    private:
        std::unordered_map<uint64_t, DependencyManager*> _managers{};
        std::mutex _mutex{};
    };
}