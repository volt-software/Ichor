#pragma once

#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <optional_bundles/logging_bundle/Logger.h>
#include "framework/Service.h"
#include "framework/LifecycleManager.h"
#include "PubsubZmqAdmin.h"

class PubsubAdminZmqService : public IPubsubAdminService, public Service {
public:
    ~PubsubAdminZmqService() final = default;

    bool start() final {
        LOG_INFO(_logger, "PubsubAdminZmqService started");
        _publisherTrackerRegistration = getManager()->registerDependencyTracker<IPubsubPublisherService>(getServiceId(), this);
        _subscriberTrackerRegistration = getManager()->registerDependencyTracker<IPubsubSubscriberService>(getServiceId(), this);
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "PubsubAdminZmqService stopped");
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
        LOG_TRACE(_logger, "Inserted logger");
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    void handleDependencyRequest(IPubsubPublisherService*, DependencyRequestEvent const * const evt) {
        auto scopeProp = evt->properties.find("scope");
        auto topicProp = evt->properties.find("topic");

        if(scopeProp == end(evt->properties) || topicProp == end(evt->properties)) {
            LOG_ERROR(_logger, "scope or topic missing");
            return;
        }

        auto scope = scopeProp->second->getAsString();
        auto topic = topicProp->second->getAsString();
        auto key = scope + ":" + topic;

        LOG_INFO(_logger, "Tracked IRuntimeCreatedService request for scope {} topic {}", scope, topic);

        auto runtimeService = _scopedPublishers.find(key);

        if(runtimeService == end(_scopedPublishers)) {
            _scopedPublishers.emplace(key, getManager()->createServiceManager<IPubsubPublisherService, ZmqPubsubPublisherService>(RequiredList<ILogger>, OptionalList<>, CppelixProperties{makeProperty<std::string>("scope", scope)}));
        }
    }

    void handleDependencyUndoRequest(IPubsubPublisherService*, DependencyUndoRequestEvent const * const evt) {
        auto scopeProp = evt->properties.find("scope");
        auto topicProp = evt->properties.find("topic");

        if(scopeProp == end(evt->properties) || topicProp == end(evt->properties)) {
            LOG_ERROR(_logger, "scope or topic missing");
            return;
        }

        auto scope = scopeProp->second->getAsString();
        auto topic = topicProp->second->getAsString();
        auto key = scope + ":" + topic;

        LOG_INFO(_logger, "Tracked IRuntimeCreatedService undo request for scope {} topic {}", scope, topic);

        _scopedPublishers.erase(key);
    }

    void handleDependencyRequest(IPubsubSubscriberService*, DependencyRequestEvent const * const evt) {
        auto scopeProp = evt->properties.find("scope");
        auto topicProp = evt->properties.find("topic");

        if(scopeProp == end(evt->properties) || topicProp == end(evt->properties)) {
            LOG_ERROR(_logger, "scope or topic missing");
            return;
        }

        auto scope = scopeProp->second->getAsString();
        auto topic = topicProp->second->getAsString();
        auto key = scope + ":" + topic;

        LOG_INFO(_logger, "Tracked IRuntimeCreatedService request for scope {} topic {}", scope, topic);

        auto runtimeService = _scopedSubscribers.find(key);

        if(runtimeService == end(_scopedSubscribers)) {
            _scopedSubscribers.emplace(key, getManager()->createServiceManager<IPubsubSubscriberService, ZmqPubsubPublisherService>(RequiredList<ILogger>, OptionalList<>, CppelixProperties{makeProperty<std::string>("scope", scope)}));
        }
    }

    void handleDependencyUndoRequest(IPubsubSubscriberService*, DependencyUndoRequestEvent const * const evt) {
        auto scopeProp = evt->properties.find("scope");
        auto topicProp = evt->properties.find("topic");

        if(scopeProp == end(evt->properties) || topicProp == end(evt->properties)) {
            LOG_ERROR(_logger, "scope or topic missing");
            return;
        }

        auto scope = scopeProp->second->getAsString();
        auto topic = topicProp->second->getAsString();
        auto key = scope + ":" + topic;

        LOG_INFO(_logger, "Tracked IRuntimeCreatedService undo request for scope {} topic {}", scope, topic);

        _scopedSubscribers.erase(key);
    }

private:
    ILogger *_logger;
    std::unique_ptr<DependencyTrackerRegistration> _publisherTrackerRegistration;
    std::unique_ptr<DependencyTrackerRegistration> _subscriberTrackerRegistration;
    std::unordered_map<std::string, std::shared_ptr<LifecycleManager>> _scopedPublishers;
    std::unordered_map<std::string, std::shared_ptr<LifecycleManager>> _scopedSubscribers;
};