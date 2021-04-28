#pragma once

#include <spdlog/spdlog.h>
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include <ichor/optional_bundles/pubsub_bundle/PubsubZmqAdmin.h>

class PubsubAdminZmqService final : public IPubsubAdminService, public Service<PubsubAdminZmqService {
public:
    ~PubsubAdminZmqService() final = default;

    bool start() final {
        _publisherTrackerRegistration = getManager()->registerDependencyTracker<IPubsubPublisherService>(this);
        _subscriberTrackerRegistration = getManager()->registerDependencyTracker<IPubsubSubscriberService>(this);
        return true;
    }

    bool stop() final {
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
        ICHOR_LOG_TRACE(_logger, "Inserted logger");
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    void handleDependencyRequest(IPubsubPublisherService*, DependencyRequestEvent const * const evt) {
        auto scopeProp = evt->properties.find("scope");
        auto topicProp = evt->properties.find("topic");

        if(scopeProp == end(evt->properties) || topicProp == end(evt->properties)) {
            ICHOR_LOG_ERROR(_logger, "scope or topic missing");
            return;
        }

        auto scope = scopeProp->second->getAsString();
        auto topic = topicProp->second->getAsString();
        auto key = scope + ":" + topic;

        ICHOR_LOG_INFO(_logger, "Tracked IRuntimeCreatedService request for scope {} topic {}", scope, topic);

        auto runtimeService = _scopedPublishers.find(key);

        if(runtimeService == end(_scopedPublishers)) {
            _scopedPublishers.emplace(key, getManager()->createServiceManager<IPubsubPublisherService, ZmqPubsubPublisherService>(RequiredList<ILogger>, OptionalList<>, Properties{makeProperty<std::string>("scope", scope)}));
        }
    }

    void handleDependencyUndoRequest(IPubsubPublisherService*, DependencyUndoRequestEvent const * const evt) {
        auto scopeProp = evt->properties.find("scope");
        auto topicProp = evt->properties.find("topic");

        if(scopeProp == end(evt->properties) || topicProp == end(evt->properties)) {
            ICHOR_LOG_ERROR(_logger, "scope or topic missing");
            return;
        }

        auto scope = scopeProp->second->getAsString();
        auto topic = topicProp->second->getAsString();
        auto key = scope + ":" + topic;

        ICHOR_LOG_INFO(_logger, "Tracked IRuntimeCreatedService undo request for scope {} topic {}", scope, topic);

        _scopedPublishers.erase(key);
    }

    void handleDependencyRequest(IPubsubSubscriberService*, DependencyRequestEvent const * const evt) {
        auto scopeProp = evt->properties.find("scope");
        auto topicProp = evt->properties.find("topic");

        if(scopeProp == end(evt->properties) || topicProp == end(evt->properties)) {
            ICHOR_LOG_ERROR(_logger, "scope or topic missing");
            return;
        }

        auto scope = scopeProp->second->getAsString();
        auto topic = topicProp->second->getAsString();
        auto key = scope + ":" + topic;

        ICHOR_LOG_INFO(_logger, "Tracked IRuntimeCreatedService request for scope {} topic {}", scope, topic);

        auto runtimeService = _scopedSubscribers.find(key);

        if(runtimeService == end(_scopedSubscribers)) {
            _scopedSubscribers.emplace(key, getManager()->createServiceManager<IPubsubSubscriberService, ZmqPubsubPublisherService>(RequiredList<ILogger>, OptionalList<>, Properties{makeProperty<std::string>("scope", scope)}));
        }
    }

    void handleDependencyUndoRequest(IPubsubSubscriberService*, DependencyUndoRequestEvent const * const evt) {
        auto scopeProp = evt->properties.find("scope");
        auto topicProp = evt->properties.find("topic");

        if(scopeProp == end(evt->properties) || topicProp == end(evt->properties)) {
            ICHOR_LOG_ERROR(_logger, "scope or topic missing");
            return;
        }

        auto scope = scopeProp->second->getAsString();
        auto topic = topicProp->second->getAsString();
        auto key = scope + ":" + topic;

        ICHOR_LOG_INFO(_logger, "Tracked IRuntimeCreatedService undo request for scope {} topic {}", scope, topic);

        _scopedSubscribers.erase(key);
    }

private:
    ILogger *_logger;
    std::unique_ptr<DependencyTrackerRegistration> _publisherTrackerRegistration;
    std::unique_ptr<DependencyTrackerRegistration> _subscriberTrackerRegistration;
    std::unordered_map<std::string, std::shared_ptr<LifecycleManager>> _scopedPublishers;
    std::unordered_map<std::string, std::shared_ptr<LifecycleManager>> _scopedSubscribers;
};