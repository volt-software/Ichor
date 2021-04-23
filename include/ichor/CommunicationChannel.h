#pragma once

#include <ichor/DependencyManager.h>

#ifdef DEBUG_CHANNEL
#include <iostream>
#endif

namespace Ichor {
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
            std::shared_lock l(_mutex);
            for(auto &[key, manager] : _managers) {
                if(manager->getId() == originatingManager->getId()) {
                    continue;
                }

#ifdef DEBUG_CHANNEL
                std::cout << "Inserting event " << typeName<EventT>() << " from manager " << originatingManager->getId() << " into manager " << manager->getId() << std::endl;
#endif
                manager->template pushEvent<EventT>(std::forward<Args>(args)...);
#ifdef DEBUG_CHANNEL
                std::cout << "Inserted event " << typeName<EventT>() << " from manager " << originatingManager->getId() << " into manager " << manager->getId() << std::endl;
#endif
            }
        }

        template <typename EventT, typename... Args>
        requires Derived<EventT, Event>
        void sendEventTo(uint64_t id, Args&&... args) {
            std::shared_lock l(_mutex);
            auto manager = _managers.find(id);

            if(manager == end(_managers)) {
                throw std::runtime_error("Couldn't find manager");
            }

            manager->second->pushEvent<EventT>(std::forward<Args>(args)...);
        }
    private:
        std::unordered_map<uint64_t, DependencyManager*> _managers{};
        RealtimeReadWriteMutex _mutex{};
    };
}