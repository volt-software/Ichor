#include "celix_shim_library/CelixShimLibrary.h"
#include "celix_shim_library/CelixShimService.h"

#include <celix_properties.h>
#include "celix_constants.h"

std::atomic<uint64_t> Cppelix::CelixBundleContext::trackerIdCounter{0};
std::unordered_map<uint64_t, Cppelix::IService *> Cppelix::CelixBundleContext::registeredServices{};
std::unordered_multimap<uint64_t, CelixShimTracker> Cppelix::CelixBundleContext::registeredServiceTrackers{};
std::mutex Cppelix::CelixBundleContext::m{};

extern "C" {
celix_bundle_t* celix_bundleContext_getBundle(const celix_bundle_context_t *ctx) {
    return reinterpret_cast<celix_bundle_t*>(const_cast<celix_bundle_context_t*>(ctx));
}

long celix_bundle_getId(const celix_bundle_t* bnd) {
    auto bundleContext = reinterpret_cast<CelixBundleContext *>(const_cast<celix_bundle_t*>(bnd));
    auto lock = std::unique_lock(bundleContext->m);
    return bundleContext->getServiceId();
}

bool celix_bundleContext_useService(
        bundle_context_t *ctx,
        const char* serviceName,
        void *callbackHandle,
        void (*use)(void *handle, void *svc)) {
    auto bundleContext = reinterpret_cast<CelixBundleContext *>(ctx);
    auto lock = std::unique_lock(bundleContext->m);
    bool found = false;
    for(auto &[key, service] : bundleContext->registeredServices) {
        if(std::any_cast<std::string>(service->getProperties()->operator[]("CelixName")) == serviceName) {
            use(callbackHandle, std::any_cast<void*>(service->getProperties()->operator[]("CelixService")));
            found = true;
        }
    }
    LOG_INFO(bundleContext->_logger, "celix_bundleContext_useService: {} in {} services", found, bundleContext->registeredServices.size());
    return found;
}

long celix_bundleContext_trackServices(
        bundle_context_t* ctx,
        const char* serviceName,
        void* callbackHandle,
        void (*add)(void* handle, void* svc),
        void (*remove)(void* handle, void* svc)) {
    auto bundleContext = reinterpret_cast<CelixBundleContext *>(ctx);
    auto lock = std::unique_lock(bundleContext->m);
    auto tracker = bundleContext->registeredServiceTrackers.emplace(bundleContext->trackerIdCounter++, CelixShimTracker{serviceName, callbackHandle, add, remove});
    for(auto &[key, service] : bundleContext->registeredServices) {
        if(std::any_cast<std::string>(service->getProperties()->operator[]("CelixName")) == serviceName) {
            add(callbackHandle, std::any_cast<void*>(service->getProperties()->operator[]("CelixService")));
        }
    }
    return tracker->first;
}
void celix_bundleContext_stopTracker(bundle_context_t *ctx, long trackerId) {
    auto bundleContext = reinterpret_cast<CelixBundleContext *>(ctx);
    auto lock = std::unique_lock(bundleContext->m);
    auto tracker = bundleContext->registeredServiceTrackers.find(trackerId);
    if(tracker != end(bundleContext->registeredServiceTrackers)) {
        for (auto &[key, service] : bundleContext->registeredServices) {
            if (std::any_cast<std::string>(service->getProperties()->operator[]("CelixName")) == tracker->second.serviceName) {
                tracker->second.remove(tracker->second.callbackHandle, std::any_cast<void *>(service->getProperties()->operator[]("CelixService")));
            }
        }
        bundleContext->registeredServiceTrackers.erase(tracker);
    }
}

long celix_bundleContext_registerService(bundle_context_t *ctx, void *svc, const char *serviceName, celix_properties_t *properties) {
    auto bundleContext = reinterpret_cast<CelixBundleContext *>(ctx);
    auto lock = std::unique_lock(bundleContext->m);
    auto service = bundleContext->getManager()->createServiceManager<ICelixShimService, CelixShimService>(RequiredList<ILogger>, OptionalList<>, CppelixProperties{{"CelixName", std::string{serviceName}}, {"CelixService", svc}});
    bundleContext->registeredServices.emplace(service->getServiceId(), service);
    LOG_INFO(bundleContext->_logger, "celix_bundleContext_registerService: registered {}, {} total services", serviceName, bundleContext->registeredServices.size());
    return service->getServiceId();
}

void celix_bundleContext_unregisterService(bundle_context_t *ctx, long serviceId) {
    auto bundleContext = reinterpret_cast<CelixBundleContext *>(ctx);
    auto lock = std::unique_lock(bundleContext->m);
    auto service = bundleContext->registeredServices.find(serviceId);
    if(service != end(bundleContext->registeredServices)) {
        bundleContext->getManager()->pushEvent<RemoveServiceEvent>(serviceId, serviceId);
        bundleContext->registeredServices.erase(service);
    }
}
}